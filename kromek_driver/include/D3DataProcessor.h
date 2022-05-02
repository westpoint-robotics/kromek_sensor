#pragma once

#include "IDataProcessor.h"
#include "IDevice.h"
#include <vector>
#include <map>
#include "Thread.h"
#include "Lock.h"
#include "Event.h"
#include "RollingQueue.h"
#include "D3Structs.h"
#include "PacketStreamers.h"


namespace kmk
{
// A class representing a data processor for the D3 detector. The D3 contains 2 components (Sigma + TN15)
class D3DataProcessor : public IDataProcessor
{
public:

	// Id values of each of the components including a cnfiguration component used when we are waiting for GetConfigurationData to be returned
	static const int GammaComponentId = 0x01;
	static const int NeutronComponentId = 0x02;
	static const int DoseComponentId = 0x03;
	static const int InterfaceBoardComponentId = 0x07;
	static const int ConfigurationComponentId = 0x0a;

private:


	enum ThreadStatus
	{
		TS_STOP,	// Thread is stopped / should be stopped immediatly
		TS_RUNNING, // Thread is running / should continue running
		TS_FINISH	// Thread is running / should finish current queue then stop
	};

	enum ExecutionState
	{
		ES_IDLE,
		ES_RUNNING,
		ES_FINISHING,
		ES_STOPPING,
	};

	enum RequestState
	{
		RS_RUN,
		RS_FINISH,
		RS_STOP,
	};

	// Each component needs its own set of callback routines and settings
	struct ComponentDesc
	{
		IDevice *pDevice;
		
		CountEventCallbackFunc countEventCallback;
		void *countEventCallbackArg;

		DoseEventCallbackFunc doseEventCallback;
		void *doseEventCallbackArg;

		FinishedProcessingCallbackFunc finishedCallback;
		void *finishedCallbackArg;
		
		ErrorCallbackFunc errorCallback;
		void *errorCallbackArg;
		
		ThreadStatus status;	// Whether this device is acquiring
		int64_t startStopTimestamp; // Timestamp - If enabled then this is the start time of acquisition otherwise the end time of acquisition
		int64_t accumilatedRealTimeMs; // Real time of the current action / previous acquisition
		
		// Properties of the detector as floating point values
		std::map<ComponentProperty, float> componentProperties;

		ComponentDesc()
			: pDevice(NULL)
			, countEventCallback(NULL)
			, countEventCallbackArg(NULL)
			, doseEventCallback(NULL)
			, doseEventCallbackArg(NULL)
			, finishedCallback(NULL)
			, finishedCallbackArg(NULL)
			, errorCallback(NULL)
			, errorCallbackArg(NULL)
			, status(TS_STOP)
			, startStopTimestamp(0)
			, accumilatedRealTimeMs(0)
		{

		}

		void Clear()
		{
			pDevice = NULL;
			countEventCallback = NULL;
			countEventCallbackArg = NULL;
			doseEventCallback = NULL;
			doseEventCallbackArg = NULL;
			finishedCallback = NULL;
			finishedCallbackArg = NULL;
			errorCallback = NULL;
			errorCallbackArg = NULL;
			status = TS_STOP;
			startStopTimestamp = 0;
			accumilatedRealTimeMs = 0;
			componentProperties.clear();
		}

		void SetProperty(ComponentProperty prop, float val)
		{

			componentProperties[prop] = val;
		}

		float GetProperty(ComponentProperty prop)
		{
			std::map<ComponentProperty, float>::iterator it = componentProperties.find(prop);
			return (it != componentProperties.end()) ? it->second : 0.0f;
		}
	};

	struct ErrorMessageDesc
	{
		int errorCode;
		String message;
	};

	// The data interface used for sending configuration requests
	IDataInterface *_pDataInterface;
	IPacketStreamerPtr _ptrPacketBuffer;
	ComponentDesc _gammaComponent;
	ComponentDesc _neutronComponent;
	ComponentDesc _doseComponent;
	
	// Thread status properties
	kmk::Thread _thread;
	kmk::CriticalSection _criticalSection;
	kmk::CriticalSection _eventSection;
	kmk::Event _waitEvent;
	
	ExecutionState _currentState;
	RequestState _requiredState;
	
	// Ignore the first spectrum data message in every acquisition
	bool _ignoreFirstSpectrumDataPacket;
	
	// Timestamp set when the first spectrum data packet is recieved and used to calculate the timestamp of all subsequent counts
	int64_t _startAcquisitionTimestamp;
	int64_t _accumilatedRealTimeMs;

	// Variables used for querying a configuration variable. 
	enum ConfigurationQueryState
	{
		CQS_IDLE,
		CQS_WAITING,
		CQS_SUCCESS,
		CQS_ERROR
	};

	ConfigurationQueryState _configurationQueryState;
	std::vector<BYTE> _configurationQueryResultData;
	kmk::Event _configurationQueryEvent;

