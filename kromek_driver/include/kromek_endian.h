#pragma once

#include "types.h"

namespace kmk
{
	class Endian
	{
	public:

		enum Order
		{
            LittleEndian,
            BigEndian,
		};

		// Determine if the data needs to be endian swapped and return the relevant value
		static uint16_t SwapUInt16(Order from, Order to, BYTE *pData)
		{
			if (from != to)
			{
				return ((uint16_t)pData[0] << 8) | pData[1];
			}

            return *((int16_t *)pData);
		}

		// Determine if the data needs to be endian swapped and return the relevant data
		static BYTE *SwapBytes16(Order from, Order to, uint16_t val, BYTE *pBytesOut)
		{
			if (from != to)
			{
				pBytesOut[0] = ((val >> 8) & 0xFF);
				pBytesOut[1] = val & 0xFF;
				return pBytesOut;
			}
			else
			{
				BYTE *pSrcBytes = (BYTE*)&val;
				pBytesOut[0] = pSrcBytes[0];
				pBytesOut[1] = pSrcBytes[1];
			}

			return pBytesOut;
		}

	};
}
