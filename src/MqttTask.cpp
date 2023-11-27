#include "MqttTask.h"

WiFiClient espClient;
PubSubClient client(espClient);
HaHelper haHelper(client);

char buffer[255];

extern Variables vars;
extern Settings settings;
extern EEManager eeSettings;
extern TinyLogger Log;

const char *MqttTask::getTaskName()
{
  return "Mqtt";
}

int MqttTask::getTaskCore()
{
  return 0;
}

void MqttTask::setup()
{
  Log.sinfoln("MQTT", F("Started"));

  client.setCallback(__callback);
  client.setBufferSize(1024);
  haHelper.setDevicePrefix(settings.mqtt.prefix);
  haHelper.setDeviceVersion(PROJECT_VERSION);
  haHelper.setDeviceModel(PROJECT_NAME);
  haHelper.setDeviceName(PROJECT_NAME);

  sprintf(buffer, CONFIG_URL, WiFi.localIP().toString().c_str());
  haHelper.setDeviceConfigUrl(buffer);
}

void MqttTask::loop()
{
  if (!client.connected() && millis() - lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL)
  {
    Log.sinfoln("MQTT", F("Not connected, state: %i, connecting to server %s..."), client.state(), settings.mqtt.server);

    client.setServer(settings.mqtt.server, settings.mqtt.port);
    if (client.connect(settings.hostname, settings.mqtt.user, settings.mqtt.password))
    {
      Log.sinfoln("MQTT", F("Connected"));

      client.subscribe(getTopicPath("settings/set").c_str());
      client.subscribe(getTopicPath("state/set").c_str());
      publishHaEntities();
      publishNonStaticHaEntities(true);

      firstFailConnect = 0;
      lastReconnectAttempt = 0;
    }
    else
    {
      Log.swarningln("MQTT", F("Failed to connect to server"));

      if (settings.emergency.enable && !vars.states.emergency)
      {
        if (firstFailConnect == 0)
        {
          firstFailConnect = millis();
        }

        if (millis() - firstFailConnect > EMERGENCY_TIME_TRESHOLD)
        {
          vars.states.emergency = true;
          Log.sinfoln("MQTT", F("Emergency mode enabled"));
        }
      }

      lastReconnectAttempt = millis();
    }
  }

  if (client.connected())
  {
    if (vars.states.emergency)
    {
      vars.states.emergency = false;

      Log.sinfoln("MQTT", F("Emergency mode disabled"));
    }

    client.loop();
    bool published = publishNonStaticHaEntities();
    publish(published);
  }
}

