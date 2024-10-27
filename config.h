#ifndef CONFIG_H
#define CONFIG_H

#define ELEMENT_COUNT(array) (sizeof(array) / sizeof(array[0]))

#if DHT_ENABLED == 1
static const uint dht_pins[] = {1, 2, 3, 4, 5};
static const uint dht_power_pin = 26;
#define DHT_COUNT ELEMENT_COUNT(dht_pins)

static uint dht_err[DHT_COUNT] = {0};
static dht_reading_t dht_dev[DHT_COUNT];
#endif

#if HEAT_ENABLED == 1
static const uint heat_zone_pins[] = {6, 7, 8, 9, 10};
#define HEAT_ZONE_COUNT ELEMENT_COUNT(heat_zone_pins)
#endif

#if CONT_ENABLED == 1
static const uint contractron_pins[] = {16, 17, 18, 19, 20, 21, 22};
static uint contractron_state[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
#define CONT_COUNT ELEMENT_COUNT(contractron_pins)
#endif

#if GATE_ENABLED == 1
static const uint gate_pin = 15;
#endif

#endif