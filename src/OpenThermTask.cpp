#include <OpenThermTask.h>

CustomOpenTherm *ot;
extern Variables vars;
extern Settings settings;
extern EEManager eeSettings;
extern TinyLogger Log;

const char S_OT[] PROGMEM = "OT";
const char S_OT_DHW[] PROGMEM = "OT.DHW";
const char S_OT_HEATING[] PROGMEM = "OT.HEATING";

void IRAM_ATTR OpenThermTask::handleInterrupt()
{
  ot->handleInterrupt();
}

const char *OpenThermTask::getTaskName()
{
  return "OpenTherm";
}

int OpenThermTask::getTaskCore()
{
  return 1;
}

void OpenThermTask::setup()
{
  Log.sinfoln(FPSTR(S_OT), F("Started. GPIO IN: %d, GPIO OUT: %d"), settings.opentherm.inPin, settings.opentherm.outPin);

  ot = new CustomOpenTherm(settings.opentherm.inPin, settings.opentherm.outPin);

  ot->setHandleSendRequestCallback(this->sendRequestCallback);
  ot->begin(OpenThermTask::handleInterrupt, this->responseCallback);

  ot->setYieldCallback([](void *self)
                       { static_cast<OpenThermTask *>(self)->delay(10); },
                       this);

#ifdef LED_OT_RX_PIN
  pinMode(LED_OT_RX_PIN, OUTPUT);
  digitalWrite(LED_OT_RX_PIN, false);
#endif

#ifdef HEATING_STATUS_PIN
  pinMode(HEATING_STATUS_PIN, OUTPUT);
  digitalWrite(HEATING_STATUS_PIN, false);
#endif
}

