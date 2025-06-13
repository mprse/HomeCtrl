#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "tcp_server.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "heat.h"

#define INCLUDE_HEAT 1
#include "config.h"

#if HEAT_ENABLED == 1
void heatting_init()
{
    for (int i = 0; i < HEAT_ZONE_COUNT; i++) {
        gpio_init(heat_zone_pins[i]);
        gpio_set_dir(heat_zone_pins[i], GPIO_OUT);
        gpio_put(heat_zone_pins[i], 1);
    }
}

void heatting_set_zone(uint zone_id, uint state)
{
    gpio_put(heat_zone_pins[zone_id], state ? 0 : 1); // Low trigger
}
#endif