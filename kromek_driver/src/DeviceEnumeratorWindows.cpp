#include "StdAfx.h"
#include "DeviceEnumeratorWindows.h"
#include "HIDDataInterfaceWindows.h"
#include "USBSerialDataInterfaceWindows.h"
#include "IDevice.h"
#include "SIGMA_25.h"

#include <INITGUID.H>
#include <devguid.h>
#include <string>
#include <fstream>

// This file is in the Windows DDK available from Microsoft.
extern "C"
{
	#include "hidsdi.h"
	#include <setupapi.h>
	#include <dbt.h>
}

namespace kmk
{

DeviceEnumerator::DeviceEnumerator()
: _initialiseCompleteStatus(false)
, _wndHandle(NULL)
, _devicesChangedCallbackFunc(NULL)
, _devicesChangedCallbackArg(NULL)
{
}

DeviceEnumerator::~DeviceEnumerator()
{
}

bool DeviceEnumerator::Initialize(DevicesChangedCallbackFunc callbackFunc, void *pCallbackArg)
{
	_devicesChangedCallbackFunc = callbackFunc;
	_devicesChangedCallbackArg = pCallbackArg;

	DSC_LOG("DeviceEnumerator::INIT _processingThread.Start");
	if (!_processingThread.Start(ThreadProc, this))
	{
		return false;
	}

	// Wait for the window to be created
	_initialiseCompleteEvent.Wait(INFINITE);

	DSC_LOG("DeviceEnumerator::INIT Returnnig " << _initialiseCompleteStatus);
	return _initialiseCompleteStatus;
}

void DeviceEnumerator::Shutdown()
{
	if (_wndHandle)
	{
	//	DestroyWindow(m_messageWindow);
		PostMessage(_wndHandle, WM_QUIT, 0, 0);
	}
	
	_processingThread.WaitForTermination();
	_wndHandle = NULL;

	
}

// Request to receive messages when a device is attached or removed.
// Also see WM_DEVICECHANGE in BEGIN_MESSAGE_MAP(CUsbhidiocDlg, CDialog).
void DeviceEnumerator::RegisterForDeviceNotifications()
{
	DEV_BROADCAST_DEVICEINTERFACE DevBroadcastDeviceInterface;
	HDEVNOTIFY DeviceNotificationHandle;

	//GUID hidGuid;
	//HidD_GetHidGuid(&hidGuid);

	DevBroadcastDeviceInterface.dbcc_size = sizeof(DevBroadcastDeviceInterface);
	DevBroadcastDeviceInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	//DevBroadcastDeviceInterface.dbcc_classguid = hidGuid;

	DeviceNotificationHandle = RegisterDeviceNotification(_wndHandle, &DevBroadcastDeviceInterface, DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
}

// Enumerate any connected devices that match the deviceIdentifier details and append to the output list
void DeviceEnumerator::EnumerateDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut)
{
	EnumerateUSBHIDDevices(deviceIdentifier, listOut);
	EnumerateUSBSerialDevices(deviceIdentifier, listOut);
	EnumerateBTSerialDevices(deviceIdentifier, listOut);

#ifdef ENABLE_EMULATED_DEVICES
	EnumerateSimulatedDevices(listOut);
#endif
}

void DeviceEnumerator::EnumerateUSBHIDDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut)
{
	// Get a list of the HID devices that are present.
	GUID hidGuid;
	HidD_GetHidGuid(&hidGuid);

	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
	SP_DEVICE_INTERFACE_DATA devIntData = {0};
	devIntData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	// Loop through the list of devices and check the device attributes
	int deviceIndex = 0;
	while (SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &hidGuid, deviceIndex, &devIntData))
	{
		// Detail data contains the device path
		// Determine the size of the detail data
		DWORD requiredSize = 0;
		SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &devIntData, NULL, NULL, &requiredSize, NULL);
		
		// Allocate and retrive the device detail
		SP_DEVICE_INTERFACE_DETAIL_DATA *pDevIntDetailData = (SP_DEVICE_INTERFACE_DETAIL_DATA*)malloc(requiredSize);
		pDevIntDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &devIntData, pDevIntDetailData, requiredSize, NULL, NULL))
		{
			// Error getting details, skip device
			free(pDevIntDetailData);
			++deviceIndex;
			continue;
		}

		// Get the device path
		String devicePath = pDevIntDetailData->DevicePath;
		free(pDevIntDetailData);
		pDevIntDetailData = NULL;

		// Create the interface and check if it matches the product we are searching for
		HIDDataInterface *pInterface = new HIDDataInterface(devicePath);
		pInterface->Initialize();
		
		if (pInterface->GetProductID() == deviceIdentifier.productId && pInterface->GetVendorID() == deviceIdentifier.vendorId)
		{	// Keep
			listOut.push_back(pInterface);
		}
		else
		{	// Discard
			delete pInterface;
		}

		++deviceIndex;
	}
}

