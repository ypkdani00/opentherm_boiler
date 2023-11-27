#ifndef SETTINGSSENSOR_H
#define SETTINGSSENSOR_H

#define INDOOR_SENSOR_NUMBER 8

enum OUTDOOR_SENSOR_TYPE
{
    OUTDOOR_SENSOR_BOILER = 0,
    OUTDOOR_SENSOR_MANUAL,
    OUTDOOR_SENSOR_DS18B20,
    OUTDOOR_SENSOR_MQTT
};

enum INDOOR_SENSOR_TYPE
{
    INDOOR_SENSOR_DISABLED,
    INDOOR_SENSOR_MANUAL,
    INDOOR_SENSOR_RELAY,
    INDOOR_SENSOR_DS18B20,
    INDOOR_SENSOR_MQTT
};

//Edit the type of sensor to use
#define OUTDOOR_SENSOR          OUTDOOR_SENSOR_TYPE::OUTDOOR_SENSOR_BOILER

#define INDOOR_ROOM1_SENSOR     INDOOR_SENSOR_TYPE::INDOOR_SENSOR_MANUAL
#define INDOOR_ROOM3_SENSOR     INDOOR_SENSOR_TYPE::INDOOR_SENSOR_MANUAL
#define INDOOR_ROOM4_SENSOR     INDOOR_SENSOR_TYPE::INDOOR_SENSOR_MANUAL
#define INDOOR_ROOM5_SENSOR     INDOOR_SENSOR_TYPE::INDOOR_SENSOR_MANUAL
#define INDOOR_ROOM6_SENSOR     INDOOR_SENSOR_TYPE::INDOOR_SENSOR_MANUAL
#define INDOOR_ROOM7_SENSOR     INDOOR_SENSOR_TYPE::INDOOR_SENSOR_MANUAL
#define INDOOR_ROOM2_SENSOR     INDOOR_SENSOR_TYPE::INDOOR_SENSOR_MANUAL
#define INDOOR_ROOM8_SENSOR     INDOOR_SENSOR_TYPE::INDOOR_SENSOR_MANUAL

#endif