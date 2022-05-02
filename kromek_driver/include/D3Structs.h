#pragma once

namespace kmk
{
// Commands
const int REPORT_ID_SET_GAIN					= 0x02;
const int REPORT_ID_SET_BIAS					= 0x07;
const int REPORT_ID_SET_SERIAL_NO				= 0x08;
const int REPORT_ID_SET_LLD						= 0x09;
const int REPORT_ID_SET_ENABLE_LLD				= 0x0c;
const int REPORT_ID_SET_FACTORYSETUP			= 0x11;
const int REPORT_ID_SET_SOFTWARE_LLD			= 0x12;
const int REPORT_ID_SET_OTG						= 0x46;
const int REPORT_ID_SET_DFU						= 0x47;
const int REPORT_ID_SET_COMPRESSION				= 0x4F;
// Requests
const int REPORT_ID_GET_GAIN					= 0x82;
const int REPORT_ID_GET_BIAS					= 0x87;
const int REPORT_ID_GET_SERIAL_NO				= 0x88;
const int REPORT_ID_GET_LLD						= 0x89;
const int REPORT_ID_GET_ENABLE_LLD				= 0x89;
const int REPORT_ID_INTERNAL_ERROR				= 0xc0;
const int REPORT_ID_GET_16BIT_SPECTRUM			= 0xc1;
const int REPORT_ID_GET_RADIOMETRICSV1_SPECTRUM = 0xC2;
const int REPORT_ID_GET_SOFTWARE_LLD			= 0xc2;
const int REPORT_ID_D3_START					= 0xc4;
const int REPORT_ID_GET_STATUS					= 0xc5;
const int REPORT_ID_GET_OTG						= 0xc6;
const int REPORT_ID_GET_DEVICE_INFO				= 0xc8;
const int REPORT_ID_GET_COMPRESSION				= 0xCF;


// Masks
const int REPORT_MASK_USE_PARENT				= 0x0100;

#pragma pack (push, 1)

// Header structure that starts all content in messages from and to the device
struct ContentHeader
{
	uint8_t componentID;
	uint8_t reportID;
};

// Header structure that starts all messages from and to a multi detector device
struct MessageHeader
{
	uint16_t messageSize;
	uint8_t mode;
	ContentHeader contentHeader;
};

// A basic template for configuration requests. A report that contains only a reportID
struct D3BasicRequestHeader
{
	uint16_t messageSize;
	uint8_t sequence;
	uint8_t componentID;
	uint8_t reportID;
	uint16_t crc;
};

struct D3StartResponseHeader
{
	// Report id
	const static int REPORT_ID = 0xc4;

	MessageHeader header;
	uint16_t version;
  	char serial[50];
	uint16_t crc;
};

// Main spectrum data returned in this structure
struct D3Spectrum16ResponseHeader
{
	const static int REPORT_ID = 0xc1;
	static const int SPECTRUM_SIZE = 4096;
  	
	MessageHeader m_message;
	uint32_t realTimeMS;
	uint16_t neutronCounts;
	uint16_t gammaSpectrum[SPECTRUM_SIZE];
	uint16_t crc;
};

struct D3RadiometricsV1ReponseHeader
{
	const static int REPORT_ID = 0xC2;
	static const int SPECTRUM_SIZE = 4096;

	MessageHeader m_message;

	uint32_t status; // 4 Bytes to cover all statuses

	uint32_t realTimeMS;
	uint32_t realTimeOffsetMs; // Time offset for old messages

	float dose; // Sv
	float doseRate; // Sv per hour averaged over 5 seconds (or 10 seconds)
	uint32_t doseReserved; // Probably going to be used for total accumulated dose or bias

	uint32_t neutronLiveTime; // Not really needed for neutron
	uint32_t neutronCounts;
	int16_t neutronTemperature; // in centiCelcius to allow -163.83 to 163.83
	float neutronBias;

	uint32_t gammaLiveTime; // Reserved
	uint32_t gammaCounts; // Not really needed for gamma as have spectrum
	int16_t gammaTemperature; // in centiCelcius to allow signed -163.83 to +163.83
	float gammaBias;

	uint8_t spectrumBitsSize; // 1-12 (Default 12)
	uint8_t spectrumReserved; // Reserved to make spectrum start on word boundary

