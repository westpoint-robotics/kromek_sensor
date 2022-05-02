#include "stdafx.h"
#include "GR05.h"
#include "IntervalCountProcessor.h"

#define EVENT_DEADTIME			1.0e-5
#define DEFAULT_LLD				32
#define DEFAULT_GAIN			0
#define DEFAULT_DIFF_GAIN		240
#define DEFAULT_HIGH_VOLTAGE	0
#define DEFAULT_WARMUP_TIME		60
#define MIN_LLD					5

namespace kmk
{

GR05::GR05(IDataInterface *pInterface)
: DeviceBase(pInterface, new IntervalCountProcessor(pInterface))
{
	
}

GR05::~GR05()
{
}

/*static */void GR05::GetDetectorProperties(DetectorProperties &propsOut)
{
	propsOut.detectorType = DT_Gamma;
	propsOut.materialType = MT_CZT;
	propsOut.materialWidth = 0.5;
	propsOut.materialHeight = 0.5;
	propsOut.materialDepth = 0.5;

	propsOut.defaultDeadTime = EVENT_DEADTIME;
	propsOut.defaultGain = DEFAULT_GAIN;
	propsOut.defaultDiffGain = DEFAULT_DIFF_GAIN;
	propsOut.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
	propsOut.defaultLLD = DEFAULT_LLD;
	propsOut.defaultPolarity = NOT_USED;
	propsOut.minLLD = MIN_LLD;
}

}
