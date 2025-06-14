#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/apps/lwiperf.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <lwip/sockets.h>
#include "tcp_server.h"
#include "pico/bootrom.h"
#include "dht22.h"
#if WATERING_ENABLED == 1
#include "hardware/adc.h"
#endif
#include "config.h"

static int server_connection = -1;

#if WATCHDOG_ENABLED == 1
static void reset_watchdog_timer() {
    last_ping_time = xTaskGetTickCount();
    printf("WATCHDOG: timer reset.\n");
}

static void watchdog_task(void *pvParameters) {
    while (true) {
        // Check if the timeout has been exceeded
        if ((xTaskGetTickCount() - last_ping_time) > watchdog_timeout) {
            printf("WATCHDOG: timeout! Rebooting the board...\n");
            reboot();
        }
        vTaskDelay(pdMS_TO_TICKS(watering_sensor_delay_ms)); // Check every second
    }
}

static void start_watchdog_timer() {
    last_ping_time = xTaskGetTickCount(); // Initialize the timer
    if (watchdog_task_handle == NULL) {
        xTaskCreate(watchdog_task, "WatchdogTask", 1024, NULL, 1, &watchdog_task_handle);
        printf("WATCHDOG: task started.\n");
    }
}
#endif

#if DHT_ENABLED == 1
static bool push_dht_all = false; // push all data to client (sync)

static  void print_dht_error(int i, const char* error_message) {
    printf("DHT: DHT(%d): %s\n", i, error_message);
}

static void send_dht_data_to_client(int i) {
    char msg[100];
    sprintf(msg, "{\"dht_id\": \"%d\", \"temp\": \"%.1f\", \"hum\": \"%.1f\"}", dht_pins[i], dht_dev[i].temp_celsius, dht_dev[i].humidity);
    tcp_send_message(server_connection, msg);
}

static uint8_t dht_read_safe(uint dht_id)
{
    portDISABLE_INTERRUPTS();
    uint8_t status = dht_read(&dht_dev[dht_id]);
    portENABLE_INTERRUPTS();

    return status;
}

static void temp_task()
{
    uint8_t dht_result = 0;
    const uint8_t force_sync = 10;
    uint8_t force_sync_cnt = force_sync;

    while (true) {
        gpio_put(dht_power_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));

        for(int i = 0; i < DHT_COUNT; i++)
        {
            dht_reading_t old = dht_dev[i];
            dht_result = dht_read_safe(i);

            switch (dht_result) {
                case DHT_ERR_CHECKSUM:
                    print_dht_error(dht_pins[i], "Bad checksum.");
                    dht_err[i]++;
                    break;
                case DHT_ERR_NAN:
                    print_dht_error(dht_pins[i], "Invalid value (nan).");
                    dht_err[i]++;
                    break;
                case DHT_ERR_TIMEOUT:
                    print_dht_error(dht_pins[i], "Sensor is not responding.");
                    dht_err[i]++;
                    break;
                case DHT_OK:
                    if(dht_dev[i].temp_celsius < 10.0f || dht_dev[i].temp_celsius > 60.0f || dht_dev[i].humidity < 20.0f || dht_dev[i].humidity > 95.0f) {
                        printf("DHT: Invalid value: DHT(%d): %.1f C, %.1f%% humidity\n", i, dht_dev[i].temp_celsius, dht_dev[i].humidity);
                        dht_err[i]++;
                        break;
                    }
                    dht_err[i] = 0;
                    printf("DHT: DHT(%d): %.1f C, %.1f%% humidity\n", dht_pins[i], dht_dev[i].temp_celsius, dht_dev[i].humidity);
                    send_dht_data_to_client(i);
                    break;
            }
            // 200 ms between reads
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (force_sync_cnt == 0) {
            push_dht_all = true;
            force_sync_cnt = force_sync;
        } else {
            force_sync_cnt--;
            push_dht_all = false;
        }

        // Read every 30 sec minute
        gpio_put(dht_power_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(dht_delay_ms));
    }
}
#endif

