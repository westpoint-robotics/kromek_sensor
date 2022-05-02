#include "stdafx.h"

#include "K102.h"
#include "IntervalCountProcessor.h"

#define EVENT_DEADTIME 1.0e-5
#define DEFAULT_LLD 32
#define DEFAULT_GAIN			0
#define DEFAULT_DIFF_GAIN		240
#define DEFAULT_HIGH_VOLTAGE	0
#define DEFAULT_WARMUP_TIME		60
#define DEFAULT_POLARITY		POSITIVE
#define MIN_LLD					5

namespace kmk
{

K102::K102(IDataInterface *pInterface)
: DeviceBase(pInterface, new IntervalCountProcessor(pInterface))
{
}

K102::~K102()
{
}

/*static*/ void K102::GetDetectorProperties(DetectorProperties &propsOut)
{
	propsOut.detectorType = DT_Gamma;
	propsOut.materialType = MT_UNKNOWN;
	propsOut.materialWidth = 0.0;
	propsOut.materialHeight = 0.0;
	propsOut.materialDepth = 0.0;

	propsOut.defaultDeadTime = EVENT_DEADTIME;
	propsOut.defaultGain = DEFAULT_GAIN;
	propsOut.defaultDiffGain = DEFAULT_DIFF_GAIN;
	propsOut.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
	propsOut.defaultLLD = DEFAULT_LLD;
	propsOut.defaultPolarity = DEFAULT_POLARITY;
	propsOut.minLLD = MIN_LLD;
}

}
