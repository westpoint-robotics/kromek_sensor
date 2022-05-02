#pragma once

#include "IDataInterface.h"
#include "types.h"
#include "Thread.h"
#include "CriticalSection.h"

namespace kmk
{

// Data reading interface for devices using the linux kromek usb driver
class USBKromekDataInterface : public IDataInterface
{
private:
    std::string _devicePath;
    int _fileHandle;
    kmk::Thread _readThread;
    bool _readThreadRunning;

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
    std::string _serialNumber;
    unsigned short _firmwareVersion;

    kmk::CriticalSection _readCriticalSection;
    InterfaceProperties _ifProperties;

    // Open the file device
    bool OpenDevice();

    // Close the file device
    bool Close();

    // Raise the error callback
    void RaiseError(int errorCode, String message);

    // Main thread routine
    static int ReadDataThread(void *pThis);

public:

    unsigned int GetHash();
    VID GetVendorID();
    PID GetProductID();

    USBKromekDataInterface(const char *pDevicePath, PID productID, VID vendorID, const char *pSerial, unsigned short firmwareVersion);
    ~USBKromekDataInterface();

    // Initialise the device once detected but before being opened
    bool Initialize();

    // Returns if the device file is open
    bool IsOpen();

    // Begin reading data from the device until StopReading is called
    bool BeginReading();

    // Stop reading data from the device
    bool StopReading();

    // Get and set configuration settings. The actual response data is returned in the main file stream
    bool GetConfigurationSetting(unsigned char *pDataBuffer, size_t dataLength);
    bool SetConfigurationSetting(unsigned char *pData, size_t dataLength);

    void SetDataReadyCallback(DataReadyCallbackFunc pFunc, void *pArg);
    void SetErrorCallback(ErrorCallbackFunc func, void *pArg);

    String GetInterfaceProperty(const String& name);
};

}
