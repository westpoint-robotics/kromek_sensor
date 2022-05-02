#include "stdafx.h"
#include "HIDDataInterfaceWindows.h"
#include <assert.h>
#include "IDevice.h"

// This file is in the Windows DDK available from Microsoft.
extern "C"
{
	#include "hidsdi.h"
	#include <setupapi.h>
	#include <dbt.h>
}

#define TOTAL_INPUT_BUFFERS 500

namespace kmk
{

HIDDataInterface::HIDDataInterface(String devicePath)
: _devicePath(devicePath)
, _fileReadHandle(NULL)
, _dataReadyCallback(NULL)
, _readThreadRunning(false)
, _numInputBuffers(0)
, _inputBufferSize(0)
, _productID(0)
, _vendorID(0)
, _versionNumber(0)
, _cancelIOEvent(false, false, L"")
{
	_ifProperties.insert(std::make_pair(IFPROP_DEVICEPATH, devicePath));
}

HIDDataInterface::~HIDDataInterface(void)
{
	kmk::Lock lock(_readCriticalSection);
	if (_readThreadRunning)
	{
		StopReading();
	}
}

bool HIDDataInterface::Initialize()
{
	if (!OpenForReading())
		return false;

	HIDD_ATTRIBUTES attrib = {0};
	attrib.Size = sizeof(HIDD_ATTRIBUTES);

	const int WSTR_LEN = 512;
	wchar_t wstr[WSTR_LEN];

	HidD_GetAttributes(_fileReadHandle, &attrib);
	_productID = attrib.ProductID;
	_vendorID = attrib.VendorID;
	_versionNumber = attrib.VersionNumber;

	// Determine the buffer size of each report + the number of cached buffers so we can calculate the maximum amount of data that can
	// be read in from a read operation
	_inputBufferSize = 63;
	_numInputBuffers = TOTAL_INPUT_BUFFERS;
	//HidD_GetNumInputBuffers(_fileReadHandle, &_numInputBuffers);
	
	PHIDP_PREPARSED_DATA ppData = NULL;
	if (HidD_GetPreparsedData(_fileReadHandle, &ppData))
	{
		HIDP_CAPS caps;
		if (HidP_GetCaps(ppData, &caps) == HIDP_STATUS_SUCCESS)
		{
			_inputBufferSize = caps.InputReportByteLength;
		}

		HidD_FreePreparsedData(ppData);
	}

	/* Serial Number */
	if (HidD_GetSerialNumberString(_fileReadHandle, wstr, sizeof(wstr)))
	{
		wstr[WSTR_LEN-1] = 0x0000;

		// Devices that dont have supporting serial numbers return a dummy value, make sure we ignore it
		if (wstr[0] != 0x409)
			_serialNumber = wstr;
	}

	Close();

	return true;
}

unsigned int HIDDataInterface::GetHash()
{
	unsigned int hash = 0; 
	for(size_t i = 0; i < _devicePath.length(); ++i)  
		hash = 65599 * hash + _devicePath.c_str()[i]; 
	return hash ^ (hash >> 16); 
}

VID HIDDataInterface::GetVendorID()
{
	return _vendorID;
}

PID HIDDataInterface::GetProductID()
{
	return _productID;
}

String HIDDataInterface::GetInterfaceProperty(const String& name)
{
	InterfaceProperties::iterator i = _ifProperties.find(name);
	return i != _ifProperties.end() ? i->second : L"";
}

void HIDDataInterface::SetDataReadyCallback(DataReadyCallbackFunc pFunc, void *pArg) 
{
	kmk::Lock lock(_readCriticalSection);
	_dataReadyCallback = pFunc; 
	_dataReadyCallbackArg = pArg;
}

void HIDDataInterface::SetErrorCallback(ErrorCallbackFunc func, void *pArg)
{
	kmk::Lock lock(_readCriticalSection);
	_errorCallback = func; 
	_errorCallbackArg = pArg;
}

bool HIDDataInterface::IsOpen()
{
	kmk::Lock lock(_readCriticalSection);

	return _fileReadHandle != NULL;
}

// Open the device ready for reading
bool HIDDataInterface::OpenForReading()
{
	if (IsOpen())
		return true;

	_fileReadHandle = CreateFile(_devicePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
	if (_fileReadHandle == NULL)
		return false;

	HidD_SetNumInputBuffers(_fileReadHandle, TOTAL_INPUT_BUFFERS);
	return true;
}

// Close the device if open
bool HIDDataInterface::Close()
{
	kmk::Lock lock(_readCriticalSection);

	if (_fileReadHandle != NULL)
	{
		CloseHandle(_fileReadHandle);
		_fileReadHandle = NULL;
		return true;
	}

	return false;
}

// Read data on a seperate thread and pass all data through into the data processor
bool HIDDataInterface::BeginReading()
{
	kmk::Lock lock(_readCriticalSection);

	if (_readThreadRunning)
		return false;

	if (!OpenForReading())
		return false;

	_cancelIOEvent.Reset();
	_readThreadRunning = true;

	if (!_readThread.Start(ReadDataThread, this))
	{
		_readThreadRunning = false;
		Close();
		return false;
	}

	return true;
}

bool HIDDataInterface::StopReading()
{
	{
		kmk::Lock lock(_readCriticalSection);

		if (!_readThreadRunning)
			return false;

		_readThreadRunning = false;
	}

	// Signal to the read thread to cancel the current operation
	_cancelIOEvent.Signal();

	_readThread.WaitForTermination();
	return true;
}

bool HIDDataInterface::GetConfigurationSetting(unsigned char *pReportdata, size_t dataLength)
{
	// This method may seem complicated given that HIdD_GetInputReport is a blocking method. However we need to make sure this interface
	// works the same as other interfaces by passing the configuration result back through the data queue so that the data processor
	// can process it

	if (pReportdata[0] == CONFIGURATION_GETSERIAL)
	{
		// If the report id of the request is for the serial number then return the serial number from the usb header rather
		// than sending the report to the device
		wcstombs_s(NULL, (char*)&pReportdata[1], dataLength - 1, _serialNumber.c_str(), _serialNumber.length());		
	}
	else if (pReportdata[0] == CONFIGURATION_GETVERSION)
	{
		// If the report id of the request is for the firmware version then return the the usb header value rather
		// than sending the report to the device
		memcpy_s(&pReportdata[1], dataLength - 1, &_versionNumber, sizeof(unsigned short));
	}
	else
	{
		{
			kmk::Lock lock(_readCriticalSection);

			// Reading thread should already be running
			assert(_readThreadRunning);

			HANDLE fileHandle = CreateFile(_devicePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
            int result = HidD_GetInputReport(fileHandle, pReportdata, (ULONG)dataLength);
			CloseHandle(fileHandle);

			if (result == FALSE)
				return false;
		}
	}

	// Post the returned data
	if (_dataReadyCallback != NULL)
	{
		(*_dataReadyCallback)(_dataReadyCallbackArg, pReportdata, dataLength);
	}

	return true;
}

bool HIDDataInterface::SetConfigurationSetting(unsigned char *pData, size_t dataLength)
{
	// Open the file for writing
	HANDLE writeHandle = CreateFile(_devicePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if (writeHandle == NULL)
		return false;
	
    int result = HidD_SetOutputReport(writeHandle, pData, (ULONG)dataLength);

	CloseHandle(writeHandle);
	writeHandle = 0;
	
	return result == TRUE;
}

// Thread function for reading data from the device until stopped. Pass all data up via the DataReadyCallback
int HIDDataInterface::ReadDataThread(void *pArg)
{
	OVERLAPPED overlap = {};
	HIDDataInterface *pThis = (HIDDataInterface*)pArg;

	overlap.hEvent = CreateEvent(NULL, TRUE, FALSE, L"");

	int numReports = 0;
	// Device should already be open before the thread is started
	if (!pThis->IsOpen())
	{
		pThis->RaiseError(ERROR_DEVICE_OPEN_FAILED, L"Can not open device");
		return false;
	}

	// Allocate a memory buffer
    DWORD dataBufferSize = pThis->_inputBufferSize * pThis->_numInputBuffers;
	unsigned char *pDataBuffer = new unsigned char[dataBufferSize];
	bool keepRunning;
	bool firstIteration = true;
	while(true)
	{
		{
			kmk::Lock lock(pThis->_readCriticalSection);
			keepRunning = pThis->_readThreadRunning;
		}

		if (!keepRunning)
			break;

		// Begin a new read operation
		bool dataReady = false;
		DWORD readBytes = 0;
		BOOL result = ReadFile(pThis->_fileReadHandle, pDataBuffer, dataBufferSize, &readBytes, &overlap);	
		
		if (result)
		{
			dataReady = true;
		}
		else
		{
			
			DWORD error = GetLastError();
			switch(error)
			{
			case ERROR_IO_PENDING:
			{
				// Wait for either the cancel or read handle to be triggered
				HANDLE pHandles[2] = { overlap.hEvent, pThis->_cancelIOEvent };
				error = WaitForMultipleObjects(2, pHandles, FALSE, INFINITE);
				switch (error)
				{
				case WAIT_OBJECT_0: // Read handle triggered	
					if (GetOverlappedResult(pThis->_fileReadHandle, &overlap, &readBytes, FALSE))
					{
						dataReady = true;
					}
					break;

				case WAIT_OBJECT_0 + 1: // Cancel IO Event
					// Cancelled, carry on out of the loop
					CancelIo(pThis->_fileReadHandle);

					// Wait for the cancel to complete
					GetOverlappedResult(pThis->_fileReadHandle, &overlap, &readBytes, TRUE); 
					break;

				case WAIT_FAILED:
					// ERROR - Attempt to read again?
					pThis->RaiseError(ERROR_READ_FAILED, L"Error reading from device.");
					Sleep(100);
					break;
				}
			}
				break;

			case ERROR_OPERATION_ABORTED:
				// Cancelled, carry on out of the loop
				break;

			default:
				// ERROR - Attempt to read again?
				pThis->RaiseError(ERROR_READ_FAILED, L"Error reading from device.");
				Sleep(100);
				break;
			}
		}

		if (dataReady)
		{
			if (!firstIteration)
			{
				numReports += readBytes / 63;

				// Raise the callback
				if (pThis->_dataReadyCallback != NULL)
				{
					(*pThis->_dataReadyCallback)(pThis->_dataReadyCallbackArg, pDataBuffer, readBytes);
				}
			}

			firstIteration = false;
		}

		
	};

	CloseHandle(overlap.hEvent);

	delete[] pDataBuffer;

	// Close the device once we have finished
	pThis->Close();
	return 0;
}

void HIDDataInterface::RaiseError(int errorCode, String message)
{
	if (_errorCallback != NULL)
	{
		(*_errorCallback)(_errorCallbackArg, errorCode, message);
	}
}

}
