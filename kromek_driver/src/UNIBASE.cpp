#include "stdafx.h"
#include "UNIBASE.h"
#include "IntervalCountProcessor.h"

#define EVENT_DEADTIME			2000E-09
#define DEFAULT_LLD				32
#define DEFAULT_GAIN			0
#define DEFAULT_DIFF_GAIN		240
#define DEFAULT_HIGH_VOLTAGE	0
#define MIN_LLD					20

namespace kmk
{

	UNIBASE_PMT::UNIBASE_PMT(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex)
		: DeviceBase(pInterface, pProcessor, componentIndex)
	{
	}

	UNIBASE_PMT::~UNIBASE_PMT()
	{
	}

	/*static*/ void UNIBASE_PMT::GetDetectorProperties(DetectorProperties &props)
	{
		props.detectorType = DT_Gamma;
		props.materialType = MT_UNKNOWN;
		props.materialWidth = 0;
		props.materialHeight = 0;
		props.materialDepth = 0;

		props.defaultDeadTime = EVENT_DEADTIME;
		props.defaultGain = DEFAULT_GAIN;
		props.defaultDiffGain = DEFAULT_DIFF_GAIN;
		props.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
		props.defaultLLD = DEFAULT_LLD;
		props.defaultPolarity = NOT_USED;
	}

	UNIBASE_SiPM::UNIBASE_SiPM(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex)
		: DeviceBase(pInterface, pProcessor, componentIndex)
	{
	}

	UNIBASE_SiPM::~UNIBASE_SiPM()
	{
	}

	/*static*/ void UNIBASE_SiPM::GetDetectorProperties(DetectorProperties &props)
	{
		props.detectorType = DT_Gamma;
		props.materialType = MT_UNKNOWN;
		props.materialWidth = 0;
		props.materialHeight = 0;
		props.materialDepth = 0;

		props.defaultDeadTime = EVENT_DEADTIME;
		props.defaultGain = DEFAULT_GAIN;
		props.defaultDiffGain = DEFAULT_DIFF_GAIN;
		props.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
		props.defaultLLD = DEFAULT_LLD;
		props.defaultPolarity = NOT_USED;
		props.minLLD = MIN_LLD;
	}
	/*
	UNIBASE_ASIC::UNIBASE_ASIC(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex)
		: DeviceBase(pInterface, pProcessor, componentIndex)
	{
	}

	UNIBASE_ASIC::~UNIBASE_ASIC()
	{
	}

	void UNIBASE_ASIC::GetDetectorProperties(DetectorProperties &props)
	{
		props.detectorType = DT_Gamma;
		props.materialType = MT_UNKNOWN;
		props.materialWidth = 0;
		props.materialHeight = 0;
		props.materialDepth = 0;

		props.defaultDeadTime = EVENT_DEADTIME;
		props.defaultGain = DEFAULT_GAIN;
		props.defaultDiffGain = DEFAULT_DIFF_GAIN;
		props.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
		props.defaultLLD = DEFAULT_LLD;
		props.defaultPolarity = NOT_USED;
		props.minLLD = MIN_LLD;
	}

	UNIBASE_ASIC_BRINGUP::UNIBASE_ASIC_BRINGUP(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex)
		: DeviceBase(pInterface, pProcessor, componentIndex)
	{
	}

	UNIBASE_ASIC_BRINGUP::~UNIBASE_ASIC_BRINGUP()
	{
	}

	void UNIBASE_ASIC_BRINGUP::GetDetectorProperties(DetectorProperties &props)
	{
		props.detectorType = DT_Gamma;
		props.materialType = MT_UNKNOWN;
		props.materialWidth = 0;
		props.materialHeight = 0;
		props.materialDepth = 0;

		props.defaultDeadTime = EVENT_DEADTIME;
		props.defaultGain = DEFAULT_GAIN;
		props.defaultDiffGain = DEFAULT_DIFF_GAIN;
		props.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
		props.defaultLLD = DEFAULT_LLD;
		props.defaultPolarity = NOT_USED;
		props.minLLD = MIN_LLD;
	}*/
}
