#ifndef HIDSPECTROMETERDLL_H
#define HIDSPECTROMETERDLL_H

/*=================================================================================================================
*	== Kromek USB driver sdk ==
*	
*		This library can be used to interface with the GR1 range of detectors sold by Kromek http://www.kromek.com
*
*	== Supported Features ==
*
*		- Acquire data from multiple attached detectors.
*		- Enumerate all supported detectors including callback when devices are added or removed from the computer
*		- Get a detectors properties such as name, serial number etc...	
*		- Acquisition of data in channel format by use of a asyncronous callback method (See Data Format below).
*
*	== Data Format ==
*
*		Data is returned to the app using the kr_SetDataReceivedCallback method. The returned data
*		is in the format of a report containing a list of numbered channels (0 to (TOTAL_RESULT_CHANNELS-1)) that have 
*		detected a response over the reporting period. By adding together the counts of each channel 
*		over many reports a view of the spectrum can be accumilated.
*
===================================================================================================================*/

	#include "SpectrometerData.h"

	#ifdef USBSPECTROMETERDLL_EXPORTS
		#if defined(_MSC_VER)
			//  Microsoft 
			#define USBSPECTROMETER_API __declspec(dllexport)
		#elif defined(_GCC)
			//  GCC
			#define USBSPECTROMETER_API __attribute__((visibility("default")))
		#else
			// Other
			#define USBSPECTROMETER_API
		#endif
	#else
		#if defined(_MSC_VER)
			//  Microsoft 
			#define USBSPECTROMETER_API __declspec(dllimport)
		#else 
			//  GCC / Other
			#define USBSPECTROMETER_API
		#endif
	#endif
	
