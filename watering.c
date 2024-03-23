#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "tcp_server.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "hardware/adc.h"
#include "watering.h"
#include "FreeRTOS.h"
#include "queue.h"

#define INCLUDE_WATERING 1
#include "config.h"

#if WATERING_ENABLED == 1

static QueueHandle_t watering_cmd_queue;

void watering_init()
{
    for (int i = 0; i < WATERING_ZONE_COUNT; i++) {
        gpio_init(watering_zone_pins[i]);
        gpio_set_dir(watering_zone_pins[i], GPIO_OUT);
        gpio_put(watering_zone_pins[i], 0);

    }
    adc_gpio_init(watering_sensor_pin);
    adc_select_input(0);

    gpio_init(watering_power_pin);
    gpio_set_dir(watering_power_pin, GPIO_OUT);
    gpio_put(watering_power_pin, 0);

    gpio_init(watering_rain_pin);
    gpio_set_dir(watering_rain_pin, GPIO_IN);
    gpio_pull_up(watering_rain_pin);


    gpio_init(watering_light_pin);
    gpio_set_dir(watering_light_pin, GPIO_OUT);
    gpio_put(watering_light_pin, 0); // Light off by default

    watering_cmd_queue = xQueueCreate(8, sizeof(WateringCommand));
}

void watering_task()
{
    // Detekt rain using DFRobot SEN0308 sensor
    char msg[128];
    TickType_t all_zones_inactive_since = 0;
    uint8_t last_rain_detected = 0xFF;
    TickType_t last_status_sent = 0;

    int active_zone = -1;
    int remaining_minutes = 0;
    TickType_t last_minute_tick = xTaskGetTickCount();
    uint8_t any_zone_active = 0;

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
            any_zone_active |= gpio_get(watering_zone_pins[i]);
        }

        // Logic for watering power
        if (rain_detected) {
            // Always disable power if rain detected
            gpio_put(watering_power_pin, 0);
            all_zones_inactive_since = 0; // Reset timer
        } else if (!any_zone_active) {
            // No zones active: start or check timer
            if (all_zones_inactive_since == 0) {
                all_zones_inactive_since = xTaskGetTickCount();
            } else if ((xTaskGetTickCount() - all_zones_inactive_since) > pdMS_TO_TICKS(60000)) {
                gpio_put(watering_power_pin, 0); // Disable after 1 min
            }
        } else {
            // At least one zone active and no rain: enable power
            gpio_put(watering_power_pin, 1);
            all_zones_inactive_since = 0; // Reset timer
        }

        // Handle queued watering commands
        if (active_zone == -1 && rain_detected == 0) {
            WateringCommand cmd;
            if (xQueueReceive(watering_cmd_queue, &cmd, 0) == pdTRUE) {
                // Disable all zones first
                for (int i = 0; i < WATERING_ZONE_COUNT; i++) {
                    gpio_put(watering_zone_pins[i], 0);
                    zone_active_since[i] = 0;
                }
                // Enable the requested zone
                gpio_put(watering_zone_pins[cmd.zone_id], 1);
                zone_active_since[cmd.zone_id] = xTaskGetTickCount();
                active_zone = cmd.zone_id;
                remaining_minutes = cmd.duration_minutes;
                last_minute_tick = xTaskGetTickCount();
                printf("WATERING: Started zone %d for %d minutes\n", cmd.zone_id, cmd.duration_minutes);
            }
        }

        // Handle active watering zone timing
        if (active_zone != -1) {
            if ((xTaskGetTickCount() - last_minute_tick) > pdMS_TO_TICKS(60000)) {
                last_minute_tick = xTaskGetTickCount();
                remaining_minutes--;
                if (remaining_minutes <= 0 ||
                    (zone_active_since[active_zone] != 0 &&
                     (xTaskGetTickCount() - zone_active_since[active_zone]) > pdMS_TO_TICKS(watering_zone_max_active_time_ms))) {
                    // Time's up or max active time exceeded
                    gpio_put(watering_zone_pins[active_zone], 0);
                    zone_active_since[active_zone] = 0;
                    printf("WATERING: Zone %d finished\n", active_zone);
                    active_zone = -1;
                }
            }
        }

        // Send status
        if ((xTaskGetTickCount() - last_status_sent) > pdMS_TO_TICKS(watering_status_delay_ms)) {
            int len = 0;
            len += sprintf(msg + len, "{\"watering_power\":%d,", gpio_get(watering_power_pin));
            len += sprintf(msg + len, "\"light\":%d,", gpio_get(watering_light_pin));
            len += sprintf(msg + len, "\"zones\":[");
            for (int i = 0; i < WATERING_ZONE_COUNT; i++) {
                len += sprintf(msg + len, "%d", gpio_get(watering_zone_pins[i]));
                if (i < WATERING_ZONE_COUNT - 1) len += sprintf(msg + len, ",");
            }
            len += sprintf(msg + len, "],\"rain_status\":%d}", rain_detected);
            tcp_send_message(msg);
            last_status_sent = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void watering_enqueue_command(int zone_id, int minutes) {
    WateringCommand cmd = { zone_id, minutes };
    xQueueSend(watering_cmd_queue, &cmd, 0);
}

void watering_clear_queue()
{
    xQueueReset(watering_cmd_queue);
}

void watering_set_zone(uint zone_id, uint state)
{
    // Ensure only one zone is active at a time
    for (int i = 0; i < WATERING_ZONE_COUNT; i++) {
        if (i == zone_id) {
            gpio_put(watering_zone_pins[i], state ? 1 : 0); // High trigger
            if (state) {
                zone_active_since[i] = xTaskGetTickCount();
            } else {
                zone_active_since[i] = 0;
                watering_clear_queue(); // Clear queue when a zone is manually disabled
            }
        } else {
            gpio_put(watering_zone_pins[i], 0); // Disable other zones
            zone_active_since[i] = 0;
        }
    }
}

void watering_set_light(uint state)
{
    gpio_put(watering_light_pin, state ? 1 : 0); // High trigger
}

void watering_power_enable()
{
    gpio_put(watering_power_pin, 1); // Enable watering power
}
#endif /* WATERING_ENABLED */