void OpenThermTask::loop()
{
  static byte currentHeatingTemp, currentDhwTemp = 0;
  unsigned long localResponse;

  if (millis() - prevUpdateNonEssentialVars > 60000)
  {
    if (updateSlaveVersion())
    {
      Log.straceln(FPSTR(S_OT), F("Slave version: %u, type: %u"), vars.parameters.slaveVersion, vars.parameters.slaveType);
    }
    else
    {
      Log.swarningln(FPSTR(S_OT), F("Get slave version failed"));
    }

    // 0x013F
    if (setMasterVersion(0x3F, 0x01))
    {
      Log.straceln(FPSTR(S_OT), F("Master version: %u, type: %u"), vars.parameters.masterVersion, vars.parameters.masterType);
    }
    else
    {
      Log.swarningln(FPSTR(S_OT), F("Set master version failed"));
    }

    if (updateSlaveConfig())
    {
      Log.straceln(FPSTR(S_OT), F("Slave member id: %u, flags: %u"), vars.parameters.slaveMemberId, vars.parameters.slaveFlags);
    }
    else
    {
      Log.swarningln(FPSTR(S_OT), F("Get slave config failed"));
    }

    if (setMasterConfig(settings.opentherm.memberIdCode & 0xFF, (settings.opentherm.memberIdCode & 0xFFFF) >> 8))
    {
      Log.straceln(FPSTR(S_OT), F("Master member id: %u, flags: %u"), vars.parameters.masterMemberId, vars.parameters.masterFlags);
    }
    else
    {
      Log.swarningln(FPSTR(S_OT), F("Set master config failed"));
    }

    yield();
  }

  bool heatingEnabled = (vars.states.emergency || settings.heating.enable) && pump && isReady();
  bool heatingCh2Enabled = settings.opentherm.heatingCh2Enabled;
  if (settings.opentherm.heatingCh1ToCh2)
  {
    heatingCh2Enabled = heatingEnabled;
  }
  else if (settings.opentherm.dhwToCh2)
  {
    heatingCh2Enabled = settings.opentherm.dhwPresent && settings.dhw.enable;
  }

  localResponse = ot->setBoilerStatus(
      heatingEnabled,
      settings.opentherm.dhwPresent && settings.dhw.enable,
      false,
      false,
      heatingCh2Enabled,
      settings.opentherm.summerWinterMode,
      settings.opentherm.dhwBlocking);

  if (!ot->isValidResponse(localResponse))
  {
    Log.swarningln(FPSTR(S_OT), F("Invalid response after setBoilerStatus: %s"), ot->statusToString(ot->getLastResponseStatus()));
    return;
  }

  if (vars.parameters.heatingEnabled != heatingEnabled)
  {
    prevUpdateNonEssentialVars = 0;
    vars.parameters.heatingEnabled = heatingEnabled;
    Log.sinfoln(FPSTR(S_OT_HEATING), "%s", heatingEnabled ? F("Enabled") : F("Disabled"));

#ifdef HEATING_STATUS_PIN
    digitalWrite(HEATING_STATUS_PIN, heatingEnabled);
#endif
  }

  vars.states.heating = ot->isCentralHeatingActive(localResponse);
  vars.states.dhw = settings.opentherm.dhwPresent ? ot->isHotWaterActive(localResponse) : false;
  vars.states.flame = ot->isFlameOn(localResponse);
  vars.states.fault = ot->isFault(localResponse);
  vars.states.diagnostic = ot->isDiagnostic(localResponse);

  //
  // These parameters will be updated every minute
  if (millis() - prevUpdateNonEssentialVars > 60000)
  {
    if (!heatingEnabled && settings.opentherm.modulationSyncWithHeating)
    {
      if (setMaxModulationLevel(0))
      {
        Log.snoticeln(FPSTR(S_OT_HEATING), F("Set max modulation 0% (off)"));
      }
      else
      {
        Log.swarningln(FPSTR(S_OT_HEATING), F("Failed set max modulation 0% (off)"));
      }
    }
    else
    {
      if (setMaxModulationLevel(settings.heating.maxModulation))
      {
        Log.snoticeln(FPSTR(S_OT_HEATING), F("Set max modulation %d%"), settings.heating.maxModulation);
      }
      else
      {
        Log.swarningln(FPSTR(S_OT_HEATING), F("Failed set max modulation %d%"), settings.heating.maxModulation);
      }
    }
    yield();

    // DHW min/max temp
    if (settings.opentherm.dhwPresent)
    {
      if (updateMinMaxDhwTemp())
      {
        if (settings.dhw.minTemp < vars.parameters.dhwMinTemp)
        {
          settings.dhw.minTemp = vars.parameters.dhwMinTemp;
          eeSettings.update();
          Log.snoticeln(FPSTR(S_OT_DHW), F("Updated min temp: %d"), settings.dhw.minTemp);
        }

        if (settings.dhw.maxTemp > vars.parameters.dhwMaxTemp)
        {
          settings.dhw.maxTemp = vars.parameters.dhwMaxTemp;
          eeSettings.update();
          Log.snoticeln(FPSTR(S_OT_DHW), F("Updated max temp: %d"), settings.dhw.maxTemp);
        }
      }
      else
      {
        Log.swarningln(FPSTR(S_OT_DHW), F("Failed get min/max temp"));
      }

      if (settings.dhw.minTemp >= settings.dhw.maxTemp)
      {
        settings.dhw.minTemp = 30;
        settings.dhw.maxTemp = 60;
        eeSettings.update();
      }

      yield();
    }

    // Heating min/max temp
    if (updateMinMaxHeatingTemp())
    {
      if (settings.heating.minTemp < vars.parameters.heatingMinTemp)
      {
        settings.heating.minTemp = vars.parameters.heatingMinTemp;
        eeSettings.update();
        Log.snoticeln(FPSTR(S_OT_HEATING), F("Updated min temp: %d"), settings.heating.minTemp);
      }

      if (settings.heating.maxTemp > vars.parameters.heatingMaxTemp)
      {
        settings.heating.maxTemp = vars.parameters.heatingMaxTemp;
        eeSettings.update();
        Log.snoticeln(FPSTR(S_OT_HEATING), F("Updated max temp: %d"), settings.heating.maxTemp);
      }
    }
    else
    {
      Log.swarningln(FPSTR(S_OT_HEATING), F("Failed get min/max temp"));
    }
    yield();

    if (settings.heating.minTemp >= settings.heating.maxTemp)
    {
      settings.heating.minTemp = 20;
      settings.heating.maxTemp = 90;
      eeSettings.update();
    }

    // force set max CH temp
    setMaxHeatingTemp(settings.heating.maxTemp);

    if (settings.sensors.outdoor.type == OUTDOOR_SENSOR_TYPE::OUTDOOR_SENSOR_BOILER)
    {
      updateOutsideTemp();
    }

    if (vars.states.fault)
    {
      updateFaultCode();
    }

    prevUpdateNonEssentialVars = millis();
    yield();
  }

  updatePressure();
  if ((settings.opentherm.dhwPresent && settings.dhw.enable) || settings.heating.enable || heatingEnabled)
  {
    updateModulationLevel();
  }
  else
  {
    vars.sensors.modulation = 0;
  }
  yield();

  if (settings.opentherm.dhwPresent)
  {
    updateDhwTemp();
    updateDhwFlowRate();
    yield();
  }
  else
  {
    vars.temperatures.dhw = 0.0f;
    vars.sensors.dhwFlowRate = 0.0f;
  }

  updateHeatingTemp();
  yield();

  // fault reset action
  if (vars.actions.resetFault)
  {
    if (vars.states.fault)
    {
      if (ot->sendBoilerReset())
      {
        Log.sinfoln(FPSTR(S_OT), F("Boiler fault reset successfully"));
      }
      else
      {
        Log.serrorln(FPSTR(S_OT), F("Boiler fault reset failed"));
      }
    }

    vars.actions.resetFault = false;
    yield();
  }

  // diag reset action
  if (vars.actions.resetDiagnostic)
  {
    if (vars.states.diagnostic)
    {
      if (ot->sendServiceReset())
      {
        Log.sinfoln(FPSTR(S_OT), F("Boiler diagnostic reset successfully"));
      }
      else
      {
        Log.serrorln(FPSTR(S_OT), F("Boiler diagnostic reset failed"));
      }
    }

    vars.actions.resetDiagnostic = false;
    yield();
  }

  //
  // Температура ГВС
  byte newDhwTemp = settings.dhw.target;
  if (settings.opentherm.dhwPresent && settings.dhw.enable && (needSetDhwTemp() || newDhwTemp != currentDhwTemp))
  {
    if (newDhwTemp < settings.dhw.minTemp || newDhwTemp > settings.dhw.maxTemp)
    {
      newDhwTemp = constrain(newDhwTemp, settings.dhw.minTemp, settings.dhw.maxTemp);
    }

    Log.sinfoln(FPSTR(S_OT_DHW), F("Set temp = %u"), newDhwTemp);

    // Записываем заданную температуру ГВС
    if (ot->setDhwTemp(newDhwTemp))
    {
      currentDhwTemp = newDhwTemp;
      dhwSetTempTime = millis();
    }
    else
    {
      Log.swarningln(FPSTR(S_OT_DHW), F("Failed set temp"));
    }

    if (settings.opentherm.dhwToCh2)
    {
      if (!ot->setHeatingCh2Temp(newDhwTemp))
      {
        Log.swarningln(FPSTR(S_OT_DHW), F("Failed set ch2 temp"));
      }
    }

    yield();
  }

  //
  // Температура отопления
  if (heatingEnabled && (needSetHeatingTemp() || fabs(vars.parameters.heatingSetpoint - currentHeatingTemp) > 0.0001))
  {
    Log.sinfoln(FPSTR(S_OT_HEATING), F("Set temp = %u"), vars.parameters.heatingSetpoint);

    // Записываем заданную температуру
    if (ot->setHeatingCh1Temp(vars.parameters.heatingSetpoint))
    {
      currentHeatingTemp = vars.parameters.heatingSetpoint;
      heatingSetTempTime = millis();
    }
    else
    {
      Log.swarningln(FPSTR(S_OT_HEATING), F("Failed set temp"));
    }

    if (settings.opentherm.heatingCh1ToCh2)
    {
      if (!ot->setHeatingCh2Temp(vars.parameters.heatingSetpoint))
      {
        Log.swarningln(FPSTR(S_OT_HEATING), F("Failed set ch2 temp"));
      }
    }

    yield();
  }

  // switching difference (hysteresis)
  // only for pid and/or equitherm
  for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
  {
    if (settings.heating.hysteresis > 0 && !vars.states.emergency && (settings.equitherm[i].enable || settings.pid[i].enable))
    {
      float halfHyst = settings.heating.hysteresis / 2;
      if (pump && vars.temperatures.indoor[i] - settings.heating.target + 0.0001 >= halfHyst)
      {
        pump |= false;
      }
      else if (!pump && vars.temperatures.indoor[i] - settings.heating.target - 0.0001 <= -(halfHyst))
      {
        pump |= true;
      }
    }
    else if (!pump)
    {
      pump = true;
    }
  }
}

