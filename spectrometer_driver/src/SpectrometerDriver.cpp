#include "stdafx.h"

#include "SpectrometerDriver.h"

#include "DriverMgr.h"
#include "Detector.h"
#include "devices.h"

////////////////////////////////////////////////////////////////////////////
// Name:		kr_GetVersionInformation
// Args:		pProduct: Ptr to int to retrieve the product version (or NULL)
//				pMajor:	Ptr to int to retrieve the major version (or NULL)	
//				pMinor: Ptr to int to retrieve the minor version (or NULL)
//				pBuild: Ptr to int to retrieve the build version (or NULL)
// Desc:		Retrieve the version number of the library. Can be called before Initialise()
////////////////////////////////////////////////////////////////////////////
void stdcall kr_GetVersionInformation(int *pProduct, int *pMajor, int *pMinor, int *pBuild)
{
	if (pProduct)
		*pProduct = VERSION_PRODUCT;

	if (pMajor)
		*pMajor = VERSION_MAJOR;

	if (pMinor)
		*pMinor = VERSION_MINOR;

	if (pBuild)
		*pBuild = VERSION_BUILD;
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_Initialise
// Args:		pErrorCallbackFunc: Ptr to a function that will get called when an error occurs
//				pUserData: User defined data that will be passed into the callback function
// Desc:		Initialise the detector library. Must be called before calling any
//				other detector function (With the exception of kr_GetVersionInformation)
////////////////////////////////////////////////////////////////////////////
int stdcall kr_Initialise(ErrorCallback pErrorCallbackFunc, void *pUserData)
{
	kmk::ValidDeviceIdentifierVector devices;
	
	GetDeviceList(devices);
	DriverMgr::GetInstance()->SetErrorCallback(pErrorCallbackFunc, pUserData);
    return DriverMgr::GetInstance()->Initialise(devices);
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_Destruct
// Args:		None
// Desc:		Shutdown the detector library. Always call at the end of the program to clean up resources.
////////////////////////////////////////////////////////////////////////////
void stdcall kr_Destruct()
{
	DriverMgr::DeleteInstance();
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_GetNextDetector
// Args:		currentDetectorID: The id of the previous detector or 0 to get the first detector in the list
// Desc:		Enumerate the attached detectors. Pass 0 to get the first detector ID and then the value of the 
//				previous detector to move through them. Returns 0 at the end of the list.
////////////////////////////////////////////////////////////////////////////
unsigned int stdcall kr_GetNextDetector(unsigned int currentDetectorID)
{
	return DriverMgr::GetInstance()->GetNextDevice(currentDetectorID);
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_SetDeviceChangedCallback
// Args:		pCallback: Ptr to function that will be called
//				pUserData: User defined data that will be passed into the callback function
// Desc:		Register a callback function that is called when a new device is connected / disconnected
////////////////////////////////////////////////////////////////////////////
void stdcall kr_SetDeviceChangedCallback(DeviceChangedCallback pCallback, void *pUserData)
{
	DriverMgr::GetInstance()->SetDeviceChangedCallback(pCallback, pUserData);
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_SetDataReceivedCallback
// Args:		pCallback: Ptr to function that will be called
//				pUserData: User defined data that will be passed into the callback function
// Desc:		Register a function that will be called when data is received from a device. Make sure you call BeginDataAcquisition to start
//				reading the data from a device
////////////////////////////////////////////////////////////////////////////
void stdcall kr_SetDataReceivedCallback(DataReceivedCallback pCallback, void *pUserData)
{
	DriverMgr::GetInstance()->SetDataReceivedCallback(pCallback, pUserData);
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_GetAcquiredData
// Args:		deviceID: id of device
//				pBuffer: Pointer to an array of size TOTAL_RESULT_CHANNELS to copy the data into. (or NULL)
//				pTotalCounts: Pointer to variable to receive total event counts (or NULL)
//				pRealTime: Pointer to variable to receive the real time (or NULL)
//				liveTime: Pointer to variable to receive the live time (or NULL)
// Desc:		Retrieve the latest counts from the device.
////////////////////////////////////////////////////////////////////////////
int stdcall kr_GetAcquiredData(unsigned int deviceID, unsigned int *pBuffer, unsigned int *pTotalCounts, unsigned int *pRealTime, unsigned int *pLiveTime)
{
	return DriverMgr::GetInstance()->GetAcquiredData(deviceID, pBuffer, pTotalCounts, pRealTime, pLiveTime);
}

int stdcall kr_GetAcquiredDataEx(unsigned int deviceID, unsigned int *pBuffer, unsigned int *pTotalCounts, unsigned int *pRealTime, unsigned int *pLiveTime, unsigned int flags)
{
	return DriverMgr::GetInstance()->GetAcquiredData(deviceID, pBuffer, pTotalCounts, pRealTime, pLiveTime, flags);
}

////////////////////////////////////////////////////////////////////////////
//  Name:		kr_ClearAcquiredData
//  Args:		deviceID: id of device
//  Desc:		Clear all count data that has been acquired in the current/previous
//              acquisition for the given device
////////////////////////////////////////////////////////////////////////////
USBSPECTROMETER_API int stdcall kr_ClearAcquiredData(unsigned int deviceID)
{
    return DriverMgr::GetInstance()->ClearAcquiredData(deviceID);
}

////////////////////////////////////////////////////////////////////////////
//	Name:		kr_IsAcquiringData
//	Args:		deviceID: id of device.
//	Returns:	Returns 1 if acquiring data otherwise 0.
//	Desc:		Determines if a device is currently acquiring data. 
////////////////////////////////////////////////////////////////////////////
int stdcall kr_IsAcquiringData(unsigned int deviceID)
{
	return DriverMgr::GetInstance()->IsAcquiringData(deviceID);
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_BeginDataAcquisition
// Args:		deviceID: id of device to begin acquisition for
//				realTime: real time to acquire data for
//				liveTime: live time to acquire data for
// Desc:		Start reading data from the device. Data is returned using the data received callback set above.
//				Alternatively data can be requested using kr_GetAcquiredData()
////////////////////////////////////////////////////////////////////////////
int stdcall kr_BeginDataAcquisition(unsigned int deviceID, unsigned int realTime, unsigned int liveTime)
{
    return DriverMgr::GetInstance()->BeginDataAcquisition(deviceID, realTime, liveTime);
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_StopDataAcquisition
// Args:		deviceID: id of device to stop acquisition on
// Desc:		Stop reading data from the device. You may still get a report 
//				for this device shortly after the call due to asyncronous behaviour
////////////////////////////////////////////////////////////////////////////
int stdcall kr_StopDataAcquisition(unsigned int deviceID)
{
    return DriverMgr::GetInstance()->EndDataAcquisition(deviceID);
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_GetDeviceName
// Args:		deviceID: id of device
//				pBuffer: Buffer to read the data into
//				bufferSize: Size of pBuffer in bytes
// Desc:		Get device name
////////////////////////////////////////////////////////////////////////////
int stdcall kr_GetDeviceName(unsigned int deviceID, char *pBuffer, int bufferSize, int *pNumBytesOut)
{
    std::wstring val;
    int errorCode = DriverMgr::GetInstance()->GetDeviceName(deviceID, val);
    if (errorCode == ERROR_OK)
    {
        size_t sizeOut = 0;
    #ifdef _WINDOWS
        wcstombs_s(&sizeOut, pBuffer, bufferSize, val.c_str(),val.size());
    #else
        wcstombs(pBuffer, val.c_str(), val.size());
        pBuffer[val.size()] = 0;
        sizeOut = val.size();
    #endif
        *pNumBytesOut = (int)sizeOut;
    }
    return errorCode;
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_GetDeviceManufacturer
// Args:		deviceID: id of device
//				pBuffer: Buffer to read the data into
//				bufferSize: Size of pBuffer in bytes
// Desc:		Get device name
////////////////////////////////////////////////////////////////////////////
int stdcall kr_GetDeviceManufacturer(unsigned int deviceID, char *pBuffer, int bufferSize, int *pNumBytesOut)
{
    std::wstring val;
    int errorCode = DriverMgr::GetInstance()->GetDeviceManufacturer(deviceID, val);
    if (errorCode == ERROR_OK)
    {
        size_t sizeOut;
    #ifdef _WINDOWS
        wcstombs_s(&sizeOut, pBuffer, bufferSize, val.c_str(),val.size());
    #else
        wcstombs(pBuffer, val.c_str(), val.size());
        pBuffer[val.size()] = 0;
        sizeOut = val.size();
    #endif
        *pNumBytesOut = (int)sizeOut;
    }
    return errorCode;

}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_GetDeviceSerial
// Args:		deviceID: id of device
//				pBuffer: Buffer to read the data into
//				bufferSize: Size of pBuffer in bytes
// Desc:		Get device name
////////////////////////////////////////////////////////////////////////////
int stdcall kr_GetDeviceSerial(unsigned int deviceID, char *pBuffer, int bufferSize, int *pNumBytesOut)
{
    std::wstring val;
    int errorCode = DriverMgr::GetInstance()->GetDeviceSerial(deviceID, val);
    if (errorCode == ERROR_OK)
    {
        size_t sizeOut;
    #ifdef _WINDOWS
        wcstombs_s(&sizeOut, pBuffer, bufferSize, val.c_str(),val.size());
    #else
        wcstombs(pBuffer, val.c_str(), val.size());
        pBuffer[val.size()] = 0;
        sizeOut = val.size();
    #endif
        *pNumBytesOut = (int)sizeOut;
    }
    return errorCode;
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_GetDeviceVendorID
// Args:		deviceID: id of device
// Desc:		Get device name
////////////////////////////////////////////////////////////////////////////
int stdcall kr_GetDeviceVendorID(unsigned int deviceID, int *pVendorIDOut)
{
    return DriverMgr::GetInstance()->GetDeviceVendorID(deviceID, *pVendorIDOut);
}

////////////////////////////////////////////////////////////////////////////
// Name:		kr_GetDeviceProductID
// Args:		deviceID: id of device
// Desc:		Get device name
////////////////////////////////////////////////////////////////////////////
int stdcall kr_GetDeviceProductID(unsigned int deviceID, int *pProductIDOut)
{
    return DriverMgr::GetInstance()->GetDeviceProductID(deviceID, *pProductIDOut);
}

int stdcall kr_SendInt8ConfigurationCommand(unsigned int deviceID, ConfigurationCommandsEnum configurationID, unsigned char command)
{
    return DriverMgr::GetInstance()->SendInt8ConfigurationCommand(deviceID, (kmk::ConfigurationID)configurationID, command);
}

int stdcall kr_SendInt16ConfigurationCommand(unsigned int deviceID, ConfigurationCommandsEnum configurationID, unsigned short command)
{
    return DriverMgr::GetInstance()->SendInt16ConfigurationCommand(deviceID, (kmk::ConfigurationID)configurationID, command);
}
