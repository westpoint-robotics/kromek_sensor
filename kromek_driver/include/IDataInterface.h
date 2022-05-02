#pragma once

#include "types.h"
#include <map>

namespace kmk
{

typedef void (*DataReadyCallbackFunc)(void *pArg, unsigned char *pData, size_t dataSize);

// Interface for objects that can read data from a device. Reading should always be done in a seperate thread (non blocking). 
// Data interfaces shouldn't care about the data format and should avoid processing data, instead raising the DataReadyCallback 
// so that the raw data can be passed onto another thread (IDataProcessor)
class IDataInterface
{
public:

	typedef void (*ErrorCallbackFunc)(void *pArg, int errorCode, String message);
	typedef std::map<String, String> InterfaceProperties;

	IDataInterface() {}
	virtual ~IDataInterface() {}
	
	// Get a hash value that is unique to this device (i.e - A hash of the device path)
	virtual unsigned int GetHash() = 0;
	
	// Initialize allows the device to setup / query details such as vendorID/productID from the hardware device. It should also
	// return false if the device can not be opened
	virtual bool Initialize() = 0;

	virtual VID GetVendorID() = 0;
	virtual PID GetProductID() = 0;

	// Begin reading data from the interface and continue until StopReading is called. Data should be passed to the data ready callback
	virtual bool BeginReading() = 0;

	// Stop reading data from the interface and clean up
	virtual bool StopReading() = 0;

	// Retrieve a configuration value. NOTE that the data is NOT returned as part of this call and is instead returned via the main data callback.
	// This allows the attached DataProcessor to process the response and keeps the interface for all devices consistant with no knowledge of the structure of
	// the data
	virtual bool GetConfigurationSetting(unsigned char *pReportdata, size_t dataLength) = 0;

	// Set a configuration setting on the device. 
	virtual bool SetConfigurationSetting(unsigned char *pData, size_t dataLength) = 0;

	// Set the data callback function that is called whenever data is recieve. This callback should run as quickly as possible to prevent
	// blocking of the read thread
	virtual void SetDataReadyCallback(DataReadyCallbackFunc func, void *pArg) = 0;

	// Set a callback function raised whenever an error occurs during the reading of data.
	virtual void SetErrorCallback(ErrorCallbackFunc func, void *pArg) = 0;

	// Get a value from a list of properties
	virtual String GetInterfaceProperty(const String& name) = 0;
};

}