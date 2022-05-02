
#include "stdafx.h"
#include "DriverMgr.h"
#include "Lock.h"
#include <assert.h>

#define PRODUCT_ID_RADANGEL		0x100

// Singleton
DriverMgr *DriverMgr::m_pInstance = NULL;
DriverMgr *DriverMgr::GetInstance()
{
	if (m_pInstance == NULL)
		m_pInstance = new DriverMgr();

	return m_pInstance;
}

void DriverMgr::DeleteInstance()
{
	if (m_pInstance)
		delete m_pInstance;

	m_pInstance = NULL;
}

DriverMgr::DriverMgr()
: m_initialised(false)
, m_keepUpdateThreadRunning(false)
, m_pErrorCallbackFunc(NULL)
, m_pErrorCallbackUserData(NULL)
, m_pDeviceChangedCallbackFunc(NULL)
, m_pDeviceChangedCallbackUserData(NULL)
, m_pDataReceivedCallbackFunc(NULL)
, m_pDataReceivedCallbackUserData(NULL)
{
	m_deviceMgr.SetDeviceChangedCallback(OnDeviceChangedProc, this);
}

// Private: Should never be called!
DriverMgr::DriverMgr(const DriverMgr &)
{
}

DriverMgr::~DriverMgr()
{
	Destruct();
}

bool DriverMgr::IsInitialised()
{
    kmk::Lock lock(m_propSection);
    return m_initialised;
}

// Initialise the mgr including starting an update thread that will update devices regularly
int DriverMgr::Initialise(kmk::ValidDeviceIdentifierVector& validDevices)
{
    kmk::Lock lock(m_propSection);
    if (m_initialised)
        return ERROR_OK;

	m_deviceMgr.Initialize(validDevices);
	m_keepUpdateThreadRunning = true;

	if (!m_updateThread.Start(UpdateThreadProc, this))
	{
        return ERROR_UNKNOWN;
	}

    m_initialised = true;
    return ERROR_OK;
}

// Must be called on exit
void DriverMgr::Destruct()
{
    if (!IsInitialised())
        return;

	// Delete devices
    {
        kmk::Lock lock(m_deviceSection);
        HIDSpectrometerDeviceVector::iterator it;
        HIDSpectrometerDeviceVector::iterator itEnd = m_attachedDevices.end();
        for (it = m_attachedDevices.begin(); it != itEnd; ++it)
        {
            delete it->second;
        }
        m_attachedDevices.clear();
    }
	// Stop the update thread and wait for it to exit
	{
		kmk::Lock lock(m_updateThreadSection);
		m_keepUpdateThreadRunning = false;
	}
    m_updateThread.WaitForTermination();

	m_deviceMgr.ShutDown();
}

// Event callback raised everytime a device is connected / disconnected. Called from a seperate thread.
void DriverMgr::OnDeviceChangedProc(kmk::IDevice *pDevice, bool added, void *pArg)
{
	DriverMgr *pThis = (DriverMgr*)pArg;
	kmk::Lock lock(pThis->m_deviceSection);

	if (added)
	{
		kmk::DetectorProperties props;
		if (pThis->m_deviceMgr.GetDetectorProperties(pDevice->GetVendorID(), pDevice->GetProductID(), props))
		{
			// Create the new device
			Detector *pDetector = new Detector(pDevice, USBDetectorDataChangedCallbackProc, pThis, props);

			pThis->m_attachedDevices[pDetector->Hash()] = pDetector;
			pDevice->SetFinishedAcquisitionCallback(DeviceFinishedAcquisitionCallbackProc, pThis);
			pDevice->SetErrorCallback(DeviceErrorCallbackProc, pThis);

			// Raise callback
			if (pThis->m_pDeviceChangedCallbackFunc != NULL)
			{
				(*pThis->m_pDeviceChangedCallbackFunc)(pDevice->GetHash(), TRUE, pThis->m_pDeviceChangedCallbackUserData);
			}
		}
	}
	else
	{
		// Remove the device
		HIDSpectrometerDeviceVector::iterator it = pThis->m_attachedDevices.find(pDevice->GetHash());
		if (it != pThis->m_attachedDevices.end())
		{
			// Raise callback
			if (pThis->m_pDeviceChangedCallbackFunc != NULL)
			{
                (*pThis->m_pDeviceChangedCallbackFunc)(pDevice->GetHash(), FALSE, pThis->m_pDeviceChangedCallbackUserData);
			}

			delete it->second;
			pThis->m_attachedDevices.erase(it);
		}
	}
}

