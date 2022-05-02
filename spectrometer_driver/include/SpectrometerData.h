#pragma once

#include "ErrorCodes.h"

// Highest channel number returned by the detector
#define TOTAL_RESULT_CHANNELS 4096

// Get Acquired data flags, bitwise flags ie 0x01, 0x02, 0x04 etc
#define GAD_CLEAR_COUNTS 0x01

#if defined(_MSC_VER)
    #define stdcall __stdcall
#else
    #define stdcall
#endif

#ifndef BOOL
    typedef int BOOL;
#endif
#ifndef TRUE
    #define TRUE 1
    #define FALSE 0
#endif

struct SReport
{
    int reportID;
    int numValidElements;
    short data[31];
};

typedef enum
{
	HIDREPORTNUMBER_SETLLD = 0x01,
	HIDREPORTNUMBER_SETGAIN = 0x02,
	HIDREPORTNUMBER_SETPOLARITY = 0x03,
	HIDREPORTNUMBER_SETBIAS16 = 0x07
} ConfigurationCommandsEnum;

typedef enum
{
	HIDREPORTDATA_SETPOLARITY_POSITIVE = 0x0,
	HIDREPORTDATA_SETPOLARITY_NEGATIVE = 0x1
} PolarityEnum;

typedef void (stdcall *ErrorCallback)(void *pCallbackObject, unsigned int deviceID, int errorCode, const char *pMessage);
typedef void (stdcall *DataReceivedCallback)(void *pCallbackObject, unsigned int deviceID, long long timestamp, int channelNumber, unsigned int numCounts);
typedef  void (stdcall *DeviceChangedCallback)(unsigned int deviceID, BOOL added, void *pObject);

/////////////////////////////////////////////////////////////////////////
// Error codes. Also see ErrorCodes in ErrorCodes.h
typedef enum
{
    ERROR_UNKNOWN = ERROR_END,

    // The initialise function has not be called
    ERROR_NOT_INITIALISED,

    // Unrecognised device id supplied
    ERROR_INVALID_DEVICE_ID,

    // Data acquisition has completed (real time or live time has expired during a time limited acquisition)
    ERROR_ACQUISITION_COMPLETE,
} DllErrorCodes;

