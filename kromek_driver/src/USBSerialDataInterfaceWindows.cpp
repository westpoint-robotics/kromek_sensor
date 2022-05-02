#include "StdAfx.h"
#include "USBSerialDataInterfaceWindows.h"
#include <assert.h>
#include "IDevice.h"
#include <iostream>
#include <fstream>

#define INPUT_BUFFER_LENGTH 1024
#define MAX_SERIAL_RX_BUFFER 16 
#define MAX_SERIAL_TX_BUFFER 16 

// Size of the output buffer. Data is buffered for writing until the main file thread is ready to send it
#define WRITE_BUFFER_LENGTH 1024

// Enable this to output the raw data comming into the serial port into a 'data.dump'
// file. Data will be appended to an existing file so make sure you delete old data before
// running
//#define OUTPUT_RAW_DATA

#ifdef OUTPUT_RAW_DATA
#pragma message( "WARNING: Build configured to output raw data from serial port! DO NOT RELEASE!"  )
#endif


namespace kmk
{



SerialDataInterface::SerialDataInterface(String comPortName, PID productID, VID vendorID)
: _comPortName(comPortName)
, _fileHandle(NULL)
, _dataReadyCallback(NULL)
, _dataReadyCallbackArg(NULL)
, _errorCallback(NULL)
, _errorCallbackArg(NULL)
, _readThreadRunning(false)
, _productID(productID)
, _vendorID(vendorID)
, _cancelIOEvent(FALSE, FALSE, L"")
, _writeBufferIndex(0)
{
}

SerialDataInterface::~SerialDataInterface(void)
{
	kmk::Lock lock(_readCriticalSection);
	
	// Stop reading if the device is open
	if (_readThreadRunning)
	{
		StopReading();
	}
}

bool SerialDataInterface::Initialize()
{
	_writeBuffer.resize(WRITE_BUFFER_LENGTH);
	return true;
}

unsigned int SerialDataInterface::GetHash()
{
	unsigned int hash = 0; 
	for(size_t i = 0; i < _comPortName.length(); ++i)  
		hash = 65599 * hash + _comPortName.c_str()[i]; 
	return hash ^ (hash >> 16); 
}

VID SerialDataInterface::GetVendorID()
{
	return _vendorID;
}

PID SerialDataInterface::GetProductID()
{
	return _productID;
}

String SerialDataInterface::GetInterfaceProperty(const String& name)
{
	InterfaceProperties::iterator i = _ifProperties.find(name);
	return (i != _ifProperties.end()) ? i->second : L"";
}

void SerialDataInterface::SetDataReadyCallback(DataReadyCallbackFunc pFunc, void *pArg) 
{
	kmk::Lock lock(_readCriticalSection);
	_dataReadyCallback = pFunc; 
	_dataReadyCallbackArg = pArg;
}

void SerialDataInterface::SetErrorCallback(ErrorCallbackFunc func, void *pArg)
{
	kmk::Lock lock(_readCriticalSection);
	_errorCallback = func; 
	_errorCallbackArg = pArg;
}

// Return if the file handle is open
bool SerialDataInterface::IsOpen()
{
	kmk::Lock lock(_readCriticalSection);

	return _fileHandle != NULL;
}

bool SerialDataInterface::TestPort(String comPortName)
{
	HANDLE handle = CreateFile(comPortName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
	if (handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(handle);
		return true;
	}
	return false;
}

// Open the device ready for reading
bool SerialDataInterface::OpenDevice()
{
	if (IsOpen())
		return true;

	// Open for reading and writing via overlapped (async) functions

	_fileHandle = CreateFile(_comPortName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
	if (_fileHandle == NULL)
	{
		return false;
	}

	// Setup prefered buffer sizes
    SetupComm(_fileHandle, MAX_SERIAL_RX_BUFFER, MAX_SERIAL_TX_BUFFER);

	// Set timeouts (Probably need tweaking!)
	COMMTIMEOUTS cto;
	GetCommTimeouts(_fileHandle, &cto);
	cto.ReadIntervalTimeout = MAXDWORD;
	cto.ReadTotalTimeoutConstant = 0;
	cto.ReadTotalTimeoutMultiplier = 0;
	cto.WriteTotalTimeoutConstant = 0;
	cto.WriteTotalTimeoutMultiplier = 0;
	if (!SetCommTimeouts(_fileHandle,&cto))
	{
		CloseHandle(_fileHandle);
		_fileHandle = NULL;
		return false;
	}
	
	// Set the serial port settings
	DCB dcb;
	dcb.DCBlength = sizeof(dcb);
	GetCommState(_fileHandle, &dcb);
	
	dcb.BaudRate = CBR_115200;
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	//dcb.fRtsControl = RTS_CONTROL_DISABLE;
	//dcb.fOutxCtsFlow = FALSE;
	//dcb.fOutxDsrFlow = FALSE;
	//dcb.fOutX = FALSE;
	//dcb.fInX = FALSE;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.ByteSize = 8;

	if (!SetCommState(_fileHandle, &dcb))
	{
		CloseHandle(_fileHandle);
		_fileHandle = NULL;
		return false;
	}

	// Clear any data already on the serial port or waiting to be sent
	_writeBufferIndex = 0;
	_writeDataReadyEvent.Reset();
	
	PurgeComm(_fileHandle, PURGE_TXCLEAR | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_RXABORT);

	return true;
}

// Close the device if open
bool SerialDataInterface::Close()
{
	kmk::Lock lock(_readCriticalSection);

	if (_fileHandle != NULL)
	{
		CloseHandle(_fileHandle);
		_fileHandle = NULL;
		return true;
	}

	return false;
}

// Start reading data on a seperate thread and pass all data through into the data processor
bool SerialDataInterface::BeginReading()
{
	kmk::Lock lock(_readCriticalSection);

	if (_readThreadRunning)
		return false;

	if (!OpenDevice())
		return false;

	_cancelIOEvent.Reset();
	_readThreadRunning = true;

	//DSC_LOG("SerialDataInterface::BeginReading _readThread.Start");
	if (!_readThread.Start(ReadDataThread, this))
	{
		_readThreadRunning = false;
		Close();
		return false;
	}

	return true;
}

// Stop reading data from the device and kill the thread
bool SerialDataInterface::StopReading()
{
	{
		kmk::Lock lock(_readCriticalSection);

		if (!_readThreadRunning)
			return false;

		_readThreadRunning = false;
	}

	_cancelIOEvent.Signal();

	// Wait for the thread to end before continuing
	_readThread.WaitForTermination();
	return true;
}

// Get a configuration setting. The actual configuration response will be returned across the serial port
bool SerialDataInterface::GetConfigurationSetting(BYTE *pReportdata, size_t dataLength)
{
	return SetConfigurationSetting(pReportdata, dataLength);
}

// Send a configuration setting / report data
bool SerialDataInterface::SetConfigurationSetting(BYTE *pData, size_t dataLength)
{
	// Determine if the read thread is running (the file is already open)
	{
		kmk::Lock lock(_readCriticalSection);
		if (_readThreadRunning)
		{
			// Already open. Add the data to the write buffer and allow the read thread to send it
			kmk::Lock lock(_writeCriticalSection);

			// Check for space in output buffer
			if (_writeBufferIndex + dataLength > _writeBuffer.size())
				return false;

			// Add to output buffer
			memcpy_s(&_writeBuffer[_writeBufferIndex], _writeBuffer.size() - _writeBufferIndex, pData, dataLength);
			_writeBufferIndex += (DWORD)dataLength;
			_writeDataReadyEvent.Signal();
			return true;
		}
	}
	
	// If the func has not returned at this point then the data has not been sent using the read thread, send it via a new file open

	// Lock the file access mutex for the entire duration of the write. This ensures that the read thread has stopped accessing the file as the
	// _readThreadRunning check above simple checks whether the read thread SHOULD continue running. 
	kmk::Lock fileLock(_fileAccessSection);

	// Open the device, send the data and close it again
	if (!OpenDevice())
		return false;

	OVERLAPPED overlap = { 0 };
	overlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	bool result = SendDataToDevice(overlap, pData, dataLength);
	CloseHandle(overlap.hEvent);
	Close();

	return result;
}


// Send the data that has been buffered over to the device (called from read thread)
bool SerialDataInterface::SendBufferedDataToDevice(OVERLAPPED &overlap)
{
	if (SendDataToDevice(overlap, &_writeBuffer[0], _writeBufferIndex))
	{
		_writeBufferIndex = 0;
		_writeDataReadyEvent.Reset();
		return true;
	}
	else
	{
		return false;
	}
}

// Write a data buffer to the device
bool SerialDataInterface::SendDataToDevice(OVERLAPPED &overlap, const BYTE *pData, size_t dataSize)
{
	bool success = false;

	if (!_fileHandle)
		return false;

	DWORD bytesSent = 0;
	int repeatAttempts = 5;
	while (bytesSent < dataSize && repeatAttempts > 0)
	{
		// Write the data
		DWORD bytesWritten = 0;
		overlap.InternalHigh = overlap.Internal = 0;
		WriteFile(_fileHandle, &pData[bytesSent], (DWORD)dataSize - bytesSent, NULL, &overlap);
		DWORD error = GetLastError();
		switch (error)
		{
		case ERROR_SUCCESS:
		case ERROR_IO_PENDING:
			success = true;
			break;

		default:
			success = false;
			break;
		}

		if (success)
		{
			// Make sure the write finishes before we continue
			if (!GetOverlappedResultEx(_fileHandle, &overlap, &bytesWritten, 1000, FALSE))
			{
				CancelIo(_fileHandle);
				success = false;
			}
		}

		if (!success)
		{
			--repeatAttempts;

			if (repeatAttempts <= 0)
			{
				wchar_t msgBuffer[200];
				swprintf_s(msgBuffer, 200, L"Write Data Error - ErrorCode = %d. Written = %d, Expected %zu", GetLastError(), bytesWritten, dataSize - bytesSent);
				RaiseError(ERROR_WRITE_FAILED, msgBuffer);
				break; // Fail out of the loop
			}
		}
		else if (bytesWritten != dataSize)
		{
			wchar_t msgBuffer[200];
			swprintf_s(msgBuffer, 200, L"Write Data Repeat: Written = %d, Expected %d", bytesWritten, _writeBufferIndex);
			RaiseError(ERROR_WRITE_FAILED, msgBuffer);
		}

		bytesSent += bytesWritten;
	}
	return success;
}


void SerialDataInterface::LogToFile(const std::wstring s)
{
	std::wstringstream ns;
	ns << "c:\\temp\\KromekDriver." << _comPortName.substr(_comPortName.length() - 4) << ".log";
	std::wofstream ts(ns.str(), std::ios::out | std::ios::app);
	ts << s << std::endl << std::flush;
}

void SerialDataInterface::ClearBuffer()
{
	OVERLAPPED overlap = { 0 };
	overlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	BYTE data[100];
	DWORD totalReadBytes = 0;
	bool continueLoop = true;
	DWORD readBytes;
	while (continueLoop)
	{
		overlap.InternalHigh = overlap.Internal = 0;
		// Read all pending data out of the buffer and discard
		ReadFile(_fileHandle, &data[0], 100, NULL, &overlap);
		DWORD error = GetLastError();
		switch (error)
		{
		case ERROR_SUCCESS:
			GetOverlappedResult(_fileHandle, &overlap, &readBytes, TRUE);
			if (readBytes == 0)
			{
				continueLoop = false;
			}
			totalReadBytes += readBytes;
			continue;

		case ERROR_IO_PENDING:
			// Cancel the request, we have all data
			// Cancel any pending IO
			CancelIo(_fileHandle);

			// Wait for the cancel to complete
			
			GetOverlappedResult(_fileHandle, &overlap, &readBytes, TRUE);
			continueLoop = false;
			break;

		default:
			DSC_LOG("SerialDataInterface::ClearBuffer Read Error");
			continueLoop = false;
			assert(false);
			break;
		}
	}

	CloseHandle(overlap.hEvent);
	std::wstringstream msg;
	msg << "SerialDataInterface::ClearBuffer discarded bytes: " << totalReadBytes;
	LogToFile(msg.str());
}

// Thread function for reading data from the device until stopped. Pass all data up via the DataReadyCallback
int SerialDataInterface::ReadDataThread(void *pArg)
{
	//DSC_LOG("SerialDataInterface::ReadDataThread Starting");
	SerialDataInterface *pThis = (SerialDataInterface*)pArg;

	// Lock the file access mutex for the entire duration of the thread, nothing else should have access to this file until this thread has exited!
	kmk::Lock fileLock(pThis->_fileAccessSection);

#ifdef OUTPUT_RAW_DATA
	String fileName =pThis-> _comPortName.substr(4) + String(L".dump");
	FILE *fp = _wfopen(fileName.c_str(), L"a");

#endif

	try
	{	
		OVERLAPPED overlap = {0};
		overlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		// Device should already be open before the thread is started
		if (!pThis->IsOpen())
		{
			pThis->RaiseError(ERROR_DEVICE_OPEN_FAILED, L"Can not open device");
			return false;
		}

		// Allocate a memory buffer
        DWORD dataBufferSize = INPUT_BUFFER_LENGTH;
		std::vector<BYTE> dataBuffer;
		dataBuffer.resize(dataBufferSize);

		pThis->ClearBuffer();

		//DSC_LOG("SerialDataInterface::ReadDataThread Initialized");
		//pThis->LogToFile(L"SerialDataInterface::ReadDataThread Initialized");
		while(true)
		{
			// Any data in the output buffer ready to send?
			{
				kmk::Lock lock(pThis->_writeCriticalSection);
				if (pThis->_writeBufferIndex != 0)
				{
					pThis->SendBufferedDataToDevice(overlap);
				}
			}

			// Continue running?
			bool success = false;
			{
				kmk::Lock lock(pThis->_readCriticalSection);

				if (!pThis->_readThreadRunning)
				{
					// Close the device
					pThis->Close();
					break;
				}
			}

			// Begin a new read operation
			success = false;
			DWORD readBytes = 0;
			overlap.InternalHigh = overlap.Internal = 0;

			ReadFile(pThis->_fileHandle, &dataBuffer[0], dataBufferSize, NULL, &overlap);
			DWORD error = GetLastError();
			switch (error)
			{
			case ERROR_SUCCESS:
				success = true;
				break;
			case ERROR_IO_PENDING:
				// Wait for the read response, cancel event or some data ready for writing
				{
					HANDLE pHandles[3] = { overlap.hEvent, pThis->_cancelIOEvent, pThis->_writeDataReadyEvent };
					DWORD waitResult = WaitForMultipleObjects(3, pHandles, FALSE, INFINITE);

					if (waitResult == WAIT_OBJECT_0) // ReadFile response
					{
						success = true;
					}
					else if (waitResult == WAIT_OBJECT_0 + 1) // Cancel event
					{
						// Cancel any pending IO
						CancelIo(pThis->_fileHandle);

						// Wait for the cancel to complete
						GetOverlappedResult(pThis->_fileHandle, &overlap, &readBytes, TRUE);
					}
					else if (waitResult == WAIT_OBJECT_0 + 2) // Write data ready
					{
						// Cancel the read request and allow it to complete in case some bytes are included
						CancelIo(pThis->_fileHandle);
						success = true;
					}
				}
				break;
			default:
				success = false;
				break;
			}

			// If the read request completed then report any bytes to the callback
			if (success)
			{
				if (GetOverlappedResult(pThis->_fileHandle, &overlap, &readBytes, TRUE))
				{
					// Have data?
					if (readBytes > 0)
					{
#ifdef OUTPUT_RAW_DATA
						fwrite(&dataBuffer[0], sizeof(char), readBytes, fp);
						fflush(fp);
#endif
						//DSC_LOG("SerialDataInterface::ReadDataThread DataRecieved: " << (int)readBytes);
						//std::wstringstream str;
						//str << L"SerialDataInterface::ReadDataThread DataRecieved: ";
						//str << (int)readBytes;
						//pThis->LogToFile(str.str() );

						// Raise the data callback
						if (pThis->_dataReadyCallback != NULL)
						{
							(*pThis->_dataReadyCallback)(pThis->_dataReadyCallbackArg, &dataBuffer[0], readBytes);
						}
					}					
					else
					{
						// No data has come through, we need a delay to ensure there is not 100% cpu load taken up by this thread. 
						// Ordinarily this is handled by setting the commtimeouts on the serial port however there appears to be an issue with
						// the microsoft usbser.sys driver that results in lost data when commtimeouts are set to anything but immediate return on data ready.
						// This error gets more pronounced with the more devices that are connected and running simultaneously 
						Sleep(1);
					}
				}
			}
		}

		// Clean up
		CloseHandle(overlap.hEvent);

		//DSC_LOG("SerialDataInterface::ReadDataThread Closed");
		//pThis->LogToFile(L"SerialDataInterface::ReadDataThread Closed");

#ifdef OUTPUT_RAW_DATA
		fclose(fp);
#endif

	}
	catch(std::exception ex)
	{
		pThis->RaiseError(ERROR_READ_FAILED, L"Unexpected error!");
	}
	//DSC_LOG("SerialDataInterface::ReadDataThread Exited");
	return 0;
}

void SerialDataInterface::RaiseError(int errorCode, String message)
{
	if (_errorCallback != NULL)
	{
		(*_errorCallback)(_errorCallbackArg, errorCode, message);
	}
}

BTSerialDataInterface::BTSerialDataInterface(String comPortName, PID productID, VID vendorID) :
	SerialDataInterface(comPortName, productID, vendorID)
{
	// Add the property info
	_ifProperties.insert(std::make_pair(IFPROP_LOCATION, L""));
	_ifProperties.insert(std::make_pair(IFPROP_CONNECTION, L"BT"));
}

USBSerialDataInterface::USBSerialDataInterface(String comPortName, PID productID, VID vendorID, const String& location) :
SerialDataInterface(comPortName, productID, vendorID)
{
	// Add the property info
	_ifProperties.insert(std::make_pair(IFPROP_LOCATION, location));
	_ifProperties.insert(std::make_pair(IFPROP_CONNECTION, L"USB"));
}

}