	uint16_t gammaSpectrum[SPECTRUM_SIZE];
	uint16_t crc;
};

// Error message returned from the D3
struct D3InternalErrorMessage
{
	const static int REPORT_ID = 0xc0;
	static const int BUFFER_SIZE = 50;
	const static int ERROR_ID_NOT_IMPLEMENTED = 0x3;
	const static int ERROR_ID_WARMING_UP = 0xB;

  	MessageHeader m_message;
	uint8_t m_errorId;
  	char m_errorText[BUFFER_SIZE];
	uint16_t m_crc;
};

// Get configuration request struct
struct D3GetConfiguration
{
	MessageHeader m_message;
	uint16_t m_crc;
};

// 16 bit configuration setting
struct D3Configuration16
{
	const static int REPORT_ID_SET_BIAS = 0x07;
	const static int REPORT_ID_SET_LLD = 0x09;
	const static int REPORT_ID_SET_SOFTWARE_LLD = 0x12;
	const static int REPORT_ID_GET_BIAS = 0x87;
	const static int REPORT_ID_GET_LLD = 0x89;
	const static int REPORT_ID_GET_SOFTWARE_LLD = 0x92;
	const static int REPORT_ID_GET_VERSION = 0x8a;
	const static int REPORT_ID_SET_ACTUAL_BIAS = 0x0B;
	const static int REPORT_ID_GET_ACTUAL_BIAS = 0x8B;

	MessageHeader m_message;
	uint16_t data;
	uint16_t m_crc;
};

// 8 but configuration setting
struct D3Configuration8
{
	const static int REPORT_ID_SET_GAIN = 0x02;
	const static int REPORT_ID_SET_OTG = 0x46;
	const static int REPORT_ID_SET_ENABLE_LLD = 0x0c;
	const static int REPORT_ID_GET_GAIN = 0x82;
	const static int REPORT_ID_GET_OTG = 0xc6;
	const static int REPORT_ID_GET_ENABLE_LLD = 0x8c;

	MessageHeader m_message;
	uint8_t data;
	uint16_t m_crc;
};

// Report returned when requesting the serial number
struct D3ConfigurationSerial
{
	const static int REPORT_ID_GET_SERIAL = 0x88;
	static const int BUFFER_SIZE = 50;

	MessageHeader m_message;
	char data[BUFFER_SIZE];
	uint16_t m_crc;
};

// Report returned when requesting the status
struct D3ConfigurationStatus
{
	const static int REPORT_ID_GET_STATUS = 0xc5;

	MessageHeader m_message;
	uint8_t	m_d3Status;
	uint8_t	m_d3Power;
	int8_t	m_d3Temperature;
	uint8_t	m_sigmaStatus;
	uint8_t	m_tn15Status;
	uint8_t	m_batteryLevel;
	uint8_t	m_batteryChargeRate;
	int8_t	m_batteryTemperature;
	uint8_t	m_usbStatus;
	uint8_t	m_btStatus;
	uint16_t m_crc;
};

struct D3DeviceInfo
{
	const static int REPORT_ID_GET_DEVICE_INFO = 0xc8;

	MessageHeader m_message;
	uint8_t	m_typeId;
	uint16_t m_pcbRevision;
	uint8_t	m_cpuManufacturer;
	uint8_t	m_cpuType;
	uint8_t	m_cpuRevision;
	uint16_t m_cpuPins;
	uint32_t m_cpuId[4];
	uint16_t m_flashSize;
	uint16_t m_nvmSize;
	uint16_t m_epromSize;
	uint16_t m_ramSize;
	uint8_t	m_reserved1[31];
	uint16_t m_crc;
};

struct D3CompressionRequest
{
	const static uint8_t HS_WINDOW_SIZE_DEFAULT = 9;
	const static uint8_t HS_LOOKAHEAD_SIZE_DEFAULT = 8;
	
	MessageHeader m_message;
	uint8_t m_enabled;
	uint8_t m_windowSize;
	uint8_t m_lookAheadSize;
	uint8_t m_direction;
	uint8_t m_reserved[2];
	uint16_t m_crc;
};

#pragma pack (pop)

}