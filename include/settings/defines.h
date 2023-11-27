#ifndef DEFINES_H
#define DEFINES_H





#define EMERGENCY_TIME_TRESHOLD     120000
#define MQTT_RECONNECT_INTERVAL     5000
#define MQTT_KEEPALIVE              30

#define OPENTHERM_OFFLINE_TRESHOLD  10

#define EXT_SENSORS_INTERVAL        5000
#define EXT_SENSORS_FILTER_K        0.15

#define CONFIG_URL                  "http://%s/"
#define SETTINGS_VALID_VALUE        "stvalid" // only 8 chars!

#define DEFAULT_HEATING_MIN_TEMP    20
#define DEFAULT_HEATING_MAX_TEMP    90
#define DEFAULT_DHW_MIN_TEMP        30
#define DEFAULT_DHW_MAX_TEMP        60


#ifndef USE_SERIAL
  #define USE_SERIAL true
#endif

#ifndef USE_TELNET
  #define USE_TELNET true
#endif

#ifndef OT_IN_PIN_DEFAULT
  #define OT_IN_PIN_DEFAULT 0
#endif

#ifndef OT_OUT_PIN_DEFAULT
  #define OT_OUT_PIN_DEFAULT 0
#endif

#ifndef SENSOR_OUTDOOR_PIN_DEFAULT
  #define SENSOR_OUTDOOR_PIN_DEFAULT 0
#endif

#ifndef SENSOR_INDOOR_PIN_DEFAULT
  #define SENSOR_INDOOR_PIN_DEFAULT 0
#endif

#endif