// Parse the vendor and product IDs from the given path
// This new function simply searches for VID_ and PID_ and converts the next 4 chars to a hex value
bool ParseHardwareID(wchar_t *pString, PID &productIDOut, VID &vendorIDOut)
{
	std::wstring str = pString;
	size_t pos = str.find(L"PID_");
	if (pos != std::wstring::npos && pos < str.length() - 7) // allow for PID_xxxx
	{
		std::wstringstream stream(str.substr(pos + 4, 4));
		stream >> std::hex >> productIDOut;
		pos = str.find(L"VID_");
		if (pos != std::wstring::npos && pos < str.length() - 7) // allow for VID_xxxx
		{
			std::wstringstream stream(str.substr(pos + 4, 4));
			stream >> std::hex >> vendorIDOut;
			return true;
		}
	}
	return false;
}

void DeviceEnumerator::EnumerateUSBSerialDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut)
{
	GUID *pGuid = (GUID*)&GUID_DEVCLASS_PORTS;

	// Find all 'PORT' devices attached to the pc
	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(pGuid, NULL, NULL, DIGCF_PRESENT);
	SP_DEVINFO_DATA devInfoData = {0};
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	
	int DeviceIndex = 0;
	wchar_t buffer[500];
	DWORD Size = 500;
	
	// Enumerate over all ports
	while (SetupDiEnumDeviceInfo(deviceInfoSet, DeviceIndex, &devInfoData)) 
	{
		DeviceIndex++;

		// Open the registry entry for this device
		HKEY regKeyHandle = SetupDiOpenDevRegKey (deviceInfoSet, &devInfoData,DICS_FLAG_GLOBAL, 0,DIREG_DEV, KEY_READ );
		if (regKeyHandle == INVALID_HANDLE_VALUE)
			continue;

		// We need some way to identify the product and vendor id of this device, Symbolic name appears to be good for this
		Size = 500;
		//if (RegQueryValueEx(regKeyHandle, TEXT("SymbolicName"), NULL, NULL, (LPBYTE)buffer, &Size) != ERROR_SUCCESS)
		if (!SetupDiGetDeviceRegistryProperty(deviceInfoSet, &devInfoData, SPDRP_HARDWAREID, NULL, (LPBYTE)buffer, Size, NULL))
			continue;

		PID productID;
		VID vendorID;
		if (!ParseHardwareID(buffer, productID, vendorID))
			continue;

		// Get the name of the com port
		Size = 500;
		std::wstring devicePort(L"\\\\.\\");
		if (RegQueryValueEx(regKeyHandle, TEXT("PortName"), NULL, NULL, (LPBYTE)buffer, &Size) != ERROR_SUCCESS)
			continue;

		devicePort += buffer;

		// Get the location of the device
		Size = 500;
		if (!SetupDiGetDeviceRegistryProperty(deviceInfoSet, &devInfoData, SPDRP_LOCATION_INFORMATION, NULL, (LPBYTE)buffer, Size, NULL))
			continue;

		std::wstring location(buffer);

		// Close the registry key as we no longer need it
		RegCloseKey(regKeyHandle);
		regKeyHandle = NULL;
	
		// Does this device match the type we are looking for?
		if (productID == deviceIdentifier.productId && vendorID == deviceIdentifier.vendorId)
		{
			// Yes - Add it
			USBSerialDataInterface *pInterface = new USBSerialDataInterface(devicePort, productID, vendorID, location);
			pInterface->Initialize();
			listOut.push_back(pInterface);
		}
	
	}
}

