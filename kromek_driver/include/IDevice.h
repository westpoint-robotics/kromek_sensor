#pragma once

#include "IDataInterface.h"
#include "IDataProcessor.h"
#include "ErrorCodes.h"
#include "types.h"

namespace kmk
{
	#define DEFAULT_NUMBER_OF_CHANNELS 4096
	
	// Number of bytes in a device serial
	#define DEVICE_SERIAL_LENGTH 50

	// The old vendor ID used for GR1 etc...
	#define OLD_KROMEK_VENDOR_ID 0x4d8

	// New vendor id assigned to kromek - use in new devices!
	#define KROMEK_VENDOR_ID 0x2A5A
	#define STM_VENDOR_ID 0x0483
	#define TOSHIBA_BT_VENDOR_ID 0x553339

	enum DetectorType
	{
		DT_Gamma = 0,
		DT_Neutron,
		DT_Dose
	};

	class IDevice;

	typedef void (*CountEventDeviceCallbackFunc)(IDevice *pDevice, int64_t timestamp, int channel, uint32_t numCounts, void *pArg);
	typedef void(*DoseEventDeviceCallbackFunc)(IDevice *pDevice, int64_t timestamp, float dose, float doseRate, float accumulatedDose, void *pArg);
	typedef void (*FinishedAcquisitionCallbackFunc)(IDevice *pDevice, bool forced, void *pArg);
	typedef void (*DeviceErrorCallbackFunc)(IDevice *pDevice, int errorCode, const String &message, void *pArg);

	enum MaterialType
	{
		MT_UNKNOWN=0,
		MT_CZT,
		MT_CSI,
	};

	enum DetectorPolarity
	{
		POSITIVE,
		NEGATIVE,
		NOT_USED,
	};

	enum ConfigurationID
	{
		// USB HID out report numbers.
		CONFIGURATION_SETLLD = 0x01,			// Old report type, use CONFIGURATION_SETLLD_CHANNEL first and only fall back on this if failed
		CONFIGURATION_SETLLD_CHANNEL = 0x09,
		CONFIGURATION_SETGAIN = 0x02,
		CONFIGURATION_SETDIFFGAIN = 0x03,
		CONFIGURATION_SETPOLARITY = 0x03,
		CONFIGURATION_SETHIGHVOLTAGE = 0x06,
		CONFIGURATION_SETBIAS = 0x06, // LCS, GR1 and GR1A only
		CONFIGURATION_SETBIAS2 = 0x07, // Sigma, TN15 only
		CONFIGURATION_SETPULSEWIDTH = 0x07, // LCS only
		CONFIGURATION_SETSERIALNO = 0x08,
		CONFIGURATION_SETENABLELLD = 0x0c,
		CONFIGURATION_SETFACTORYSETUP = 0x11,
		CONFIGURATION_SETSWLLD_CHANNEL = 0x12,
		CONFIGURATION_SETDFUMODE = 0x47,
		CONFIGURATION_SETACTUALBIAS = 0x0B, // UNIBASE PMT/SiPM

		// USB HID in reports
		CONFIGURATION_GETSETTINGS = 0x05, // K102 only

		CONFIGURATION_GETGAIN = 0x82,
		CONFIGURATION_GETDIFFGAIN = 0x83,
		CONFIGURATION_GETPOLARITY = 0x83,
		CONFIGURATION_GETHIGHVOLTAGE = 0x86,
		CONFIGURATION_GETBIAS = 0x86,
		CONFIGURATION_GETBIAS2 = 0x87, // Sigma, TN15 only
		CONFIGURATION_GETPULSEWIDTH = 0x87, // LCS only
		CONFIGURATION_GETSERIAL = 0x88,
		CONFIGURATION_GETLLD_CHANNEL = 0x89,
		CONFIGURATION_GETENABLELLD = 0x8c,
		CONFIGURATION_GETSWLLD_CHANNEL = 0x92,
		CONFIGURATION_GETSTATUS = 0xc5,
		CONFIGURATION_GETDEVICEINFO = 0xc8,
		CONFIGURATION_GETACTUALBIAS = 0x8B,

