#include "stdafx.h"
#include "DeviceMgr.h"
#include "GR1.h"
#include "GR05.h"
#include "TN15.h"
#include "K102.h"
#include "SIGMA_25.h"
#include "SIGMA_50.h"
#include "RadAngel.h"
#include "UNIBASE.h"
#include "DoseDevice.h"

#include "D3DataProcessor.h"

#include "Lock.h"

namespace kmk
{

DeviceMgr::DeviceMgr(void)
: _deviceChangedCallbackFunc(NULL)
, _deviceChangedCallbackArg(NULL)
{
}

DeviceMgr::~DeviceMgr(void)
{
	ShutDown();
}

bool DeviceMgr::Initialize(ValidDeviceIdentifierVector &supportedDevices)
{
	_supportedDeviceList = supportedDevices;
	if(!_deviceEnumerator.Initialize(DevicesChangedCallbackFuncProc, this))
		return false;

	UpdateAttachedDevices();
	return true;
}

bool DeviceMgr::ShutDown()
{
	// Make sure all detectors are stopped
	_deviceEnumerator.Shutdown();

	Lock lock(_deviceListCS);
	while (!_deviceList.empty())
	{
		RemoveDevice(_deviceList.begin()->second);
	}

	return true;
}

std::vector<IDevice*> DeviceMgr::CreateDevices(IDataInterface *pInterface)
{
	std::vector<IDevice*> devices;
	
	switch (pInterface->GetVendorID())
	{
		case OLD_KROMEK_VENDOR_ID:
		case KROMEK_VENDOR_ID:
		{
			switch (pInterface->GetProductID())
			{
			case GR1::ProductId:
				devices.push_back(new GR1(pInterface));
				break;

			case GR1A::ProductId:
				devices.push_back(new GR1A(pInterface));
				break;

			case GR05::ProductId:
				devices.push_back(new GR05(pInterface));
				break;

			case TN15::ProductId:
				devices.push_back(new TN15(pInterface));
				break;

			case K102::ProductId:
				devices.push_back(new K102(pInterface));
				break;

			case SIGMA_25::ProductId:
				devices.push_back(new SIGMA_25(pInterface));
				break;
			case SIGMA_50::ProductId:
				devices.push_back(new SIGMA_50(pInterface));
				break;

			case RadAngel::ProductId:
				devices.push_back(new RadAngel(pInterface));
				break;

			case SIGMA_25_D3S::D3SProductId:
				// D3S contains both a sigma and a tn15
			{
				D3DataProcessor *pProc = new D3DataProcessor(pInterface, false, std::make_shared<SerialPacketStreamer>());
				SIGMA_25_D3S *pSigma = new SIGMA_25_D3S(pInterface, pProc, D3DataProcessor::GammaComponentId);
				TN15_D3S *pTN15 = new TN15_D3S(pInterface, pProc, D3DataProcessor::NeutronComponentId);
				HighDose_D3S *pDose = new HighDose_D3S(pInterface, pProc, D3DataProcessor::DoseComponentId);
				devices.push_back(pSigma);
				devices.push_back(pTN15);
				devices.push_back(pDose);
				break;
			}
			case UNIBASE_PMT::ProductId:
			{
				D3DataProcessor *pProc = new D3DataProcessor(pInterface, false, std::make_shared<SerialPacketStreamer>());
				devices.push_back(new UNIBASE_PMT(pInterface, pProc, D3DataProcessor::GammaComponentId));
				break;
			}
			case UNIBASE_SiPM::ProductId:
			{
				D3DataProcessor *pProc = new D3DataProcessor(pInterface, false, std::make_shared<SerialPacketStreamer>());
				devices.push_back(new UNIBASE_SiPM(pInterface, pProc, D3DataProcessor::GammaComponentId));
				break;
			}
			case SIGMA_25_D3M::D3MProductId:
			case SIGMA_25_D3M::D3M_BUB_ProductId:
			{
				// D3M contains both a sigma and a tn15
				D3DataProcessor *pProc = new D3DataProcessor(pInterface, true, std::make_shared<SerialPacketStreamer>());
				SIGMA_25_D3M *pSigma = new SIGMA_25_D3M(pInterface, pProc, D3DataProcessor::GammaComponentId);
				TN15_D3M *pTN15 = new TN15_D3M(pInterface, pProc, D3DataProcessor::NeutronComponentId);
				HighDose_D3M *pDose = new HighDose_D3M(pInterface, pProc, D3DataProcessor::DoseComponentId);
				devices.push_back(pSigma);
				devices.push_back(pTN15);
				devices.push_back(pDose);
				break;
			}
			case SIGMA_25_D3PRD::D3MProductId:
			{
				// D3M+ contains both a sigma and a tn15
				D3DataProcessor* pProc = new D3DataProcessor(pInterface, true, std::make_shared<SerialPacketStreamer>());
				SIGMA_25_D3PRD* pSigma = new SIGMA_25_D3PRD(pInterface, pProc, D3DataProcessor::GammaComponentId);
				HighDose_D3PRD* pDose = new HighDose_D3PRD(pInterface, pProc, D3DataProcessor::DoseComponentId);
				devices.push_back(pSigma);
				devices.push_back(pDose);
				break;
			}
			case SIGMA_25_D4::D4ProductId:
			{
				// D3M contains both a sigma and a tn15
				D3DataProcessor* pProc = new D3DataProcessor(pInterface, true, std::make_shared<SerialPacketStreamer>());
				SIGMA_25_D4 *pSigma = new SIGMA_25_D4(pInterface, pProc, D3DataProcessor::GammaComponentId);
				TN15_D4* pTN15 = new TN15_D4(pInterface, pProc, D3DataProcessor::NeutronComponentId);
				HighDose_D4* pDose = new HighDose_D4(pInterface, pProc, D3DataProcessor::DoseComponentId);
				devices.push_back(pSigma);
				devices.push_back(pTN15);
				devices.push_back(pDose);
				break;
			}

			case SIGMA_25_D5RIID::M2R2ProductId:
			{
				// D3M contains both a sigma and a tn15
				D3DataProcessor* pProc = new D3DataProcessor(pInterface, true, std::make_shared<FramedPacketStreamer>(), true);
				SIGMA_25_D5RIID* pSigma = new SIGMA_25_D5RIID(pInterface, pProc, D3DataProcessor::GammaComponentId);
				TN15_D5RIID* pTN15 = new TN15_D5RIID(pInterface, pProc, D3DataProcessor::NeutronComponentId);
				HighDose_D5RIID* pDose = new HighDose_D5RIID(pInterface, pProc, D3DataProcessor::DoseComponentId);
				devices.push_back(pSigma);
				devices.push_back(pTN15);
				devices.push_back(pDose);
				break;
			}
			}
		}

		// D3 vendor id is not kromeks
		case STM_VENDOR_ID:
		{
			switch(pInterface->GetProductID())
			{
				case SIGMA_25_D3::D3ProductId: // D3
				{
					// D3 contains both a sigma and a tn15
					D3DataProcessor *pProc = new D3DataProcessor(pInterface, false, std::make_shared<SerialPacketStreamer>());
					SIGMA_25_D3 *pSigma = new SIGMA_25_D3(pInterface, pProc, D3DataProcessor::GammaComponentId);
					TN15_D3 *pTN15 = new TN15_D3(pInterface, pProc, D3DataProcessor::NeutronComponentId);
					devices.push_back(pSigma);
					devices.push_back(pTN15);
					break;
				}
			}
			break;
		}

		// D3S bluetooth vendor id
		case TOSHIBA_BT_VENDOR_ID:
		{
			switch (pInterface->GetProductID())
			{
				case SIGMA_25_D3S::D3SBTProductId: // D3S
				{
					// D3S contains both a sigma and a tn15
					D3DataProcessor *pProc = new D3DataProcessor(pInterface, false, std::make_shared<SerialPacketStreamer>());
					SIGMA_25_D3S *pSigma = new SIGMA_25_D3S(pInterface, pProc, D3DataProcessor::GammaComponentId);
					TN15_D3S *pTN15 = new TN15_D3S(pInterface, pProc, D3DataProcessor::NeutronComponentId);
					devices.push_back(pSigma);
					devices.push_back(pTN15);
					break;
				}

				case SIGMA_25_D3M::D3MBTProductId: // D3M
				{
					// D3M contains both a sigma and a tn15
					D3DataProcessor *pProc = new D3DataProcessor(pInterface, true, std::make_shared<SerialPacketStreamer>());
					SIGMA_25_D3M *pSigma = new SIGMA_25_D3M(pInterface, pProc, D3DataProcessor::GammaComponentId);
					TN15_D3M *pTN15 = new TN15_D3M(pInterface, pProc, D3DataProcessor::NeutronComponentId);
					devices.push_back(pSigma);
					devices.push_back(pTN15);
					break;
				}
			}
			break;
		}
	}

	return devices;
}

// Create the relevant devices and add to the internal list, if the device is unsupported then return NULL
std::vector<IDevice*> DeviceMgr::AddInterface(IDataInterface *pInterface)
{
	// Create the relevant device given the interface details
	std::vector<IDevice*> newDevices = CreateDevices(pInterface);

	std::vector<IDevice*>::iterator it;
	for (it = newDevices.begin(); it != newDevices.end(); ++it)
	{
		_deviceList[(*it)->GetHash()] =  (*it);
	}
	return newDevices;
}

void DeviceMgr::RemoveDevice(IDevice *pDevice)
{
	pDevice->Stop(true);
	_deviceList.erase(pDevice->GetHash());
	delete pDevice;
}

// Update the list of attached devices
void DeviceMgr::UpdateAttachedDevices()
{
	DSC_TRACE(L"DeviceMgr::UpdateAttachedDevices");

	Lock lock(_deviceListCS);

	std::vector<IDataInterface*> interfacesFound;
	
	ValidDeviceIdentifierVector::const_iterator itIdentifier;
	for (itIdentifier = _supportedDeviceList.begin(); itIdentifier != _supportedDeviceList.end(); ++itIdentifier)
	{
		_deviceEnumerator.EnumerateDevices(*itIdentifier, interfacesFound);
	}

	// Create all new devices and determine removed devices
	DeviceMap removedDevices(_deviceList);

	std::vector<IDataInterface*>::const_iterator itInterface;
	for (itInterface = interfacesFound.begin(); itInterface != interfacesFound.end(); ++itInterface)
	{
		// Determine if any devices for this interface already exist. If not then its a new interface
		bool hasDevices = false;
		unsigned int interfaceHash = (*itInterface)->GetHash();
		DeviceMap::iterator itDevice;
		for (itDevice = _deviceList.begin(); itDevice != _deviceList.end(); ++itDevice)
		{
			if (itDevice->second->GetInterface()->GetHash() == interfaceHash)
			{
				// Already exists and still connected, take out of the removed list
				removedDevices.erase(itDevice->first);
				hasDevices = true;
			}
		}

		if (!hasDevices)
		{
			// New device
			std::vector<IDevice*> newDevices = AddInterface(*itInterface);
			for (std::vector<IDevice*>::iterator it = newDevices.begin(); it != newDevices.end(); ++it)
			{
				if (_deviceChangedCallbackFunc != NULL)
				{
					(*_deviceChangedCallbackFunc)(*it, true, _deviceChangedCallbackArg);
				}
			}
		}
	}

	// Delete removed devices
	for(DeviceMap::iterator itRemove = removedDevices.begin(); itRemove != removedDevices.end(); ++itRemove)
	{
		// Raise callback event
		if (_deviceChangedCallbackFunc != NULL)
		{
			(*_deviceChangedCallbackFunc)(itRemove->second, false, _deviceChangedCallbackArg);
		}

		RemoveDevice(itRemove->second);
	}
}

void DeviceMgr::DevicesChangedCallbackFuncProc(void *pArg)
{
	DeviceMgr *pThis = (DeviceMgr*)pArg;

	pThis->UpdateAttachedDevices();
}

void DeviceMgr::SetDeviceChangedCallback(DeviceChangedCallbackFunc func, void *pArg)
{
	_deviceChangedCallbackFunc = func;
	_deviceChangedCallbackArg = pArg;
}

IDevice *DeviceMgr::GetNextDevice(IDevice *pPrevious)
{
	Lock lock(_deviceListCS);

	if (_deviceList.size() == 0)
		return NULL;

	if (pPrevious == NULL)
		return _deviceList.begin()->second;

	DeviceMap::iterator itPrev = _deviceList.find(pPrevious->GetHash());
	if (itPrev == _deviceList.end())
		return NULL;

	++itPrev;
	if (itPrev == _deviceList.end())
		return NULL;
	else
		return itPrev->second;
}

bool DeviceMgr::GetDetectorProperties(VID vendorID, PID productID, DetectorProperties &propsOut)
{
	switch(vendorID)
	{
	case OLD_KROMEK_VENDOR_ID:
	case KROMEK_VENDOR_ID:
		switch(productID)
		{
		case GR1::ProductId:
		case GR1A::ProductId:
			GR1::GetDetectorProperties(propsOut);
			return true;
		
		case GR05::ProductId:
			GR05::GetDetectorProperties(propsOut);
			return true;

		case RadAngel::ProductId:
			RadAngel::GetDetectorProperties(propsOut);
			return true;

		case K102::ProductId:
			K102::GetDetectorProperties(propsOut);
			return true;

		case SIGMA_25::ProductId:
			SIGMA_25::GetDetectorProperties(propsOut);
			return true;

		case SIGMA_50::ProductId:
			SIGMA_50::GetDetectorProperties(propsOut);
			return true;

		case SIGMA_25_D3::ProductId:
			SIGMA_25_D3::GetDetectorProperties(propsOut);
			return true;

		case TN15::ProductId:
			TN15::GetDetectorProperties(propsOut);
			return true;

		case TN15_D3::ProductId:
			TN15_D3::GetDetectorProperties(propsOut);
			return true;

		case TN15_D3S::ProductId:
			TN15_D3S::GetDetectorProperties(propsOut);
			return true;

		case SIGMA_25_D3S::ProductId:
			SIGMA_25_D3S::GetDetectorProperties(propsOut);
			return true;

		case TN15_D3M::ProductId:
			TN15_D3M::GetDetectorProperties(propsOut);
			return true;

		case SIGMA_25_D3M::ProductId:
			SIGMA_25_D3M::GetDetectorProperties(propsOut);
			return true;

		case TN15_D4::ProductId:
			TN15_D4::GetDetectorProperties(propsOut);
			return true;

		case SIGMA_25_D4::ProductId:
			SIGMA_25_D4::GetDetectorProperties(propsOut);
			return true;

		case UNIBASE_PMT::ProductId:
			UNIBASE_PMT::GetDetectorProperties(propsOut);
			return true;

		case UNIBASE_SiPM::ProductId:
			UNIBASE_SiPM::GetDetectorProperties(propsOut);
			return true;

		case TN15_D5RIID::ProductId:
			TN15_D5RIID::GetDetectorProperties(propsOut);
			return true;

		case SIGMA_25_D5RIID::ProductId:
			SIGMA_25_D5RIID::GetDetectorProperties(propsOut);
			return true;

		case SIGMA_25_D3PRD::ProductId:
			SIGMA_25_D3PRD::GetDetectorProperties(propsOut);
			return true;

		case HighDose_D3S::ProductId:
			HighDose_D3S::GetDetectorProperties(propsOut);
			return true;

		case HighDose_D3M::ProductId:
			HighDose_D3M::GetDetectorProperties(propsOut);
			return true;

		case HighDose_D4::ProductId:
			HighDose_D4::GetDetectorProperties(propsOut);
			return true;

		case HighDose_D5RIID::ProductId:
			HighDose_D5RIID::GetDetectorProperties(propsOut);
			return true;

		case HighDose_D3PRD::ProductId:
			HighDose_D3PRD::GetDetectorProperties(propsOut);
			return true;
		}
	}

	return false;
}
}
