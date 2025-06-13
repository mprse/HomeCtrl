// description of the watering functionality
// watering.h

#ifndef WATERING_H
#define WATERING_H

void watering_init();
void watering_set_zone(uint zone_id, uint state);
void watering_task();
void watering_power_enable();

#endif
