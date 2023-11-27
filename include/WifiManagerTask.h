#ifndef WIFIMANAGERTASK_H
#define WIFIMANAGERTASK_H

#define WM_MDNS

#include <common.h>
#include <Task.h>
#include <EEManager.h>
#include <ESPTelnetStream.h>
#include <WiFiManager.h>
#include <WiFiManagerParameters.h>
#include <netif/etharp.h>

#define WIFI_DEBUG_LEVEL WM_DEBUG_DEV

class WifiManagerTask : public Task {
public:
  WifiManagerTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {}

protected:
  bool connected = false;
  unsigned long lastArpGratuitous = 0;

  const char* getTaskName();
  int getTaskCore();

  void setup();
  void loop();

  static void saveParamsCallback();
  static void arpGratuitous();
};

#endif