void OpenThermTask::sendRequestCallback(unsigned long request, unsigned long response, OpenThermResponseStatus status, byte attempt)
{
  printRequestDetail(ot->getDataID(request), status, request, response, attempt);
}

void OpenThermTask::responseCallback(unsigned long result, OpenThermResponseStatus status)
{
  static byte attempt = 0;

  switch (status)
  {
  case OpenThermResponseStatus::TIMEOUT:
    if (vars.states.otStatus && ++attempt > OPENTHERM_OFFLINE_TRESHOLD)
    {
      vars.states.otStatus = false;
      attempt = OPENTHERM_OFFLINE_TRESHOLD;
    }
    break;

  case OpenThermResponseStatus::SUCCESS:
    attempt = 0;
    if (!vars.states.otStatus)
    {
      vars.states.otStatus = true;
    }

#ifdef LED_OT_RX_PIN
    {
      digitalWrite(LED_OT_RX_PIN, true);
      unsigned long ts = millis();
      while (millis() - ts < 2)
      {
      }
      digitalWrite(LED_OT_RX_PIN, false);
    }
#endif
    break;

  default:
    break;
  }
}

bool OpenThermTask::isReady()
{
  return millis() - startupTime > readyTime;
}

bool OpenThermTask::needSetDhwTemp()
{
  return millis() - dhwSetTempTime > dhwSetTempInterval;
}

