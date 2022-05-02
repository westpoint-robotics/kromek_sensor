#pragma once

#include "SpectrometerData.h"
#include "Detector.h"
#include <string>
#include <map>
#include "CriticalSection.h"
#include "Thread.h"
#include "DeviceMgr.h"


class DriverMgr
{
	/////////////////// Singleton

	private:
		DriverMgr();
		DriverMgr(const DriverMgr &rhs); // Private to prevent calling (Singleton)
		~DriverMgr();

	public:
		// Basic Singleton
		static DriverMgr *m_pInstance;
		static DriverMgr *GetInstance();
		static void DeleteInstance();

	////////////////////////////////

	private:
		
		bool m_initialised;
		kmk::DeviceMgr m_deviceMgr;
        kmk::CriticalSection m_propSection;
		kmk::CriticalSection m_deviceSection;
		kmk::CriticalSection m_updateThreadSection;
		kmk::Thread m_updateThread;
		bool m_keepUpdateThreadRunning;

		typedef std::map<unsigned int, Detector*> HIDSpectrometerDeviceVector;

		HIDSpectrometerDeviceVector m_attachedDevices;

		// Callback on error
		ErrorCallback m_pErrorCallbackFunc;
		void *m_pErrorCallbackUserData;

		// Callback for when the device list changes
		DeviceChangedCallback m_pDeviceChangedCallbackFunc;
		void *m_pDeviceChangedCallbackUserData;

		// Callback when data is received
		DataReceivedCallback m_pDataReceivedCallbackFunc;
		void *m_pDataReceivedCallbackUserData;

		static void OnDeviceChangedProc(kmk::IDevice *pDevice, bool added, void *pArg);
		static void USBDetectorDataChangedCallbackProc(Detector *pDetector, int64_t timestamp, int channel, uint32_t counts, void *pArg);
        static void DeviceFinishedAcquisitionCallbackProc(kmk::IDevice *pDevice, bool forced, void *pArg);
        static void DeviceErrorCallbackProc(kmk::IDevice *pDevice, int errorCode, const String &message, void *pArg);
        static int UpdateThreadProc(void *pThis);

	public:

		// Must be called at beginning and end!
        int Initialise(kmk::ValidDeviceIdentifierVector& validDevices);
		void Destruct();
		
		// Set a callback func for when an error occurs
		void SetErrorCallback(ErrorCallback pFunc, void *pUserData);

		// Set the callback func for when devices change
		void SetDeviceChangedCallback(DeviceChangedCallback pFunc, void *pUserData);

		// Set callback for when data is received
		void SetDataReceivedCallback(DataReceivedCallback pFunc, void *pUserData);
		
		// Retrieve acquired data for the specified detector
		int GetAcquiredData(unsigned int deviceID, unsigned int *pBuffer, unsigned int *pTotalCounts, unsigned int *pRealTime, unsigned int *pLiveTime, int flags = 0U);

        // Clear the acquired data from a device
        int ClearAcquiredData(unsigned int deviceID);

		// Determine if the device is currently acquiring data
		int IsAcquiringData(unsigned int deviceID);

		// Start reading data from the device
        int BeginDataAcquisition(unsigned int deviceID, unsigned int realTime, unsigned int liveTime);

		// Stop reading data from the device
        int EndDataAcquisition(unsigned int deviceID);

		void ReportReceived(Detector *pDetector, SReport &report, unsigned int realTime, unsigned int liveTime);
		bool HasReportReceivedCallback() {return m_pDataReceivedCallbackFunc != NULL;}

		// Enumerate devices. Pass 0 to get first device or id of device to get the next one
		unsigned int GetNextDevice(unsigned int deviceID);

		// Device properties
        int GetDeviceName(unsigned int deviceID, std::wstring &strOut);
        int GetDeviceManufacturer(unsigned int deviceID, std::wstring &strOut);
        int GetDeviceSerial(unsigned int deviceID, std::wstring &strOut);
        int GetDeviceVendorID(unsigned int deviceID, int &vendorIDOut);
        int GetDeviceProductID(unsigned int deviceID, int &productIDOut);

        int SendInt8ConfigurationCommand(unsigned int deviceID, kmk::ConfigurationID configurationID, BYTE command);
        int SendInt16ConfigurationCommand(unsigned int deviceID, kmk::ConfigurationID configurationID, unsigned short command);

		// Call the error callback
		void RaiseError(unsigned int deviceID, int errorCode);

		void OnTimer();

	private:

        // Returns true if the library is initialised. Prefer this over directly accessing m_initialised as it is thread safe
        bool IsInitialised();

		// Register to receive a notification when devices change (internal)
		void RegisterForDeviceNotifications();
		
		// Find all attached devices (internal)
		void FindAllDevices();

		// Internal processing funcs for the PostMessages
		void ProcessMessageBeginAcquisition(unsigned int deviceID, unsigned int realTime, unsigned int liveTime);
		void ProcessMessageEndAcquisition(unsigned int deviceID);
		
		// Check all devices for new data (internal)
		void CheckAsyncOperationStatus();		
};
