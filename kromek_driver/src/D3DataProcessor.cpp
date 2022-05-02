#include "stdafx.h"
#include "D3DataProcessor.h"
#include "D3Structs.h"
#include "kmkTime.h"
#include <cstring>
#include <stdlib.h>
#include <algorithm>
#include <fstream>

#include "SIGMA_25.h"
#include "TN15.h"
#include <assert.h>
#include <thread>
#include <chrono>
#include "Heatshrink.hpp"


// The main reports max size is 8205 bytes and we only request it one at a time. 
// Add some more space for configuration setting reports that might also be returned
#define MAX_REPORT_SIZE 8500

// Allocate enough space for (approx) a seconds worth of data
#define MAX_BUFFER_SIZE (MAX_REPORT_SIZE * 20)

// Number of milliseconds to wait between querying for a new spectrum
#define QUERY_SPECTRUM_RATE 100

// Timeout in ms for configuration querys
#define CONFIGURATION_QUERY_TIMEOUT 3000

namespace kmk
{

D3DataProcessor::D3DataProcessor(IDataInterface* pDataInterface, bool supportsRadiometricsV1, IPacketStreamerPtr ptrPacketBuffer, bool neutronIsGamma)
	: _pDataInterface(pDataInterface)
	, _waitEvent(false, false, L"")
	, _currentState(ES_IDLE)
	, _requiredState(RS_STOP)
	, _ignoreFirstSpectrumDataPacket(true)
	, _accumilatedRealTimeMs(0)
	, _configurationQueryState(CQS_IDLE)
	, _configurationQueryEvent(false, false, L"")
	, _lastSpectrumRequestTime(0)
	, _ptrPacketBuffer(ptrPacketBuffer)
	, _startAcquisitionTimestamp(0)
	, _neutronIsGamma(neutronIsGamma)
{
	_reportType = supportsRadiometricsV1 ? SRT_RADIOMETRICS_V1 : SRT_UNKNOWN;

	_pDataInterface->SetDataReadyCallback(ReadDataCallbackProc, this);
	_pDataInterface->SetErrorCallback(DataInterfaceErrorCallbackProc, this);
}

D3DataProcessor::~D3DataProcessor()
{
	_pDataInterface->SetDataReadyCallback(NULL, NULL);
	_pDataInterface->SetErrorCallback(NULL, NULL);
}

void D3DataProcessor::AddComponent(uint8_t componentId, IDevice *pDevice, 
	CountEventCallbackFunc pCountEventFunc, void *pCountEventArg, 
	DoseEventCallbackFunc pDoseEventFunc, void *pDoseEventArg,
	FinishedProcessingCallbackFunc pFinishedFunc, void *pFinishedArg,
	ErrorCallbackFunc pErrorFunc, void *pErrorArg)
{
	kmk::Lock lock(_criticalSection);
	kmk::Lock eventLock(_eventSection);

	ComponentDesc *pDesc;
	if (componentId == GammaComponentId)
		pDesc = &_gammaComponent;
	else if (componentId == NeutronComponentId)
		pDesc = &_neutronComponent;
	else if (componentId == DoseComponentId)
		pDesc = &_doseComponent;
	else
		return;
	
	pDesc->pDevice = pDevice;
	pDesc->countEventCallback = pCountEventFunc;
	pDesc->countEventCallbackArg = pCountEventArg;

	pDesc->doseEventCallback = pDoseEventFunc;
	pDesc->doseEventCallbackArg = pDoseEventArg;
	
	pDesc->finishedCallback = pFinishedFunc;
	pDesc->finishedCallbackArg = pFinishedArg;

	pDesc->errorCallback = pErrorFunc;
	pDesc->errorCallbackArg = pErrorArg;
}

void D3DataProcessor::RemoveComponent(uint8_t componentId, IDevice *)
{
	kmk::Lock lock(_criticalSection);
	kmk::Lock eventLock(_eventSection);

	// Remove the reference to the component
	switch(componentId)
	{
	case GammaComponentId:
		_gammaComponent.Clear();
		break;

	case NeutronComponentId:
		_neutronComponent.Clear();
		break;

	case DoseComponentId:
		_doseComponent.Clear();
		break;
	}
}

float D3DataProcessor::GetComponentProperty(uint8_t componentId, ComponentProperty prop)
{
	kmk::Lock lock(_criticalSection);

	switch (componentId)
	{
	case GammaComponentId:
		return _gammaComponent.GetProperty(prop);

	case NeutronComponentId:
		return _neutronComponent.GetProperty(prop);

	case DoseComponentId:
		return _doseComponent.GetProperty(prop);
	}
	return 0.0f;
}

int64_t D3DataProcessor::GetRealTime(uint8_t componentId)
{
	kmk::Lock lock(_criticalSection);
	switch(componentId)
	{
	case GammaComponentId:
		return _gammaComponent.accumilatedRealTimeMs;

	case NeutronComponentId:
		return _neutronComponent.accumilatedRealTimeMs;

	case DoseComponentId:
		return _doseComponent.accumilatedRealTimeMs;
	}

	return 0;
}

void D3DataProcessor::ResetRealTime(uint8_t componentId)
{
    kmk::Lock lock(_criticalSection);
    switch(componentId)
    {
	case GammaComponentId:
        _gammaComponent.accumilatedRealTimeMs = 0;
        break;

    case NeutronComponentId:
        _neutronComponent.accumilatedRealTimeMs = 0;
        break;

	case DoseComponentId:
		_doseComponent.accumilatedRealTimeMs = 0;
		break;
    }
}

int64_t D3DataProcessor::GetStartTime(uint8_t componentId)
{
	kmk::Lock lock(_criticalSection);
	switch (componentId)
	{
	case GammaComponentId:
		return _gammaComponent.startStopTimestamp;

	case NeutronComponentId:
		return _neutronComponent.startStopTimestamp;

	case DoseComponentId:
		return _doseComponent.startStopTimestamp;
	}
	return 0;
}

void D3DataProcessor::SetStartTime(uint8_t componentId, int64_t value)
{
	kmk::Lock lock(_criticalSection);
	switch (componentId)
	{
	case GammaComponentId:
		_gammaComponent.startStopTimestamp = value;
		break;
	case NeutronComponentId:
		_neutronComponent.startStopTimestamp = value;
		break;

	case DoseComponentId:
		_doseComponent.startStopTimestamp = value;
		break;
	}
}

// Check the input buffer to see if a full report is ready to process. Return the data and remove it from the input buffer if its ready.
// Returns false if no report is ready
bool D3DataProcessor::GetNextReport(std::vector<BYTE> &dataBufferOut, size_t &reportSizeOut)
{
	try
	{
		if (!_ptrPacketBuffer->ReadPacket(dataBufferOut))
			return false;

		// The first two bytes of a report give the size of the report
		reportSizeOut = dataBufferOut.size();
		return true;
	}
	catch (const std::exception& ex)
	{
#ifdef _UNICODE
		wchar_t buffer[D3InternalErrorMessage::BUFFER_SIZE];
#ifdef _WINDOWS
		size_t charsToConvert = D3InternalErrorMessage::BUFFER_SIZE;
		mbstowcs_s(&charsToConvert, buffer, ex.what(), D3InternalErrorMessage::BUFFER_SIZE);
#else
		mbstowcs(buffer, ex.what(), D3InternalErrorMessage::BUFFER_SIZE);
#endif
		String str(buffer);
#else
		String str(ex.what());
#endif
		RaiseError(ERROR_INTERNAL_DEVICE, str);
		return false;
	}
}

void D3DataProcessor::Reset()
{
	kmk::Lock lock(_criticalSection);

	// Clear all acquired data
	_accumilatedRealTimeMs = 0;
	_ptrPacketBuffer->Clear();
}

bool D3DataProcessor::RequestExecutionState(RequestState request)
{
	{
		kmk::Lock lock(_criticalSection);

		if (_requiredState == request)
			return true;

		_requiredState = request;
	}
	return TransitionExecutionState();
}

// Determine if the current state and requested state results in a need to change the current state
bool D3DataProcessor::TransitionExecutionState()
{
	switch (_currentState)
	{
	case ES_IDLE:
		// If we are idle check to see if we need to start the thread
		switch (_requiredState)
		{
		case RS_RUN:
			if (!StartProcessingThread())
			{
				{
					kmk::Lock lock(_criticalSection);
					_requiredState = RS_STOP;
					_gammaComponent.status = TS_STOP;
					_neutronComponent.status = TS_STOP;
					_doseComponent.status = TS_STOP;
				}
				SetExecutionState(ES_IDLE);

				// Raise error?
				RaiseError(0, L"Unable to start Processing Thread");
				return false;
			}
			break;

		case RS_FINISH:
			// Todo: Raise Event?
		{
			kmk::Lock lock(_criticalSection);
			_requiredState = RS_STOP;
		}
		break;
		}
		break;

	case ES_RUNNING:
		switch (_requiredState)
		{
		case RS_FINISH:
		case RS_STOP:

			// Stop reading from the device and allow the processing thread to finish/exit
			if (_pDataInterface != NULL)
			{
				_pDataInterface->StopReading();
			}

			SetExecutionState(_requiredState == RS_FINISH ? ES_FINISHING : ES_STOPPING);

			break;

		}
		break;

	case ES_FINISHING:
		if (_requiredState == RS_STOP)
		{
			SetExecutionState(ES_STOPPING);
		}
		break;
	case ES_STOPPING:
		break;
	}

	return true;
}

bool D3DataProcessor::SetExecutionState(ExecutionState state)
{
	{
		kmk::Lock lock(_criticalSection);
		_currentState = state;
	}

	DSC_LOG("EXECUTION_STATE = " << _currentState);
	return TransitionExecutionState();
}

bool D3DataProcessor::StartProcessingThread()
{
	// If a thread is running then it is still cleaning up a previous run. Let it finish
	if (_thread.IsRunning())
	{
		_thread.WaitForTermination();
	}

	Reset();

	if (_pDataInterface != NULL)
	{
		_pDataInterface->BeginReading();
	}

	// The first data packet in any acquisition represents the total time since the very last request which is of no use to the 
	// new acquisition so ignore it
	_ignoreFirstSpectrumDataPacket = true;
	_accumilatedRealTimeMs = 0;

	if (!_thread.Start(ProcessThreadProc, this))
	{
		return false;
	}

	return SetExecutionState(ES_RUNNING);
}

bool D3DataProcessor::StartProcessing(unsigned char componentId)
{
	DSC_LOG("D3DataProcessor::StartProcessing " << (int)componentId)

	bool threadIsExiting = false;
	{
		kmk::Lock lock(_criticalSection);
		_currentState == ES_FINISHING || _currentState == ES_STOPPING;
	}
	
	// If the thread if still exiting wait
	if (threadIsExiting)
	{
		if (_thread.IsRunning())
		{
			_thread.WaitForTermination();
		}
	}

	{
		kmk::Lock lock(_criticalSection);
		switch (componentId)
		{
		case GammaComponentId:
			if (_gammaComponent.status == TS_RUNNING)
				return true;

			
			_gammaComponent.status = TS_RUNNING;
			_gammaComponent.startStopTimestamp = Time::GetTime();
			_gammaComponent.accumilatedRealTimeMs = 0;
			break;

		case NeutronComponentId:
			if (_neutronComponent.status == TS_RUNNING)
				return true;

			_neutronComponent.status = TS_RUNNING;
			_neutronComponent.startStopTimestamp = Time::GetTime();
			_neutronComponent.accumilatedRealTimeMs = 0;
			break;

		case DoseComponentId:
			if (_doseComponent.status == TS_RUNNING)
				return true;

			_doseComponent.status = TS_RUNNING;
			_doseComponent.startStopTimestamp = Time::GetTime();
			_doseComponent.accumilatedRealTimeMs = 0;
			break;

		case ConfigurationComponentId:
			break;

		default:
			return false;
		}
	}
	DSC_LOG("Gamma Status " << _gammaComponent.status);
	RequestExecutionState(RS_RUN);

	return true;
}

bool D3DataProcessor::StopProcessing(unsigned char componentId, bool force)
{
	bool stopReading = false;
	//DSC_LOG("D3DataProcessor::StopProcessing " << (int)componentId << " Force " << force);
	{
		kmk::Lock lock(_criticalSection);

		switch (componentId)
		{
		case GammaComponentId:
			if (_gammaComponent.status == TS_STOP)
				return true;
			
			_gammaComponent.status = force ? TS_STOP : TS_FINISH;
			_gammaComponent.startStopTimestamp = Time::GetTime();
			break;

		case NeutronComponentId:
			if (_neutronComponent.status == TS_STOP)
				return true;
			
			_neutronComponent.status = force ? TS_STOP : TS_FINISH;
			_neutronComponent.startStopTimestamp = Time::GetTime();
			break;

		case DoseComponentId:
			if (_doseComponent.status == TS_STOP)
				return true;

			_doseComponent.status = force ? TS_STOP : TS_FINISH;
			_doseComponent.startStopTimestamp = Time::GetTime();
			break;

		case ConfigurationComponentId:
			break;
		default:
			return false;
		}

		DSC_LOG("Gamma Status " << _gammaComponent.status);
		stopReading =	_gammaComponent.status != TS_RUNNING && 
						_neutronComponent.status != TS_RUNNING &&
						_doseComponent.status != TS_RUNNING &&
						_configurationQueryState != CQS_WAITING;
	}
	
	if (stopReading)
	{
		const int64_t SPECTRUM_TRANSMISSION_TIME = 100;
        int64_t waitTime = std::max(static_cast<int64_t>(0), SPECTRUM_TRANSMISSION_TIME - (kmk::Time::GetTimeMs() - _lastSpectrumRequestTime));
	
		if (waitTime > 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(waitTime) );
		}
		
		RequestExecutionState(force ? RS_STOP : RS_FINISH);
		
		_waitEvent.Signal(); // Interrupt any wait on the thread	

		// If we are not allowing the queue to be completed then wait for the thread to exit before continuing
		if (force)
            _thread.WaitForTermination();
	}
	
