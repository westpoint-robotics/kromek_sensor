#pragma once

#include "Detector.h"

void GetDeviceList(kmk::ValidDeviceIdentifierVector &listOut)
{
    listOut.reserve(8);

    // GR1/A
    kmk::ValidDeviceIdentifier gr1Dev = {0x0, 0x4d8};
    kmk::ValidDeviceIdentifier gr1ADev = {0x101, 0x4d8};
    listOut.push_back(gr1Dev);
    listOut.push_back(gr1ADev);

    // K102
    kmk::ValidDeviceIdentifier k102Dev = {0x011, 0x4d8};
    listOut.push_back(k102Dev);

    // Radangel
    kmk::ValidDeviceIdentifier angelDev = {0x100, 0x4d8};
    listOut.push_back(angelDev);

    // Sigma25
    kmk::ValidDeviceIdentifier sig25Dev = {0x022, 0x4d8};
    listOut.push_back(sig25Dev);

    // Sigma50
    kmk::ValidDeviceIdentifier sig50Dev = {0x023, 0x4d8};
    listOut.push_back(sig50Dev);

    // TN15
    kmk::ValidDeviceIdentifier tn15Dev = {0x030, 0x4d8};
    listOut.push_back(tn15Dev);

    // GR05
    kmk::ValidDeviceIdentifier gr05Dev = {0x050, 0x2A5A};
    listOut.push_back(gr05Dev);

	// D3SGamma + Neutron + Dose
	kmk::ValidDeviceIdentifier ds3G = { 0x01D3,  0x2A5A };
	listOut.push_back(ds3G);

	// D3MGamma + Neutron
	kmk::ValidDeviceIdentifier ds3M = { 0x0044,  0x2A5A };
	listOut.push_back(ds3M);

	// D4Gamma + Neutron + Dose
	kmk::ValidDeviceIdentifier d4 = { 0x0045,  0x2A5A };
	listOut.push_back(d4);

    // M2R2Gamma + Neutron + Dose
    kmk::ValidDeviceIdentifier m2r2 = { 0x0046,  0x2A5A };
    listOut.push_back(m2r2);

}
