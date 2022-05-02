#include "stdafx.h"
#include "GR1.h"
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

GR1::GR1(IDataInterface *pInterface)
: DeviceBase(pInterface, new IntervalCountProcessor(pInterface))
{
	
}

GR1::~GR1()
{
}

/*static */void GR1::GetDetectorProperties(DetectorProperties &propsOut)
{
	propsOut.detectorType = DT_Gamma;
	propsOut.materialType = MT_CZT;
	propsOut.materialWidth = 1.0;
	propsOut.materialHeight = 1.0;
	propsOut.materialDepth = 1.0;

	propsOut.defaultDeadTime = EVENT_DEADTIME;
	propsOut.defaultGain = DEFAULT_GAIN;
	propsOut.defaultDiffGain = DEFAULT_DIFF_GAIN;
	propsOut.defaultHighVoltage = DEFAULT_HIGH_VOLTAGE;
	propsOut.defaultLLD = DEFAULT_LLD;
	propsOut.defaultPolarity = NOT_USED;
	propsOut.minLLD = MIN_LLD;
}

GR1A::GR1A(IDataInterface *pInterface)
: GR1(pInterface)
{
}

GR1A::~GR1A()
{
}

}
