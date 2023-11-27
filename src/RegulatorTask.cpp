#include <RegulatorTask.h>

extern Variables vars;
extern Settings settings;
extern TinyLogger Log;

Equitherm etRegulator;
GyverPID pidRegulator(0, 0, 0);
PIDtuner pidTuner;

const char *RegulatorTask::getTaskName()
{
  return "Regulator";
}

int RegulatorTask::getTaskCore()
{
  return 1;
}

void RegulatorTask::loop()
{
  byte newTemp[INDOOR_SENSOR_NUMBER] = {vars.parameters.heatingSetpoint};

  if (vars.states.emergency)
  {
    if (settings.heating.turbo)
    {
      settings.heating.turbo = false;
      Log.sinfoln("REGULATOR", F("Turbo mode auto disabled"));
    }

    getEmergencyModeTemp(newTemp);
  }
  else
  {
    if (vars.tuning.enable || tunerInit)
    {
      if (settings.heating.turbo)
      {
        settings.heating.turbo = false;
        Log.sinfoln("REGULATOR", F("Turbo mode auto disabled"));
      }
      getTuningModeTemp(newTemp);
      if (newTemp == 0)
      {
        vars.tuning.enable = false;
      }
    }

    if (!vars.tuning.enable)
    {
      for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
      {
        if (settings.heating.turbo && (fabs(settings.heating.target[i] - vars.temperatures.indoor[i]) < 1 || (settings.equitherm[i].enable && settings.pid[i].enable)))
        {
          settings.heating.turbo = false;
          Log.sinfoln("REGULATOR", F("Turbo mode auto disabled"));
        }
      }

      getNormalModeTemp(newTemp);
    }
  }

  // Restrict if not previously restricted
  for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
  {
    if (newTemp[i] < settings.heating.minTemp || newTemp[i] > settings.heating.maxTemp)
    {
      newTemp[i] = constrain(newTemp[i], settings.heating.minTemp, settings.heating.maxTemp);
    }

    if (abs(vars.parameters.heatingSetpoint - newTemp[i]) + 0.0001 >= 1)
    {
      vars.parameters.heatingSetpoint = newTemp[i];
    }
  }
}

void RegulatorTask::getEmergencyModeTemp(byte *newTemp)
{
  float _newTemp = 0;

  // if use equitherm
  if (settings.emergency.useEquitherm && settings.sensors.outdoor.type != OUTDOOR_SENSOR_TYPE::OUTDOOR_SENSOR_MANUAL)
  {
    float etResult = getEquithermTemp(settings.heating.minTemp, settings.heating.maxTemp);

    if (fabs(prevEtResult - etResult) + 0.0001 >= 0.5)
    {
      prevEtResult = etResult;
      _newTemp += etResult;

      Log.sinfoln("REGULATOR.EQUITHERM", F("New emergency result: %u (%f)"), (int)round(etResult), etResult);
    }
    else
    {
      _newTemp += prevEtResult;
    }
  }
  else
  {
    // default temp, manual mode
    _newTemp = settings.emergency.target;
  }

  return round(newTemp);
}

byte *RegulatorTask::getNormalModeTemp()
{
  float newTemp[INDOOR_SENSOR_NUMBER] = {0};

  for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
  {
    if (fabs(prevHeatingTarget - settings.heating.target[i]) > 0.0001)
    {
      prevHeatingTarget = settings.heating.target[i];
      Log.sinfoln("REGULATOR", F("New target: %f"), settings.heating.target[i]);

      if (settings.equitherm[i].enable && settings.pid[i].enable)
      {
        pidRegulator.integral = 0;
        Log.sinfoln("REGULATOR.PID", F("Integral sum has been reset"));
      }
    }

    // if use equitherm
    if (settings.equitherm[i].enable)
    {
      float etResult = getEquithermTemp(settings.heating.minTemp, settings.heating.maxTemp);

      if (fabs(prevEtResult - etResult) + 0.0001 >= 0.5)
      {
        prevEtResult = etResult;
        newTemp[i] += etResult;

        Log.sinfoln("REGULATOR.EQUITHERM", F("New result: %u (%f)"), (int)round(etResult), etResult);
      }
      else
      {
        newTemp[i] += prevEtResult;
      }
    }

    // if use pid
    if (settings.pid[i].enable && vars.parameters.heatingEnabled)
    {
      float pidResult = getPidTemp(
          settings.equitherm[i].enable ? (settings.pid[i].maxTemp * -1) : settings.pid[i].minTemp,
          settings.equitherm[i].enable ? settings.pid[i].maxTemp : settings.pid[i].maxTemp);

      if (fabs(prevPidResult - pidResult) + 0.0001 >= 0.5)
      {
        prevPidResult = pidResult;
        newTemp[i] += pidResult;

        Log.sinfoln("REGULATOR.PID", F("New result: %d (%f)"), (int)round(pidResult), pidResult);
      }
      else
      {
        newTemp[i] += prevPidResult;
      }
    }
    else if (settings.pid[i].enable && !vars.parameters.heatingEnabled && prevPidResult != 0)
    {
      newTemp[i] += prevPidResult;
    }

    // default temp, manual mode
    if (!settings.equitherm[i].enable && !settings.pid[i].enable)
    {
      newTemp[i] = settings.heating.target[i];
    }

    newTemp[i] = round(newTemp[i]);
    newTemp[i] = constrain(newTemp[i], 0, 100);
  }

  return *newTemp;
}

