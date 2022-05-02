#pragma once

#include "IDataInterface.h"
#include "DeviceBase.h"



namespace kmk
{

class RadAngel : public DeviceBase
{
public:

	static const VID VendorId = 0x4d8;
	static const PID ProductId = 0x100;

	RadAngel(IDataInterface *pInterface);
	virtual ~RadAngel();

	virtual String GetProductName() const { return L"RadAngel"; }
	virtual VID GetVendorID() const {return VendorId; }
	virtual PID GetProductID() const {return ProductId; }
	String GetManufacturer() const { return L"Kromek"; }

	DetectorType GetDetectorType() const { return DT_Gamma; }

	static void GetDetectorProperties(DetectorProperties &props);
};

}
