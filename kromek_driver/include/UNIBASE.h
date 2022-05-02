#pragma once

#include "DeviceBase.h"

namespace kmk
{
	// Unibase PMT
	class UNIBASE_PMT : public DeviceBase
	{
	public:
		static const VID VendorId = KROMEK_VENDOR_ID;
		static const PID ProductId = 0x0041;
		
		UNIBASE_PMT(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex);
		virtual ~UNIBASE_PMT();

		virtual String GetProductName() const { return L"UNIBASE (PMT)"; }
		virtual VID GetVendorID() const { return VendorId; }
		virtual PID GetProductID() const { return ProductId; }
		String GetManufacturer() const { return L"Kromek"; }

		DetectorType GetDetectorType() const { return DT_Gamma; }

		static void GetDetectorProperties(DetectorProperties &props);
	};

	// Unibase SiPM
	class UNIBASE_SiPM : public DeviceBase
	{
	public:
		static const VID VendorId = KROMEK_VENDOR_ID;
		static const PID ProductId = 0x0042;
		
		UNIBASE_SiPM(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex);
		virtual ~UNIBASE_SiPM();

		virtual String GetProductName() const { return L"UNIBASE (SiPM)"; }
		virtual VID GetVendorID() const { return VendorId; }
		virtual PID GetProductID() const { return ProductId; }
		String GetManufacturer() const { return L"Kromek"; }

		DetectorType GetDetectorType() const { return DT_Gamma; }

		static void GetDetectorProperties(DetectorProperties &props);
	};

	// Unibase ASIC
	/*class UNIBASE_ASIC : public DeviceBase
	{
	public:
		static const VID VendorId = KROMEK_VENDOR_ID;
		static const PID ProductId = 0x0044;

		UNIBASE_ASIC(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex);
		virtual ~UNIBASE_ASIC();

		virtual String GetProductName() const { return L"D3M"; }
		virtual VID GetVendorID() const { return VendorId; }
		virtual PID GetProductID() const { return ProductId; }
		String GetManufacturer() const { return L"Kromek"; }

		DetectorType GetDetectorType() const { return DT_Gamma; }

		static void GetDetectorProperties(DetectorProperties &props);
	};

	// Unibase ASIC BRINGUP Board
	class UNIBASE_ASIC_BRINGUP : public DeviceBase
	{
	public:
		static const VID VendorId = KROMEK_VENDOR_ID;
		static const PID ProductId = 0x0043;

		UNIBASE_ASIC_BRINGUP(IDataInterface *pInterface, IDataProcessor *pProcessor, unsigned char componentIndex);
		virtual ~UNIBASE_ASIC_BRINGUP();

		virtual String GetProductName() const { return L"D3M"; }
		virtual VID GetVendorID() const { return VendorId; }
		virtual PID GetProductID() const { return ProductId; }
		String GetManufacturer() const { return L"Kromek"; }

		DetectorType GetDetectorType() const { return DT_Gamma; }

		static void GetDetectorProperties(DetectorProperties &props);
	};*/
}