	if (force)
	{
		// If it is forced but we are not stopping the read (another component is still active) then raise the finished event 
		// for this component immediatly
		kmk::Lock lock(_eventSection);
		switch (componentId)
		{
		case GammaComponentId:
			(*_gammaComponent.finishedCallback)(_gammaComponent.finishedCallbackArg, true);
			break;

		case NeutronComponentId:
			(*_neutronComponent.finishedCallback)(_neutronComponent.finishedCallbackArg, true);
			break;

		case DoseComponentId:
			(*_doseComponent.finishedCallback)(_doseComponent.finishedCallbackArg, true);
			break;
		}
	}

	return true;
}

// Decompress the content (excluding header and crc) and ensure the returned data is in the same format as the original
// packet including header and crc
bool D3DataProcessor::Decompress(MessageHeader* pMessage, std::vector<BYTE>& dataOut)
{
	const int HeaderAndCrcSize = 5;

	uint32_t bufferSize = MAX_REPORT_SIZE;

	std::vector<BYTE> decompressBuffer;
	decompressBuffer.resize(bufferSize);

	BYTE* pDecodeData = (BYTE*)&pMessage->contentHeader;
	uint32_t decompressedLength = 0;

	Heatshrink heatshrink(D3CompressionRequest::HS_WINDOW_SIZE_DEFAULT, D3CompressionRequest::HS_LOOKAHEAD_SIZE_DEFAULT);
	if (!heatshrink.Expand(pDecodeData, pMessage->messageSize - HeaderAndCrcSize, &decompressBuffer[0], bufferSize, &decompressedLength))
		return false;
	
	// Allocate enough space in the output buffer
	dataOut.resize(static_cast<size_t>(decompressedLength) + HeaderAndCrcSize);
	memcpy(&dataOut[3], &decompressBuffer[0], decompressedLength);

	// Fill in the header
	MessageHeader* pNewMessage = (MessageHeader*)&dataOut[0];
	pNewMessage->mode = pMessage->mode & ~0x1; // Removed compressed bit
	pNewMessage->messageSize = decompressedLength + HeaderAndCrcSize;

	return true;
}

