// Description: Header file for the watchdog timer functionality
// watchdog.h

#ifndef WATCHDOG_H
void reset_watchdog_timer();
void start_watchdog_timer();
void watchdog_task(void *pvParameters);
#endif /* WATCHDOG_H */