bool OpenThermTask::needSetHeatingTemp()
{
  return millis() - heatingSetTempTime > heatingSetTempInterval;
}

void OpenThermTask::printRequestDetail(OpenThermMessageID id, OpenThermResponseStatus status, unsigned long request, unsigned long response, byte attempt)
{
  Log.straceln(FPSTR(S_OT), F("OT REQUEST ID: %4d   Request: %8lx   Response: %8lx   Attempt: %2d   Status: %s"), id, request, response, attempt, ot->statusToString(status));
}

bool OpenThermTask::updateSlaveConfig()
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SConfigSMemberIDcode, 0));
  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.parameters.slaveMemberId = response & 0xFF;
  vars.parameters.slaveFlags = (response & 0xFFFF) >> 8;

  /*uint8_t flags = (response & 0xFFFF) >> 8;
  Log.straceln(
    "OT",
    F("MasterMemberIdCode:\r\n  DHW present: %u\r\n  Control type: %u\r\n  Cooling configuration: %u\r\n  DHW configuration: %u\r\n  Pump control: %u\r\n  CH2 present: %u\r\n  Remote water filling function: %u\r\n  Heat/cool mode control: %u\r\n  Slave MemberID Code: %u\r\n  Raw: %u"),
    (bool) (flags & 0x01),
    (bool) (flags & 0x02),
    (bool) (flags & 0x04),
    (bool) (flags & 0x08),
    (bool) (flags & 0x10),
    (bool) (flags & 0x20),
    (bool) (flags & 0x40),
    (bool) (flags & 0x80),
    response & 0xFF,
    response
  );*/

  return true;
}

/**
 * @brief Set the Master Config
 * From slave member id code:
 * id: slave.memberIdCode & 0xFF,
 * flags: (slave.memberIdCode & 0xFFFF) >> 8
 * @param id
 * @param flags
 * @param force
 * @return true
 * @return false
 */
bool OpenThermTask::setMasterConfig(uint8_t id, uint8_t flags, bool force)
{
  // uint8_t configId = settings.opentherm.memberIdCode & 0xFF;
  // uint8_t configFlags = (settings.opentherm.memberIdCode & 0xFFFF) >> 8;

  vars.parameters.masterMemberId = (force || id || settings.opentherm.memberIdCode > 65535)
                                       ? id
                                       : vars.parameters.slaveMemberId;

  vars.parameters.masterFlags = (force || flags || settings.opentherm.memberIdCode > 65535)
                                    ? flags
                                    : vars.parameters.slaveFlags;

  unsigned int request = (unsigned int)vars.parameters.masterMemberId | (unsigned int)vars.parameters.masterFlags << 8;
  // if empty request
  if (!request)
  {
    return true;
  }

  unsigned long response = ot->sendRequest(ot->buildRequest(
      OpenThermRequestType::WRITE,
      OpenThermMessageID::MConfigMMemberIDcode,
      request));

  return ot->isValidResponse(response);
}