// Process a report (called on the process thread)
void D3DataProcessor::ProcessReport(BYTE *pData)
{
	std::vector <BYTE> decompressedData;

	// Determine the report type
	MessageHeader *pMessageHeader = (MessageHeader*)pData;

	// Determine if the payload needs decoding?
	if ((pMessageHeader->mode & 0x1) != 0)
	{
		// Decompress
		if (!Decompress(pMessageHeader, decompressedData))
		{
			RaiseError(ERROR_DECOMPRESSION_FAILED, L"Decompression of packet failed");
			return;
		}

		// Point to the new decompressed data
		pMessageHeader = (MessageHeader*)&decompressedData[0];
	}

	switch(pMessageHeader->contentHeader.reportID)
	{
	case D3StartResponseHeader::REPORT_ID:
		break;

	case D3Spectrum16ResponseHeader::REPORT_ID:
		// Parse the spectrum data
		ProcessSpectrum16Report((D3Spectrum16ResponseHeader*)pMessageHeader);
		break;

	case D3RadiometricsV1ReponseHeader::REPORT_ID:
		// Parse the spectrum data
		ProcessRadiometricsV1Report((D3RadiometricsV1ReponseHeader*)pMessageHeader);
		break;

	case D3InternalErrorMessage::REPORT_ID:
		// TODO: Report an error
		{
			D3InternalErrorMessage *pErrorHeader = (D3InternalErrorMessage*)pMessageHeader;
			
			// Suppress id not implemented as we use it to find out device capabilities
			if (pErrorHeader->m_errorId == D3InternalErrorMessage::ERROR_ID_WARMING_UP)
			{
				// Device is still warming up. Ignore this message and keep running as normal. If we get a warming up response
				// then we know this device supports at least RADIOMETRICS_V1
				if (_reportType == SRT_DETERMINING)
					_reportType= SRT_RADIOMETRICS_V1;
			}
			else if (pErrorHeader->m_errorId != D3InternalErrorMessage::ERROR_ID_NOT_IMPLEMENTED)
			{
#ifdef _UNICODE
				wchar_t buffer[D3InternalErrorMessage::BUFFER_SIZE];
#ifdef _WINDOWS
				size_t charsToConvert = D3InternalErrorMessage::BUFFER_SIZE;
				mbstowcs_s(&charsToConvert, buffer, pErrorHeader->m_errorText, D3InternalErrorMessage::BUFFER_SIZE);
#else
				mbstowcs(buffer, pErrorHeader->m_errorText, D3InternalErrorMessage::BUFFER_SIZE);
#endif
				String str(buffer);
#else
				String str(pErrorHeader->m_errorText);
#endif
				RaiseError(ERROR_INTERNAL_DEVICE, str);
			}
		}
		break;

	case D3Configuration16::REPORT_ID_GET_BIAS:
	case D3Configuration16::REPORT_ID_GET_LLD:
	case D3Configuration16::REPORT_ID_GET_SOFTWARE_LLD:
	case D3Configuration16::REPORT_ID_GET_VERSION:
	case D3Configuration16::REPORT_ID_GET_ACTUAL_BIAS:
	case D3Configuration8::REPORT_ID_GET_GAIN:
	case D3Configuration8::REPORT_ID_GET_OTG:
	case D3Configuration8::REPORT_ID_GET_ENABLE_LLD:
		ProcessConfigurationReport(pMessageHeader);
		break;

	case D3ConfigurationSerial::REPORT_ID_GET_SERIAL:
	case D3ConfigurationStatus::REPORT_ID_GET_STATUS:
	case D3DeviceInfo::REPORT_ID_GET_DEVICE_INFO:
		ProcessConfigurationReport(pMessageHeader);
		break;
	}

}

