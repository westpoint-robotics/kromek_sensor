#include "stdafx.h"
#include "SIGMA_50.h"
#include "IntervalCountProcessor.h"

#define EVENT_DEADTIME 5.813E-05
#define DEFAULT_LLD 80
#define DEFAULT_GAIN			0
#define DEFAULT_DIFF_GAIN		240
#define DEFAULT_HIGH_VOLTAGE	0
#define DEFAULT_WARMUP_TIME		60
#define MIN_LLD					5

namespace kmk
{

SIGMA_50::SIGMA_50(IDataInterface *pInterface)
: DeviceBase(pInterface, new IntervalCountProcessor(pInterface))
{
}

SIGMA_50::~SIGMA_50(void)
{
}

/*static*/ void SIGMA_50::GetDetectorProperties(DetectorProperties &props)
{
	props.detectorType = DT_Gamma;
	props.materialType = MT_CSI;
	props.materialWidth = 2.54;
	props.materialHeight = 2.54;
	props.materialDepth = 5.1;

	props.defaultDeadTime = EVENT_DEADTIME;
	props.defaultGain = DEFAULT_GAIN;
	props.defaultDiffGain = DEFAULT_DIFF_GAIN;
	props.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
	props.defaultLLD = DEFAULT_LLD;
	props.defaultPolarity = NOT_USED;
	props.minLLD = MIN_LLD;
}

}
