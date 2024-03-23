#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "tcp_server.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "dht.h"
#include "dht22.h"

#define INCLUDE_DHT 1
#include "config.h"

#if DHT_ENABLED == 1

static uint dht_err[DHT_COUNT] = {0};
static dht_reading_t dht_dev[DHT_COUNT];

static uint8_t dht_read_safe(uint dht_id)
{
    portDISABLE_INTERRUPTS();
    uint8_t status = dht_read(&dht_dev[dht_id]);
    portENABLE_INTERRUPTS();

    return status;
}

void dht_init() {
    for (int i = 0; i < DHT_COUNT; i++) {
        dht_dev[i] = dht22_init(dht_pins[i]);
    }

    // Init dht power pin
    gpio_init(dht_power_pin);
    gpio_set_dir(dht_power_pin, GPIO_OUT);
    gpio_put(dht_power_pin, 0);
}

void print_dht_error(int i, const char* error_message) {
    printf("DHT: DHT(%d): %s\n", i, error_message);
}

void send_dht_data_to_client(int i) {
    char msg[100];
    sprintf(msg, "{\"dht_id\": \"%d\", \"temp\": \"%.1f\", \"hum\": \"%.1f\"}", dht_pins[i], dht_dev[i].temp_celsius, dht_dev[i].humidity);
    tcp_send_message(msg);
}

void temp_task()
{
    uint8_t dht_result = 0;

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

        // Read every 30 sec minute
        gpio_put(dht_power_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(dht_delay_ms));
    }
}
#endif /* DHT_ENABLED */