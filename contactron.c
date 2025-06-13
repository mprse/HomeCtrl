#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "tcp_server.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "contactron.h"

#define INCLUDE_CONT 1
#include "config.h"


#if CONT_ENABLED == 1
static bool push_con_all = false; // push all data to client (sync)

static bool read_contactron(uint contactron_id)
{
    return gpio_get(contractron_pins[contactron_id]);
}

void contactron_init()
{
    for (int i = 0; i < CONT_COUNT; i++) {
        gpio_init(contractron_pins[i]);
        gpio_set_dir(contractron_pins[i], GPIO_IN);
        gpio_pull_up(contractron_pins[i]);
    }
}

void contactron_push_all()
{
    push_con_all = true;
}

void contactron_task()
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
                tcp_send_message(msg);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        push_con_all = false;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
#endif /* CONT_ENABLED */