	typedef std::vector<ErrorMessageDesc> ErrorList;
	ErrorList _pendingErrors;
	kmk::CriticalSection _errorSection;

	int64_t _lastSpectrumRequestTime;
	kmk::Event _spectrumQueryEvent;
	
	enum SpectrumReportType
	{
		SRT_UNKNOWN,
		SRT_DETERMINING,
		SRT_RADIOMETRICS_V1,
		SRT_16BITSPECTRUM
	};
	SpectrumReportType _reportType;
	bool _neutronIsGamma;

	// Check the input buffer to see if a full report is ready to process. Return the data and remove it from the input buffer if its ready.
	// Returns false if no report is ready
	bool GetNextReport(std::vector<BYTE> &dataBufferOut, size_t &reportSizeOut);

	// Processing thread routine
    static int ProcessThreadProc(void *pArg);

	// Callback routine called when data is received from the data interface
	static void ReadDataCallbackProc(void *pArg, unsigned char *pData, size_t dataSize);

	// Callback routine when an error occurs on the data interface
	static void DataInterfaceErrorCallbackProc(void *pArg, int errorCode, String message);

	// Process a single report
    void ProcessReport(BYTE *pData);

	// Process a report containing the main spectrum data
	void ProcessSpectrum16Report(D3Spectrum16ResponseHeader *pMessage);

	// Process a report containing spectrum data for three detectors
	void ProcessRadiometricsV1Report(D3RadiometricsV1ReponseHeader *pMessage);

	// Process the return data from a configuration request
	void ProcessConfigurationReport(MessageHeader *pMessageHeader);

	// Execute the error callback routine. Do not call direct, Call Raise error instead
	void ExecuteError(int errorCode, String message);

	// Raise an error on the error callback routine
	void RaiseError(int errorCode, String message);

	// Request an execution state. The state machine will transition to this state
	// as quickly as it can
	bool RequestExecutionState(RequestState request);

	// Set the execution state of the process. Only call this to force a change. 
	// You probably want to call RequestExecutionState above
	bool SetExecutionState(ExecutionState newState);

	// Called when execution state or required state has changed
	bool TransitionExecutionState();

	bool StartProcessingThread();

	bool Decompress(MessageHeader* pMessage, std::vector<BYTE> &dataOut);

	void EnableCompression(bool enabled);

public:

	D3DataProcessor(IDataInterface* pDataInterface, bool supportsRadiometricsV1, IPacketStreamerPtr ptrPacketBuffer, bool neutronIsGamma = false);
	~D3DataProcessor(void);

    kmk::Endian::Order GetEndian() const { return kmk::Endian::LittleEndian;  }

	// Reset the data processor ready to start again
	void Reset();
	
	// Start the processing thread (if not already running) and acquire data for the given component until StopProcessing is called. Components can run independently
	bool StartProcessing(unsigned char componentId);

	// Stop the processing thread. If force = true then stop the thread as soon as possible without finishing the processing of data in the queue - Blocks until 
	// finished. If force = false then the processing queue will be completed and this function will not block (use finished callback)
	bool StopProcessing(unsigned char componentId, bool force);
	
	// Add a component device and its associated callbacks. The D3 accepts a Sigma and TN15
	void AddComponent(uint8_t, IDevice *pDevice, 
		CountEventCallbackFunc pCountEventFunc, void *pCountEventArg, 
		DoseEventCallbackFunc pDoseEventFunc, void *pDoseEventArg,
		FinishedProcessingCallbackFunc pFinishedFunc, void *pFinishedArg,
		ErrorCallbackFunc pErrorFunc, void *pErrorArg);

	// After a call to RemoveComponent the component device should never be accessed from within the data processor again (possibly deleted)
	void RemoveComponent(uint8_t componentId, IDevice *pDevice);
	
	float GetComponentProperty(uint8_t componentId, ComponentProperty prop);

	// Return the real time of the current / last acquisition on the component
	int64_t GetRealTime(uint8_t componentId);

    // Reset the real time of the component to 0
    void ResetRealTime(uint8_t componentId);

	// Return the start time of the current / last acquisition on the component
	int64_t GetStartTime(uint8_t /*componentId*/);

	// Set the start time of the current / last acquisition on the component
	void SetStartTime(uint8_t /*componentId*/, int64_t value);

	// Get a configuration setting. dataLength should be set as the size of the buffer in and will be set on return to the size of the data out
	bool GetConfigurationData(uint8_t componentId, uint16_t configurationIds, BYTE *pDataOut, size_t &dataLength);

	// Set a configuration setting. dataLength should be set to the length pDataIn
	bool SetConfigurationData(uint8_t componentId, uint16_t configurationIds, BYTE *pDataIn, size_t dataLength);
	void SendSpectrumRequest();
};

}