void D3DataProcessor::ProcessSpectrum16Report(D3Spectrum16ResponseHeader *pMessage)
{
	CountEventCallbackFunc sigmaEventFunc = NULL;
	CountEventCallbackFunc tn15EventFunc = NULL;
	CountEventCallbackFunc doseEventFunc = NULL;
	void *pSigmaEventArg = NULL;
	void *ptn15EventArg = NULL;
	void* pdoseEventArg = NULL;

	FinishedProcessingCallbackFunc sigmaFinishedFunc = NULL;
	FinishedProcessingCallbackFunc tn15FinishedFunc = NULL;
	FinishedProcessingCallbackFunc doseFinishedFunc = NULL;
	void *pSigmaFinishedArg = NULL;
	void *ptn15FinishedArg = NULL;
	void* pdoseFinishedArg = NULL;
	int64_t timestamp = 0;

	_spectrumQueryEvent.Signal();

	// Grab a local copy of the event functions (We dont want the critical section locked during the call to the actual event functions)
	{
		kmk::Lock lock(_criticalSection);

		// The first data packet in any acquisition represents the total time since the very last request which is of no use to the 
		// new acquisition so ignore it
		if (_ignoreFirstSpectrumDataPacket)
		{
			_ignoreFirstSpectrumDataPacket = false;

			// Store the timestamp of the current time as a reference point
			_startAcquisitionTimestamp = Time::GetTime();
			return;
		}

		// Calculate the timestamp for this data
		_accumilatedRealTimeMs += pMessage->realTimeMS;

		timestamp = _startAcquisitionTimestamp + MS_TO_TICKS(_accumilatedRealTimeMs);

		int64_t currentTime = Time::GetTime();
		
		// The timestamp should never exceed the current time on the system clock, if it does then we are seeing a drift in time
		// between the device and host machine. Correct it at this point
		if (timestamp > currentTime)
		{
			_accumilatedRealTimeMs = kmk::Time::TicksToMs(currentTime - _startAcquisitionTimestamp);
			timestamp = currentTime;
		}

		// If the components are enabled then set the callbacks
		if (_gammaComponent.status != TS_STOP)
		{
			if ((_gammaComponent.status == TS_RUNNING && _gammaComponent.startStopTimestamp <= timestamp) || _gammaComponent.startStopTimestamp >= timestamp)
			{
				sigmaEventFunc = _gammaComponent.countEventCallback;
				pSigmaEventArg = _gammaComponent.countEventCallbackArg;
				_gammaComponent.accumilatedRealTimeMs += pMessage->realTimeMS;
			}
			else if (_gammaComponent.status == TS_FINISH)
			{	// The component is waiting to finish and the latest message is beyond the stop time so raise the event
				_gammaComponent.status = TS_STOP;
				sigmaFinishedFunc = _gammaComponent.finishedCallback;
				pSigmaFinishedArg = _gammaComponent.finishedCallbackArg;
			}
		}

		if (_neutronComponent.status != TS_STOP)
		{
			if ((_neutronComponent.status == TS_RUNNING && _neutronComponent.startStopTimestamp <= timestamp) || _neutronComponent.startStopTimestamp >= timestamp)
			{
				tn15EventFunc = _neutronComponent.countEventCallback;
				ptn15EventArg = _neutronComponent.countEventCallbackArg;
				_neutronComponent.accumilatedRealTimeMs += pMessage->realTimeMS;
			}
			else if (_neutronComponent.status == TS_FINISH)
			{	// The component is waiting to finish and the latest message is beyond the stop time so raise the event
				_neutronComponent.status = TS_STOP;
				tn15FinishedFunc = _neutronComponent.finishedCallback;
				ptn15FinishedArg = _neutronComponent.finishedCallbackArg;
			}
		}

		if (_doseComponent.status != TS_STOP)
		{
			if ((_doseComponent.status == TS_RUNNING && _doseComponent.startStopTimestamp <= timestamp) || _doseComponent.startStopTimestamp >= timestamp)
			{
				doseEventFunc = _doseComponent.countEventCallback;
				pdoseEventArg = _doseComponent.countEventCallbackArg;
				_doseComponent.accumilatedRealTimeMs += pMessage->realTimeMS;
			}
			else if (_doseComponent.status == TS_FINISH)
			{	// The component is waiting to finish and the latest message is beyond the stop time so raise the event
				_doseComponent.status = TS_STOP;
				doseFinishedFunc = _doseComponent.finishedCallback;
				pdoseFinishedArg = _doseComponent.finishedCallbackArg;
			}
		}
	}

	// Gamma spectrum / SIGMA
	// Raise an event for each channel containing counts
	if (sigmaEventFunc != NULL)
	{
		for (int i = 0; i < D3Spectrum16ResponseHeader::SPECTRUM_SIZE; ++i)
		{
			if (pMessage->gammaSpectrum[i] > 0)
				(*sigmaEventFunc)(pSigmaEventArg, timestamp, i, pMessage->gammaSpectrum[i]);
		}
	}
	else if (sigmaFinishedFunc != NULL)
	{
		(*sigmaFinishedFunc)(pSigmaFinishedArg, false);
	}

	// Neutron / TN15
	if (tn15EventFunc != NULL)
	{
		if (pMessage->neutronCounts > 0)
			(*tn15EventFunc)(ptn15EventArg, timestamp, 0, pMessage->neutronCounts);		
	}
	else if (tn15FinishedFunc != NULL)
	{
		(*tn15FinishedFunc)(ptn15FinishedArg, false);
	}

	// Dose 
	//if (doseEventFunc != NULL)
	//{
	//		(*doseEventFunc)(pdoseEventArg, timestamp, 0, 0);
	//}
	if (doseFinishedFunc != NULL)
	{
		(*doseFinishedFunc)(pdoseFinishedArg, false);
	}

}

