#include "stdafx.h"
#include "DoseDevice.h"

#define D3S_DEADTIME			0.00001428
#define DEFAULT_LLD				380
#define DEFAULT_GAIN			0
#define DEFAULT_DIFF_GAIN		240
#define DEFAULT_HIGH_VOLTAGE	0
#define DEFAULT_WARMUP_TIME		60
#define MIN_LLD					20

namespace kmk
{

	HighDose_D3S::HighDose_D3S(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex)
		: DeviceBase(pInterface, pProcessor, componentIndex)
	{

	}

	HighDose_D3S::~HighDose_D3S()
	{
	}

	/*static */void HighDose_D3S::GetDetectorProperties(DetectorProperties &props)
	{
		props.detectorType = DT_Dose;
		props.materialType = MT_CSI;
		props.materialWidth = 2.54;
		props.materialHeight = 2.54;
		props.materialDepth = 2.54;

		props.defaultDeadTime = D3S_DEADTIME;
		props.defaultGain = DEFAULT_GAIN;
		props.defaultDiffGain = DEFAULT_DIFF_GAIN;
		props.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
		props.defaultLLD = DEFAULT_LLD;
		props.defaultPolarity = NOT_USED;
		props.minLLD = MIN_LLD;
	}

	HighDose_D3M::HighDose_D3M(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex)
		: DeviceBase(pInterface, pProcessor, componentIndex)
	{

	}

	HighDose_D3M::~HighDose_D3M()
	{
	}

	/*static */void HighDose_D3M::GetDetectorProperties(DetectorProperties &props)
	{
		props.detectorType = DT_Dose;
		props.materialType = MT_CSI;
		props.materialWidth = 2.54;
		props.materialHeight = 2.54;
		props.materialDepth = 2.54;

		props.defaultDeadTime = D3S_DEADTIME;
		props.defaultGain = DEFAULT_GAIN;
		props.defaultDiffGain = DEFAULT_DIFF_GAIN;
		props.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
		props.defaultLLD = DEFAULT_LLD;
		props.defaultPolarity = NOT_USED;
	}

	HighDose_D3PRD::HighDose_D3PRD(IDataInterface* pInterface, IDataProcessor* pProcessor, unsigned char componentIndex)
		: DeviceBase(pInterface, pProcessor, componentIndex)
	{

	}

	HighDose_D3PRD::~HighDose_D3PRD()
	{
	}

	/*static */void HighDose_D3PRD::GetDetectorProperties(DetectorProperties& props)
	{
		props.detectorType = DT_Dose;
		props.materialType = MT_CSI;
		props.materialWidth = 2.54;
		props.materialHeight = 2.54;
		props.materialDepth = 2.54;

		props.defaultDeadTime = D3S_DEADTIME;
		props.defaultGain = DEFAULT_GAIN;
		props.defaultDiffGain = DEFAULT_DIFF_GAIN;
		props.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
		props.defaultLLD = DEFAULT_LLD;
		props.defaultPolarity = NOT_USED;
	}


	HighDose_D5RIID::HighDose_D5RIID(IDataInterface* pInterface, IDataProcessor* pProcessor, unsigned char componentIndex)
		: DeviceBase(pInterface, pProcessor, componentIndex)
	{

	}

	HighDose_D5RIID::~HighDose_D5RIID()
	{
	}

	/*static */void HighDose_D5RIID::GetDetectorProperties(DetectorProperties& props)
	{
		props.detectorType = DT_Dose;
		props.materialType = MT_CSI;
		props.materialWidth = 2.54;
		props.materialHeight = 2.54;
		props.materialDepth = 2.54;

		props.defaultDeadTime = D3S_DEADTIME;
		props.defaultGain = DEFAULT_GAIN;
		props.defaultDiffGain = DEFAULT_DIFF_GAIN;
		props.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
		props.defaultLLD = DEFAULT_LLD;
		props.defaultPolarity = NOT_USED;
	}

	HighDose_D4::HighDose_D4(IDataInterface* pInterface, IDataProcessor* pProcessor, unsigned char componentIndex)
		: DeviceBase(pInterface, pProcessor, componentIndex)
	{

	}

	HighDose_D4::~HighDose_D4()
	{
	}

	/*static */void HighDose_D4::GetDetectorProperties(DetectorProperties& props)
	{
		props.detectorType = DT_Dose;
		props.materialType = MT_CSI;
		props.materialWidth = 2.54;
		props.materialHeight = 2.54;
		props.materialDepth = 2.54;

		props.defaultDeadTime = D3S_DEADTIME;
		props.defaultGain = DEFAULT_GAIN;
		props.defaultDiffGain = DEFAULT_DIFF_GAIN;
		props.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
		props.defaultLLD = DEFAULT_LLD;
		props.defaultPolarity = NOT_USED;
	}
}