bool MqttTask::updateSettings(JsonDocument &doc)
{
  bool flag = false;

  if (!doc["debug"].isNull() && doc["debug"].is<bool>())
  {
    settings.debug = doc["debug"].as<bool>();
    flag = true;
  }

  // emergency
  if (!doc["emergency"]["enable"].isNull() && doc["emergency"]["enable"].is<bool>())
  {
    settings.emergency.enable = doc["emergency"]["enable"].as<bool>();
    flag = true;
  }

  if (!doc["emergency"]["target"].isNull() && doc["emergency"]["target"].is<float>())
  {
    if (doc["emergency"]["target"].as<float>() > 0 && doc["emergency"]["target"].as<float>() < 100)
    {
      settings.emergency.target = round(doc["emergency"]["target"].as<float>() * 10) / 10;
      flag = true;
    }
  }

  if (!doc["emergency"]["useEquitherm"].isNull() && doc["emergency"]["useEquitherm"].is<bool>())
  {
    settings.emergency.useEquitherm = doc["emergency"]["useEquitherm"].as<bool>();
    flag = true;
  }

  // heating
  if (!doc["heating"]["enable"].isNull() && doc["heating"]["enable"].is<bool>())
  {
    settings.heating.enable = doc["heating"]["enable"].as<bool>();
    flag = true;
  }

  if (!doc["heating"]["turbo"].isNull() && doc["heating"]["turbo"].is<bool>())
  {
    settings.heating.turbo = doc["heating"]["turbo"].as<bool>();
    flag = true;
  }

  for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
  {
    if (!doc["heating"]["target"].isNull() && doc["heating"]["target"].is<float>())
    {
      if (doc["heating"]["target"].as<float>() > 0 && doc["heating"]["target"].as<float>() < 100)
      {
        settings.heating.target[i] = round(doc["heating"]["target"].as<float>() * 10) / 10;
        flag = true;
      }
    }
  }

  if (!doc["heating"]["hysteresis"].isNull() && doc["heating"]["hysteresis"].is<float>())
  {
    if (doc["heating"]["hysteresis"].as<float>() >= 0 && doc["heating"]["hysteresis"].as<float>() <= 5)
    {
      settings.heating.hysteresis = round(doc["heating"]["hysteresis"].as<float>() * 10) / 10;
      flag = true;
    }
  }

  if (!doc["heating"]["maxModulation"].isNull() && doc["heating"]["maxModulation"].is<unsigned char>())
  {
    if (doc["heating"]["maxModulation"].as<unsigned char>() > 0 && doc["heating"]["maxModulation"].as<unsigned char>() <= 100)
    {
      settings.heating.maxModulation = doc["heating"]["maxModulation"].as<unsigned char>();
      flag = true;
    }
  }

  if (!doc["heating"]["maxTemp"].isNull() && doc["heating"]["maxTemp"].is<unsigned char>())
  {
    if (doc["heating"]["maxTemp"].as<unsigned char>() > 0 && doc["heating"]["maxTemp"].as<unsigned char>() <= 100)
    {
      settings.heating.maxTemp = doc["heating"]["maxTemp"].as<unsigned char>();
      flag = true;
    }
  }

  if (!doc["heating"]["minTemp"].isNull() && doc["heating"]["minTemp"].is<unsigned char>())
  {
    if (doc["heating"]["minTemp"].as<unsigned char>() >= 0 && doc["heating"]["minTemp"].as<unsigned char>() < 100)
    {
      settings.heating.minTemp = doc["heating"]["minTemp"].as<unsigned char>();
      flag = true;
    }
  }

  // dhw
  if (!doc["dhw"]["enable"].isNull() && doc["dhw"]["enable"].is<bool>())
  {
    settings.dhw.enable = doc["dhw"]["enable"].as<bool>();
    flag = true;
  }

  if (!doc["dhw"]["target"].isNull() && doc["dhw"]["target"].is<unsigned char>())
  {
    if (doc["dhw"]["target"].as<unsigned char>() >= 0 && doc["dhw"]["target"].as<unsigned char>() < 100)
    {
      settings.dhw.target = doc["dhw"]["target"].as<unsigned char>();
      flag = true;
    }
  }

  if (!doc["dhw"]["maxTemp"].isNull() && doc["dhw"]["maxTemp"].is<unsigned char>())
  {
    if (doc["dhw"]["maxTemp"].as<unsigned char>() > 0 && doc["dhw"]["maxTemp"].as<unsigned char>() <= 100)
    {
      settings.dhw.maxTemp = doc["dhw"]["maxTemp"].as<unsigned char>();
      flag = true;
    }
  }

  if (!doc["dhw"]["minTemp"].isNull() && doc["dhw"]["minTemp"].is<unsigned char>())
  {
    if (doc["dhw"]["minTemp"].as<unsigned char>() >= 0 && doc["dhw"]["minTemp"].as<unsigned char>() < 100)
    {
      settings.dhw.minTemp = doc["dhw"]["minTemp"].as<unsigned char>();
      flag = true;
    }
  }

  // pid
  for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
  {
    if (!doc["pid"]["enable"].isNull() && doc["pid"]["enable"].is<bool>())
    {
      settings.pid[i].enable = doc["pid"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["pid"]["p_factor"].isNull() && doc["pid"]["p_factor"].is<float>())
    {
      if (doc["pid"]["p_factor"].as<float>() > 0 && doc["pid"]["p_factor"].as<float>() <= 10)
      {
        settings.pid[i].p_factor = round(doc["pid"]["p_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    if (!doc["pid"]["i_factor"].isNull() && doc["pid"]["i_factor"].is<float>())
    {
      if (doc["pid"]["i_factor"].as<float>() >= 0 && doc["pid"]["i_factor"].as<float>() <= 10)
      {
        settings.pid[i].i_factor = round(doc["pid"]["i_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    if (!doc["pid"]["d_factor"].isNull() && doc["pid"]["d_factor"].is<float>())
    {
      if (doc["pid"]["d_factor"].as<float>() >= 0 && doc["pid"]["d_factor"].as<float>() <= 10)
      {
        settings.pid[i].d_factor = round(doc["pid"]["d_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    if (!doc["pid"]["maxTemp"].isNull() && doc["pid"]["maxTemp"].is<unsigned char>())
    {
      if (doc["pid"]["maxTemp"].as<unsigned char>() > 0 && doc["pid"]["maxTemp"].as<unsigned char>() <= 100 && doc["pid"]["maxTemp"].as<unsigned char>() > settings.pid[i].minTemp)
      {
        settings.pid[i].maxTemp = doc["pid"]["maxTemp"].as<unsigned char>();
        flag = true;
      }
    }

    if (!doc["pid"]["minTemp"].isNull() && doc["pid"]["minTemp"].is<unsigned char>())
    {
      if (doc["pid"]["minTemp"].as<unsigned char>() >= 0 && doc["pid"]["minTemp"].as<unsigned char>() < 100 && doc["pid"]["minTemp"].as<unsigned char>() < settings.pid[i].maxTemp)
      {
        settings.pid[i].minTemp = doc["pid"]["minTemp"].as<unsigned char>();
        flag = true;
      }
    }

    // equitherm
    if (!doc["equitherm"]["enable"].isNull() && doc["equitherm"]["enable"].is<bool>())
    {
      settings.equitherm[i].enable = doc["equitherm"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["equitherm"]["n_factor"].isNull() && doc["equitherm"]["n_factor"].is<float>())
    {
      if (doc["equitherm"]["n_factor"].as<float>() > 0 && doc["equitherm"]["n_factor"].as<float>() <= 10)
      {
        settings.equitherm[i].n_factor = round(doc["equitherm"]["n_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    if (!doc["equitherm"]["k_factor"].isNull() && doc["equitherm"]["k_factor"].is<float>())
    {
      if (doc["equitherm"]["k_factor"].as<float>() >= 0 && doc["equitherm"]["k_factor"].as<float>() <= 10)
      {
        settings.equitherm[i].k_factor = round(doc["equitherm"]["k_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }

    if (!doc["equitherm"]["t_factor"].isNull() && doc["equitherm"]["t_factor"].is<float>())
    {
      if (doc["equitherm"]["t_factor"].as<float>() >= 0 && doc["equitherm"]["t_factor"].as<float>() <= 10)
      {
        settings.equitherm[i].t_factor = round(doc["equitherm"]["t_factor"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }
  }

  // sensors
  if (!doc["sensors"]["outdoor"]["type"].isNull() && doc["sensors"]["outdoor"]["type"].is<unsigned char>())
  {
    if (doc["sensors"]["outdoor"]["type"].as<unsigned char>() >= 0 && doc["sensors"]["outdoor"]["type"].as<unsigned char>() <= 2)
    {
      settings.sensors.outdoor.type = doc["sensors"]["outdoor"]["type"].as<unsigned char>();
      flag = true;
    }
  }

  if (!doc["sensors"]["outdoor"]["offset"].isNull() && doc["sensors"]["outdoor"]["offset"].is<float>())
  {
    if (doc["sensors"]["outdoor"]["offset"].as<float>() >= -10 && doc["sensors"]["outdoor"]["offset"].as<float>() <= 10)
    {
      settings.sensors.outdoor.offset = round(doc["sensors"]["outdoor"]["offset"].as<float>() * 1000) / 1000;
      flag = true;
    }
  }

  for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
  {
    if (!doc["sensors"]["indoor"]["type"].isNull() && doc["sensors"]["indoor"]["type"].is<unsigned char>())
    {
      if (doc["sensors"]["indoor"]["type"].as<unsigned char>() >= 1 && doc["sensors"]["indoor"]["type"].as<unsigned char>() <= 2)
      {
        settings.sensors.indoor[i].type = doc["sensors"]["indoor"]["type"].as<unsigned char>();
        flag = true;
      }
    }

    if (!doc["sensors"]["indoor"]["offset"].isNull() && doc["sensors"]["indoor"]["offset"].is<float>())
    {
      if (doc["sensors"]["indoor"]["offset"].as<float>() >= -10 && doc["sensors"]["indoor"]["offset"].as<float>() <= 10)
      {
        settings.sensors.indoor[i].offset = round(doc["sensors"]["indoor"]["offset"].as<float>() * 1000) / 1000;
        flag = true;
      }
    }
  }

  if (flag)
  {
    eeSettings.update();
    publish(true);

    return true;
  }

  return false;
}

bool MqttTask::updateVariables(const JsonDocument &doc)
{
  bool flag = false;

  if (!doc["ping"].isNull() && doc["ping"])
  {
    flag = true;
  }

  if (!doc["tuning"]["enable"].isNull() && doc["tuning"]["enable"].is<bool>())
  {
    vars.tuning.enable = doc["tuning"]["enable"].as<bool>();
    flag = true;
  }

  if (!doc["tuning"]["regulator"].isNull() && doc["tuning"]["regulator"].is<unsigned char>())
  {
    if (doc["tuning"]["regulator"].as<unsigned char>() >= 0 && doc["tuning"]["regulator"].as<unsigned char>() <= 1)
    {
      vars.tuning.regulator = doc["tuning"]["regulator"].as<unsigned char>();
      flag = true;
    }
  }

  for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
  {
    if (!doc["temperatures"]["indoor"].isNull() && doc["temperatures"]["indoor"].is<float>())
    {
      if (settings.sensors.indoor[i].type == INDOOR_SENSOR_TYPE::INDOOR_SENSOR_MANUAL && doc["temperatures"]["indoor"].as<float>() > -100 && doc["temperatures"]["indoor"].as<float>() < 100)
      {
        vars.temperatures.indoor[i] = round(doc["temperatures"]["indoor"].as<float>() * 100) / 100;
        flag = true;
      }
    }
  }

  if (!doc["temperatures"]["outdoor"].isNull() && doc["temperatures"]["outdoor"].is<float>())
  {
    if (settings.sensors.outdoor.type == OUTDOOR_SENSOR_TYPE::OUTDOOR_SENSOR_MANUAL && doc["temperatures"]["outdoor"].as<float>() > -100 && doc["temperatures"]["outdoor"].as<float>() < 100)
    {
      vars.temperatures.outdoor = round(doc["temperatures"]["outdoor"].as<float>() * 100) / 100;
      flag = true;
    }
  }

  if (!doc["actions"]["restart"].isNull() && doc["actions"]["restart"].is<bool>() && doc["actions"]["restart"].as<bool>())
  {
    vars.actions.restart = true;
  }

  if (!doc["actions"]["resetFault"].isNull() && doc["actions"]["resetFault"].is<bool>() && doc["actions"]["resetFault"].as<bool>())
  {
    vars.actions.resetFault = true;
  }

  if (!doc["actions"]["resetDiagnostic"].isNull() && doc["actions"]["resetDiagnostic"].is<bool>() && doc["actions"]["resetDiagnostic"].as<bool>())
  {
    vars.actions.resetDiagnostic = true;
  }

  if (flag)
  {
    publish(true);

    return true;
  }

  return false;
}

void MqttTask::publish(bool force)
{
  static unsigned int prevPubVars = 0;
  static unsigned int prevPubSettings = 0;

  // publish variables and status
  if (force || millis() - prevPubVars > settings.mqtt.interval)
  {
    publishVariables(getTopicPath("state").c_str());

    if (vars.states.fault)
    {
      client.publish(getTopicPath("status").c_str(), "fault");
    }
    else
    {
      client.publish(getTopicPath("status").c_str(), vars.states.otStatus ? "online" : "offline");
    }

    prevPubVars = millis();
  }

  // publish settings
  if (force || millis() - prevPubSettings > settings.mqtt.interval * 10)
  {
    publishSettings(getTopicPath("settings").c_str());
    prevPubSettings = millis();
  }
}

void MqttTask::publishHaEntities()
{
  // main
  haHelper.publishSelectOutdoorSensorType();
  haHelper.publishSelectIndoorSensorType();
  haHelper.publishNumberOutdoorSensorOffset(false);
  haHelper.publishNumberIndoorSensorOffset(false);
  haHelper.publishSwitchDebug(false);

  // emergency
  haHelper.publishSwitchEmergency();
  haHelper.publishNumberEmergencyTarget();
  haHelper.publishSwitchEmergencyUseEquitherm();

  // heating
  haHelper.publishSwitchHeating(false);
  haHelper.publishSwitchHeatingTurbo();
  haHelper.publishNumberHeatingHysteresis();
  haHelper.publishSensorHeatingSetpoint(false);
  haHelper.publishSensorCurrentHeatingMinTemp(false);
  haHelper.publishSensorCurrentHeatingMaxTemp(false);
  haHelper.publishNumberHeatingMinTemp(false);
  haHelper.publishNumberHeatingMaxTemp(false);
  haHelper.publishNumberHeatingMaxModulation(false);

  // pid
  haHelper.publishSwitchPID();
  haHelper.publishNumberPIDFactorP();
  haHelper.publishNumberPIDFactorI();
  haHelper.publishNumberPIDFactorD();
  haHelper.publishNumberPIDMinTemp(false);
  haHelper.publishNumberPIDMaxTemp(false);

  // equitherm
  haHelper.publishSwitchEquitherm();
  haHelper.publishNumberEquithermFactorN();
  haHelper.publishNumberEquithermFactorK();
  haHelper.publishNumberEquithermFactorT();

  // tuning
  haHelper.publishSwitchTuning();
  haHelper.publishSelectTuningRegulator();

  // states
  haHelper.publishBinSensorStatus();
  haHelper.publishBinSensorOtStatus();
  haHelper.publishBinSensorHeating();
  haHelper.publishBinSensorFlame();
  haHelper.publishBinSensorFault();
  haHelper.publishBinSensorDiagnostic();

  // sensors
  haHelper.publishSensorModulation(false);
  haHelper.publishSensorPressure(false);
  haHelper.publishSensorFaultCode();
  haHelper.publishSensorRssi(false);
  haHelper.publishSensorUptime(false);

  // temperatures
  haHelper.publishNumberIndoorTemp();
  haHelper.publishSensorHeatingTemp();

  // buttons
  haHelper.publishButtonRestart(false);
  haHelper.publishButtonResetFault();
  haHelper.publishButtonResetDiagnostic();
}

bool MqttTask::publishNonStaticHaEntities(bool force)
{
  static byte _heatingMinTemp, _heatingMaxTemp, _dhwMinTemp, _dhwMaxTemp;
  static bool _editableIndoorTemp[INDOOR_SENSOR_NUMBER], _editableOutdoorTemp, _dhwPresent;

  bool published = false;
  bool isStupidMode[INDOOR_SENSOR_NUMBER];
  byte heatingMinTemp = isStupidMode ? vars.parameters.heatingMinTemp : 10;
  byte heatingMaxTemp = isStupidMode ? vars.parameters.heatingMaxTemp : 30;
  for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
  {
    isStupidMode[INDOOR_SENSOR_NUMBER] = !settings.pid[i].enable && !settings.equitherm[i].enable;
  }
  bool editableOutdoorTemp = settings.sensors.outdoor.type == OUTDOOR_SENSOR_TYPE::OUTDOOR_SENSOR_MANUAL;
  bool editableIndoorTemp[8];
  for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
  {
    editableIndoorTemp[i] = settings.sensors.indoor[i].type == INDOOR_SENSOR_TYPE::INDOOR_SENSOR_MANUAL;
  }

  if (force || _dhwPresent != settings.opentherm.dhwPresent)
  {
    _dhwPresent = settings.opentherm.dhwPresent;

    if (_dhwPresent)
    {
      haHelper.publishSwitchDhw(false);
      haHelper.publishSensorCurrentDhwMinTemp(false);
      haHelper.publishSensorCurrentDhwMaxTemp(false);
      haHelper.publishNumberDhwMinTemp(false);
      haHelper.publishNumberDhwMaxTemp(false);
      haHelper.publishBinSensorDhw();
      haHelper.publishSensorDhwTemp();
      haHelper.publishSensorDhwFlowRate(false);
    }
    else
    {
      haHelper.deleteSwitchDhw();
      haHelper.deleteSensorCurrentDhwMinTemp();
      haHelper.deleteSensorCurrentDhwMaxTemp();
      haHelper.deleteNumberDhwMinTemp();
      haHelper.deleteNumberDhwMaxTemp();
      haHelper.deleteBinSensorDhw();
      haHelper.deleteSensorDhwTemp();
      haHelper.deleteNumberDhwTarget();
      haHelper.deleteClimateDhw();
      haHelper.deleteSensorDhwFlowRate();
    }

    published = true;
  }

  if (force || _heatingMinTemp != heatingMinTemp || _heatingMaxTemp != heatingMaxTemp)
  {
    for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
    {
      if (settings.heating.target[i] < heatingMinTemp || settings.heating.target[i] > heatingMaxTemp)
      {
        settings.heating.target[i] = constrain(settings.heating.target[i], heatingMinTemp, heatingMaxTemp);
      }
    }

    _heatingMinTemp = heatingMinTemp;
    _heatingMaxTemp = heatingMaxTemp;

    haHelper.publishNumberHeatingTarget(heatingMinTemp, heatingMaxTemp, false);
    haHelper.publishClimateHeating(heatingMinTemp, heatingMaxTemp);

    published = true;
  }

  for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
  {
    if (force || _editableIndoorTemp[i] != editableIndoorTemp[i])
    {
      _editableIndoorTemp[i] = editableIndoorTemp[i];

      if (editableIndoorTemp[i])
      {
        haHelper.deleteSensorIndoorTemp();
        haHelper.publishNumberIndoorTemp();
      }
      else
      {
        haHelper.deleteNumberIndoorTemp();
        haHelper.publishSensorIndoorTemp();
      }

      published = true;
    }
  }

  if (_dhwPresent && (force || _dhwMinTemp != settings.dhw.minTemp || _dhwMaxTemp != settings.dhw.maxTemp))
  {
    _dhwMinTemp = settings.dhw.minTemp;
    _dhwMaxTemp = settings.dhw.maxTemp;

    haHelper.publishNumberDhwTarget(settings.dhw.minTemp, settings.dhw.maxTemp, false);
    haHelper.publishClimateDhw(settings.dhw.minTemp, settings.dhw.maxTemp);

    published = true;
  }

  if (force || _editableOutdoorTemp != editableOutdoorTemp)
  {
    _editableOutdoorTemp = editableOutdoorTemp;

    if (editableOutdoorTemp)
    {
      haHelper.deleteSensorOutdoorTemp();
      haHelper.publishNumberOutdoorTemp();
    }
    else
    {
      haHelper.deleteNumberOutdoorTemp();
      haHelper.publishSensorOutdoorTemp();
    }

    published = true;
  }

  return published;
}

bool MqttTask::publishSettings(const char *topic)
{
  StaticJsonDocument<2048> doc;

  doc["debug"] = settings.debug;

  doc["emergency"]["enable"] = settings.emergency.enable;
  doc["emergency"]["target"] = settings.emergency.target;
  doc["emergency"]["useEquitherm"] = settings.emergency.useEquitherm;

  doc["heating"]["enable"] = settings.heating.enable;
  doc["heating"]["turbo"] = settings.heating.turbo;
  doc["heating"]["target"] = settings.heating.target;
  doc["heating"]["hysteresis"] = settings.heating.hysteresis;
  doc["heating"]["minTemp"] = settings.heating.minTemp;
  doc["heating"]["maxTemp"] = settings.heating.maxTemp;

  for (int i = 0; i < INDOOR_SENSOR_NUMBER; i++)
  {

    doc["pid"][std::to_string(i)]["enable"] = settings.pid[i].enable;
    doc["pid"][std::to_string(i)]["p_factor"] = settings.pid[i].p_factor;
    doc["pid"][std::to_string(i)]["i_factor"] = settings.pid[i].i_factor;
    doc["pid"][std::to_string(i)]["d_factor"] = settings.pid[i].d_factor;
    doc["pid"][std::to_string(i)]["minTemp"] = settings.pid[i].minTemp;
    doc["pid"][std::to_string(i)]["maxTemp"] = settings.pid[i].maxTemp;

    doc["equitherm"][std::to_string(i)]["enable"] = settings.equitherm[i].enable;
    doc["equitherm"][std::to_string(i)]["n_factor"] = settings.equitherm[i].n_factor;
    doc["equitherm"][std::to_string(i)]["k_factor"] = settings.equitherm[i].k_factor;
    doc["equitherm"][std::to_string(i)]["t_factor"] = settings.equitherm[i].t_factor;

    doc["sensors"]["indoor"][std::to_string(i)]["type"] = settings.sensors.indoor[i].type;
    doc["sensors"]["indoor"][std::to_string(i)]["offset"] = settings.sensors.indoor[i].offset;
  }
  doc["dhw"]["enable"] = settings.dhw.enable;
  doc["dhw"]["target"] = settings.dhw.target;
  doc["dhw"]["minTemp"] = settings.dhw.minTemp;
  doc["dhw"]["maxTemp"] = settings.dhw.maxTemp;

  doc["sensors"]["outdoor"]["type"] = settings.sensors.outdoor.type;
  doc["sensors"]["outdoor"]["offset"] = settings.sensors.outdoor.offset;

  client.beginPublish(topic, measureJson(doc), false);
  serializeJson(doc, client);
  return client.endPublish();
}

bool MqttTask::publishVariables(const char *topic)
{
  StaticJsonDocument<2048> doc;

  doc["tuning"]["enable"] = vars.tuning.enable;
  doc["tuning"]["regulator"] = vars.tuning.regulator;

  doc["states"]["otStatus"] = vars.states.otStatus;
  doc["states"]["heating"] = vars.states.heating;
  doc["states"]["dhw"] = vars.states.dhw;
  doc["states"]["flame"] = vars.states.flame;
  doc["states"]["fault"] = vars.states.fault;
  doc["states"]["diagnostic"] = vars.states.diagnostic;

  doc["sensors"]["modulation"] = vars.sensors.modulation;
  doc["sensors"]["pressure"] = vars.sensors.pressure;
  doc["sensors"]["dhwFlowRate"] = vars.sensors.dhwFlowRate;
  doc["sensors"]["faultCode"] = vars.sensors.faultCode;
  doc["sensors"]["rssi"] = vars.sensors.rssi;
  doc["sensors"]["uptime"] = (unsigned long)(millis() / 1000);

  doc["temperatures"]["indoor"] = vars.temperatures.indoor;
  doc["temperatures"]["outdoor"] = vars.temperatures.outdoor;
  doc["temperatures"]["heating"] = vars.temperatures.heating;
  doc["temperatures"]["dhw"] = vars.temperatures.dhw;

  doc["parameters"]["heatingEnabled"] = vars.parameters.heatingEnabled;
  doc["parameters"]["heatingMinTemp"] = vars.parameters.heatingMinTemp;
  doc["parameters"]["heatingMaxTemp"] = vars.parameters.heatingMaxTemp;
  doc["parameters"]["heatingSetpoint"] = vars.parameters.heatingSetpoint;
  doc["parameters"]["dhwMinTemp"] = vars.parameters.dhwMinTemp;
  doc["parameters"]["dhwMaxTemp"] = vars.parameters.dhwMaxTemp;

  client.beginPublish(topic, measureJson(doc), false);
  serializeJson(doc, client);
  return client.endPublish();
}

std::string MqttTask::getTopicPath(const char *topic)
{
  return std::string(settings.mqtt.prefix) + "/" + std::string(topic);
}

void MqttTask::__callback(char *topic, byte *payload, unsigned int length)
{
  if (!length)
  {
    return;
  }

  if (settings.debug)
  {
    Log.strace("MQTT.MSG", F("Topic: %s\r\n>  "), topic);
    for (unsigned int i = 0; i < length; i++)
    {
      if (payload[i] == 10)
      {
        Log.print("\r\n>  ");
      }
      else
      {
        Log.print((char)payload[i]);
      }
    }
    Log.print("\r\n\n");

    for (Stream *stream : Log.getStreams())
    {
      stream->flush();
    }
  }

  StaticJsonDocument<2048> doc;
  DeserializationError dErr = deserializeJson(doc, (const byte *)payload, length);
  if (dErr != DeserializationError::Ok || doc.isNull())
  {
    const char *errMsg;
    switch (dErr.code())
    {
    case DeserializationError::EmptyInput:
    case DeserializationError::IncompleteInput:
    case DeserializationError::InvalidInput:
      errMsg = "invalid input";
      break;

    case DeserializationError::NoMemory:
      errMsg = "no memory";
      break;

    case DeserializationError::TooDeep:
      errMsg = "too deep";
      break;

    default:
      errMsg = "failed";
      break;
    }
    Log.swarningln("MQTT.MSG", F("No deserialization: %s"), errMsg);

    return;
  }

  if (getTopicPath("state/set").compare(topic) == 0)
  {
    updateVariables(doc);
    client.publish(getTopicPath("state/set").c_str(), NULL, true);
  }
  else if (getTopicPath("settings/set").compare(topic) == 0)
  {
    updateSettings(doc);
    client.publish(getTopicPath("settings/set").c_str(), NULL, true);
  }
}