void D3DataProcessor::ProcessRadiometricsV1Report(D3RadiometricsV1ReponseHeader *pMessage)
{
	CountEventCallbackFunc sigmaEventFunc = NULL;
	CountEventCallbackFunc tn15EventFunc = NULL;
	DoseEventCallbackFunc doseEventFunc = NULL;
	void *pSigmaEventArg = NULL;
	void *ptn15EventArg = NULL;
	void *pdoseEventArg = NULL;

	FinishedProcessingCallbackFunc sigmaFinishedFunc = NULL;
	FinishedProcessingCallbackFunc tn15FinishedFunc = NULL;
	FinishedProcessingCallbackFunc doseFinishedFunc = NULL;
	void *pSigmaFinishedArg = NULL;
	void *ptn15FinishedArg = NULL;
	void *pdoseFinishedArg = NULL;
	int64_t timestamp = 0;

	_spectrumQueryEvent.Signal();

	// Grab a local copy of the event functions (We dont want the critical section locked during the call to the actual event functions)
	{
		kmk::Lock lock(_criticalSection);

		// The first data packet in any acquisition represents the total time since the very last request which is of no use to the 
		// new acquisition so ignore it
		if (_ignoreFirstSpectrumDataPacket)
		{
			_ignoreFirstSpectrumDataPacket = false;

			// Store the timestamp of the current time as a reference point
			_startAcquisitionTimestamp = Time::GetTime();
			return;
		}

		// Calculate the timestamp for this data
		_accumilatedRealTimeMs += pMessage->realTimeMS;

		timestamp = _startAcquisitionTimestamp + MS_TO_TICKS(_accumilatedRealTimeMs);

		int64_t currentTime = Time::GetTime();

		// The timestamp should never exceed the current time on the system clock, if it does then we are seeing a drift in time
		// between the device and host machine. Correct it at this point
		if (timestamp > currentTime)
		{
			_accumilatedRealTimeMs = kmk::Time::TicksToMs(currentTime - _startAcquisitionTimestamp);
			timestamp = currentTime;
		}

		// If the components are enabled then set the callbacks
		if (_gammaComponent.status != TS_STOP)
		{
			if ((_gammaComponent.status == TS_RUNNING && _gammaComponent.startStopTimestamp <= timestamp) || _gammaComponent.startStopTimestamp >= timestamp)
			{
				sigmaEventFunc = _gammaComponent.countEventCallback;
				pSigmaEventArg = _gammaComponent.countEventCallbackArg;
				_gammaComponent.accumilatedRealTimeMs += pMessage->realTimeMS;
				_gammaComponent.SetProperty(CP_Temperature, pMessage->gammaTemperature / 100.0f);
				_gammaComponent.SetProperty(CP_LiveTime, _gammaComponent.GetProperty(CP_LiveTime) + ( pMessage->gammaLiveTime / 100.0f));
			}
			else if (_gammaComponent.status == TS_FINISH)
			{	// The component is waiting to finish and the latest message is beyond the stop time so raise the event
				_gammaComponent.status = TS_STOP;
				sigmaFinishedFunc = _gammaComponent.finishedCallback;
				pSigmaFinishedArg = _gammaComponent.finishedCallbackArg;
			}
		}

		if (_neutronComponent.status != TS_STOP)
		{
			if ((_neutronComponent.status == TS_RUNNING && _neutronComponent.startStopTimestamp <= timestamp) || _neutronComponent.startStopTimestamp >= timestamp)
			{
				tn15EventFunc = _neutronComponent.countEventCallback;
				ptn15EventArg = _neutronComponent.countEventCallbackArg;
				_neutronComponent.accumilatedRealTimeMs += pMessage->realTimeMS;
				_neutronComponent.SetProperty(CP_Temperature, pMessage->neutronTemperature / 100.0f);
				_neutronComponent.SetProperty(CP_LiveTime, _neutronComponent.GetProperty(CP_LiveTime) + (pMessage->neutronLiveTime / 100.0f));
			}
			else if (_neutronComponent.status == TS_FINISH)
			{	// The component is waiting to finish and the latest message is beyond the stop time so raise the event
				_neutronComponent.status = TS_STOP;
				tn15FinishedFunc = _neutronComponent.finishedCallback;
				ptn15FinishedArg = _neutronComponent.finishedCallbackArg;
			}
		}

		if (_doseComponent.status != TS_STOP)
		{
			if ((_doseComponent.status == TS_RUNNING && _doseComponent.startStopTimestamp <= timestamp) || _doseComponent.startStopTimestamp >= timestamp)
			{
				doseEventFunc = _doseComponent.doseEventCallback;
				pdoseEventArg = _doseComponent.doseEventCallbackArg;
				_doseComponent.accumilatedRealTimeMs += pMessage->realTimeMS;
			}
			else if (_doseComponent.status == TS_FINISH)
			{	// The component is waiting to finish and the latest message is beyond the stop time so raise the event
				_doseComponent.status = TS_STOP;
				doseFinishedFunc = _doseComponent.finishedCallback;
				pdoseFinishedArg = _doseComponent.finishedCallbackArg;
			}
		}
	}

	// Gamma spectrum / SIGMA
	// Raise an event for each channel containing counts
	if (sigmaEventFunc != NULL)
	{
		for (int i = 0; i < D3Spectrum16ResponseHeader::SPECTRUM_SIZE; ++i)
		{
			//assert(pMessage->gammaSpectrum[i] < 1000);
			if (pMessage->gammaSpectrum[i] > 0)
				(*sigmaEventFunc)(pSigmaEventArg, timestamp, i, pMessage->gammaSpectrum[i]);
		}
	}
	else if (sigmaFinishedFunc != NULL)
	{
		(*sigmaFinishedFunc)(pSigmaFinishedArg, false);
	}

	// Neutron / TN15
	if (tn15EventFunc != NULL)
	{
		if (pMessage->neutronCounts > 0)
			(*tn15EventFunc)(ptn15EventArg, timestamp, 0, pMessage->neutronCounts);
	}
	else if (tn15FinishedFunc != NULL)
	{
		(*tn15FinishedFunc)(ptn15FinishedArg, false);
	}
	
	// Dose
	if (doseEventFunc != NULL)
	{
		(*doseEventFunc)(pdoseEventArg, timestamp, pMessage->dose * 1000000.0f, pMessage->doseRate * 1000000.0f, 0/*pMessage->doseReserved*/);
	}
	else if (doseFinishedFunc != NULL)
	{
		(*doseFinishedFunc)(pdoseFinishedArg, false);
	}
}