// Event callback from each detector raised whenever data is received from the device. Called from a seperate thread for each detector 
// (as part of the data processor thread)
void DriverMgr::USBDetectorDataChangedCallbackProc(Detector *pDetector, int64_t timestamp, int channel, uint32_t counts, void *pArg)
{
	DriverMgr *pThis = (DriverMgr*)pArg;

    // Avoid calling event within the locked mutex
    DataReceivedCallback pCallbackFunc = NULL;
    void *pUserData = NULL;

    {
        kmk::Lock lock(pThis->m_propSection);
        if (pThis->m_pDataReceivedCallbackFunc != NULL)
        {
            pCallbackFunc = pThis->m_pDataReceivedCallbackFunc;
            pUserData = pThis->m_pDataReceivedCallbackUserData;
        }
    }

    if (pCallbackFunc)
	{
        (*pCallbackFunc)(pUserData, pDetector->Hash(), timestamp, channel, counts);
	}
}

// Callback raised whenever data acquisition (including final data processing) has completed.
void DriverMgr::DeviceFinishedAcquisitionCallbackProc(kmk::IDevice *pDevice, bool /*forced*/, void *pArg)
{
	DriverMgr *pThis = (DriverMgr*)pArg;

    // Avoid calling event within the locked mutex
    ErrorCallback pCallbackFunc = NULL;
    void *pUserData = NULL;

    {
        kmk::Lock lock(pThis->m_propSection);
        if (pThis->m_pErrorCallbackFunc != NULL)
        {
            pCallbackFunc = pThis->m_pErrorCallbackFunc;
            pUserData = pThis->m_pErrorCallbackUserData;
        }
    }

    if (pCallbackFunc)
    {
        (*pCallbackFunc)(pUserData, pDevice->GetHash(), ERROR_ACQUISITION_COMPLETE, NULL);
	}
}

void DriverMgr::DeviceErrorCallbackProc(kmk::IDevice *pDevice, int errorCode, const String &message, void *pArg)
{
    // Avoid calling event within the locked mutex
    DriverMgr *pThis = (DriverMgr*)pArg;
    ErrorCallback pCallbackFunc = NULL;
    void *pUserData = NULL;

    {
        kmk::Lock lock(pThis->m_propSection);
        if (pThis->m_pErrorCallbackFunc != NULL)
        {
            pCallbackFunc = pThis->m_pErrorCallbackFunc;
            pUserData = pThis->m_pErrorCallbackUserData;
        }
    }

    if (pCallbackFunc)
    {
        size_t bufferSize = message.length()+1;
        char *pTemp = new char[bufferSize];        
#ifdef _WINDOWS
        size_t sizeOut;
        wcstombs_s(&sizeOut, pTemp, bufferSize, message.c_str(), message.length());
#else
        wcstombs(pTemp, message.c_str(), message.size());
        pTemp[message.size()] = 0;
#endif

        (*pCallbackFunc)(pUserData, pDevice->GetHash(), errorCode, pTemp);
        delete[] pTemp;
    }
}

// Return the ID of the next detector in the list or 0 if no more are available
unsigned int DriverMgr::GetNextDevice(unsigned int deviceID)
{
	kmk::Lock lock(m_deviceSection);
	if (m_attachedDevices.size() == 0)
		return 0;

	if (deviceID == 0)
		return m_attachedDevices.begin()->first;

	HIDSpectrometerDeviceVector::const_iterator itFound = m_attachedDevices.find(deviceID);
	if (itFound == m_attachedDevices.end())
		return 0; // Not found existing device!

	if (++itFound == m_attachedDevices.end())
		return 0;
	else
		return itFound->first;
}

