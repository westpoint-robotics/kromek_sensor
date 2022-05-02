#pragma once

#include <windows.h>
#include "types.h"
#include "Thread.h"
#include "Lock.h"
#include "IDataInterface.h"
#include "Event.h"
#include <map>

namespace kmk
{

// Data interface for a HID device. Data is read from a single interval based endpoint (0)

class HIDDataInterface : public IDataInterface
{
private:
	String _devicePath;
	HANDLE _fileReadHandle;
	kmk::Thread _readThread;
	bool _readThreadRunning;
	kmk::Event _cancelIOEvent;
	
	DataReadyCallbackFunc _dataReadyCallback;
	void *_dataReadyCallbackArg;
	ErrorCallbackFunc _errorCallback;
	void *_errorCallbackArg;

	DWORD _inputBufferSize;
	DWORD _numInputBuffers;
	VID _vendorID;
	PID _productID;
	unsigned short _versionNumber;
	String _serialNumber;
	InterfaceProperties _ifProperties;
	
	kmk::CriticalSection _readCriticalSection;

	bool OpenForReading();
	bool Close();

	void RaiseError(int errorCode, String message);
    static int ReadDataThread(void *pThis);
	
public:

	unsigned int GetHash();
	VID GetVendorID();
	PID GetProductID();
	
	HIDDataInterface(String devicePath);
	~HIDDataInterface();

	bool Initialize();
	bool IsOpen();
	
	bool BeginReading();
	bool StopReading();

	bool GetConfigurationSetting(unsigned char *pDataOut, size_t dataLength);
	bool SetConfigurationSetting(unsigned char *pData, size_t dataLength);

	void SetDataReadyCallback(DataReadyCallbackFunc pFunc, void *pArg);
	void SetErrorCallback(ErrorCallbackFunc func, void *pArg);

	String GetInterfaceProperty(const String& name);
};

}
