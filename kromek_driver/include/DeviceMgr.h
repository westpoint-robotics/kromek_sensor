#pragma once

#include "kromek.h"
#include <map>
#include <vector>
#include "IDataInterface.h"
#include "IDevice.h"
#include "CriticalSection.h"

#ifdef _WINDOWS
#include "DeviceEnumeratorWindows.h"
#else
#include "DeviceEnumeratorLinux.h"
#endif 

namespace kmk
{
typedef void (*DeviceChangedCallbackFunc)(IDevice *pDevice, bool added, void *pArg);
typedef std::vector<ValidDeviceIdentifier> ValidDeviceIdentifierVector;
typedef std::map<unsigned int, IDevice*> DeviceMap;

// Manage all devices attached to the machine
class DeviceMgr
{
private:
	DeviceMap _deviceList;
	ValidDeviceIdentifierVector _supportedDeviceList;
	DeviceEnumerator _deviceEnumerator;
	DeviceChangedCallbackFunc _deviceChangedCallbackFunc;
	void *_deviceChangedCallbackArg;

	CriticalSection _deviceListCS;

	void UpdateAttachedDevices();
	static void DevicesChangedCallbackFuncProc(void *pArg);

	std::vector<IDevice*> CreateDevices(IDataInterface *pInterface);
	std::vector<IDevice*> AddInterface(IDataInterface *pDevice);
	void RemoveDevice(IDevice *pDevice);

public:
	DeviceMgr(void);
	~DeviceMgr(void);
	bool Initialize(ValidDeviceIdentifierVector &supportedDevices);
	bool ShutDown();

	// Callbacks
	void SetDeviceChangedCallback(DeviceChangedCallbackFunc func, void *pArg);

	IDevice *GetNextDevice(IDevice *pPrevious);
	bool GetDetectorProperties(VID vendorID, PID productID, DetectorProperties &propsOut);
};

}
