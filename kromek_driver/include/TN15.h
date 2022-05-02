#pragma once

#include "IDataInterface.h"
#include "DeviceBase.h"

namespace kmk
{

class TN15 : public DeviceBase
{
public:
	static const VID VendorId = 0x4d8;
	static const PID ProductId = 0x30;
	
	TN15(IDataInterface *pInterface);
	virtual ~TN15(void);

	virtual String GetProductName() const { return L"TN15"; }
	virtual VID GetVendorID() const {return VendorId; }
	virtual PID GetProductID() const {return ProductId; }
	String GetManufacturer() const { return L"Kromek"; }

	DetectorType GetDetectorType() const { return DT_Neutron; }
	
	static void GetDetectorProperties(DetectorProperties &props);
};

// Multi component version of the TN15 (D3)
class TN15_D3 : public DeviceBase
{
public:
	static const VID VendorId = 0x4d8;
	static const PID ProductId = 0x31;
	
	TN15_D3(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex);
	virtual ~TN15_D3(void);

	virtual String GetProductName() const { return L"D3 Neutron"; }
	virtual VID GetVendorID() const {return VendorId; }
	virtual PID GetProductID() const {return ProductId; }
	String GetManufacturer() const { return L"Kromek"; }

	DetectorType GetDetectorType() const { return DT_Neutron; }

	static void GetDetectorProperties(DetectorProperties &props);
};

// Multi component version of the TN15 (D3S)
class TN15_D3S : public DeviceBase
{
public:
	static const VID VendorId = 0x4d8;
	static const PID ProductId = 0x32;

	TN15_D3S(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex);
	virtual ~TN15_D3S(void);

	virtual String GetProductName() const { return L"D3S Neutron"; }
	virtual VID GetVendorID() const { return VendorId; }
	virtual PID GetProductID() const { return ProductId; }
	String GetManufacturer() const { return L"Kromek"; }

	DetectorType GetDetectorType() const { return DT_Neutron; }

	static void GetDetectorProperties(DetectorProperties &props);
};

// Multi component version of the TN15 (D3S)
class TN15_D3M : public DeviceBase
{
public:
	static const VID VendorId = 0x4d8;
	static const PID ProductId = 0x33;

	TN15_D3M(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex);
	virtual ~TN15_D3M(void);

	virtual String GetProductName() const { return L"D3M Neutron"; }
	virtual VID GetVendorID() const { return VendorId; }
	virtual PID GetProductID() const { return ProductId; }
	String GetManufacturer() const { return L"Kromek"; }

	DetectorType GetDetectorType() const { return DT_Neutron; }

	static void GetDetectorProperties(DetectorProperties &props);
};


// Multi component version of the TN15 (D3S)
class TN15_D4 : public DeviceBase
{
public:
	static const VID VendorId = 0x4d8;
	static const PID ProductId = 0x34;

	TN15_D4(IDataInterface* pInterface, IDataProcessor* pProcessor, unsigned char componentIndex);
	virtual ~TN15_D4(void);

	virtual String GetProductName() const { return L"D4 Neutron"; }
	virtual VID GetVendorID() const { return VendorId; }
	virtual PID GetProductID() const { return ProductId; }
	String GetManufacturer() const { return L"Kromek"; }

	DetectorType GetDetectorType() const { return DT_Neutron; }

	static void GetDetectorProperties(DetectorProperties& props);
};

// Multi component version of the TN15 (D5 RIID)
class TN15_D5RIID : public DeviceBase
{
public:
	static const VID VendorId = 0x4d8;
	static const PID ProductId = 0x35;

	TN15_D5RIID(IDataInterface* pInterface, IDataProcessor* pProcessor, unsigned char componentIndex);
	virtual ~TN15_D5RIID(void);

	virtual String GetProductName() const { return L"D5 RIID Neutron"; }
	virtual VID GetVendorID() const { return VendorId; }
	virtual PID GetProductID() const { return ProductId; }
	String GetManufacturer() const { return L"Kromek"; }

	DetectorType GetDetectorType() const { return DT_Neutron; }

	static void GetDetectorProperties(DetectorProperties& props);
};



}
