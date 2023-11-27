#ifndef SETTING_H
#define SETTING_H

#include "Arduino.h"
#include "settings_sensor.h"

#define DEBUG 1
#ifdef DEBUG
#define DEBUG_LEVEL TinyLogger::Level::VERBOSE
#else
#define DEBUG_LEVEL TinyLogger::Level::ERROR
#endif

struct Settings
{
  bool debug = DEBUG;
  char hostname[80] = "opentherm";

  struct
  {
    byte inPin = OT_IN_PIN_DEFAULT;
    byte outPin = OT_OUT_PIN_DEFAULT;
    unsigned int memberIdCode = 0;
    bool dhwPresent = true;
    bool summerWinterMode = false;
    bool heatingCh2Enabled = true;
    bool heatingCh1ToCh2 = false;
    bool dhwToCh2 = false;
    bool dhwBlocking = false;
    bool modulationSyncWithHeating = false;
  } opentherm;

  struct
  {
    char server[80];
    unsigned int port = 1883;
    char user[32];
    char password[32];
    char prefix[80] = "opentherm";
    unsigned int interval = 5000;
  } mqtt;

  struct
  {
    bool enable = true;
    float target = 40.0f;
    bool useEquitherm = false;
  } emergency;

  struct
  {
    bool enable = true;
    bool turbo = false;
    float target[INDOOR_SENSOR_NUMBER] = {20.0f};
    float hysteresis = 0.5f;
    byte minTemp = DEFAULT_HEATING_MIN_TEMP;
    byte maxTemp = DEFAULT_HEATING_MAX_TEMP;
    byte maxModulation = 100;
  } heating;

  struct
  {
    bool enable = true;
    byte target = 40;
    byte minTemp = DEFAULT_DHW_MIN_TEMP;
    byte maxTemp = DEFAULT_DHW_MAX_TEMP;
  } dhw;

  struct
  {
    bool enable = false;
    float p_factor = 3;
    float i_factor = 0.2f;
    float d_factor = 0;
    byte minTemp = 0;
    byte maxTemp = DEFAULT_HEATING_MAX_TEMP;
  } pid[INDOOR_SENSOR_NUMBER];

  struct
  {
    bool enable = false;
    float n_factor = 0.7f;
    float k_factor = 3.0f;
    float t_factor = 2.0f;
  } equitherm[INDOOR_SENSOR_NUMBER];

  struct
  {
    struct
    {
      // 0 - boiler, 1 - manual, 2 - ds18b20, 3 - MQTT
      byte type = 0;
      byte pin = SENSOR_OUTDOOR_PIN_DEFAULT;
      float offset = 0.0f;
    } outdoor;

    struct
    {
      // 1 - manual, 2 - ds18b20, 3 - MQTT
      byte type = INDOOR_SENSOR_TYPE::INDOOR_SENSOR_MANUAL;
      byte pin = SENSOR_INDOOR_PIN_DEFAULT;
      float offset = 0.0f;
    } indoor[INDOOR_SENSOR_NUMBER];

  } sensors;

  char validationValue[8] = SETTINGS_VALID_VALUE;
};

struct Variables
{
  struct
  {
    bool enable = false;
    byte regulator = 0;
  } tuning;

  struct
  {
    bool otStatus = false;
    bool emergency = false;
    bool heating = false;
    bool dhw = false;
    bool flame = false;
    bool fault = false;
    bool diagnostic = false;
  } states;

  struct
  {
    float modulation = 0.0f;
    float pressure = 0.0f;
    float dhwFlowRate = 0.0f;
    byte faultCode = 0;
    int8_t rssi = 0;
  } sensors;

  struct
  {
    float indoor[INDOOR_SENSOR_NUMBER] = {0.0f};
    float outdoor = 0.0f;
    float heating = 0.0f;
    float dhw = 0.0f;
  } temperatures;

  struct
  {
    bool heatingEnabled = false;
    byte heatingMinTemp = DEFAULT_HEATING_MIN_TEMP;
    byte heatingMaxTemp = DEFAULT_HEATING_MAX_TEMP;
    byte heatingSetpoint = 0;
    byte dhwMinTemp = DEFAULT_DHW_MIN_TEMP;
    byte dhwMaxTemp = DEFAULT_DHW_MAX_TEMP;
    byte maxModulation;
    uint8_t slaveMemberId;
    uint8_t slaveFlags;
    uint8_t slaveType;
    uint8_t slaveVersion;
    float slaveOtVersion;
    uint8_t masterMemberId;
    uint8_t masterFlags;
    uint8_t masterType;
    uint8_t masterVersion;
    float masterOtVersion;
  } parameters;

  struct
  {
    bool restart = false;
    bool resetFault = false;
    bool resetDiagnostic = false;
  } actions;
};

#endif