bool OpenThermTask::setMaxModulationLevel(byte value)
{
  unsigned long response = ot->sendRequest(ot->buildRequest(
      OpenThermRequestType::WRITE,
      OpenThermMessageID::MaxRelModLevelSetting,
      ot->toF88(value)));

  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.parameters.maxModulation = ot->fromF88(response);
  return true;
}

bool OpenThermTask::updateSlaveOtVersion()
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::OpenThermVersionSlave, 0));
  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.parameters.slaveOtVersion = ot->getFloat(response);
  return true;
}

bool OpenThermTask::setMasterOtVersion(float version)
{
  unsigned long response = ot->sendRequest(ot->buildRequest(
      OpenThermRequestType::WRITE_DATA,
      OpenThermMessageID::OpenThermVersionMaster,
      ot->toF88(version)));

  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.parameters.masterOtVersion = ot->fromF88(response);

  return true;
}

bool OpenThermTask::updateSlaveVersion()
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SlaveVersion, 0));
  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.parameters.slaveVersion = response & 0xFF;
  vars.parameters.slaveType = (response & 0xFFFF) >> 8;

  return true;
}

bool OpenThermTask::setMasterVersion(uint8_t version, uint8_t type)
{
  unsigned long response = ot->sendRequest(ot->buildRequest(
      OpenThermRequestType::WRITE,
      OpenThermMessageID::MasterVersion,
      (unsigned int)version | (unsigned int)type << 8 // 0x013F
      ));

  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.parameters.masterVersion = response & 0xFF;
  vars.parameters.masterType = (response & 0xFFFF) >> 8;

  return true;
}

bool OpenThermTask::updateMinMaxDhwTemp()
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::TdhwSetUBTdhwSetLB, 0));
  if (!ot->isValidResponse(response))
  {
    return false;
  }

  byte minTemp = response & 0xFF;
  byte maxTemp = (response & 0xFFFF) >> 8;

  if (minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp)
  {
    vars.parameters.dhwMinTemp = minTemp;
    vars.parameters.dhwMaxTemp = maxTemp;

    return true;
  }

  return false;
}

bool OpenThermTask::updateMinMaxHeatingTemp()
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::MaxTSetUBMaxTSetLB, 0));
  if (!ot->isValidResponse(response))
  {
    return false;
  }

  byte minTemp = response & 0xFF;
  byte maxTemp = (response & 0xFFFF) >> 8;

  if (minTemp >= 0 && maxTemp > 0 && maxTemp > minTemp)
  {
    vars.parameters.heatingMinTemp = minTemp;
    vars.parameters.heatingMaxTemp = maxTemp;
    return true;
  }

  return false;
}

bool OpenThermTask::setMaxHeatingTemp(byte value)
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::MaxTSet, ot->temperatureToData(value)));
  if (!ot->isValidResponse(response))
  {
    return false;
  }

  return true;
}

bool OpenThermTask::updateOutsideTemp()
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Toutside, 0));
  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.temperatures.outdoor = ot->getFloat(response) + settings.sensors.outdoor.offset;
  return true;
}

bool OpenThermTask::updateHeatingTemp()
{
  unsigned long response = ot->sendRequest(ot->buildGetBoilerTemperatureRequest());
  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.temperatures.heating = ot->getFloat(response);
  return true;
}

bool OpenThermTask::updateDhwTemp()
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermMessageType::READ, OpenThermMessageID::Tdhw, 0));
  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.temperatures.dhw = ot->getFloat(response);
  return true;
}

bool OpenThermTask::updateDhwFlowRate()
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermMessageType::READ, OpenThermMessageID::DHWFlowRate, 0));
  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.sensors.dhwFlowRate = ot->getFloat(response);
  return true;
}

bool OpenThermTask::updateFaultCode()
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::ASFflags, 0));

  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.sensors.faultCode = response & 0xFF;
  return true;
}

bool OpenThermTask::updateModulationLevel()
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::RelModLevel, 0));

  if (!ot->isValidResponse(response))
  {
    return false;
  }

  float modulation = ot->fromF88(response);
  if (!vars.states.flame)
  {
    vars.sensors.modulation = 0;
  }
  else
  {
    vars.sensors.modulation = modulation;
  }

  return true;
}

bool OpenThermTask::updatePressure()
{
  unsigned long response = ot->sendRequest(ot->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::CHPressure, 0));

  if (!ot->isValidResponse(response))
  {
    return false;
  }

  vars.sensors.pressure = ot->getFloat(response);
  return true;
}