#ifndef CONFIG_H
#define CONFIG_H

#define ELEMENT_COUNT(array) (sizeof(array) / sizeof(array[0]))

#if WATCHDOG_ENABLED == 1 && defined(INCLUDE_WATCHDOG)
static const uint watchdog_timeout_ms = (5 * 60 * 1000);
#endif

#if DHT_ENABLED == 1 && defined(INCLUDE_DHT)
static const uint dht_pins[] = {0, 1, 2, 4, 5, 6};
static const uint dht_power_pin = 26;
static const uint dht_delay_ms = (1 * 60 * 1000);
#define DHT_COUNT ELEMENT_COUNT(dht_pins)
#endif

#if HEAT_ENABLED == 1  && defined(INCLUDE_HEAT)
static const uint heat_zone_pins[] = {7, 8, 9, 10, 11, 12};
#define HEAT_ZONE_COUNT ELEMENT_COUNT(heat_zone_pins)
#endif

#if WATERING_ENABLED == 1  && defined(INCLUDE_WATERING)
static const uint watering_zone_pins[] = {7, 8, 9, 6};
static const uint watering_sensor_pin = 26; // ADC pin
static const uint watering_rain_pin = 12;
static const uint watering_power_pin = 10;
static const uint watering_sensor_delay_ms = (1 * 60 * 1000);
#define WATERING_ZONE_COUNT ELEMENT_COUNT(watering_zone_pins)
#endif

#if CONT_ENABLED == 1 && defined(INCLUDE_CONT)
static const uint contractron_pins[] = {16, 17, 18, 19, 20, 21, 22};
static uint contractron_state[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
#define CONT_COUNT ELEMENT_COUNT(contractron_pins)
#endif

#if GATE_ENABLED == 1  && defined(INCLUDE_GATE)
static const uint gate_pin = 15;
#endif

#if BEEPER_ENABLED == 1 && defined(INCLUDE_BEEPER)
static const uint beeper_pin = 14;
#endif

#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
static inline void reboot()
{
    AIRCR_Register = 0x5FA0004;
}

#endif