// Parse the vendor and product IDs from the given path
// This new function simply searches for VID_ and PID_ and converts the next 4 chars to a hex value
bool ParseBluetoothID(wchar_t *pString, PID &productIDOut, VID &vendorIDOut)
{
	// Format {GUID}#vvvvvvnnnnpp_????
	// v = 6 digit vendor id
	// n = 4 digit serial
	// p = 2 digit product id
	std::wstring str = pString;
	size_t pos = str.find(L"#");
	if (pos != std::wstring::npos && pos < str.length() - 7) // allow for #123456
	{
		std::wstringstream stream(str.substr(pos + 1, 6));
		stream >> std::hex >> vendorIDOut;
		pos = str.find(L"_");
		if (pos != std::wstring::npos && pos > 1) // allow for VID_xxxx
		{
			std::wstringstream stream(str.substr(pos - 2, 2));
			stream >> std::hex >> productIDOut;
			return true;
		}
	}
	return false;
}

void DeviceEnumerator::EnumerateBTSerialDevices(const ValidDeviceIdentifier &deviceIdentifier, std::vector<IDataInterface*> &listOut)
{
	DSC_LOG(std::hex << "DeviceEnumerator::ENUMBT VID 0x" << deviceIdentifier.vendorId << " PID 0x" << deviceIdentifier.productId)

	GUID *pGuid = (GUID*)&GUID_DEVCLASS_PORTS;

	// Find all 'PORT' devices attached to the pc
	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(pGuid, NULL, NULL, DIGCF_PRESENT);
	SP_DEVINFO_DATA devInfoData = { 0 };
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	int DeviceIndex = 0;
	wchar_t buffer[500];
	DWORD Size = 500;

	// Enumerate over all ports
	while (SetupDiEnumDeviceInfo(deviceInfoSet, DeviceIndex, &devInfoData))
	{
		DeviceIndex++;

		// Open the registry entry for this device
		HKEY regKeyHandle = SetupDiOpenDevRegKey(deviceInfoSet, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
		if (regKeyHandle == INVALID_HANDLE_VALUE)
			continue;

		// We need some way to identify the product and vendor id of this device, Symbolic name appears to be good for this
		Size = 500;
		if (RegQueryValueEx(regKeyHandle, TEXT("Bluetooth_UniqueID"), NULL, NULL, (LPBYTE)buffer, &Size) != ERROR_SUCCESS)
			continue;

		PID productID;
		VID vendorID;
		if (!ParseBluetoothID(buffer, productID, vendorID))
			continue;

		// Get the name of the com port
		Size = 500;
		std::wstring devicePort(L"\\\\.\\");
		if (RegQueryValueEx(regKeyHandle, TEXT("PortName"), NULL, NULL, (LPBYTE)buffer, &Size) != ERROR_SUCCESS)
			continue;

		devicePort += buffer;

		// Close the registry key as we no longer need it
		RegCloseKey(regKeyHandle);
		regKeyHandle = NULL;

		// Does this device match the type we are looking for?
		if (productID == deviceIdentifier.productId && vendorID == deviceIdentifier.vendorId)
		{
			DSC_LOG("Bluetooth_UniqueID " << std::wstring(buffer));
			// Test the com port, as for BT devices the com port may appear to be there but if 
			// the device is out of range etc it will fail
			if (SerialDataInterface::TestPort(devicePort))
			{
				DSC_LOG("Port " << devicePort << " OK");
				BTSerialDataInterface *pInterface = new BTSerialDataInterface(devicePort, productID, vendorID);
				pInterface->Initialize();
				listOut.push_back(pInterface);
			}
			else
			{
				DSC_LOG("Port " << devicePort << " Failure");
			}
		}
	}
}

void DeviceEnumerator::EnumerateSimulatedDevices(std::vector<IDataInterface*> &listOut)
{
	// Determine if the simulated port exists and if so open it
	int startPort = 2000;
	int lastPort = 2050;
	
	// D3S
	for (int i = startPort; i < lastPort; ++i)
	{
		std::wstringstream stream;
		stream << L"\\\\.\\COM_" << i;

		HANDLE hFile = CreateFile(stream.str().c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
        //int error = GetLastError();
		if (hFile != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFile);

			// Yes - Add it
			USBSerialDataInterface *pInterface = new USBSerialDataInterface(stream.str(), kmk::SIGMA_25_D3S::D3SProductId, KROMEK_VENDOR_ID, stream.str());
			pInterface->Initialize();
			listOut.push_back(pInterface);
		}
	}
}

// Window procedure for the invisible message window (Required for detecting usb device changes in windows).
LRESULT CALLBACK DeviceEnumerator::DummyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static DeviceEnumerator *pThis = NULL;

	 switch(message)
	 {
	 case WM_CREATE:
		 {
			// Grab the param passed into the create window func
			CREATESTRUCT *pData = (CREATESTRUCT*)lParam;
			pThis = (DeviceEnumerator*)pData->lpCreateParams;
		 }
		 break;
	   case WM_CLOSE:
		 PostQuitMessage(0);
		 break;

	   case WM_DEVICECHANGE:
			// Raise usb device changed event
		{
			if (pThis != NULL)
			{
				DSC_LOG(std::hex << "WM_DEVICECHANGE WP 0x" << wParam << " LP 0x" << lParam)
				if (pThis->_devicesChangedCallbackFunc != NULL)
				{
					(*pThis->_devicesChangedCallbackFunc)(pThis->_devicesChangedCallbackArg);
				}
			}
			break;
		}

	   case WM_QUIT:
		   DestroyWindow(hWnd);
		   break;

	   default:
		 return DefWindowProc(hWnd, message, wParam, lParam);
	 }
	return 0;

}

