#pragma once

#include "DeviceBase.h"

namespace kmk
{

class SIGMA_50 : public DeviceBase
{
public:
	static const VID VendorId = 0x4d8;
	static const PID ProductId = 0x023;

	SIGMA_50(IDataInterface *pInterface);
	virtual ~SIGMA_50();

	DetectorType GetDetectorType() const { return DT_Gamma; }

	virtual String GetProductName() const { return L"SIGMA 50"; }
	virtual VID GetVendorID() const {return VendorId; }
	virtual PID GetProductID() const {return ProductId; }
	String GetManufacturer() const { return L"Kromek"; }

	static void GetDetectorProperties(DetectorProperties &props);
};

}