// description of the beeper functionality
// beeper.h

#ifndef BEEPER_H
#define BEEPER_H

typedef enum beeper_state {
    ALARM_OFF = 0,
    ALARM_ARM,
    ALARM_DISARM,
    ALARM_DETECTION,
    ALARM_RUN
} beeper_state_t;

void beeper_init();
void beeper_task();
void beeper_set_state(beeper_state_t state);
void beeper_reset();

#endif
