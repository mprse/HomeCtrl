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
}

void watering_task()
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
        tcp_send_message(msg);
        vTaskDelay(pdMS_TO_TICKS(watering_sensor_delay_ms));
    }
}

void watering_set_zone(uint zone_id, uint state)
{
    gpio_put(watering_zone_pins[zone_id], state ? 0 : 1); // Low trigger
}

void watering_power_enable()
{
    gpio_put(watering_power_pin, 0); // Enable watering power
}
#endif /* WATERING_ENABLED */