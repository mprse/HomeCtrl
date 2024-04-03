#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/apps/lwiperf.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <lwip/sockets.h>
#include"tcp_server.h"
#include "pico/bootrom.h"
#include "dht22.h"


#define DHT_COUNT 10
#define DHT_ERR_TRESHOLD 5
static const uint dht_pins[DHT_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static uint dht_err[DHT_COUNT] = {0};
static dht_reading_t dht_dev[DHT_COUNT];
static const uint dht_power_pin = 26;

#define CONT_COUNT 7
static const uint contractron_pins[CONT_COUNT] = {16, 17, 18, 19, 20, 21, 22};
static uint contractron_state[CONT_COUNT] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


static const uint gate_pin = 15;


static int server_connection = -1;

static bool push_dht_all = false; // push all data to client (sync)
static bool push_con_all = false; // push all data to client (sync)

static  void print_dht_error(int i, const char* error_message) {
    printf("DHT(%d): %s\n", i, error_message);
}

static void send_dht_data_to_client(int i) {
    char msg[100];
    sprintf(msg, "{\"dht_id\": \"%d\", \"temp\": \"%.1f\", \"hum\": \"%.1f\"}", i, dht_dev[i].temp_celsius, dht_dev[i].humidity);
    tcp_send_message(server_connection, msg);
}

static uint8_t dht_read_safe(uint dht_id)
{
    portDISABLE_INTERRUPTS();
    uint8_t status = dht_read(&dht_dev[dht_id]);
    portENABLE_INTERRUPTS();

    return status;
}

static bool read_contactron(uint contactron_id)
{
    return gpio_get(contractron_pins[contactron_id]);
}

static void temp_task()
{
    uint8_t dht_result = 0;
    const uint8_t force_sync = 10;
    uint8_t force_sync_cnt = force_sync;

    while (true) {
        gpio_put(dht_power_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(500));

        for(int i = 0; i < DHT_COUNT; i++)
        {
            dht_reading_t old = dht_dev[i];
            dht_result = dht_read_safe(i);

            switch (dht_result) {
                case DHT_ERR_CHECKSUM:
                    print_dht_error(i, "Bad checksum.");
                    dht_err[i]++;
                    break;
                case DHT_ERR_NAN:
                    print_dht_error(i, "Invalid value (nan).");
                    dht_err[i]++;
                    break;
                case DHT_ERR_TIMEOUT:
                    print_dht_error(i, "Sensor is not responding.");
                    dht_err[i]++;
                    break;
                case DHT_OK:
                    dht_err[i] = 0;
                    printf("DHT(%d): %.1f C, %.1f%% humidity\n", i, dht_dev[i].temp_celsius, dht_dev[i].humidity);
                    if (push_dht_all || (dht_dev[i].humidity != old.humidity || dht_dev[i].temp_celsius != old.temp_celsius)) {
                        send_dht_data_to_client(i);
                    }
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
        
        for (int i = 0; i < DHT_COUNT; i++) {
            if ((i==0 || i==1 || i==2 || i==4 || i==5 || i==6) && dht_err[i] >= DHT_ERR_TRESHOLD) {
                printf("DHT RESET id: %d\n", i);
                gpio_put(dht_power_pin, 0);
                dht_err[i] = 0;
            }
        }
        // Read every 1 minute
        gpio_put(dht_power_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

static void contactron_task()
{
    // Read all contactrons states and send changed states by udp
    char msg[50];

    while (true) {
        for (int i = 0; i < CONT_COUNT; i++) {
            bool state = read_contactron(i);
            if (state != contractron_state[i] || push_con_all) {
                printf("Contactron %d changed state to %d\n", i, state);
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


static void on_tcp_connection(int con_id)
{
    printf("TCP connected, connection id: %d\n", con_id);
    cyw43_arch_gpio_put(0, true);
    server_connection = con_id;
    push_dht_all = true;
    push_con_all = true;
    xTaskCreate(temp_task, "TEMP_Task", 1024, NULL, TCP_TASK_PRIORITY+1, NULL);
    xTaskCreate(contactron_task, "CONT_Task", 1024, NULL, TCP_TASK_PRIORITY-1, NULL);
}

static void on_tcp_disconnection(int con_id)
{
    printf("TCP disconnected, connection id: %d\n", con_id);
    cyw43_arch_gpio_put(0, false);
    server_connection = -1;
}

static void on_tcp_msg_received(int con_id, char* msg)
{
    printf("msg(%d): %s\n", con_id, msg);

    if (strcmp(msg, "flash") == 0) {
        reset_usb_boot(0, 0);
    } else if (strcmp(msg, "reboot") == 0) {
        reboot();
    } else if (strcmp(msg, "sync") == 0) {
        push_dht_all = true; 
    } else if (strcmp(msg, "brama") == 0) {
        gpio_put(gate_pin, 1);
        sleep_ms(300);
        gpio_put(gate_pin, 0);
    }
}

int main(void)
{
    stdio_init_all();

    printf("--- Starting HomeCtrl ---\n");

    printf("Init sensors\n");

    // Init DHT sensors
    for (int i = 0; i < DHT_COUNT; i++) {
        dht_dev[i] = dht22_init(dht_pins[i]);
    }

    // Init contactrons
    for (int i = 0; i < CONT_COUNT; i++) {
        gpio_init(contractron_pins[i]);
        gpio_set_dir(contractron_pins[i], GPIO_IN);
        gpio_pull_up(contractron_pins[i]);
    }

    gpio_init(dht_power_pin);
    gpio_set_dir(dht_power_pin, GPIO_OUT);
    gpio_put(dht_power_pin, 0);

    gpio_init(gate_pin);
    gpio_set_dir(gate_pin, GPIO_OUT);
    gpio_put(gate_pin, 0);

    printf("Wait 1 sec before connecting to wifi \n");
    sleep_ms(1000);
    printf("xxx \n");

    // Init TCP server
    tcp_server_init(on_tcp_connection, on_tcp_disconnection, on_tcp_msg_received);

    xTaskCreate(tcp_server_task, "tcp_server_task", 2*configMINIMAL_STACK_SIZE, NULL, TCP_TASK_PRIORITY, NULL);

    vTaskStartScheduler();
}