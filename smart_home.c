#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/apps/lwiperf.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <lwip/sockets.h>
#include <dht.h>
#include"tcp_server.h"
#include "pico/bootrom.h"

typedef struct {
    float humidity;
    float temperature;
} dht_data_t;

#define DHT_COUNT 10
static const uint dht_pins[DHT_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const dht_model_t dht_model = DHT22;
static dht_t dht_dev[DHT_COUNT];
static dht_data_t dht_values[DHT_COUNT] = {0};

#define CONT_COUNT 9
static const uint contractron_pins[CONT_COUNT] = {16, 17, 18, 19, 20, 21, 22, 26, 27};
static uint contractron_state[CONT_COUNT] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static int server_connection = -1;

static uint push_all = 0;

static  void print_dht_error(int i, const char* error_message) {
    printf("DHT(%d): %s\n", i, error_message);
}

static void send_dht_data_to_client(int i, dht_data_t dht_date) {
    char msg[100];
    sprintf(msg, "{\"dht_id\": %d, \"temperature\": %.1f, \"humidity\": %.1f}", i, dht_date.temperature, dht_date.humidity);
    tcp_send_message(server_connection, msg);
    printf("DHT(%d): %.1f C, %.1f%% humidity\n", i, dht_date.temperature, dht_date.humidity);
}

static dht_result_t read_dht(uint dht_id, dht_data_t* dht_data)
{
    float humidity;
    float temperature_c;
    dht_data_t data = {0};

    dht_start_measurement(&dht_dev[dht_id]);

    dht_result_t result = dht_finish_measurement_blocking(&dht_dev[dht_id], &humidity, &temperature_c);
    if (result == DHT_RESULT_OK) {
        dht_data->humidity = humidity;
        dht_data->temperature = temperature_c;
    } else {
        dht_data->humidity = 0;
        dht_data->temperature = 0;
    }

    return result;
}

static bool read_contactron(uint contactron_id)
{
    return gpio_get(contractron_pins[contactron_id]);
}

void temp_task()
{
    dht_data_t new_data = {0};
    dht_result_t dht_result = 0;

    while (true) {
        for(int i = 0; i < DHT_COUNT; i++)
        {
            dht_result = read_dht(i, &new_data);
            if (dht_result == DHT_RESULT_BAD_CHECKSUM) {
                print_dht_error(i, "Bad checksum - repeat.");
                dht_result = read_dht(i, &new_data);
            }

            switch (dht_result) {
                case DHT_RESULT_BAD_CHECKSUM:
                    print_dht_error(i, "Bad checksum - skip.");
                    break;
                case DHT_RESULT_TIMEOUT:
                    print_dht_error(i, "Sensor not responding.");
                    break;
                case DHT_RESULT_OK:
                    if (push_all || (new_data.humidity != dht_values[i].humidity || new_data.temperature != dht_values[i].temperature)) {
                        dht_values[i] = new_data;
                        send_dht_data_to_client(i, new_data);
                    }
                    break;
            }
        }
        push_all = 0;       
        vTaskDelay(10000);
    }
}

void contactron_task()
{
    // Read all contactrons states and send changed states by udp
    char msg[50];
    while (true) {
        for (int i = 0; i < CONT_COUNT; i++) {
            bool state = read_contactron(i);
            if (state != contractron_state[i]) {
                printf("Contactron %d changed state to %d\n", i, state);
                contractron_state[i] = state;
                snprintf(msg, 50, "{\"cont_id\":\"%d\", \"val\":\"%d\"}", i, state);
                tcp_send_message(server_connection, msg);
            }
        }
    }   

    vTaskDelay(10000);
}


static void on_tcp_connection(int con_id)
{
    printf("TCP connected, connection id: %d\n", con_id);
    cyw43_arch_gpio_put(0, true);
    server_connection = con_id;
    push_all = 1;
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
    }
}

int main(void)
{
    stdio_init_all();

    printf("--- Starting HomeCtrl ---\n");

    // Init DHT sensors
    for (int i = 0; i < DHT_COUNT; i++) {
        dht_init(&dht_dev[i], dht_model, pio0, dht_pins[i], true /* pull_up */);
    }

    // Init contactrons
    for (int i = 0; i < CONT_COUNT; i++) {
        gpio_init(contractron_pins[i]);
        gpio_set_dir(contractron_pins[i], GPIO_IN);
        gpio_pull_down(contractron_pins[i]);
    }

    // Init TCP server
    tcp_server_init(on_tcp_connection, on_tcp_disconnection, on_tcp_msg_received);

    xTaskCreate(tcp_server_task, "tcp_server_task", configMINIMAL_STACK_SIZE, NULL, TCP_TASK_PRIORITY, NULL);
    xTaskCreate(temp_task, "TEMP_Task", 256, NULL, 1, NULL);
    xTaskCreate(contactron_task, "CONT_Task", 256, NULL, 1, NULL);
    

    vTaskStartScheduler();
}