		CONFIGURATION_GETVERSION = 0x8a, // Return firmware version

		// USB HID out report data.
		CONFIGURATION_SETPOLARITY_POSITIVE = 0x0,
		CONFIGURATION_SETPOLARITY_NEGATIVE = 0x1,

		// masks
		CONFIGURATION_USE_PARENT = 0x0100,
	};

	enum ErrorCodes
	{
		// Data Interface errors
		ERROR_DEVICE_OPEN_FAILED = 100,
		ERROR_READ_FAILED = 101,
		ERROR_INTERNAL_DEVICE = 102,
		ERROR_WRITE_FAILED = 103,
		ERROR_DECOMPRESSION_FAILED = 104,
	};

	struct DetectorProperties
	{
		DetectorType detectorType;
		MaterialType materialType;
		double materialWidth;
		double materialHeight;
		double materialDepth;

		double defaultDeadTime;
		int defaultGain;
		int defaultDiffGain;
		int defaultHighVoltage;
		int defaultLLD;
		int minLLD;
		DetectorPolarity defaultPolarity;

	};

	// List of well known keys for use in GetProperty
	const String IFPROP_LOCATION = L"LocationInformation";
	const String IFPROP_CONNECTION = L"Connection";
	const String IFPROP_DEVICEPATH = L"DevicePath";

	class IDevice
	{
	public:

		IDevice() {};
		virtual ~IDevice() {};

		// Return a unique hash value for the device. The hash value should be based upon something that will remain
		// the same as long as the device remains plugged in (for example the device file path)
		virtual unsigned int GetHash() const = 0;
		virtual IDataInterface *GetInterface() = 0;

		virtual VID GetVendorID() const = 0;
		virtual PID GetProductID() const = 0;
		virtual String GetSerialNumber() = 0;
		virtual String GetManufacturer() const = 0;
		virtual String GetProductName() const = 0;
		virtual unsigned short GetVersion() = 0;
		virtual String GetInterfaceProperty(const String& param) = 0;
		
		// Detector defaults / consts
		virtual DetectorType GetDetectorType() const = 0;	

		//virtual double 

		virtual int64_t GetRealTime() const = 0;
        virtual void ResetRealTime() = 0;
		virtual int64_t GetStartTime() const = 0;
		virtual void SetStartTime(int64_t value) = 0;

		virtual float GetTemperature() const = 0;

		virtual void SetCountEventCallback(CountEventDeviceCallbackFunc func, void *pArg) = 0;
		virtual void SetDoseEventCallback(DoseEventDeviceCallbackFunc func, void *pArg) = 0;
		virtual void SetFinishedAcquisitionCallback(FinishedAcquisitionCallbackFunc func, void *pArg) = 0;
		virtual void SetErrorCallback(DeviceErrorCallbackFunc func, void *pArg) = 0;

		virtual bool Start() = 0;
		virtual bool Stop(bool force) = 0;

		// Get and set functions for configuration settings. componentIndex is for devices that contain more than one detector.
		virtual bool SetConfigurationSettingUInt8(ConfigurationID command, uint8_t val) = 0;
		virtual bool SetConfigurationSettingUInt16(ConfigurationID command, uint16_t val) = 0;
		virtual bool GetConfigurationSettingUInt8(ConfigurationID command, uint8_t &valOut) = 0;
		virtual bool GetConfigurationSettingUInt16(ConfigurationID command, uint16_t &valOut) = 0;
		virtual bool SetConfigurationData(ConfigurationID command, BYTE* buffer, int len) = 0;
		virtual bool GetConfigurationData(ConfigurationID command, BYTE* buffer, int len) = 0;
	};
}
