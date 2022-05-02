#pragma once

#include "kromek.h"
#include "IDataInterface.h"
#include "Thread.h"
#include "Event.h"
#include <vector>

// Uncomment to enable the use of emulated D3S devices
//#define ENABLE_EMULATED_DEVICES

namespace kmk
{

typedef void (*DevicesChangedCallbackFunc)(void *pArg);

// Object for enumerating devices on windows. Should raise the _deviceChangedCallbackFunc when a change is detected. 
// The callback should then enumerate the supported devices by calling EnumerateDevices() for each device type
class DeviceEnumerator
{
private:
	Thread _processingThread;
	Event _initialiseCompleteEvent;
	bool _initialiseCompleteStatus;
	HWND _wndHandle;
	DevicesChangedCallbackFunc _devicesChangedCallbackFunc;
	void *_devicesChangedCallbackArg;


	// Window callback function for detecting device changes
	static LRESULT CALLBACK DummyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	
	// Thread function for maintaining device change dummy window
    static int ThreadProc(void *pArg);

	// Register for a windows message when usb devices are plugged in / unplugged
	void RegisterForDeviceNotifications();
	void EnumerateUSBHIDDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut);
	void EnumerateUSBSerialDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut);
	void EnumerateBTSerialDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut);
	void EnumerateSimulatedDevices(std::vector<IDataInterface*> &listOut);

public:
	DeviceEnumerator();
	~DeviceEnumerator();

	bool Initialize(DevicesChangedCallbackFunc callbackFunc, void *pCallbackArg);
	void Shutdown();

	void EnumerateDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut);
};

}
