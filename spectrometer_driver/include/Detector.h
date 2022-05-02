#pragma once

#include "SpectrometerData.h"
#include "Lock.h"
#include <vector>

#include "IDevice.h"

struct VersionInformation
{
	unsigned int productId;
	unsigned int major;
	unsigned int minor;
	unsigned int build;
};

class Detector;
typedef void (*DataReceivedCallbackFunc)(Detector *pDetector, int64_t timestamp, int channel, uint32_t numCounts, void *pArg);

class Detector
{
private:
	Detector(const Detector &rhs); // Private as it should not be called!
protected:

	kmk::IDevice *m_pDevice;
	bool m_acquiringData;

	int64_t m_clearedTime;			// Time the data was last cleared by GetAcquiredData
	unsigned int m_targetRealTime;	// Real time to run before ending acquisition
	unsigned int m_targetLiveTime;	// Live time to run before ending acquisition
    int64_t m_accumilatedRealTime; // Realtime of any previous acquisitions that contributed to the acquired data set
    unsigned int m_totalCounts;		// Total counts (used to calc live time)
	
	// Default properties associated with the detector hardware
	kmk::DetectorProperties m_detectorProperties;

	kmk::CriticalSection m_dataCS;
	std::vector<unsigned int> m_pData;			// Counts received for the last acquisition period
	
	DataReceivedCallbackFunc m_dataReceivedCallbackFunc;
	void *m_dataReceivedCallbackArg;
	
	double CalculateLiveTime(double realTimeMs, unsigned int totalCounts) const;

	bool SendLLDConfigurationCommand(int channelLLD);

	static void OnDataRecievedProc(kmk::IDevice *pDetector, int64_t timestamp, int channel, uint32_t numCounts, void *pThis);
public:

	Detector(kmk::IDevice *pDevice, DataReceivedCallbackFunc dataReceivedCallback, void *pCallbackArg, const kmk::DetectorProperties &detectorProps);
	
	virtual ~Detector();

	// Properties
	const std::wstring GetDeviceName() const {return m_pDevice->GetProductName();}
	const std::wstring GetDeviceManufacturer() const {return m_pDevice->GetManufacturer();}
	const std::wstring GetDeviceSerial() const {return m_pDevice->GetSerialNumber();}
	unsigned short GetDeviceVendorID() const {return m_pDevice->GetVendorID();}
	unsigned short GetDeviceProductID() const {return m_pDevice->GetProductID();}

	unsigned int Hash() const {return m_pDevice->GetHash();}

    virtual bool BeginDataAcquisition(unsigned int realTime, unsigned int liveTime);
	virtual void EndDataAcquisition();

    void ClearAcquiredData();
    bool GetAcquiredData(unsigned int *pBuffer, unsigned int *pTotalCounts, unsigned int *pRealTime, unsigned int *pLiveTime, unsigned int flags = 0);
	bool IsAcquiringData() const {return m_acquiringData;}

	bool SendInt16ConfigurationCommand(kmk::ConfigurationID configurationID, unsigned short command);
	bool SendInt8ConfigurationCommand(kmk::ConfigurationID configurationID, unsigned char command);

	// Update the device state - called once every few milliseconds
	void Update();
};
