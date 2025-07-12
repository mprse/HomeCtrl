#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "tcp_server.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "hardware/adc.h"
#include "watering.h"

#define INCLUDE_WATERING 1
#include "config.h"

#if WATERING_ENABLED == 1
void watering_init()
{
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


    gpio_init(watering_light_pin);
    gpio_set_dir(watering_light_pin, GPIO_OUT);
    gpio_put(watering_light_pin, 1); // Light off by default
}

void watering_task()
{
    // Detekt rain using DFRobot SEN0308 sensor
    char msg[50];
    TickType_t all_zones_inactive_since = 0;
    uint8_t last_rain_detected = 0xFF;
    uint8_t any_zone_active = 0;
    TickType_t last_status_sent = 0;

    while (true) {
        //uint16_t result = 666;//adc_read();
        //uint adc_percentage = (result * 100) / 4096;
        // pin is connected to gnd via sensor and pulled up to 3.3V
        // sensor is connected when no rain detected
        uint8_t rain_detected = (gpio_get(watering_rain_pin) == 0) ? 0 : 1;
        any_zone_active = 0;

        // Only send log and message on rain status change
        if (rain_detected != last_rain_detected) {
            if (rain_detected) {
                printf("WATERING: Detected rain, watering disabled.\n");
            } else {
                printf("WATERING: Rain stopped, watering enabled.\n");
            }
            last_rain_detected = rain_detected;
        }

        for (int i = 0; i < WATERING_ZONE_COUNT; i++) {
            any_zone_active |= !gpio_get(watering_zone_pins[i]);
        }

        // Logic for watering power
        if (rain_detected) {
            // Always disable power if rain detected
            gpio_put(watering_power_pin, 1);
            all_zones_inactive_since = 0; // Reset timer
        } else if (!any_zone_active) {
            // No zones active: start or check timer
            if (all_zones_inactive_since == 0) {
                all_zones_inactive_since = xTaskGetTickCount();
            } else if ((xTaskGetTickCount() - all_zones_inactive_since) > pdMS_TO_TICKS(60000)) {
                gpio_put(watering_power_pin, 1); // Disable after 1 min
            }
        } else {
            // At least one zone active and no rain: enable power
            gpio_put(watering_power_pin, 0);
            all_zones_inactive_since = 0; // Reset timer
        }

        // Safety fuse: auto-disable zone after 30 minutes
        for (int i = 0; i < WATERING_ZONE_COUNT; i++) {
            if (!gpio_get(watering_zone_pins[i])) { // Zone is active (low trigger)
                if (zone_active_since[i] != 0 &&
                    (xTaskGetTickCount() - zone_active_since[i]) > pdMS_TO_TICKS(watering_zone_max_active_time_ms)) {
                    printf("WATERING: Zone %d active too long, disabling!\n", i);
                    gpio_put(watering_zone_pins[i], 1); // Disable zone
                    zone_active_since[i] = 0;
                }
            }
        }

        // Send status
        if ((xTaskGetTickCount() - last_status_sent) > pdMS_TO_TICKS(watering_status_delay_ms)) {
            int len = 0;
            len += sprintf(msg + len, "{\"watering_power\":%d,", !gpio_get(watering_power_pin));
            len += sprintf(msg + len, "\"light\":%d,", !gpio_get(watering_light_pin));
            len += sprintf(msg + len, "\"zones\":[");
            for (int i = 0; i < WATERING_ZONE_COUNT; i++) {
                len += sprintf(msg + len, "%d", !gpio_get(watering_zone_pins[i]));
                if (i < WATERING_ZONE_COUNT - 1) len += sprintf(msg + len, ",");
            }
            len += sprintf(msg + len, "],\"rain_status\":%d}", rain_detected);
            tcp_send_message(msg);
            last_status_sent = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void watering_set_zone(uint zone_id, uint state)
{
    // Ensure only one zone is active at a time
    for (int i = 0; i < WATERING_ZONE_COUNT; i++) {
        if (i == zone_id) {
            gpio_put(watering_zone_pins[i], state ? 0 : 1); // Low trigger
            if (state) {
                zone_active_since[i] = xTaskGetTickCount();
            } else {
                zone_active_since[i] = 0;
            }
        } else {
            gpio_put(watering_zone_pins[i], 1); // Disable other zones
            zone_active_since[i] = 0;
        }
    }
}

void watering_set_light(uint state)
{
    gpio_put(watering_light_pin, state ? 0 : 1); // Low trigger
}

void watering_power_enable()
{
    gpio_put(watering_power_pin, 0); // Enable watering power
}
#endif /* WATERING_ENABLED */