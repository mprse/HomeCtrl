#ifndef CONFIG_H
#define CONFIG_H

#define ELEMENT_COUNT(array) (sizeof(array) / sizeof(array[0]))

#if DHT_ENABLED == 1
static const uint dht_pins[] = {1, 2, 3, 4, 5};
static const uint dht_power_pin = 26;
static const uint dht_delay_ms = (1 * 60 * 1000);
#define DHT_COUNT ELEMENT_COUNT(dht_pins)

static uint dht_err[DHT_COUNT] = {0};
static dht_reading_t dht_dev[DHT_COUNT];
#endif

#if HEAT_ENABLED == 1
static const uint heat_zone_pins[] = {6, 7, 8, 9, 10};
#define HEAT_ZONE_COUNT ELEMENT_COUNT(heat_zone_pins)
#endif

#if WATERING_ENABLED == 1
static const uint watering_zone_pins[] = {7, 8, 9, 10, 11, 12};
static const uint watering_sensor_pin = 13;
static const uint watering_sensor_delay_ms = (5 * 60 * 1000);
#define WATERING_ZONE_COUNT ELEMENT_COUNT(watering_zone_pins)
#endif

#if CONT_ENABLED == 1
static const uint contractron_pins[] = {16, 17, 18, 19, 20, 21, 22};
static uint contractron_state[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
#define CONT_COUNT ELEMENT_COUNT(contractron_pins)
#endif

#if GATE_ENABLED == 1
static const uint gate_pin = 15;
#endif

#if BEEPER_ENABLED == 1
typedef enum beeper_state {
    ALARM_OFF = 0,
    ALARM_ARM,
    ALARM_DISARM,
    ALARM_DETECTION,
    ALARM_RUN
} beeper_state_t;

static const uint beeper_pin = 14;
static volatile beeper_state_t beeper_state = ALARM_OFF;
static volatile beeper_state_t next_beeper_state = ALARM_OFF;
#endif

static inline void reboot()
{
    AIRCR_Register = 0x5FA0004;
}

#endif