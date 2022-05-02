#pragma once

#include "types.h"

namespace kmk
{
	// Identifier structure used to detail which devices should be available to the driver
	struct ValidDeviceIdentifier
	{
		PID productId;
		VID vendorId;
	};
}