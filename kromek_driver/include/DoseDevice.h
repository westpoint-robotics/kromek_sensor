#pragma once

#include "DeviceBase.h"

namespace kmk
{
	// High rate dose detector in D3S
	class HighDose_D3S : public DeviceBase
	{
	public:
		static const VID VendorId = 0x4d8;
		static const PID ProductId = 0x51;

		HighDose_D3S(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex);
		virtual ~HighDose_D3S();

		virtual String GetProductName() const { return L"D3S Dose Monitor"; }
		virtual VID GetVendorID() const { return VendorId; }
		virtual PID GetProductID() const { return ProductId; }

		DetectorType GetDetectorType() const { return DT_Dose; }

		static void GetDetectorProperties(DetectorProperties &propsOut);

		String GetManufacturer() const { return L"Kromek"; }

	};

	// High rate dose detector in D3M
	class HighDose_D3M : public DeviceBase
	{
	public:
		static const VID VendorId = 0x4d8;
		static const PID ProductId = 0x52;

		HighDose_D3M(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex);
		virtual ~HighDose_D3M();

		virtual String GetProductName() const { return L"D3M Dose Monitor"; }
		virtual VID GetVendorID() const { return VendorId; }
		virtual PID GetProductID() const { return ProductId; }

		DetectorType GetDetectorType() const { return DT_Dose; }

		static void GetDetectorProperties(DetectorProperties &propsOut);

		String GetManufacturer() const { return L"Kromek"; }

	};

	// High rate dose detector in D3M
	class HighDose_D3PRD : public DeviceBase
	{
	public:
		static const VID VendorId = 0x4d8;
		static const PID ProductId = 0x55;

		HighDose_D3PRD(IDataInterface* pInterface, IDataProcessor* pProcessor, unsigned char componentIndex);
		virtual ~HighDose_D3PRD();

		virtual String GetProductName() const { return L"D3 PRD Dose Monitor"; }
		virtual VID GetVendorID() const { return VendorId; }
		virtual PID GetProductID() const { return ProductId; }

		DetectorType GetDetectorType() const { return DT_Dose; }

		static void GetDetectorProperties(DetectorProperties& propsOut);

		String GetManufacturer() const { return L"Kromek"; }

	};

	// High rate dose detector in D3M
	class HighDose_D4 : public DeviceBase
	{
	public:
		static const VID VendorId = 0x4d8;
		static const PID ProductId = 0x53;

		HighDose_D4(IDataInterface* pInterface, IDataProcessor* pProcessor, unsigned char componentIndex);
		virtual ~HighDose_D4();

		virtual String GetProductName() const { return L"D4 Dose Monitor"; }
		virtual VID GetVendorID() const { return VendorId; }
		virtual PID GetProductID() const { return ProductId; }

		DetectorType GetDetectorType() const { return DT_Dose; }

		static void GetDetectorProperties(DetectorProperties& propsOut);

		String GetManufacturer() const { return L"Kromek"; }

	};

	// High rate dose detector in D5 RIID
	class HighDose_D5RIID : public DeviceBase
	{
	public:
		static const VID VendorId = 0x4d8;
		static const PID ProductId = 0x54;

		HighDose_D5RIID(IDataInterface* pInterface, IDataProcessor* pProcessor, unsigned char componentIndex);
		virtual ~HighDose_D5RIID();

		virtual String GetProductName() const { return L"D5 RIID Dose Monitor"; }
		virtual VID GetVendorID() const { return VendorId; }
		virtual PID GetProductID() const { return ProductId; }

		DetectorType GetDetectorType() const { return DT_Dose; }

		static void GetDetectorProperties(DetectorProperties& propsOut);

		String GetManufacturer() const { return L"Kromek"; }

	};

}