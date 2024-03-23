#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "awning.h"

#define INCLUDE_AWNING 1
#include "config.h"

#if AWNING_ENABLED == 1

void awning_init(void)
{
    // Power pin configuration
    gpio_init(awning_power_pin);
    gpio_set_dir(awning_power_pin, GPIO_OUT);
    gpio_put(awning_power_pin, 0);

    // Control pins configuration
    gpio_init(awning_up_pin);
    gpio_set_dir(awning_up_pin, GPIO_OUT);
    gpio_put(awning_up_pin, 0);

    gpio_init(awning_stop_pin);
    gpio_set_dir(awning_stop_pin, GPIO_OUT);
    gpio_put(awning_stop_pin, 0);

    gpio_init(awning_down_pin);
    gpio_set_dir(awning_down_pin, GPIO_OUT);
    gpio_put(awning_down_pin, 0);
}

void awning_send_cmd(awning_cmd_t cmd)
{
    uint target_pin = 0;

    switch (cmd) {
        case AWNING_CMD_UP:   target_pin = awning_up_pin; break;
        case AWNING_CMD_STOP: target_pin = awning_stop_pin; break;
        case AWNING_CMD_DOWN: target_pin = awning_down_pin; break;
        default: return; // Nieznana komenda
    }

    gpio_put(target_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_put(target_pin, 0);


    gpio_put(awning_power_pin, 0);
}

#endif /* AWNING_ENABLED */