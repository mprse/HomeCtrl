#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/apps/lwiperf.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <lwip/sockets.h>
#include "tcp_server.h"
#include "pico/bootrom.h"
#include "watchdog.h"
#include "dht.h"
#include "contactron.h"
#include "heat.h"
#include "watering.h"
#include "beeper.h"
#include "gate.h"
#include "config.h"

#if WATERING_ENABLED == 1
static TaskHandle_t watering_task_handle = NULL;
#endif

static void on_tcp_connection(int con_id)
{
    printf("TCP: connected, connection id: %d\n", con_id);
    cyw43_arch_gpio_put(0, true);
#if DHT_ENABLED == 1
    printf("DHT: Start temp task.\n");
    xTaskCreate(temp_task, "TEMP_Task", 1024, NULL, TCP_TASK_PRIORITY+1, NULL);
#endif /* DHT_ENABLED */
#if CONT_ENABLED == 1
    contactron_push_all();
    printf("CONT: Start contactron task.\n");
    xTaskCreate(contactron_task, "CONT_Task", 1024, NULL, TCP_TASK_PRIORITY-1, NULL);
#endif /* CONT_ENABLED */
#if BEEPER_ENABLED == 1
    beeper_reset();
    printf("BEEP: Start beeper task.\n");
    xTaskCreate(beeper_task, "BEEP_Task", 1024, NULL, TCP_TASK_PRIORITY+2, NULL);
#endif /* BEEPER_ENABLED */

#if WATERING_ENABLED == 1
    if (watering_task_handle != NULL) {
        vTaskDelete(watering_task_handle);
        watering_task_handle = NULL;
        printf("WATERING: Deleted existing watering task.\n");
    }
    if (xTaskCreate(watering_task, "WATERING_Task", 1024, NULL, TCP_TASK_PRIORITY-1, &watering_task_handle) == pdPASS) {
        printf("WATERING: Created watering task.\n");
    } else {
        printf("WATERING: Failed to create watering task.\n");
    }
#endif /* WATERING_ENABLED */

#if WATCHDOG_ENABLED == 1
    start_watchdog_timer();
#endif /* WATCHDOG_ENABLED */
}

static void on_tcp_disconnection(int con_id)
{
    printf("TCP: disconnected, connection id: %d\n", con_id);
    cyw43_arch_gpio_put(0, false);
}

static void on_tcp_msg_received(int con_id, char* msg)
{
    char command[10];
    int zone_id, state;

    printf("TCP: < (%d): %s\n", con_id, msg);

    if (strcmp(msg, "flash") == 0) {
        reset_usb_boot(0, 0);
    } else if (strcmp(msg, "reboot") == 0) {
        reboot();
    } else if (strcmp(msg, "Hello!") == 0) {
        printf("TCP: Hello from client :-)\n");
    } else if (strcmp(msg, "sync") == 0) {
        printf("TCP: Sync request received.\n");
    }
    else if (strcmp(msg, "ping") == 0) {
        tcp_send_message("pong");
#if WATCHDOG_ENABLED == 1
        reset_watchdog_timer();
#endif /* WATCHDOG_ENABLED */
    }
#if GATE_ENABLED == 1
    else if (strcmp(msg, "brama") == 0) {
        gate_toggle();
    }
#endif /* GATE_ENABLED */
#if BEEPER_ENABLED == 1
    else if (strcmp(msg, "alarm_off") == 0) {
        beeper_set_state(ALARM_OFF);
    }
    else if (strcmp(msg, "alarm_arm") == 0) {
        beeper_set_state(ALARM_ARM);
    }
    else if (strcmp(msg, "alarm_disarm") == 0) {
        beeper_set_state(ALARM_DISARM);
    }
    else if (strcmp(msg, "alarm_detect") == 0) {
        beeper_set_state(ALARM_DETECTION);
    }
    else if (strcmp(msg, "alarm_run") == 0) {
        beeper_set_state(ALARM_RUN);
    }
#endif /* BEEPER_ENABLED */
#if HEAT_ENABLED == 1 || WATERING_ENABLED == 1
    else if (sscanf(msg, "%s %d %d", command, &zone_id, &state) == 3) {
        bool recognized = false;

#if HEAT_ENABLED == 1
        if (strcmp(command, "heat") == 0) {
            heatting_set_zone(zone_id, state);
            printf("HEAT: Zone[%d]: %s\n", zone_id, state == 0 ? "Disabled" : "Enabled");
            recognized = true;
        }
#endif /* HEAT_ENABLED */

#if WATERING_ENABLED == 1
        if (strcmp(command, "water") == 0) {
            if(state == 1) {
                watering_power_enable();
            }
            watering_set_zone(zone_id, state);
            printf("WATER: Zone[%d]: %s\n", zone_id, state == 0 ? "Disabled" : "Enabled");
            recognized = true;
        }
#endif /* WATERING_ENABLED */

        if (!recognized) {
            printf("ERROR: Unknown command: %s\n", command);
        }
    } else {
        printf("ERROR: Invalid message\n");
    }
#endif /* HEAT_ENABLED || WATERING_ENABLED */
}

int main(void)
{
    stdio_init_all();
    printf("--- Starting HomeCtrl --- \n");
    printf("--> Init stage\n");

#if DHT_ENABLED == 1
    dht_init();
#endif /* DHT_ENABLED */

#if CONT_ENABLED == 1
    contactron_init();
#endif /* CONT_ENABLED */

#if HEAT_ENABLED == 1
    heatting_init();
#endif /* HEAT_ENABLED */

#if WATERING_ENABLED == 1
    watering_init();
#endif /* WATERING_ENABLED */

#if GATE_ENABLED == 1
    gate_init();
#endif /* GATE_ENABLED */

#if BEEPER_ENABLED == 1
    beeper_init();
#endif /* BEEPER_ENABLED */

    sleep_ms(1000);

    printf("--> Run Scheduler \n");

    // Init TCP server
    tcp_server_init(on_tcp_connection, on_tcp_disconnection, on_tcp_msg_received);

    xTaskCreate(tcp_server_task, "tcp_server_task", 2*configMINIMAL_STACK_SIZE, NULL, TCP_TASK_PRIORITY, NULL);

    vTaskStartScheduler();
}