void D3DataProcessor::ProcessConfigurationReport(MessageHeader *pMessageHeader)
{
	// A configuration report response has been received. If we are still waiting
	// for the response then store the data and notify the original calling thread
	Lock lock(_criticalSection);
	if (_configurationQueryState != CQS_WAITING)
	{
		// We are not waiting for a response (probably timed out). Just ignore this report
		return;
	}

	// Copy the full packet into the result buffer
	if (_configurationQueryResultData.size() < pMessageHeader->messageSize)
		_configurationQueryResultData.resize(pMessageHeader->messageSize);

    std::memcpy(&_configurationQueryResultData[0], pMessageHeader, pMessageHeader->messageSize);
	_configurationQueryState = CQS_SUCCESS;
	
	// Signal the waiting thread that data is ready
	_configurationQueryEvent.Signal();
}

bool D3DataProcessor::GetConfigurationData(uint8_t componentId, uint16_t configurationIds, BYTE *pDataOut, size_t &dataLength)
{
	DSC_LOG(std::hex << "D3DataProcessor::GETCD Cmp 0x" << (int)componentId << " Cnf 0x" << (int)configurationIds << " Size 0x" << dataLength << " VID 0x" << _pDataInterface->GetVendorID() << " PID 0x" << _pDataInterface->GetProductID() << " #" << _pDataInterface->GetHash())

	uint8_t configurationId = configurationIds & 0xFF;
	uint8_t requestComponentId = componentId;
	// For D3 force some commands to if board
	if ((configurationIds & REPORT_MASK_USE_PARENT) || configurationId == REPORT_ID_GET_STATUS || configurationId == REPORT_ID_GET_DEVICE_INFO || configurationId == REPORT_ID_GET_SERIAL_NO)
		requestComponentId = InterfaceBoardComponentId;
	
	// There isnt really a dose component in the hardware, pass the request onto the gamma as that is the closest we have
	if (requestComponentId == DoseComponentId)
		requestComponentId = GammaComponentId;		

	// On some devices the gamma and neutron are actually the same component
	if (_neutronIsGamma && requestComponentId == NeutronComponentId)
		requestComponentId = GammaComponentId;

	std::vector<BYTE> requestBuffer;
	requestBuffer.resize(sizeof(D3GetConfiguration));
	D3GetConfiguration *pRequest = reinterpret_cast<D3GetConfiguration*>(&requestBuffer[0]);
	pRequest->m_message.messageSize = sizeof(D3GetConfiguration);
	pRequest->m_message.contentHeader.componentID = requestComponentId;
	pRequest->m_message.contentHeader.reportID = configurationId;
	std::vector<BYTE> preparedRequest;
	_ptrPacketBuffer->PrepareForSend(requestBuffer, preparedRequest);

	{
		kmk::Lock lock(_criticalSection);

		// Getting a configuration requires writing a report to the device and waiting for a response
		if (_configurationQueryState != CQS_IDLE)
			return false;

		_configurationQueryEvent.Reset();
		_configurationQueryState = CQS_WAITING;
	}
	
	// Make sure the data processor is running to process the configuration response
	StartProcessing(ConfigurationComponentId);

	// Send the request
	if (!_pDataInterface->GetConfigurationSetting(&preparedRequest[0], preparedRequest.size()))
	{
		{
			kmk::Lock lock(_criticalSection);
			_configurationQueryState = CQS_IDLE;
		}

		StopProcessing(ConfigurationComponentId, true);
		return false;
	}

	// Wait for a response
	bool result = _configurationQueryEvent.Wait(CONFIGURATION_QUERY_TIMEOUT);
	{
		DSC_LOG(std::hex << "D3DataProcessor::GETCD Wait Complete");
		{
			kmk::Lock lock(_criticalSection);
			result &= _configurationQueryState == CQS_SUCCESS;
			_configurationQueryState = CQS_IDLE;
		}

		// No longer waiting for configuration response, close the connection if its not being used
		StopProcessing(ConfigurationComponentId, true);
		
		if (result)
		{
			kmk::Lock lock(_criticalSection);

			// Grab the data out of the packet and copy into the output buffer
			MessageHeader *pHeader = (MessageHeader*)&_configurationQueryResultData[0];
			assert(pHeader->contentHeader.componentID == requestComponentId);

			// Calculate the size of the data by removing the size of the header and crc
			size_t sizeOfData = pHeader->messageSize - sizeof(MessageHeader) - sizeof(uint16_t);

			// Success, copy the result back into the buffer
			if (sizeOfData > dataLength)
			{
				result = false; // Not enough space in the buffer!
				dataLength = 0;
			}
			else
			{
				DSC_LOG(std::hex << "D3DataProcessor::GETCD Got " << dataLength << " Bytes" );
				size_t stringLength = strlen((char*)&_configurationQueryResultData[sizeof(MessageHeader)]);
                std::memcpy(pDataOut, &_configurationQueryResultData[sizeof(MessageHeader)], sizeOfData);
				dataLength = sizeOfData;

				if (componentId != InterfaceBoardComponentId && configurationId == REPORT_ID_GET_SERIAL_NO)
				{
					// Append a letter to the serial number for the component
					switch (componentId)
					{
					case NeutronComponentId:
						pDataOut[stringLength] = 'N';
						++dataLength;
						break;
					case GammaComponentId:
						pDataOut[stringLength] = 'G';
						++dataLength;
						break;

					case DoseComponentId:
						pDataOut[stringLength] = 'D';
						++dataLength;
						break;
					}
				}
			}
		}
		else
		{
			dataLength = 0;
			DSC_LOG(std::hex << "D3DataProcessor::GETCD Wait Timed Out")
		}
	}
	return result;
}

