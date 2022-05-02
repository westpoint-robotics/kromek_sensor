#pragma once

#include "IDataProcessor.h"
#include <vector>
#include "Thread.h"
#include "Lock.h"
#include "Event.h"
#include "RollingQueue.h"
#include "IDataInterface.h"

namespace kmk
{

// Data processor for detectors that receive event counts via a report containing a list of channel numbers that events occured on (Most detectors)
class IntervalCountProcessor : public IDataProcessor
{
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

	struct ErrorMessageDesc
	{
		int errorCode;
		String message;
	};

	// Data interface to write to
	IDataInterface *_pDataInterface;

	// Event callback raised for every count received
	CountEventCallbackFunc _countEventCallback;
	void *_countEventCallbackArg;

	// Callback raised once processing has finished
	FinishedProcessingCallbackFunc _finishedCallback;
	void *_finishedCallbackArg;

	// Callback raised if an error during acquisition occurs
	ErrorCallbackFunc _errorCallback;
	void *_errorCallbackArg;
	typedef std::vector<ErrorMessageDesc> ErrorList;
	ErrorList _pendingErrors;
	kmk::CriticalSection _errorSection;

	// Thread members
	kmk::Thread _thread;
	kmk::CriticalSection _criticalSection;
	kmk::Event _waitEvent;
	
	ThreadStatus _componentRunning;
	ExecutionState _currentState;
	RequestState _requiredState;

	// We need to construct two input buffers. The first will be used as soon as data comes in to construct a full packet.
	// Only when we have a full packet will it be added to the full circular message buffer. By only trying to add full packets to
	// the circular messages buffer we can discard the full packet if the buffer is full
	std::vector<BYTE> _inputPacketBuffer;
	size_t _inputPacketBufferDataSize;
	RollingQueue _dataQueue;
	
	int64_t _startAcquisitionTime; // Time at which the current / last acquisition was started
	int64_t _endAcquisitionTime; // Time at which acquisition was last stopped
 
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

	// Main processing thread routine
    static int ProcessThreadProc(void *pArg);

	static void ReadDataCallbackProc(void *pThis, unsigned char *pData, size_t dataSize);
	static void DataInterfaceErrorCallbackProc(void *pArg, int errorCode, String message);

	// Return the size of a packet based on its report id as the size is not contained in the report
	int DeterminePacketSize(BYTE reportId);

	// Process a report
	void ProcessReport(int64_t timestamp, BYTE *pData, size_t dataSize);

	// Process a report containing the count data
	void ProcessDataReport(int64_t timestamp, BYTE *pData, size_t dataSize);

	// Process a reponse to a configuration request
	void ProcessConfigurationReport(BYTE *pData, size_t dataSize);
	
	// Execute error callback on data processor thread. Do not call direct, call RaiseError
	void ExecuteError(int errorCode, String message);

	// Raise an error callback
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

public:

	IntervalCountProcessor(IDataInterface *pDataInterface);
	~IntervalCountProcessor(void);

    kmk::Endian::Order GetEndian() const { return kmk::Endian::BigEndian; }

	// Queue data received from the data interface. 
	void QueueData(int64_t timeStamp, BYTE *pData, size_t dataLength);
	
	// Reset the data processor ready to start again
	void Reset();
	
	// Add a component device. Only a single component is supported
	void AddComponent(uint8_t componentId, IDevice *pDevice, 
			CountEventCallbackFunc pCountEventFunc, void *pCountEventArg, 
			DoseEventCallbackFunc pDoseEventFunc, void *pDoseEventArg,
			FinishedProcessingCallbackFunc pFinishedFunc, void *pFinishedArg,
			ErrorCallbackFunc pErrorFunc, void *pErrorArg);

	// After a call to RemoveComponent the component device should never be accessed from within the data processor again (possibly deleted)
	void RemoveComponent(uint8_t componentId, IDevice *pDevice);

	float GetComponentProperty(uint8_t componentId, ComponentProperty prop);

	// Start the processing thread (if not already running) and acquire data for the given component until StopProcessing is called.
	bool StartProcessing(uint8_t componentId);
	
	// Stop the processing thread. If force = true then stop the thread as soon as possible without finishing the processing of data in the queue - Blocks until 
	// finished. If force = false then the processing queue will be completed and this function will not block (use finished callback)
	bool StopProcessing(uint8_t componentId, bool force);
	
	// Return the real time of the current / last acquisition on the component
	int64_t GetRealTime(uint8_t componentId);

    // Reset the real time to 0 of the component
    void ResetRealTime(uint8_t componentId);

	// Return the start time of the current / last acquisition on the component
	int64_t GetStartTime(uint8_t /*componentId*/);

	// Set the start time of the current / last acquisition on the component
	void SetStartTime(uint8_t /*componentId*/, int64_t value);

	// Get a configuration setting. dataLength should be set as the size of the buffer in and will be set on return to the size of the data out
	bool GetConfigurationData(uint8_t componentId, uint16_t configurationIds, BYTE *pDataOut, size_t &dataLength);

	// Set a configuration setting. dataLength should be set to the length pDataIn
	bool SetConfigurationData(uint8_t componentId, uint16_t configurationIds, BYTE *pDataIn, size_t dataLength);
};

}
