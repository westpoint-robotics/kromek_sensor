#pragma once

#include "types.h"
#include "IDevice.h"
#include "IDataInterface.h"
#include "IDataProcessor.h"
#include "CriticalSection.h"

namespace kmk
{

#define DEFAULT_CHANNEL_COUNT 4096

// Base class for all devices. Contains basic code suitable for most detectors
class DeviceBase : public IDevice
{
protected:
	// Unique hash of the device (normally based on the interface it is connected to)
	unsigned int _hash;

	// Data interface associated with this device
	IDataInterface *_pInterface;

	// Data processor associated with this device
	IDataProcessor *_pDataProcessor;
	CriticalSection _eventCS;
	CriticalSection _dataCS;
	bool _isAcquiring;
	
	// Id of the component this detector represents (for multicomponent devices)
	uint8_t _componentId;

	// Cache the serial number as it can be slow to query and should not change
	bool _deviceSerialCached;
	String _deviceSerial;

	// Cache version number of device
	bool _deviceVersionCached;
	unsigned short _deviceVersion;

	float _accumulatedDose;
	
	// Function raised when acquisition is stopped
	void OnAcquisitionStopped();

private:

	// Callback passed on from the data processor
	CountEventDeviceCallbackFunc _countEventCallback;
	void *_countEventCallbackArg;

	// Callback passed on from the data processor
	DoseEventDeviceCallbackFunc _doseEventCallback;
	void *_doseEventCallbackArg;

	// Callback raised when acquisition finishes
	FinishedAcquisitionCallbackFunc _finishedAcquisitionCallbackFunc;
	void *_finishedAcquisitionCallbackArg;

	// Callback raised whenever an error occurs on the device
	DeviceErrorCallbackFunc _errorCallbackFunc;
	void *_errorCallbackArg;

	// Callback routine raised for every count received
	static void CountEventCallbackProc(void *pThis, int64_t timestamp, int channel, uint32_t numCounts);

	static void DoseEventCallbackProc(void * pArg, int64_t timestamp, float dose, float doseRate, float accumulatedDose);
	
	// Callback routine raised when acqusition completes
	static void FinishedProcessingCallbackProc(void *pArg, bool forced);

	// Callback routine for errors that occur
	static void ErrorCallbackProc(void *pArg, int errorCode, String message);

	// Set the LLD on the device.
	bool SetLLD(uint16_t val);

public:


	DeviceBase(IDataInterface *pInterface, IDataProcessor *pDataProcessor, uint8_t componentId = 0);
	virtual ~DeviceBase(void);
	IDataInterface *GetInterface() {return _pInterface;}

	virtual unsigned int GetHash() const;
	
	// Return the serial number associated with the device
	virtual String GetSerialNumber();

	// Return the firmware version of the device
	virtual unsigned short GetVersion();
	
	// Return a value from the interface
	virtual String GetInterfaceProperty(const String& param);

	// Set callbacks raised when certain events occur
	void SetCountEventCallback(CountEventDeviceCallbackFunc func, void *pArg);
	void SetDoseEventCallback(DoseEventDeviceCallbackFunc func, void *pArg);
	void SetFinishedAcquisitionCallback(FinishedAcquisitionCallbackFunc func, void *pArg);
	void SetErrorCallback(DeviceErrorCallbackFunc func, void *pArg);

	// Start acquiring data on the device
	virtual bool Start();

	// Stop acquiring data. If finishProcessing = true then the data processor thread will finish processing all data left in the queue. Otherwise the process will end
	// immediatly
	virtual bool Stop(bool force);

	// Return the real time of the current / last acquisition on this device
	virtual int64_t GetRealTime() const;

    virtual void ResetRealTime();

	virtual int64_t GetStartTime() const;
	virtual void SetStartTime(int64_t value);

	virtual float GetTemperature() const;
	
	// Get and set configuration values on the hardware
	virtual bool SetConfigurationSettingUInt8(ConfigurationID command, uint8_t val);
	virtual bool SetConfigurationSettingUInt16(ConfigurationID command, uint16_t val);
	virtual bool GetConfigurationSettingUInt8(ConfigurationID command, uint8_t &valOut);
	virtual bool GetConfigurationSettingUInt16(ConfigurationID command, uint16_t &valOut);
	virtual bool SetConfigurationData(ConfigurationID command, BYTE* buffer, int len);
	virtual bool GetConfigurationData(ConfigurationID command, BYTE* buffer, int len);
};

}