bool D3DataProcessor::SetConfigurationData(uint8_t componentId, uint16_t configurationIds, BYTE *pDataIn, size_t dataLength)
{
	const int fullHeaderSize = sizeof(MessageHeader) + sizeof(uint16_t); // Size of header + crc
	
	uint8_t configurationId = configurationIds & 0xFF;

	// For D3 force some commands to if board
	if ((configurationIds & REPORT_MASK_USE_PARENT) || configurationId == REPORT_ID_SET_DFU || configurationId == REPORT_ID_SET_SERIAL_NO || configurationId == REPORT_ID_SET_FACTORYSETUP)
		componentId = InterfaceBoardComponentId;

	// Create the header and insert the data
	std::vector<BYTE> buffer;
	buffer.resize(fullHeaderSize + dataLength);

	MessageHeader *pHeader = (MessageHeader*)&buffer[0];
	pHeader->contentHeader.componentID = componentId;
	pHeader->contentHeader.reportID = configurationId;
    pHeader->messageSize = (uint16_t)buffer.size();
    std::memcpy(&buffer[sizeof(MessageHeader)], pDataIn, dataLength);

	// Send the configuration to the device
	std::vector<BYTE> preparedBuffer;
	_ptrPacketBuffer->PrepareForSend(buffer, preparedBuffer);
	return _pDataInterface->SetConfigurationSetting(&preparedBuffer[0], preparedBuffer.size());
}

void D3DataProcessor::SendSpectrumRequest()
{
	// Only send requests if we need them, connections serving only configuration requests need not apply
	bool configOnly = (_neutronComponent.status == TS_STOP && _gammaComponent.status == TS_STOP && _doseComponent.status == TS_STOP);
	if (!configOnly)
	{
		int reportID = 0;

		// If we dont know the report type supported on this device already..#
		switch (_reportType)
		{
		case SRT_UNKNOWN:
		{
			// First request should be a Radiometrics V1.
			reportID = REPORT_ID_GET_RADIOMETRICSV1_SPECTRUM;
			_reportType = SRT_DETERMINING;
		}
		break;

		case SRT_DETERMINING:
		{
			// If we have already sent a request then check if we had a valid response(an unrecognised request will have no response)
			if (_spectrumQueryEvent.Wait(0))
			{
				_reportType = SRT_RADIOMETRICS_V1;
				reportID = REPORT_ID_GET_RADIOMETRICSV1_SPECTRUM;
			}
			else
			{
				// Assume unsupported
				_reportType = SRT_16BITSPECTRUM;
				reportID = REPORT_ID_GET_16BIT_SPECTRUM;
			}
		}
		break;

		case SRT_RADIOMETRICS_V1:
			reportID = REPORT_ID_GET_RADIOMETRICSV1_SPECTRUM;
			break;

		case SRT_16BITSPECTRUM:
		default:
			reportID = REPORT_ID_GET_16BIT_SPECTRUM;
			break;
		}

		_spectrumQueryEvent.Reset();

		std::vector<BYTE> requestBuffer;
		requestBuffer.resize(sizeof(D3BasicRequestHeader));
		D3BasicRequestHeader *pRequest = reinterpret_cast<D3BasicRequestHeader*>(&requestBuffer[0]);
		pRequest->componentID = InterfaceBoardComponentId;
		pRequest->reportID = reportID;
		pRequest->messageSize = sizeof(D3BasicRequestHeader);
		std::vector<BYTE> preparedBuffer;
		_ptrPacketBuffer->PrepareForSend(requestBuffer, preparedBuffer);
		_pDataInterface->SetConfigurationSetting(&preparedBuffer[0], preparedBuffer.size());
	}
}

void D3DataProcessor::EnableCompression(bool enabled)
{
	std::vector<BYTE> requestBuffer;
	requestBuffer.resize(sizeof(D3CompressionRequest));
	D3CompressionRequest* pRequest = (D3CompressionRequest*)&requestBuffer[0];
	pRequest->m_message.messageSize = sizeof(D3CompressionRequest);
	pRequest->m_message.mode = 0;
	pRequest->m_message.contentHeader.componentID = InterfaceBoardComponentId;
	pRequest->m_message.contentHeader.reportID = REPORT_ID_SET_COMPRESSION;
	pRequest->m_direction = 0;
	pRequest->m_lookAheadSize = D3CompressionRequest::HS_LOOKAHEAD_SIZE_DEFAULT;
	pRequest->m_windowSize = D3CompressionRequest::HS_WINDOW_SIZE_DEFAULT;
	pRequest->m_enabled = enabled ? 1 : 0;

	std::vector<BYTE> preparedBuffer;
	_ptrPacketBuffer->PrepareForSend(requestBuffer, preparedBuffer);
	_pDataInterface->SetConfigurationSetting(&preparedBuffer[0], preparedBuffer.size());
}

