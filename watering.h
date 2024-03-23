// description of the watering functionality
// watering.h

#ifndef WATERING_H
#define WATERING_H

typedef struct {
    int zone_id;
    int duration_minutes;
} WateringCommand;

void watering_init();
void watering_set_zone(uint zone_id, uint state);
void watering_task();
void watering_power_enable();
void watering_set_light(uint state);
void watering_enqueue_command(int zone_id, int minutes);
void watering_clear_queue();

#endif