#if CONT_ENABLED == 1
static bool push_con_all = false; // push all data to client (sync)

static bool read_contactron(uint contactron_id)
{
    return gpio_get(contractron_pins[contactron_id]);
}

static void contactron_task()
{
    // Read all contactrons states and send changed states by udp
    char msg[50];

    while (true) {
        for (int i = 0; i < CONT_COUNT; i++) {
            bool state = read_contactron(i);
            if (state != contractron_state[i] || push_con_all) {
                printf("CONT: Contactron[%d] %s\n", i, state == 0 ? "Closed" : "Open");
                contractron_state[i] = state;
                snprintf(msg, 50, "{\"cont_id\":\"%d\", \"val\":\"%d\"}", i, state);
                tcp_send_message(server_connection, msg);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        push_con_all = false;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
#endif

#if WATERING_ENABLED == 1
static TaskHandle_t watering_task_handle = NULL;
static void watering_task()
{
    // Detekt rain using DFRobot SEN0308 sensor
    char msg[50];

    while (true) {
        //uint16_t result = 666;//adc_read();
        //uint adc_percentage = (result * 100) / 4096;
        uint8_t rain_status = gpio_get(watering_rain_pin);
        if (rain_status == 0) {
            printf("WATERING: Detected rain, watering disabled.\n");
            gpio_put(watering_power_pin, 1); // Disable watering power
        } else {
            gpio_put(watering_power_pin, 0); // Enable watering power
        }

        uint8_t any_zone_active = 0;
        for (int i = 0; i < WATERING_ZONE_COUNT; i++) {
            any_zone_active |= !gpio_get(watering_zone_pins[i]);
        }
        if (!any_zone_active) {
            printf("WATERING: No zones are active.\n");
            gpio_put(watering_power_pin, 1); // Disable watering power
        } else {

        }

        printf("WATERING: Sensor: %d%%\n", rain_status);
        sprintf(msg, "{\"rain_status\": \"%d\", \"value\": \"%d\"}", 0, rain_status);
        tcp_send_message(server_connection, msg);
        vTaskDelay(pdMS_TO_TICKS(watering_sensor_delay_ms));
    }
}
#endif

#if BEEPER_ENABLED == 1
static void beeper_task()
{
    static int task_delay = 100;
    static int step = 0;
    static bool print_flag = true;

    while (true) {
        if(beeper_state != next_beeper_state) {
            if(step == 0) {
                printf("BEEP: state: %d\n", beeper_state);
                beeper_state = next_beeper_state;
                step = 0;
            }
        }
        switch(beeper_state) {
            case ALARM_OFF:
                if(print_flag){
                    printf("BEEP: OFF\n");
                    print_flag = false;
                }
                task_delay = 100;
                break;
            case ALARM_ARM:
                print_flag = true;
                switch(step) {
                    case 0:
                        printf("BEEP: ARM\n");
                        gpio_put(beeper_pin, 1);
                        task_delay = 300;
                        step++;
                    break;
                    case 1:
                        gpio_put(beeper_pin, 0);
                        task_delay = 500;
                        step++;
                    break;
                    case 2:
                        gpio_put(beeper_pin, 1);
                        task_delay = 1000;
                        step++;
                    break;
                    case 3:
                        gpio_put(beeper_pin, 0);
                        task_delay = 100;
                        step = 0;
                        next_beeper_state = ALARM_OFF;
                    break;
                }
                break;
            case ALARM_DISARM:
                print_flag = true;
                switch(step) {
                    case 0:
                        printf("BEEP: DISARM\n");
                        gpio_put(beeper_pin, 1);
                        task_delay = 1500;
                        step++;
                    break;
                    case 1:
                        gpio_put(beeper_pin, 0);
                        task_delay = 100;
                        step = 0;
                        next_beeper_state = ALARM_OFF;
                    break;
                }
                break;
            case ALARM_DETECTION:
                print_flag = true;
                switch(step) {
                    case 0:
                        printf("BEEP: DETECTION\n");
                        gpio_put(beeper_pin, 1);
                        task_delay = 300;
                        step++;
                    break;
                    case 1:
                        gpio_put(beeper_pin, 0);
                        task_delay = 300;
                        step = 0;
                    break;
                }
                break;
            case ALARM_RUN:
                print_flag = true;
                switch(step) {
                    case 0:
                        printf("BEEP: RUN\n");
                        gpio_put(beeper_pin, 1);
                        task_delay = 1000;
                        step++;
                    break;
                    case 1:
                        gpio_put(beeper_pin, 0);
                        task_delay = 500;
                        step = 0;
                    break;
                }
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay));
    }
}
#endif

static void on_tcp_connection(int con_id)
{
    printf("TCP: connected, connection id: %d\n", con_id);
    cyw43_arch_gpio_put(0, true);
    server_connection = con_id;
#if DHT_ENABLED == 1
    push_dht_all = true;
    printf("DHT: Start temp task.\n");
    xTaskCreate(temp_task, "TEMP_Task", 1024, NULL, TCP_TASK_PRIORITY+1, NULL);
#endif
#if CONT_ENABLED == 1
    push_con_all = true;
    printf("CONT: Start contactron task.\n");
    xTaskCreate(contactron_task, "CONT_Task", 1024, NULL, TCP_TASK_PRIORITY-1, NULL);
#endif
#if BEEPER_ENABLED == 1
    beeper_state = ALARM_OFF;
    next_beeper_state = ALARM_OFF;
    printf("BEEP: Start beeper task.\n");
    xTaskCreate(beeper_task, "BEEP_Task", 1024, NULL, TCP_TASK_PRIORITY+2, NULL);
#endif

#if WATERING_ENABLED == 1
    if (watering_task_handle != NULL) {
        vTaskDelete(watering_task_handle);
        watering_task_handle = NULL;
        printf("WATERING: Deleted existing watering task.\n");
    }
    if (xTaskCreate(watering_task, "WATERING_Task", 1024, NULL, TCP_TASK_PRIORITY-1, &watering_task_handle) == pdPASS) {
        printf("WATERING: Created watering task.\n");
    } else {
        printf("WATERING: Failed to create watering task.\n");
    }
#endif

#if WATCHDOG_ENABLED == 1
    start_watchdog_timer();
#endif
}

static void on_tcp_disconnection(int con_id)
{
    printf("TCP: disconnected, connection id: %d\n", con_id);
    cyw43_arch_gpio_put(0, false);
    server_connection = -1;
}

static void on_tcp_msg_received(int con_id, char* msg)
{
    char command[10];
    int zone_id, state;

    printf("TCP: < (%d): %s\n", con_id, msg);

    if (strcmp(msg, "flash") == 0) {
        reset_usb_boot(0, 0);
    } else if (strcmp(msg, "reboot") == 0) {
        reboot();
    } else if (strcmp(msg, "Hello!") == 0) {
        printf("TCP: Hello from client :-)\n");
    } else if (strcmp(msg, "sync") == 0) {
#if DHT_ENABLED == 1
        push_dht_all = true;
#endif
    }
    else if (strcmp(msg, "ping") == 0) {
        tcp_send_message(server_connection, "pong");
#if WATCHDOG_ENABLED == 1
        reset_watchdog_timer();
#endif
    }
#if GATE_ENABLED == 1
    else if (strcmp(msg, "brama") == 0) {
        gpio_put(gate_pin, 1);
        sleep_ms(300);
        gpio_put(gate_pin, 0);
    }
#endif
#if BEEPER_ENABLED == 1
    else if (strcmp(msg, "alarm_off") == 0) {
        next_beeper_state = ALARM_OFF;
    }
    else if (strcmp(msg, "alarm_arm") == 0) {
        next_beeper_state = ALARM_ARM;
    }
    else if (strcmp(msg, "alarm_disarm") == 0) {
        next_beeper_state = ALARM_DISARM;
    }
    else if (strcmp(msg, "alarm_detect") == 0) {
        next_beeper_state = ALARM_DETECTION;
    }
    else if (strcmp(msg, "alarm_run") == 0) {
        next_beeper_state = ALARM_RUN;
    }
#endif
#if HEAT_ENABLED == 1 || WATERING_ENABLED == 1
    else if (sscanf(msg, "%s %d %d", command, &zone_id, &state) == 3) {
        bool recognized = false;

#if HEAT_ENABLED == 1
        if (strcmp(command, "heat") == 0) {
            gpio_put(heat_zone_pins[zone_id], state ? 0 : 1); // Low trigger
            printf("HEAT: Zone[%d]: %s\n", zone_id, state == 0 ? "Disabled" : "Enabled");
            recognized = true;
        }
#endif

#if WATERING_ENABLED == 1
        if (strcmp(command, "water") == 0) {
            if(state == 1) {
                gpio_put(watering_power_pin, 0);
            }
            gpio_put(watering_zone_pins[zone_id], state ? 0 : 1); // Low trigger
            printf("WATER: Zone[%d]: %s\n", zone_id, state == 0 ? "Disabled" : "Enabled");
            recognized = true;
        }
#endif

        if (!recognized) {
            printf("ERROR: Unknown command: %s\n", command);
        }
    } else {
        printf("ERROR: Invalid message\n");
    }
#endif
}

int main(void)
{
    stdio_init_all();

    printf("--- Starting HomeCtrl ---\n");

    printf("--> Init stage\n");

#if DHT_ENABLED == 1
    // Init DHT sensors
    for (int i = 0; i < DHT_COUNT; i++) {
        dht_dev[i] = dht22_init(dht_pins[i]);
    }

    // Init dht power pin
    gpio_init(dht_power_pin);
    gpio_set_dir(dht_power_pin, GPIO_OUT);
    gpio_put(dht_power_pin, 0);
#endif

#if CONT_ENABLED == 1
    // Init contactrons
    for (int i = 0; i < CONT_COUNT; i++) {
        gpio_init(contractron_pins[i]);
        gpio_set_dir(contractron_pins[i], GPIO_IN);
        gpio_pull_up(contractron_pins[i]);
    }
#endif

#if HEAT_ENABLED == 1
    // Init heating zones
    for (int i = 0; i < HEAT_ZONE_COUNT; i++) {
        gpio_init(heat_zone_pins[i]);
        gpio_set_dir(heat_zone_pins[i], GPIO_OUT);
        gpio_put(heat_zone_pins[i], 1);
    }
#endif

#if WATERING_ENABLED == 1
    // Init watering zones and sensors
    for (int i = 0; i < WATERING_ZONE_COUNT; i++) {
        gpio_init(watering_zone_pins[i]);
        gpio_set_dir(watering_zone_pins[i], GPIO_OUT);
        gpio_put(watering_zone_pins[i], 1);

    }
    adc_gpio_init(watering_sensor_pin);
    adc_select_input(0);

    gpio_init(watering_power_pin);
    gpio_set_dir(watering_power_pin, GPIO_OUT);
    gpio_put(watering_power_pin, 1);

    gpio_init(watering_rain_pin);
    gpio_set_dir(watering_rain_pin, GPIO_IN);
    gpio_pull_up(watering_rain_pin);
#endif

#if GATE_ENABLED == 1
    // Init gate control pin
    gpio_init(gate_pin);
    gpio_set_dir(gate_pin, GPIO_OUT);
    gpio_put(gate_pin, 0);
#endif

#if BEEPER_ENABLED == 1
    // Init gate control pin
    gpio_init(beeper_pin);
    gpio_set_dir(beeper_pin, GPIO_OUT);
    gpio_put(beeper_pin, 0);
#endif

    sleep_ms(1000);

    printf("--> Run Scheduler \n");

    // Init TCP server
    tcp_server_init(on_tcp_connection, on_tcp_disconnection, on_tcp_msg_received);

    xTaskCreate(tcp_server_task, "tcp_server_task", 2*configMINIMAL_STACK_SIZE, NULL, TCP_TASK_PRIORITY, NULL);

    vTaskStartScheduler();
}