// Retrieve acquired data for the specified detector
int DriverMgr::GetAcquiredData(unsigned int deviceID, unsigned int *pBuffer, unsigned int *pTotalCounts, unsigned int *pRealTime, unsigned int *pLiveTime, int flags)
{
    if (!IsInitialised())
        return ERROR_NOT_INITIALISED;

	kmk::Lock lock(m_deviceSection);
	HIDSpectrometerDeviceVector::iterator itDevice = m_attachedDevices.find(deviceID);
	if (itDevice != m_attachedDevices.end())
	{
		if (itDevice->second->GetAcquiredData(pBuffer, pTotalCounts, pRealTime, pLiveTime, flags))
            return ERROR_OK;
		else
            return ERROR_UNKNOWN;
	}
	else
	{
        return ERROR_INVALID_DEVICE_ID;
	}
}

int DriverMgr::ClearAcquiredData(unsigned int deviceID)
{
    if (!IsInitialised())
        return ERROR_NOT_INITIALISED;

    kmk::Lock lock(m_deviceSection);
    HIDSpectrometerDeviceVector::iterator itDevice = m_attachedDevices.find(deviceID);
    if (itDevice != m_attachedDevices.end())
    {

        itDevice->second->ClearAcquiredData();
        return ERROR_OK;
    }
    else
    {
        return ERROR_INVALID_DEVICE_ID;
    }
}

int DriverMgr::IsAcquiringData(unsigned int deviceID)
{
    if (!IsInitialised())
        return ERROR_NOT_INITIALISED;

	kmk::Lock lock(m_deviceSection);
	HIDSpectrometerDeviceVector::iterator itDevice = m_attachedDevices.find(deviceID);
	if (itDevice != m_attachedDevices.end())
	{
        return itDevice->second->IsAcquiringData() ? 1: 0;
	}
	else
	{
        return 0;
	}
}

int DriverMgr::BeginDataAcquisition(unsigned int deviceID, unsigned int realTime, unsigned int liveTime)
{
    if (!IsInitialised())
        return ERROR_NOT_INITIALISED;

    kmk::Lock lock(m_deviceSection);
	HIDSpectrometerDeviceVector::iterator itDevice = m_attachedDevices.find(deviceID);
	if (itDevice != m_attachedDevices.end())
	{
        return itDevice->second->BeginDataAcquisition(realTime, liveTime) ? ERROR_OK : ERROR_UNKNOWN;
	}
	else
	{
        return ERROR_INVALID_DEVICE_ID;
	}
}

int DriverMgr::EndDataAcquisition(unsigned int deviceID)
{
    if (!IsInitialised())
        return ERROR_NOT_INITIALISED;

    kmk::Lock lock(m_deviceSection);
	HIDSpectrometerDeviceVector::iterator itDevice = m_attachedDevices.find(deviceID);
	if (itDevice != m_attachedDevices.end())
	{
		itDevice->second->EndDataAcquisition();
        return ERROR_OK;
	}
	else
	{
        return ERROR_INVALID_DEVICE_ID;
	}
}

// Raise the error callback routine
void DriverMgr::RaiseError(unsigned int deviceID, int errorCode)
{
    // Prevent raising the event within the mutex lock
    ErrorCallback pFunc = NULL;
    void *pData = NULL;

    {
        kmk::Lock lock(m_propSection);
        if (m_pErrorCallbackFunc)
        {
            pFunc = m_pErrorCallbackFunc;
            pData = m_pErrorCallbackUserData;
        }
    }

    if (pFunc)
    {
        (*pFunc)(pData, deviceID, errorCode, NULL);
	}
}

// Set a callback function for when an error occurs
void DriverMgr::SetErrorCallback(ErrorCallback pFunc, void *pUserData)
{
    kmk::Lock lock(m_propSection);

	m_pErrorCallbackFunc = pFunc;
	m_pErrorCallbackUserData = pUserData;
}

// Set a callback function for when the devices change
void DriverMgr::SetDeviceChangedCallback(DeviceChangedCallback pFunc, void *pUserData)
{
    kmk::Lock lock(m_propSection);

	m_pDeviceChangedCallbackFunc = pFunc;
	m_pDeviceChangedCallbackUserData = pUserData;
}

// Set a callback function for when new data arrives on any device
void DriverMgr::SetDataReceivedCallback(DataReceivedCallback pFunc, void *pUserData)
{
    kmk::Lock lock(m_propSection);
	m_pDataReceivedCallbackFunc = pFunc;
	m_pDataReceivedCallbackUserData = pUserData;
}

