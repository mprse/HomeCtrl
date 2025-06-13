#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "tcp_server.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define INCLUDE_GATE 1
#include "config.h"

#if GATE_ENABLED == 1

void gate_init()
{
    gpio_init(gate_pin);
    gpio_set_dir(gate_pin, GPIO_OUT);
    gpio_put(gate_pin, 0);
}

void gate_toggle()
{
    gpio_put(gate_pin, 1);
    sleep_ms(300);
    gpio_put(gate_pin, 0);
}

#endif /* GATE_ENABLED */