#ifdef __cplusplus
extern "C" {
#endif
	/*==========================================================================
	*	Name:		kr_GetVersionInformation
	*	Args:		pProduct: Ptr to int to retrieve the product version (or NULL)
	*				pMajor:	Ptr to int to retrieve the major version (or NULL)	
	*				pMinor: Ptr to int to retrieve the minor version (or NULL)
	*				pBuild: Ptr to int to retrieve the build version (or NULL)
	*	Desc:		Retrieve the version number of the library. Can be called before Initialise()
	==========================================================================*/
    USBSPECTROMETER_API void stdcall kr_GetVersionInformation(int *pProduct, int *pMajor, int *pMinor, int *pBuild);
	
	
	/*==========================================================================
	*	Name:		kr_Initialise
	*	Args:		errorCallbackFunc: Ptr to a function that will get called when an error occurs
	*				pUserData: User defined data that will be passed into the callback function
	*	Desc:		Initialise the detector library. Must be called before calling any
	*				other detector function (With the exception of kr_GetVersionInformation)
    *   Returns:    ERROR_OK on success or error code on failure
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_Initialise(ErrorCallback errorCallbackFunc, void *pUserData);
 
	
	/*==========================================================================
	*	Name:		kr_Destruct
	*	Args:		None
	*	Desc:		Shutdown the detector library. Always call at the end of the program to clean up resources.
	==========================================================================*/
    USBSPECTROMETER_API void stdcall kr_Destruct();

	
	/*==========================================================================
	*	Name:		kr_GetNextDetector
	*	Args:		currentDetectorID: The id of the previous detector or 0 to get the first detector in the list
	*	Desc:		Enumerate the attached detectors. Pass 0 to get the first detector ID and then the value of the 
	*				previous detector to move through them. Returns 0 at the end of the list.
	==========================================================================*/
    USBSPECTROMETER_API unsigned int stdcall kr_GetNextDetector(unsigned int currentDetectorID);

	
	/*==========================================================================
	*	Name:		kr_SetDeviceChangedCallback
	*	Args:		callback: Ptr to function that will be called
	*				pUserData: User defined data that will be passed into the callback function
	*	Desc:		Register a callback function that is called when a new device is connected / disconnected
	==========================================================================*/
    USBSPECTROMETER_API void stdcall kr_SetDeviceChangedCallback(DeviceChangedCallback callbackFunc, void *pUserData);

	
	/*==========================================================================
	*	Name:		kr_SetDataReceivedCallback
	*	Args:		callback: Ptr to function that will be called
	*				pUserData: User defined data that will be passed into the callback function
	*	Desc:		Register a function that will be called when data is received from a device. Make sure you call BeginDataAcquisition to start
	*				reading the data from a device
	==========================================================================*/
    USBSPECTROMETER_API void stdcall kr_SetDataReceivedCallback(DataReceivedCallback callbackFunc, void *pUserData);

	/*==========================================================================
	*	Name:		kr_GetAcquiredData
	*	Args:		deviceID: id of device to begin acquisition for.
	*				pBuffer: pointer to a buffer of unsigned int[TOTAL_RESULT_CHANNELS] to copy the counts into. 
	*						 Can be NULL to just get real/live time.
	*				realTime: variable to return the real time in or NULL to ignore.
	*				liveTime: variable to return the live time in or NULL to ignore.
    *               flags: Additional flags for advanced options
    *   Returns:    ERROR_OK on success or error code on failure
	*	Desc:		Retrieve the latest set of acquired data for the device. 
	*				Results only includes counts from the start of the last
	*				kr_BeginDataAcquisition
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_GetAcquiredData(unsigned int deviceID, unsigned int *pBuffer, unsigned int *pTotalCounts, unsigned int *pRealTime, unsigned int *pLiveTime);
    USBSPECTROMETER_API int stdcall kr_GetAcquiredDataEx(unsigned int deviceID, unsigned int *pBuffer, unsigned int *pTotalCounts, unsigned int *pRealTime, unsigned int *pLiveTime, unsigned int flags);

    /*==========================================================================
    *   Name:		kr_ClearAcquiredData
    *   Args:		deviceID: id of device
    *   Returns:    ERROR_OK on success or error code on failure
    *   Desc:		Clear all count data that has been acquired in the current/previous
    *               acquisition for the given device
    ===========================================================================*/
    USBSPECTROMETER_API int stdcall kr_ClearAcquiredData(unsigned int deviceID);

	/*==========================================================================
	*	Name:		kr_IsAcquiringData
	*	Args:		deviceID: id of device.
	*	Returns:	Returns 1 if acquiring data otherwise 0.
	*	Desc:		Determines if a device is currently acquiring data. 
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_IsAcquiringData(unsigned int deviceID);

	/*==========================================================================
	*	Name:		kr_BeginDataAcquisition
	*	Args:		deviceID: id of device to begin acquisition for
	*				realTime: time to run in ms (0 for infinite)
	*				liveTime: time to run in ms (0 for infinite)
	*	Desc:		Start reading data from the device. Data is returned using the data recieved callback set above.
    *   Returns:    ERROR_OK on success or error code on failure
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_BeginDataAcquisition(unsigned int deviceID, unsigned int realTime, unsigned int liveTime);

	
	/*==========================================================================
	*	Name:		kr_StopDataAcquisition
	*	Args:		deviceID: id of device to stop acquisition on
	*	 Desc:		Stop reading data from the device. You may still get a report 
	*				for this device shortly after the call due to asyncronous behaviour
    *   Returns:    ERROR_OK on success or error code on failure
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_StopDataAcquisition(unsigned int deviceID);

	
	/*==========================================================================
	*	Name:		kr_GetDeviceName
	*	Args:		deviceID: id of device
	*				pBuffer: Buffer to read the data into
    *				pBufferSize: Size of pBuffer in bytes. Changed to the number of bytes returned.
    *   Returns:    ERROR_OK on success or error code on failure
	*	Desc:		Get device name
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_GetDeviceName(unsigned int deviceID, char *pBuffer, int bufferSize, int *pNumBytesOut);

	
	/*==========================================================================
	*	Name:		kr_GetDeviceManufacturer
	*	Args:		deviceID: id of device
	*				pBuffer: Buffer to read the data into
    *				pBufferSize: Size of pBuffer in bytes. Changed to the number of bytes returned.
    *   Returns:    ERROR_OK on success or error code on failure
	*	Desc:		Get device manufacturer
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_GetDeviceManufacturer(unsigned int deviceID, char *pBuffer, int bufferSize, int *pNumBytesOut);

	
	/*==========================================================================
	*	Name:		kr_GetDeviceSerial
	*	Args:		deviceID: id of device
	*				pBuffer: Buffer to read the data into
    *				pBufferSize: Size of pBuffer in bytes. Changed to the number of bytes returned
    *   Returns:    ERROR_OK on success or error code on failure
	*	Desc:		Get device serial
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_GetDeviceSerial(unsigned int deviceID, char *pBuffer, int bufferSize, int *pNumBytesOut);

	
	/*==========================================================================
	*	Name:		kr_GetDeviceVendorID
	*	Args:		deviceID: id of device
    *               pVendorIDOut: Variable to return the value into
    *   Returns:    ERROR_OK on success or error code on failure
	*	Desc:		Get device vendor id
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_GetDeviceVendorID(unsigned int deviceID, int *pVendorIDOut);

	
	/*==========================================================================
    *   Name:		kr_GetDeviceProductID
    *   Args:		deviceID: id of device
    *               pProductIDOut@ Variable to return the value into
    *   Returns:    ERROR_OK on success or error code on failure
    *   Desc:		Get device product id
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_GetDeviceProductID(unsigned int deviceID, int *pProductIDOut);

	/*==========================================================================
    *   Name:		kr_SendInt8ConfigurationCommand
    *   Args:		deviceID: id of device
    *               commandNumber: id of the type of command to send
    *               command: value to send for the command
    *   Returns:    ERROR_OK on success or error code on failure
    *   Desc:		Send an 8 bit configuration command to the device
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_SendInt8ConfigurationCommand(unsigned int deviceID, ConfigurationCommandsEnum configurationID, unsigned char command);

	/*==========================================================================
    *   Name:		kr_SendInt8ConfigurationCommand
    *   Args:		deviceID: id of device
    *               commandNumber: id of the type of command to send
    *               command: value to send for the command
    *   Returns:    ERROR_OK on success or error code on failure
    *   Desc:		Send an 16 bit configuration command to the device
	==========================================================================*/
    USBSPECTROMETER_API int stdcall kr_SendInt16ConfigurationCommand(unsigned int deviceID, ConfigurationCommandsEnum configurationID, unsigned short command);

#ifdef __cplusplus
}
#endif

#endif // HIDSPECTROMETERDLL_H
