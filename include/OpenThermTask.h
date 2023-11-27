#ifndef OPENTHERMTASK_H
#define OPENTHERMTASK_H

#include <new>
#include <common.h>
#include <EEManager.h>
#include <OpenTherm.h>
#include <CustomOpenTherm.h>

class OpenThermTask : public Task
{
public:
    OpenThermTask(bool _enabled = false, unsigned long _interval = 0) : Task(_enabled, _interval) {}

    static void IRAM_ATTR handleInterrupt();

protected:
    unsigned short readyTime = 60000;
    unsigned short dhwSetTempInterval = 60000;
    unsigned short heatingSetTempInterval = 60000;

    bool pump = true;
    unsigned long prevUpdateNonEssentialVars = 0;
    unsigned long startupTime = millis();
    unsigned long dhwSetTempTime = 0;
    unsigned long heatingSetTempTime = 0;

    const char *getTaskName();
    int getTaskCore();

    void setup();
    void loop();

    void static sendRequestCallback(unsigned long request, unsigned long response, OpenThermResponseStatus status, byte attempt);
    void static responseCallback(unsigned long result, OpenThermResponseStatus status);

    void begin(void (*handleInterruptCallback)(void));
    bool isReady();
    bool needSetDhwTemp();
    bool needSetHeatingTemp();

    void static printRequestDetail(OpenThermMessageID id, OpenThermResponseStatus status, unsigned long request, unsigned long response, byte attempt);

    bool updateSlaveConfig();
    bool setMasterConfig(uint8_t id, uint8_t flags, bool force = false);
    bool setMaxModulationLevel(byte value);
    bool updateSlaveOtVersion();
    bool setMasterOtVersion(float version);
    bool updateSlaveVersion();
    bool setMasterVersion(uint8_t version, uint8_t type);
    bool updateMinMaxDhwTemp();
    bool updateMinMaxHeatingTemp();
    bool setMaxHeatingTemp(byte value);
    bool updateOutsideTemp();
    bool updateHeatingTemp();
    bool updateDhwTemp();
    bool updateDhwFlowRate();
    bool updateFaultCode();
    bool updateModulationLevel();
    bool updatePressure();
};

#endif