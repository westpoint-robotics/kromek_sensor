#pragma once


#include "DeviceBase.h"



namespace kmk
{

class K102 : public DeviceBase
{
public:

	static const VID VendorId = 0x4d8;
	static const PID ProductId = 0x011;

	K102(IDataInterface *pInterface);
	virtual ~K102();

	virtual String GetProductName() const { return L"K102"; }
	virtual VID GetVendorID() const {return VendorId; }
	virtual PID GetProductID() const {return ProductId; }
	String GetManufacturer() const { return L"Kromek"; }

	DetectorType GetDetectorType() const { return DT_Gamma; }
	
	static void GetDetectorProperties(DetectorProperties &props);
};

}