#ifndef MAINTASK_H
#define MAINTASK_H

#include <common.h>
#include <Blinker.h>
#include "WiFi.h"
#include "MqttTask.h"
#include "SensorsTask.h"
#include "OpenThermTask.h"
#include "EEManager.h"


class MainTask : public Task {
public:
  MainTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval){};

protected:
  Blinker* blinker = nullptr;
  unsigned long lastHeapInfo = 0;
  unsigned long firstFailConnect = 0;
  unsigned int heapSize = 0;
  unsigned int minFreeHeapSize = 0;
  unsigned long restartSignalTime = 0;

  const char* getTaskName();
  int getTaskCore();

  void setup();
  void loop();

  void heap();
  void ledStatus(uint8_t ledPin);
};

#endif