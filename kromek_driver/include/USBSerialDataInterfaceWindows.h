#pragma once

#include <windows.h>
#include "types.h"
#include "Thread.h"
#include "Lock.h"
#include "IDataInterface.h"
#include "Event.h"
#include <vector>
#include <map>

namespace kmk
{
// Data interface for a serial port connection

class SerialDataInterface : public IDataInterface
{
protected:
	String _comPortName;

	HANDLE _fileHandle;
	kmk::Thread _readThread;
	bool _readThreadRunning;
	kmk::Event _cancelIOEvent;
	kmk::Event _writeDataReadyEvent;

	std::vector<BYTE> _writeBuffer;
    DWORD _writeBufferIndex;

	// Callback to pass data to once read from the port
	DataReadyCallbackFunc _dataReadyCallback;
	void *_dataReadyCallbackArg;

	// Callback raised whenever an error occurs
	ErrorCallbackFunc _errorCallback;
	void *_errorCallbackArg;

	// Product and vendor id of the device, actually passed into the constructor as a serial port does not
	// have access to these values directly
	VID _vendorID;
	PID _productID;
	InterfaceProperties _ifProperties;
	
	kmk::CriticalSection _readCriticalSection;
	kmk::CriticalSection _writeCriticalSection;
	
	// Mutex that should be locked whenever the file is open (predominatly by the main read thread)
	kmk::CriticalSection _fileAccessSection;

	// Open the file device
	bool OpenDevice();

	// Close the file device
	bool Close();

	// Raise the error callback
	void RaiseError(int errorCode, String message);

	// Send any data in the send buffer to the device
	bool SendBufferedDataToDevice(OVERLAPPED &overlap);

	// Send data to the file (file must already be opened)
	bool SendDataToDevice(OVERLAPPED &overlap, const BYTE *pData, size_t dataSize);

	void LogToFile(const std::wstring s);

	void ClearBuffer();

	// Main thread routine
    static int ReadDataThread(void *pThis);

public:

	unsigned int GetHash();
	VID GetVendorID();
	PID GetProductID();
	
	SerialDataInterface(String comPortName, PID productID, VID vendorID);
	~SerialDataInterface();

	// Initialise the device once detected but before being opened
	bool Initialize();

	// Test a com port can be opened
	static bool TestPort(String comPortName);

	// Returns if the device file is open
	bool IsOpen();
	
	// Begin reading data from the device until StopReading is called
	bool BeginReading();

	// Stop reading data from the device
	bool StopReading();

	// Get and set configuration settings. The actual response data is returned in the main file stream
	bool GetConfigurationSetting(BYTE *pDataBuffer, size_t dataLength);
	bool SetConfigurationSetting(BYTE *pData, size_t dataLength);

	void SetDataReadyCallback(DataReadyCallbackFunc pFunc, void *pArg);
	void SetErrorCallback(ErrorCallbackFunc func, void *pArg);

	String GetInterfaceProperty(const String& name);
};

class BTSerialDataInterface : public SerialDataInterface
{
public:
	BTSerialDataInterface(String comPortName, PID productID, VID vendorID);
};

class USBSerialDataInterface : public SerialDataInterface
{
public:
	USBSerialDataInterface(String comPortName, PID productID, VID vendorID, const String& location);
};

}
