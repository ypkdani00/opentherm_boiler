#ifndef MQTTTASK_H
#define MQTTTASK_H

#include "common.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "HaHelper.h"
#include "EEManager.h"

class MqttTask : public Task {
public:
  MqttTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {}

protected:
  unsigned long lastReconnectAttempt = 0;
  unsigned long firstFailConnect = 0;

  const char* getTaskName();
  int getTaskCore();

  void setup();
  void loop();


  static bool updateSettings(JsonDocument& doc);
  static bool updateVariables(const JsonDocument& doc);
  static void publish(bool force = false);

  static void publishHaEntities();
  static bool publishNonStaticHaEntities(bool force = false);
  static bool publishSettings(const char* topic);
  static bool publishVariables(const char* topic);
  static std::string getTopicPath(const char* topic);
  static void __callback(char *topic, byte *payload, unsigned int length);
};

#endif