int DriverMgr::GetDeviceName(unsigned int deviceID, std::wstring &strOut)
{
	kmk::Lock lock(m_deviceSection);

	HIDSpectrometerDeviceVector::const_iterator itDevice = m_attachedDevices.find(deviceID);
	if (itDevice == m_attachedDevices.end())
        return ERROR_INVALID_DEVICE_ID;
	
    strOut = itDevice->second->GetDeviceName();
    return ERROR_OK;
}

int DriverMgr::GetDeviceManufacturer(unsigned int deviceID, std::wstring &strOut)
{
	kmk::Lock lock(m_deviceSection);

	HIDSpectrometerDeviceVector::const_iterator itDevice = m_attachedDevices.find(deviceID);
	if (itDevice == m_attachedDevices.end())
        return ERROR_INVALID_DEVICE_ID;
	
    strOut = itDevice->second->GetDeviceManufacturer();
    return ERROR_OK;
}

int DriverMgr::GetDeviceSerial(unsigned int deviceID, std::wstring &strOut)
{
	kmk::Lock lock(m_deviceSection);

	HIDSpectrometerDeviceVector::const_iterator itDevice = m_attachedDevices.find(deviceID);
	if (itDevice == m_attachedDevices.end())
        return ERROR_INVALID_DEVICE_ID;
	
    strOut = itDevice->second->GetDeviceSerial();
    return ERROR_OK;
}

int DriverMgr::GetDeviceVendorID(unsigned int deviceID, int &vendorIDOut)
{
	kmk::Lock lock(m_deviceSection);

	HIDSpectrometerDeviceVector::const_iterator itDevice = m_attachedDevices.find(deviceID);
	if (itDevice == m_attachedDevices.end())
        return ERROR_INVALID_DEVICE_ID;
	
    vendorIDOut = itDevice->second->GetDeviceVendorID();
    return ERROR_OK;
}

int DriverMgr::GetDeviceProductID(unsigned int deviceID, int &productIDOut)
{
	kmk::Lock lock(m_deviceSection);

	HIDSpectrometerDeviceVector::const_iterator itDevice = m_attachedDevices.find(deviceID);
	if (itDevice == m_attachedDevices.end())
        return ERROR_INVALID_DEVICE_ID;
	
    productIDOut = itDevice->second->GetDeviceProductID();
    return ERROR_OK;
}

int DriverMgr::SendInt8ConfigurationCommand(unsigned int deviceID, kmk::ConfigurationID configurationID, unsigned char command)
{
	kmk::Lock lock(m_deviceSection);

	HIDSpectrometerDeviceVector::const_iterator itDevice = m_attachedDevices.find(deviceID);
	if (itDevice == m_attachedDevices.end())
        return ERROR_INVALID_DEVICE_ID;

    return itDevice->second->SendInt8ConfigurationCommand(configurationID, command) ? ERROR_OK : ERROR_UNKNOWN;
}

int DriverMgr::SendInt16ConfigurationCommand(unsigned int deviceID, kmk::ConfigurationID configurationID, unsigned short command)
{
	kmk::Lock lock(m_deviceSection);

	HIDSpectrometerDeviceVector::const_iterator itDevice = m_attachedDevices.find(deviceID);
	if (itDevice == m_attachedDevices.end())
        return ERROR_INVALID_DEVICE_ID;

    return itDevice->second->SendInt16ConfigurationCommand(configurationID, command) ? ERROR_OK : ERROR_UNKNOWN;
}

// Thread used to update all detectors. Started on call to Initialize and killed on call to shutdown.
int DriverMgr::UpdateThreadProc(void *pArg)
{
	const int SLEEP_TIME_MS = 5;
	DriverMgr *pThis = (DriverMgr*)pArg;
	bool keepRunning;
	do
	{
		// Lock the devices while we iterate over them
		{
			kmk::Lock lock(pThis->m_deviceSection);
			HIDSpectrometerDeviceVector::iterator it;
			for (it = pThis->m_attachedDevices.begin(); it != pThis->m_attachedDevices.end(); ++it)
			{
				Detector *pDetector = it->second;
				pDetector->Update();
			}
		}

        kmk::Thread::Sleep(SLEEP_TIME_MS);

		{
			kmk::Lock lock(pThis->m_updateThreadSection);
			keepRunning = pThis->m_keepUpdateThreadRunning;
		}
	} while (keepRunning);

	return 0;
}
