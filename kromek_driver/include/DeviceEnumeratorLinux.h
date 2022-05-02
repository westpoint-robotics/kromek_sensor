#pragma once

#include "kromek.h"
#include "IDataInterface.h"
#include "Thread.h"
#include <vector>
#include "Event.h"

namespace kmk
{

typedef void (*DevicesChangedCallbackFunc)(void *pArg);

// Object for enumerating devices on linux. Should raise the _deviceChangedCallbackFunc when a change is detected. 
// The callback should then enumerate the supported devices by calling EnumerateDevices() for each device type
class DeviceEnumerator
{
private:
    DevicesChangedCallbackFunc _devicesChangedCallbackFunc;
	void *_devicesChangedCallbackArg;
    kmk::Thread _deviceChangeMonitorThread;
    kmk::Event _finishThreadEvent;
    
    void EnumerateUSBHIDDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut);
//	void EnumerateUSBSerialDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut);
    
    // Thread func for monitoring device changes
    static int DeviceChangeMonitorThreadProc(void *pArg);

public:
    DeviceEnumerator();
	~DeviceEnumerator();

	bool Initialize(DevicesChangedCallbackFunc callbackFunc, void *pCallbackArg);
	void Shutdown();

	void EnumerateDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut);
};

}
