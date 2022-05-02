#include "stdafx.h"
#include "DeviceBase.h"
#include "Lock.h"
#include <vector>
#include "kmkTime.h"

#include "SIGMA_50.h"
#include "SIGMA_25.h"
#include "TN15.h"
#include <stdlib.h>

#define HOST_ORDER kmk::Endian::LittleEndian

namespace kmk
{

DeviceBase::DeviceBase(IDataInterface *pInterface, IDataProcessor *pDataProcessor, uint8_t componentId /*= 0*/)
: _pInterface(pInterface)
, _pDataProcessor(pDataProcessor)
, _isAcquiring(false)
, _componentId(componentId)
, _deviceSerialCached(false)
, _deviceVersionCached(false)
, _deviceVersion(0)
, _countEventCallback(NULL)
, _countEventCallbackArg(NULL)
, _doseEventCallback(NULL)
, _doseEventCallbackArg(NULL)
, _finishedAcquisitionCallbackFunc(NULL)
, _errorCallbackFunc(NULL)
, _errorCallbackArg(NULL)
, _finishedAcquisitionCallbackArg(NULL)
, _accumulatedDose(0)
{
	// Initialise the data interface callbacks
	if (_pInterface != NULL)
	{
		_hash = pInterface->GetHash() + (_componentId * 1000);
	}

	// Initialise the data processing callbacks
	if (_pDataProcessor != NULL)
	{
		_pDataProcessor->AddComponent(_componentId, this, 
			CountEventCallbackProc, this, 
			DoseEventCallbackProc, this,
			FinishedProcessingCallbackProc, this,
			ErrorCallbackProc, this);
	}
}

DeviceBase::~DeviceBase()
{
	if (_pDataProcessor != NULL)
	{
		_pDataProcessor->RemoveComponent(_componentId, this);
	}
}

unsigned int DeviceBase::GetHash() const
{
	return _hash;
}

String DeviceBase::GetSerialNumber()
{
	if (!_deviceSerialCached)
	{
		// 25 unicode chars
		size_t bufferSize = DEVICE_SERIAL_LENGTH;
		BYTE buffer[DEVICE_SERIAL_LENGTH];
		if (_pDataProcessor->GetConfigurationData(_componentId, CONFIGURATION_GETSERIAL, buffer, bufferSize))
		{
			// Convert mbs to wcs
			wchar_t str[DEVICE_SERIAL_LENGTH / sizeof(wchar_t)];
#if _WINDOWS
			size_t numCharsToConvert = DEVICE_SERIAL_LENGTH / sizeof(wchar_t);
			mbstowcs_s(&numCharsToConvert, str, (const char*)buffer, (bufferSize / sizeof(wchar_t))-1);
#else
            mbstowcs(str, (const char*)buffer, bufferSize / sizeof(wchar_t));
#endif
			_deviceSerial = str;
			_deviceSerialCached = true;
		}
	}
	return _deviceSerial;
}

unsigned short DeviceBase::GetVersion()
{
	if (!_deviceVersionCached)
	{
		// 25 unicode chars
		size_t bufferSize = sizeof(unsigned short);
		unsigned short buffer = 0;
		if (_pDataProcessor->GetConfigurationData(_componentId, CONFIGURATION_GETVERSION, (BYTE*)&buffer, bufferSize))
		{
			_deviceVersion = buffer;
			_deviceVersionCached = true;
		}
	}
	return _deviceVersion;
}

String DeviceBase::GetInterfaceProperty(const String& param)
{
	return _pInterface ? _pInterface->GetInterfaceProperty(param) : L"";
}

void DeviceBase::SetCountEventCallback(CountEventDeviceCallbackFunc func, void *pArg)
{
	Lock lock (_eventCS);
	_countEventCallback = func;
	_countEventCallbackArg = pArg;
}


void DeviceBase::SetDoseEventCallback(DoseEventDeviceCallbackFunc func, void *pArg)
{
	Lock lock(_eventCS);
	_doseEventCallback = func;
	_doseEventCallbackArg = pArg;
}

void DeviceBase::SetFinishedAcquisitionCallback(FinishedAcquisitionCallbackFunc func, void *pArg)
{
	Lock lock (_eventCS);
	_finishedAcquisitionCallbackFunc = func;
	_finishedAcquisitionCallbackArg = pArg;
}

void DeviceBase::SetErrorCallback(DeviceErrorCallbackFunc func, void *pArg)
{
	Lock lock (_eventCS);
	_errorCallbackFunc = func;
	_errorCallbackArg = pArg;
}


// Start acquisition from the interface and processing of the acquired data
bool DeviceBase::Start()
{
	{
		Lock lock(_dataCS);
		_accumulatedDose = 0;
	}

	if (_pDataProcessor != NULL)
	{
		if (!_pDataProcessor->StartProcessing(_componentId))
			return false;
	}

	{
		Lock lock(_dataCS);
		_isAcquiring = true;
	}
	return true;
}

// Stop acquisition. force determines whether any data in the processing queue is to be completed or not (true = do not complete queue)
bool DeviceBase::Stop(bool force)
{
	{
		Lock lock(_dataCS);

		if (!_isAcquiring)
			return false;
	
		OnAcquisitionStopped();
	}

	if (_pDataProcessor != NULL)
	{
		_pDataProcessor->StopProcessing(_componentId, force);
	}
	return true;
}

// Return the real time of the currently active or previously active acquisition.
int64_t DeviceBase::GetRealTime() const
{
	return _pDataProcessor->GetRealTime(_componentId);
}

// Reset the real time
void DeviceBase::ResetRealTime()
{
    _pDataProcessor->ResetRealTime(_componentId);
}

int64_t DeviceBase::GetStartTime() const
{
	return _pDataProcessor->GetStartTime(_componentId);
}

void DeviceBase::SetStartTime(int64_t value)
{
	_pDataProcessor->SetStartTime(_componentId, value);
}

// Return the temperature last reported from the device.
float DeviceBase::GetTemperature() const
{
	return _pDataProcessor->GetComponentProperty(_componentId, CP_Temperature);
}

// Callback raised from the data processor everytime counts are processed for a particular channel
void DeviceBase::CountEventCallbackProc(void *pArg, int64_t timestamp, int channel, uint32_t numCounts)
{
	// Pass on to the registered callback
	DeviceBase *pThis = (DeviceBase*)pArg;
	Lock lock (pThis->_eventCS);

	if (pThis->_countEventCallback != NULL)
	{
		(*pThis->_countEventCallback)(pThis, timestamp, channel, numCounts, pThis->_countEventCallbackArg);
	}
}

void DeviceBase::DoseEventCallbackProc(void *pArg, int64_t timestamp, float dose, float doseRate, float accumulatedDose)
{
	// Pass on to the registered callback
	DeviceBase *pThis = (DeviceBase*)pArg;
	Lock lock(pThis->_eventCS);

	pThis->_accumulatedDose = accumulatedDose;

	if (pThis->_doseEventCallback != NULL)
	{
		(*pThis->_doseEventCallback)(pThis, timestamp, dose, doseRate, accumulatedDose, pThis->_countEventCallbackArg);
	}
}

void DeviceBase::OnAcquisitionStopped()
{
	_isAcquiring = false;
}

// Callback raised when processing of any outstanding data has completed and acquisition is stopped
void DeviceBase::FinishedProcessingCallbackProc(void *pArg, bool forced)
{
	DeviceBase *pThis = (DeviceBase*)pArg;

	// In a multi-component device its possible for one component to call stop before this one does, make sure this device is stopped also
	{
		Lock lock(pThis->_dataCS);
		if (pThis->_isAcquiring)
		{
			pThis->OnAcquisitionStopped();
		}
	}

	{
		Lock lock(pThis->_eventCS);

		// Forward on
		if (pThis->_finishedAcquisitionCallbackFunc != NULL)
		{
			(*pThis->_finishedAcquisitionCallbackFunc)(pThis, forced, pThis->_finishedAcquisitionCallbackArg);
		}
	}
}

// Callback when an error occurs
void DeviceBase::ErrorCallbackProc(void *pArg, int errorCode, String errorMessage)
{
	DeviceBase *pDevice = (DeviceBase*)pArg;

	if (errorCode == ERROR_READ_FAILED)
	{
		// Critical. Stop all acquisition immediatly!
		pDevice->Stop(true);
	}

	// Pass it on
	if (pDevice->_errorCallbackFunc != NULL)
	{
		(*pDevice->_errorCallbackFunc)(pDevice, errorCode, errorMessage, pDevice->_errorCallbackArg);
	}
}

bool DeviceBase::SetConfigurationSettingUInt8(ConfigurationID command, uint8_t val)
{
	// Send the command
	return _pDataProcessor->SetConfigurationData(_componentId, command, &val, sizeof(uint8_t));
}

// LLD Is a special form of SetConfigurationSettingUInt16. Older devices do not support the LLDChannel report, in
// these cases we have to send the old LLD report that bitshifted the value
bool DeviceBase::SetLLD(uint16_t val)
{
	// Attempt to send LLD Channel
	unsigned char valBytes[2];
	kmk::Endian::SwapBytes16(HOST_ORDER, _pDataProcessor->GetEndian(), val, valBytes);

	if (_pDataProcessor->SetConfigurationData(_componentId, CONFIGURATION_SETLLD_CHANNEL, valBytes, sizeof(uint16_t)))
	{
		return true;
	}
	
	// Failed to send, assume new report is not supported so send the old report type
	uint16_t detectorLLD = val;

	if (_pInterface->GetProductID() == (int)kmk::SIGMA_25::ProductId ||
		_pInterface->GetProductID() == (int)kmk::SIGMA_50::ProductId ||
		_pInterface->GetProductID() == (int)kmk::TN15::ProductId)
	{
		detectorLLD = (uint16_t)((int)detectorLLD << 3);
	}
	else
	{
		detectorLLD = (uint16_t)((int)detectorLLD << 4);
	}

	kmk::Endian::SwapBytes16(HOST_ORDER, _pDataProcessor->GetEndian(), detectorLLD, valBytes);
	return _pDataProcessor->SetConfigurationData(_componentId, CONFIGURATION_SETLLD, valBytes, sizeof(uint16_t));
}

bool DeviceBase::SetConfigurationSettingUInt16(ConfigurationID command, uint16_t val)
{
	// LLD is a special case
	if (command == CONFIGURATION_SETLLD_CHANNEL)
	{
		return SetLLD(val);
	}

	unsigned char valBytes[2];
	kmk::Endian::SwapBytes16(HOST_ORDER, _pDataProcessor->GetEndian(), val, valBytes);

	return _pDataProcessor->SetConfigurationData(_componentId, command, valBytes, sizeof(uint16_t));
}

bool DeviceBase::SetConfigurationData(ConfigurationID command, BYTE* buffer, int len)
{
	if (command == CONFIGURATION_SETSERIALNO)
	{
		_deviceSerialCached = false;
	}

	return _pDataProcessor->SetConfigurationData(_componentId, command, buffer, len);
}

bool DeviceBase::GetConfigurationData(ConfigurationID command, BYTE* buffer, int len)
{
	size_t length;
	return _pDataProcessor->GetConfigurationData(_componentId, command, buffer, length = len);
}

bool DeviceBase::GetConfigurationSettingUInt8(ConfigurationID command, uint8_t &valOut)
{
	// Send the command
	size_t bufferSize = sizeof(uint8_t);
	if (!_pDataProcessor->GetConfigurationData(_componentId, command, (BYTE*)&valOut, bufferSize))
	{
		return false;
	}
	
	return true;
}

bool DeviceBase::GetConfigurationSettingUInt16(ConfigurationID command, uint16_t &valOut)
{
	// Send the command
	size_t bufferSize = sizeof(uint16_t);
	BYTE valBytes[2];
	if (!_pDataProcessor->GetConfigurationData(_componentId, command, valBytes, bufferSize))
	{
		return false;
	}
	
	valOut = kmk::Endian::SwapUInt16(_pDataProcessor->GetEndian(), HOST_ORDER, valBytes);
	return true;
}

}
