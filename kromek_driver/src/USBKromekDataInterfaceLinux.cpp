#include "stdafx.h"

#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <errno.h>

#include <memory.h>
#include <vector>

#include "IDevice.h"
#include "USBKromekDataInterfaceLinux.h"
#include "Lock.h"

#define INPUT_BUFFER_LENGTH 1024
#define MAX_SERIAL_RX_BUFFER 16
#define MAX_SERIAL_TX_BUFFER 16

namespace kmk
{



USBKromekDataInterface::USBKromekDataInterface(const char *pDevicePath, PID productID, VID vendorID, const char *pSerial, unsigned short firmwareVersion)
: _devicePath(pDevicePath)
, _fileHandle(0)
, _readThreadRunning(false)
, _dataReadyCallback(NULL)
, _dataReadyCallbackArg(NULL)
, _errorCallback(NULL)
, _errorCallbackArg(NULL)
, _vendorID(vendorID)
, _productID(productID)
, _firmwareVersion(firmwareVersion)
{
    const char *locale;

    locale = setlocale(LC_CTYPE, NULL);
    if (!locale)
        setlocale(LC_CTYPE, "");

    if (pSerial != NULL)
        _serialNumber = pSerial;
}

USBKromekDataInterface::~USBKromekDataInterface(void)
{
    kmk::Lock lock(_readCriticalSection);

    // Stop reading if the device is open
    if (_readThreadRunning)
    {
        StopReading();
    }
}

bool USBKromekDataInterface::Initialize()
{
    return true;
}

unsigned int USBKromekDataInterface::GetHash()
{
    unsigned int hash = 0;
    for(size_t i = 0; i < _devicePath.length(); ++i)
        hash = 65599 * hash + _devicePath.c_str()[i];
    return hash ^ (hash >> 16);
}

VID USBKromekDataInterface::GetVendorID()
{
    return _vendorID;
}

PID USBKromekDataInterface::GetProductID()
{
    return _productID;
}

String USBKromekDataInterface::GetInterfaceProperty(const String& name)
{
    InterfaceProperties::iterator i = _ifProperties.find(name);
    return (i != _ifProperties.end()) ? i->second : L"";
}

void USBKromekDataInterface::SetDataReadyCallback(DataReadyCallbackFunc pFunc, void *pArg)
{
    kmk::Lock lock(_readCriticalSection);
    _dataReadyCallback = pFunc;
    _dataReadyCallbackArg = pArg;
}

void USBKromekDataInterface::SetErrorCallback(ErrorCallbackFunc func, void *pArg)
{
    kmk::Lock lock(_readCriticalSection);
    _errorCallback = func;
    _errorCallbackArg = pArg;
}

// Return if the file handle is open
bool USBKromekDataInterface::IsOpen()
{
    kmk::Lock lock(_readCriticalSection);

    return _fileHandle != 0;
}

// Open the device ready for reading
bool USBKromekDataInterface::OpenDevice()
{
    if (IsOpen())
        return true;

    // Open for reading and writing via async functions
    _fileHandle = open(_devicePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (_fileHandle == 0)
    {
        return false;
    }

    return true;
}

// Close the device if open
bool USBKromekDataInterface::Close()
{
    kmk::Lock lock(_readCriticalSection);

    if (_fileHandle != 0)
    {
        close(_fileHandle);
        _fileHandle = 0;
        return true;
    }

    return false;
}

// Start reading data on a seperate thread and pass all data through into the data processor
bool USBKromekDataInterface::BeginReading()
{
    kmk::Lock lock(_readCriticalSection);

    if (_readThreadRunning)
        return false;

    if (!OpenDevice())
        return false;

    _readThreadRunning = true;

    if (!_readThread.Start(ReadDataThread, this))
    {
        _readThreadRunning = false;
        Close();
        return false;
    }

    return true;
}

// Stop reading data from the device and kill the thread
bool USBKromekDataInterface::StopReading()
{
    {
        kmk::Lock lock(_readCriticalSection);

        if (!_readThreadRunning)
            return false;

        _readThreadRunning = false;
    }

    // Wait for the thread to end before continuing
    _readThread.WaitForTermination();
    return true;
}

// Get a configuration setting. The actual configuration response will be returned across the serial port
bool USBKromekDataInterface::GetConfigurationSetting(unsigned char *pReportdata, size_t dataLength)
{
    // This method may seem complicated given that HIdD_GetInputReport is a blocking method. However we need to make sure this interface
    // works the same as other interfaces by passing the configuration result back through the data queue so that the data processor
    // can process it

    if (pReportdata[0] == CONFIGURATION_GETSERIAL)
    {
        // If the report id of the request is for the serial number then return the serial number from the usb header rather
        // than sending the report to the device
        strcpy((char*)&pReportdata[1], _serialNumber.c_str());

    }
    else if (pReportdata[0] == CONFIGURATION_GETVERSION)
    {
        // If the report id of the request is for the firmware version then return the the usb header value rather
        // than sending the report to the device
        memcpy(&pReportdata[1], &_firmwareVersion, sizeof(unsigned short));
        return true;
    }
    else
    {
        // Send a request, the response will be returned in the input buffer?
        //SetConfigurationSetting(pReportdata, dataLength);
        return false;
    }

    // Post the returned data
    if (_dataReadyCallback != NULL)
    {
        (*_dataReadyCallback)(_dataReadyCallbackArg, pReportdata, dataLength);
    }

    return true;
}

// Send a configuration setting / report data
bool USBKromekDataInterface::SetConfigurationSetting(unsigned char *pData, size_t dataLength)
{

    kmk::Lock lock(_readCriticalSection);

    int fd = open(_devicePath.c_str(), O_WRONLY);
    if (fd == 0)
    {
        return false;
    }

    // Write the data async
    int bytesOut = write(fd, pData, dataLength);

    close(fd);
    return (bytesOut == (int)dataLength);
}

// Thread function for reading data from the device until stopped. Pass all data up via the DataReadyCallback
int USBKromekDataInterface::ReadDataThread(void *pArg)
{
    const int MAX_EPOLL_DEVICES = 1;

    // Max blocking time in ms for the read operation
    const int TIMEOUT = 200;

    USBKromekDataInterface *pThis = (USBKromekDataInterface*)pArg;

    try
    {
        // Device should already be open before the thread is started
        if (!pThis->IsOpen())
        {
            pThis->RaiseError(ERROR_DEVICE_OPEN_FAILED, L"Can not open device");
            return false;
        }

        // Allocate a memory buffer
        size_t dataBufferSize = INPUT_BUFFER_LENGTH;
        std::vector<BYTE> dataBuffer;
        dataBuffer.resize(dataBufferSize);

        // Use epoll to monitor file reads. This allows us to use a timeout in the read routines
        epoll_event eventList[MAX_EPOLL_DEVICES];
        int epollFile = epoll_create(MAX_EPOLL_DEVICES);
        if (epollFile == -1)
        {
            pThis->RaiseError(ERROR_DEVICE_OPEN_FAILED, L"epoll error");
            return false;
        }

        epoll_event ev;
        ev.events = EPOLLIN; // Only interested in input events
        ev.data.fd = pThis->_fileHandle;
        if (epoll_ctl(epollFile, EPOLL_CTL_ADD, pThis->_fileHandle, &ev) == -1)
        {
            pThis->RaiseError(ERROR_DEVICE_OPEN_FAILED, L"Failed to register epoll device");
            close(epollFile);
            return false;
        }

        bool keepRunning = true;

        while(true)
        {
            {
                kmk::Lock lock(pThis->_readCriticalSection);
                keepRunning = pThis->_readThreadRunning;
            }

            if (!keepRunning)
                break;

            // Check for pending data but dont block indefinitely
            int ready = epoll_wait(epollFile, eventList, MAX_EPOLL_DEVICES, TIMEOUT);
            if (ready == -1)
            {
                // If interrupted by a signal then restart the wait
                if (errno == EINTR)
                {
                    continue;
                }
                else
                {
                    pThis->RaiseError(ERROR_READ_FAILED, L"Unexpected error. Reading from device has been stopped!");
                    break;
                }
            }
            else if (ready > 0)
            {
                if (eventList[0].events & EPOLLIN)
                {
                    // Read data
                    int bytesRead = read(pThis->_fileHandle, &dataBuffer[0], dataBuffer.size());
                    if (bytesRead > 0)
                    {
                        // Raise the data callback
                        if (pThis->_dataReadyCallback != NULL)
                        {
                            (*pThis->_dataReadyCallback)(pThis->_dataReadyCallbackArg, &dataBuffer[0], bytesRead);
                        }
                    }
                }
                else if (eventList[0].events & (EPOLLHUP | EPOLLERR))
                {
                    pThis->RaiseError(ERROR_READ_FAILED, L"Unexpected error. Reading from device has been stopped!");
                    break;
                }
            }
        }

        close (epollFile);

        // Close the device once we have finished
        pThis->Close();
    }
    catch(std::exception ex)
    {
        pThis->RaiseError(ERROR_READ_FAILED, L"Unexpected error. Reading from device has been stopped!");
    }

    return 0;
}

void USBKromekDataInterface::RaiseError(int errorCode, String message)
{
    if (_errorCallback != NULL)
    {
        (*_errorCallback)(_errorCallbackArg, errorCode, message);
    }
}

}