byte RegulatorTask::getTuningModeTemp()
{
  if (tunerInit && (!vars.tuning.enable || vars.tuning.regulator != tunerRegulator))
  {
    if (tunerRegulator == 0)
    {
      pidTuner.reset();
    }

    tunerInit = false;
    tunerRegulator = 0;
    tunerState = 0;
    Log.sinfoln("REGULATOR.TUNING", F("Stopped"));
  }

  if (!vars.tuning.enable)
  {
    return 0;
  }

  if (vars.tuning.regulator == 0)
  {
    // @TODO дописать
    Log.sinfoln("REGULATOR.TUNING.EQUITHERM", F("Not implemented"));
    return 0;
  }
  else if (vars.tuning.regulator == 1)
  {
    // PID tuner
    float defaultTemp = settings.equitherm.enable
                            ? getEquithermTemp(settings.heating.minTemp, settings.heating.maxTemp)
                            : settings.heating.target;

    if (tunerInit && pidTuner.getState() == 3)
    {
      Log.sinfoln("REGULATOR.TUNING.PID", F("Finished"));
      for (Stream *stream : Log.getStreams())
      {
        pidTuner.debugText(stream);
      }

      pidTuner.reset();
      tunerInit = false;
      tunerRegulator = 0;
      tunerState = 0;

      if (pidTuner.getAccuracy() < 90)
      {
        Log.swarningln("REGULATOR.TUNING.PID", F("Bad result, try again..."));
      }
      else
      {
        settings.pid.p_factor = pidTuner.getPID_p();
        settings.pid.i_factor = pidTuner.getPID_i();
        settings.pid.d_factor = pidTuner.getPID_d();

        return 0;
      }
    }

    if (!tunerInit)
    {
      Log.sinfoln("REGULATOR.TUNING.PID", F("Start..."));

      float step;
      if (vars.temperatures.indoor - vars.temperatures.outdoor > 10)
      {
        step = ceil(vars.parameters.heatingSetpoint / vars.temperatures.indoor * 2);
      }
      else
      {
        step = 5.0f;
      }

      float startTemp = step;
      Log.sinfoln("REGULATOR.TUNING.PID", F("Started. Start value: %f, step: %f"), startTemp, step);
      pidTuner.setParameters(NORMAL, startTemp, step, 20 * 60 * 1000, 0.15, 60 * 1000, 10000);
      tunerInit = true;
      tunerRegulator = 1;
    }

    pidTuner.setInput(vars.temperatures.indoor);
    pidTuner.compute();

    if (tunerState > 0 && pidTuner.getState() != tunerState)
    {
      Log.sinfoln("REGULATOR.TUNING.PID", F("Log:"));
      for (Stream *stream : Log.getStreams())
      {
        pidTuner.debugText(stream);
      }

      tunerState = pidTuner.getState();
    }

    return round(defaultTemp + pidTuner.getOutput());
  }
  else
  {
    return 0;
  }
}

float RegulatorTask::getEquithermTemp(int minTemp, int maxTemp)
{
  if (vars.states.emergency)
  {
    etRegulator.Kt = 0;
    etRegulator.indoorTemp = 0;
    etRegulator.outdoorTemp = vars.temperatures.outdoor;
  }
  else if (settings.pid.enable)
  {
    etRegulator.Kt = 0;
    etRegulator.indoorTemp = round(vars.temperatures.indoor);
    etRegulator.outdoorTemp = round(vars.temperatures.outdoor);
  }
  else
  {
    if (settings.heating.turbo)
    {
      etRegulator.Kt = 10;
    }
    else
    {
      etRegulator.Kt = settings.equitherm.t_factor;
    }
    etRegulator.indoorTemp = vars.temperatures.indoor;
    etRegulator.outdoorTemp = vars.temperatures.outdoor;
  }

  etRegulator.setLimits(minTemp, maxTemp);
  etRegulator.Kn = settings.equitherm.n_factor;
  // etRegulator.Kn = tuneEquithermN(etRegulator.Kn, vars.temperatures.indoor, settings.heating.target, 300, 1800, 0.01, 1);
  etRegulator.Kk = settings.equitherm.k_factor;
  etRegulator.targetTemp = vars.states.emergency ? settings.emergency.target : settings.heating.target;

  return etRegulator.getResult();
}

float RegulatorTask::getPidTemp(int minTemp, int maxTemp)
{
  pidRegulator.Kp = settings.pid.p_factor;
  pidRegulator.Ki = settings.pid.i_factor;
  pidRegulator.Kd = settings.pid.d_factor;

  pidRegulator.setLimits(minTemp, maxTemp);
  pidRegulator.input = vars.temperatures.indoor;
  pidRegulator.setpoint = settings.heating.target;

  return pidRegulator.getResultNow();
}

float RegulatorTask::tuneEquithermN(float ratio, float currentTemp, float setTemp, unsigned int dirtyInterval, unsigned int accurateInterval, float accurateStep, float accurateStepAfter)
{
  static uint32_t _prevIteration = millis();

  if (abs(currentTemp - setTemp) < accurateStepAfter)
  {
    if (millis() - _prevIteration < (accurateInterval * 1000))
    {
      return ratio;
    }

    if (currentTemp - setTemp > 0.1f)
    {
      ratio -= accurateStep;
    }
    else if (currentTemp - setTemp < -0.1f)
    {
      ratio += accurateStep;
    }
  }
  else
  {
    if (millis() - _prevIteration < (dirtyInterval * 1000))
    {
      return ratio;
    }

    ratio = ratio * (setTemp / currentTemp);
  }

  _prevIteration = millis();
  return ratio;
}
