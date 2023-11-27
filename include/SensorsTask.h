#ifndef SENSORTASK_H
#define SENSORTASK_H

#include "common.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LeanTask.h>

class SensorsTask : public LeanTask {
public:
  SensorsTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

protected:
  OneWire* oneWireOutdoorSensor;
  OneWire* oneWireIndoorSensor;

  DallasTemperature* outdoorSensor;
  DallasTemperature* indoorSensor;

  bool initOutdoorSensor = false;
  unsigned long startOutdoorConversionTime = 0;
  float filteredOutdoorTemp = 0;
  bool emptyOutdoorTemp = true;

  bool initIndoorSensor[8] = {false};
  unsigned long startIndoorConversionTime = 0;
  float filteredIndoorTemp = 0;
  bool emptyIndoorTemp = true;


  const char* getTaskName();

  void loop();

  void outdoorTemperatureSensor();
  void indoorTemperatureSensor(int i);
};

#endif