int D3DataProcessor::ProcessThreadProc(void *pArg)
{
	DSC_TRACE(L"D3DataProcessor::ProcessThreadProc");

	D3DataProcessor *pThis = (D3DataProcessor*)pArg;
	std::vector<BYTE> tempBuffer;
	tempBuffer.resize(MAX_REPORT_SIZE);
	size_t tempReportSize = 0;
	int64_t nextQueryTime = kmk::Time::GetTimeMs() + QUERY_SPECTRUM_RATE;

	bool forcedStop = true;

        // Disable Compression
        pThis->EnableCompression(false);

	ExecutionState keepRunning = ES_RUNNING;
	do
	{
		// Are we ready to query for a new spectrum?
		if (kmk::Time::GetTimeMs() >= nextQueryTime)
		{
			pThis->SendSpectrumRequest();

			pThis->_lastSpectrumRequestTime = kmk::Time::GetTimeMs();
			nextQueryTime = pThis->_lastSpectrumRequestTime + QUERY_SPECTRUM_RATE;
		}

		// If something is in the queue then process it, otherwise wait for data		
		if (pThis->GetNextReport(tempBuffer, tempReportSize))
		{
			// Process
            pThis->ProcessReport(&tempBuffer[0]);
		}
		else
		{
			// No more data, check if we have been asked to finish once all data is processed
			{
				kmk::Lock lock(pThis->_criticalSection);
				if (pThis->_currentState == ES_FINISHING)
				{
					// Mark as finished and break from the loop
					forcedStop = false;
					break;
				}

				pThis->_waitEvent.Reset();
			}
			
			// Wait for event signalling new data or the time to query the next spectrum data
            uint32_t waitTime = (uint32_t)std::max<int64_t>(nextQueryTime - kmk::Time::GetTimeMs(), 1);
			pThis->_waitEvent.Wait(waitTime);
		}

		{
			kmk::Lock lock(pThis->_errorSection);

			// Process any errors
			for (ErrorList::iterator it = pThis->_pendingErrors.begin(); it != pThis->_pendingErrors.end(); ++it)
			{
				pThis->ExecuteError(it->errorCode, it->message);
			}

			pThis->_pendingErrors.clear();
		}

		// Check thread continue status
		{
			kmk::Lock lock(pThis->_criticalSection);
			keepRunning = pThis->_currentState;
		}
	} while (keepRunning == ES_RUNNING || keepRunning == ES_FINISHING);

	{
		// Raise finished callback (if not already raised)
		FinishedProcessingCallbackFunc gammaCallback = NULL;
		void *pGammaArg = NULL;
		FinishedProcessingCallbackFunc neutronCallback = NULL;
		void *pNeutronArg = NULL;
		FinishedProcessingCallbackFunc doseCallback = NULL;
		void *pDoseArg = NULL;

		{
			kmk::Lock lock(pThis->_eventSection);
			if (pThis->_gammaComponent.finishedCallback != NULL && pThis->_gammaComponent.status != TS_STOP)
			{
				pThis->_gammaComponent.status = TS_STOP;
				gammaCallback = pThis->_gammaComponent.finishedCallback;
				pGammaArg = pThis->_gammaComponent.finishedCallbackArg;
			}

			if (pThis->_neutronComponent.finishedCallback != NULL && pThis->_neutronComponent.status != TS_STOP)
			{
				pThis->_neutronComponent.status = TS_STOP;
				neutronCallback = pThis->_neutronComponent.finishedCallback;
				pNeutronArg = pThis->_neutronComponent.finishedCallbackArg;
			}

			if (pThis->_doseComponent.finishedCallback != NULL && pThis->_doseComponent.status != TS_STOP)
			{
				pThis->_doseComponent.status = TS_STOP;
				doseCallback = pThis->_doseComponent.finishedCallback;
				pDoseArg = pThis->_doseComponent.finishedCallbackArg;
			}
		}

		if (gammaCallback != NULL)
			(*gammaCallback)(pGammaArg, forcedStop);

		if (neutronCallback != NULL)
			(*neutronCallback)(pNeutronArg, forcedStop);

		if (doseCallback != NULL)
			(*doseCallback)(pDoseArg, forcedStop);
	}
	
	pThis->SetExecutionState(ES_IDLE);

	return 0;
}

// Callback raised everytime data is received from the data interface.
void D3DataProcessor::ReadDataCallbackProc(void *pArg, unsigned char *pData, size_t dataSize)
{
	D3DataProcessor *pThis = (D3DataProcessor*)pArg;

	// Pass data onto the data processor
	if (!pThis->_ptrPacketBuffer->AddIncomingData(pData, dataSize))
	{
		pThis->RaiseError(ERROR_INTERNAL_DEVICE, L"Failed to add data to buffer. Buffer is probably full");
	}

	pThis->_waitEvent.Signal();
}

void D3DataProcessor::DataInterfaceErrorCallbackProc(void *pArg, int errorCode, String message)
{
	D3DataProcessor *pThis = (D3DataProcessor*)pArg;
	pThis->RaiseError(errorCode, message);
}

// Execute the error callback routine. Do not call direct, Call Raise error instead
void D3DataProcessor::ExecuteError(int errorCode, String message)
{
	kmk::ErrorCallbackFunc	gammaCallback = NULL, 
							neutronCallback = NULL,
							doseCallback = NULL;

	{

		kmk::Lock lock(_eventSection);

		// Raise an error accross each component if its active
		if (_gammaComponent.status != TS_STOP)
		{
			if (_gammaComponent.errorCallback != NULL)
			{
				gammaCallback = _gammaComponent.errorCallback;
			}
		}

		if (_neutronComponent.status != TS_STOP)
		{
			if (_neutronComponent.errorCallback != NULL)
			{
				neutronCallback = _neutronComponent.errorCallback;
			}
		}

		if (_doseComponent.status != TS_STOP)
		{
			if (_doseComponent.errorCallback != NULL)
			{
				doseCallback = _doseComponent.errorCallback;
			}
		}
	}

	if (gammaCallback)
		(*gammaCallback)(_gammaComponent.errorCallbackArg, errorCode, message);

	if (neutronCallback)
		(*neutronCallback)(_neutronComponent.errorCallbackArg, errorCode, message);

	if (doseCallback)
		(*doseCallback)(_doseComponent.errorCallbackArg, errorCode, message);
	
}

// Queue to be processed on the data processor thread
void D3DataProcessor::RaiseError(int errorCode, String message)
{
	kmk::Lock lock(_errorSection);

	ErrorMessageDesc desc;
	desc.errorCode = errorCode;
	desc.message = message;
	_pendingErrors.push_back(desc);
}

}
