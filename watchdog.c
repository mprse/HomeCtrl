#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "watchdog.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "watchdog.h"

#define INCLUDE_WATCHDOG 1
#include "config.h"

#if WATCHDOG_ENABLED == 1
static TickType_t last_ping_time = 0;
static const TickType_t watchdog_timeout = pdMS_TO_TICKS(watchdog_timeout_ms);
static TaskHandle_t watchdog_task_handle = NULL;


void reset_watchdog_timer() {
    last_ping_time = xTaskGetTickCount();
    printf("WATCHDOG: timer reset.\n");
}

void watchdog_task(void *pvParameters) {
    while (true) {
        // Check if the timeout has been exceeded
        if ((xTaskGetTickCount() - last_ping_time) > watchdog_timeout) {
            printf("WATCHDOG: timeout! Rebooting the board...\n");
            reboot();
        }
        vTaskDelay(pdMS_TO_TICKS(watchdog_timeout_ms)); // Check every second
    }
}

void start_watchdog_timer() {
    last_ping_time = xTaskGetTickCount(); // Initialize the timer
    if (watchdog_task_handle == NULL) {
        xTaskCreate(watchdog_task, "WatchdogTask", 1024, NULL, 1, &watchdog_task_handle);
        printf("WATCHDOG: task started.\n");
    }
}
#endif /* WATCHDOG_ENABLED */