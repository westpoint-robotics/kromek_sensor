#include "stdafx.h"
#include <string.h>
#include <stdio.h>
#include <libudev.h>
#include "DeviceEnumeratorLinux.h"
#include "USBKromekDataInterfaceLinux.h"
#include "Lock.h"

namespace kmk
{
    DeviceEnumerator::DeviceEnumerator()
        : _devicesChangedCallbackFunc(NULL)
        , _devicesChangedCallbackArg(NULL)
        , _finishThreadEvent(false, false, L"")
    {
        
    }
    
    DeviceEnumerator::~DeviceEnumerator()
    {
        
    }
    
    void DeviceEnumerator::EnumerateUSBHIDDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut)
    {
        struct udev *udev;
        struct udev_enumerate *enumerate;
        struct udev_list_entry *devices, *dev_list_entry;
        
        // Create the udev object
        udev = udev_new();
        if (!udev) 
        {
            return;
        }
    
        // Find all usb devices
        enumerate = udev_enumerate_new(udev);
        udev_enumerate_add_match_subsystem(enumerate, "usb");
        udev_enumerate_add_match_subsystem(enumerate, "usbmisc");
        udev_enumerate_scan_devices(enumerate);
        devices = udev_enumerate_get_list_entry(enumerate);
        
        // For each item, see if it matches the vid/pid, and if so
        // create a udev_device record for it 
        udev_list_entry_foreach(dev_list_entry, devices) 
        {
            const char *sysfs_path;
            const char *dev_path;
            struct udev_device *current_dev; // The device's hidraw udev node.
            struct udev_device *usb_dev; // The device's USB udev node.
    
            unsigned short dev_vid;
            unsigned short dev_pid;
            std::string deviceSerial;
            unsigned short firmware_version = 0;

            // Get the system path and then the device path i.e '/dev/kromek0' 
            sysfs_path = udev_list_entry_get_name(dev_list_entry);
            current_dev = udev_device_new_from_syspath(udev, sysfs_path);
            dev_path = udev_device_get_devnode(current_dev);
    
            if (!dev_path || strstr(sysfs_path, "kromek") == NULL)
            {
                // No dev node associated so ignore
                udev_device_unref(current_dev);
                continue;
            }
    
            // Try to get the product and vendor id. This might mean walking up the parent nodes to find it
            usb_dev = current_dev;
            udev_device_ref(usb_dev);
            while (usb_dev)
            {
                struct udev_device *next_device = NULL;
                
                // Grab the properties from the device
                const char *pVendor = udev_device_get_sysattr_value(usb_dev, "idVendor");
                const char *pProduct = udev_device_get_sysattr_value(usb_dev, "idProduct");
                const char *pSerial = udev_device_get_sysattr_value(usb_dev, "serial");

                if (pVendor != NULL && pProduct != NULL) 
                {
                    // Found it!
                    int retVendor = sscanf(pVendor, "%hx", &dev_vid);
                    int retProduct = sscanf(pProduct, "%hx", &dev_pid);
                    if (retVendor != 1 || retProduct != 1) 
                    {
                        usb_dev = NULL;
                    }

                    if (pSerial)
                    {
                        deviceSerial = pSerial ? pSerial : "";
                    }

                    
                    break;
                }
                
                // Attempt to extract the firmware version
                const char *firmware_number_utf8 = udev_device_get_sysattr_value(usb_dev, "bcdDevice");
                if (firmware_number_utf8 != NULL)
                {
                    // TODO: Extract the value correctly
                    //firmware_version = sscanf(firmware_number_utf8, "%d");
                }
                else
                {
                    firmware_version = 0;
                }

                // Move up the hierarchy to the parent and try again 
                next_device = udev_device_get_parent_with_subsystem_devtype(usb_dev, "usb", NULL);
                udev_device_ref(next_device);
                udev_device_unref(usb_dev);
                usb_dev = next_device;
            }
            
            // Not found details 
            if (!usb_dev)
            {
                udev_device_unref(current_dev);
                continue;
            }
                
            udev_device_unref(usb_dev);
            usb_dev = NULL;
                
            // Check the VID/PID against the arguments  
            if (deviceIdentifier.productId == dev_pid && deviceIdentifier.vendorId == dev_vid)
            {
                const char *pDevicePath = strdup(dev_path);
                USBKromekDataInterface *pInterface = new USBKromekDataInterface(pDevicePath, dev_pid, dev_vid, deviceSerial.c_str(), firmware_version);
                pInterface->Initialize();
                listOut.push_back(pInterface);
           }
            
            udev_device_unref(current_dev);
        }
        
        // Cleanup
        udev_enumerate_unref(enumerate);
        udev_unref(udev);
    }
    
    void DeviceEnumerator::EnumerateDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut)
    {
        EnumerateUSBHIDDevices(deviceIdentifier, listOut);
        //EnumerateUSBSerialDevices(deviceIdentifier, listOut);
    }
    
    bool DeviceEnumerator::Initialize(DevicesChangedCallbackFunc callbackFunc, void *pCallbackArg)
    {
        _devicesChangedCallbackFunc = callbackFunc;
        _devicesChangedCallbackArg = pCallbackArg;

        // Start a thread to monitor for changes to devices
        _finishThreadEvent.Reset();
        if (!_deviceChangeMonitorThread.Start(DeviceChangeMonitorThreadProc, this))
        {
            return false;
        }

        return true;
    }

	void DeviceEnumerator::Shutdown()
    {
        // Signal the thread to end and wait for it to exit
        _finishThreadEvent.Signal();
        _deviceChangeMonitorThread.WaitForTermination();
    }

    int DeviceEnumerator::DeviceChangeMonitorThreadProc(void *pArg)
    {
        DeviceEnumerator *pThis = (DeviceEnumerator *)pArg;

        // Setup for notifications from udev

        udev *pudev = udev_new();
        udev_monitor *pudevMonitor = udev_monitor_new_from_netlink(pudev, "udev");
        udev_monitor_filter_add_match_subsystem_devtype(pudevMonitor, "usb", NULL);
        udev_monitor_enable_receiving(pudevMonitor);
        /* Get the file descriptor (fd) for the monitor. This fd will get passed to select() */
        int monitorFd = udev_monitor_get_fd(pudevMonitor);

        // Continue until the thread is notified of exiting
        bool continueRunning = true;
        while(continueRunning)
        {
            // Setup the file descriptor for the select
            fd_set fdSet;
            FD_ZERO(&fdSet);
            FD_SET(monitorFd, &fdSet);

            // No timeout means immediate return from select
            struct timeval tv;
            tv.tv_sec = tv.tv_usec = 0;

            bool devicesChanged = false;

            // Retrieve all outstanding messages from udev
            for (;;)
            {
                int result = select(monitorFd + 1, &fdSet, NULL, NULL, &tv);
                if (result > 0 && FD_ISSET(monitorFd, &fdSet))
                {
                    // Data is waiting...
                    udev_device *pDev = udev_monitor_receive_device(pudevMonitor);
                    if (pDev)
                    {
                        // All we needed to know was something changed and we have retrieved the message off the queue...
                        udev_device_unref(pDev);
                        devicesChanged = true;
                    }
                }
                else
                    break; // No more messages to process
            }

            if (devicesChanged)
            {
                // Add a device changed message
                // Raise the callback function
                if (pThis->_devicesChangedCallbackFunc != NULL)
                {
                    (*pThis->_devicesChangedCallbackFunc)(pThis->_devicesChangedCallbackArg);
                }
            }

            // Add a delay until we are either informed of the thread exiting or a short time expires
            continueRunning = !pThis->_finishThreadEvent.Wait(500);
        }

        // Release udev objects
        if (pudevMonitor)
        {
            udev_monitor_unref(pudevMonitor);
        }

        if (pudev)
        {
            udev_unref(pudev);
        }

        return 0;
    }
}
