#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "beeper.h"

#define INCLUDE_BEEPER 1
#include "config.h"

#if BEEPER_ENABLED == 1
static volatile beeper_state_t beeper_state = ALARM_OFF;
static volatile beeper_state_t next_beeper_state = ALARM_OFF;

void beeper_init()
{
    gpio_init(beeper_pin);
    gpio_set_dir(beeper_pin, GPIO_OUT);
    gpio_put(beeper_pin, 0);
}

void beeper_reset()
{
    beeper_state = ALARM_OFF;
    next_beeper_state = ALARM_OFF;
}

void beeper_set_state(beeper_state_t state)
{
    next_beeper_state = state;
}

void beeper_task()
{
    static int task_delay = 100;
    static int step = 0;
    static bool print_flag = true;

    while (true) {
        if(beeper_state != next_beeper_state) {
            if(step == 0) {
                printf("BEEP: state: %d\n", beeper_state);
                beeper_state = next_beeper_state;
                step = 0;
            }
        }
        switch(beeper_state) {
            case ALARM_OFF:
                if(print_flag){
                    printf("BEEP: OFF\n");
                    print_flag = false;
                }
                task_delay = 100;
                break;
            case ALARM_ARM:
                print_flag = true;
                switch(step) {
                    case 0:
                        printf("BEEP: ARM\n");
                        gpio_put(beeper_pin, 1);
                        task_delay = 300;
                        step++;
                    break;
                    case 1:
                        gpio_put(beeper_pin, 0);
                        task_delay = 500;
                        step++;
                    break;
                    case 2:
                        gpio_put(beeper_pin, 1);
                        task_delay = 1000;
                        step++;
                    break;
                    case 3:
                        gpio_put(beeper_pin, 0);
                        task_delay = 100;
                        step = 0;
                        next_beeper_state = ALARM_OFF;
                    break;
                }
                break;
            case ALARM_DISARM:
                print_flag = true;
                switch(step) {
                    case 0:
                        printf("BEEP: DISARM\n");
                        gpio_put(beeper_pin, 1);
                        task_delay = 1500;
                        step++;
                    break;
                    case 1:
                        gpio_put(beeper_pin, 0);
                        task_delay = 100;
                        step = 0;
                        next_beeper_state = ALARM_OFF;
                    break;
                }
                break;
            case ALARM_DETECTION:
                print_flag = true;
                switch(step) {
                    case 0:
                        printf("BEEP: DETECTION\n");
                        gpio_put(beeper_pin, 1);
                        task_delay = 300;
                        step++;
                    break;
                    case 1:
                        gpio_put(beeper_pin, 0);
                        task_delay = 300;
                        step = 0;
                    break;
                }
                break;
            case ALARM_RUN:
                print_flag = true;
                switch(step) {
                    case 0:
                        printf("BEEP: RUN\n");
                        gpio_put(beeper_pin, 1);
                        task_delay = 1000;
                        step++;
                    break;
                    case 1:
                        gpio_put(beeper_pin, 0);
                        task_delay = 500;
                        step = 0;
                    break;
                }
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay));
    }
}
#endif /* BEEPER_ENABLED */