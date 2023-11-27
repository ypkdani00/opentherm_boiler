
#ifndef REGULATORTASK_H
#define REGULATORTASK_H

#include <common.h>
#include <Equitherm.h>
#include <GyverPID.h>
#include <PIDtuner.h>
#include <LeanTask.h>

class RegulatorTask : public LeanTask {
public:
  RegulatorTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

protected:
  bool tunerInit = false;
  byte tunerState = 0;
  byte tunerRegulator = 0;
  float prevHeatingTarget = 0;
  float prevEtResult = 0;
  float prevPidResult = 0;

  const char* getTaskName();
  int getTaskCore();
  
  void loop();

void getEmergencyModeTemp(byte *newTemp);
  byte* getNormalModeTemp();
  byte getTuningModeTemp();
  float getEquithermTemp(int minTemp, int maxTemp);
  float getPidTemp(int minTemp, int maxTemp);
  float tuneEquithermN(float ratio, float currentTemp, float setTemp, unsigned int dirtyInterval = 60, unsigned int accurateInterval = 1800, float accurateStep = 0.01, float accurateStepAfter = 1);

};

#endif