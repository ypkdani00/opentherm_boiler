#include "common.h"
#include <EEManager.h>


#if USE_TELNET
  #include "ESPTelnetStream.h"
#endif

#if defined(ESP32)
  #include <ESP32Scheduler.h>
#elif defined(ESP8266)
  #include <Scheduler.h>
#elif
  #error Wrong board. Supported boards: esp8266, esp32
#endif

#include <Task.h>
#include <LeanTask.h>
#include "WifiManagerTask.h"
#include "MqttTask.h"
#include "OpenThermTask.h"
#include "SensorsTask.h"
#include "RegulatorTask.h"
#include "MainTask.h"

Variables vars;
Settings settings;

// Vars
EEManager eeSettings(settings, 60000);
#if USE_TELNET
  ESPTelnetStream TelnetStream;
#endif

// Tasks
WifiManagerTask* tWm;
MqttTask* tMqtt;
OpenThermTask* tOt;
SensorsTask* tSensors;
RegulatorTask* tRegulator;
MainTask* tMain;
TinyLogger Log;


void setup() {
  Log = TinyLogger();
  Log.setLevel(DEBUG_LEVEL);

  #if USE_SERIAL
    Serial.begin(115200);
    Log.addStream(&Serial);
    Serial.println("\n\n");
  #endif

  #if USE_TELNET
    Log.addStream(&TelnetStream);
  #endif
  //Log.setNtpClient(&timeClient);

  EEPROM.begin(eeSettings.blockSize());
  uint8_t eeSettingsResult = eeSettings.begin(0, 's');
  if (eeSettingsResult == 0) {
    Log.sinfoln("MAIN", F("Settings loaded"));

    if (strcmp(SETTINGS_VALID_VALUE, settings.validationValue) != 0) {
      Log.swarningln("MAIN", F("Settings not valid, reset and restart..."));
      eeSettings.reset();
      delay(1000);
      ESP.restart();
    }

  } else if (eeSettingsResult == 1) {
    Log.sinfoln("MAIN", F("Settings NOT loaded, first start"));

  } else if (eeSettingsResult == 2) {
    Log.serrorln("MAIN", F("Settings NOT loaded (error)"));
  }

    tWm = new WifiManagerTask(true);
  Scheduler.start(tWm);

  tMqtt = new MqttTask(false);
  Scheduler.start(tMqtt);

  tOt = new OpenThermTask(false);
  Scheduler.start(tOt);

  tSensors = new SensorsTask(true, EXT_SENSORS_INTERVAL);
  Scheduler.start(tSensors);

  tRegulator = new RegulatorTask(true, 10000);
  Scheduler.start(tRegulator);

  tMain = new MainTask(true, 10);
  Scheduler.start(tMain);

  Scheduler.begin();
}

void loop() {
  #if defined(ESP32)
    vTaskDelete(NULL);
  #endif
}