int DeviceEnumerator::ThreadProc(void *pArg)
{
	DSC_TRACE(L"DeviceEnumerator::ThreadProc");
	DeviceEnumerator *pThis = (DeviceEnumerator*)pArg;

	// Create an invisible dummy window to receive notifications of device changes.
	WNDCLASSEX wc;
	memset(&wc, NULL, sizeof(WNDCLASSEX));
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.lpfnWndProc   = DummyWndProc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.lpszClassName = L"DummyWindow";
	RegisterClassEx(&wc);

	pThis->_wndHandle =  CreateWindowEx(NULL, L"DummyWindow", L"DummyWindow",0 , 0, 0,0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), pThis);	

	if (pThis->_wndHandle == NULL)
	{
		// Error, notify original thread
		pThis->_initialiseCompleteStatus = false;
		pThis->_initialiseCompleteEvent.Signal();
		return 0;
	}

	pThis->RegisterForDeviceNotifications();

	// Inform the original thread initialization is complete
	DSC_LOG("DeviceEnumerator::TP Initialized");
	pThis->_initialiseCompleteStatus = true;
	pThis->_initialiseCompleteEvent.Signal();

	// Process messages until a quit message.
	MSG WndMsg;
	while (GetMessage(&WndMsg,NULL,0,0))
    {
	   TranslateMessage(&WndMsg);
	   DispatchMessage(&WndMsg);
    }

   return 0;
}



}
