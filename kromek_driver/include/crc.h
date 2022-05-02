#pragma once

#include "types.h"

namespace kmk
{
	namespace crc
	{
	
		static uint16_t CalculateCrc(const BYTE* pData, size_t dataLength, int crc = 0xFFFF)
		{
			for (int j = 0; j < dataLength; j++)
			{
				crc = ((crc >> 8) | (crc << 8)) & 0xffff;
				crc ^= (pData[j] & 0xff);//byte to int, trunc sign
				crc ^= ((crc & 0xff) >> 4);
				crc ^= (crc << 12) & 0xffff;
				crc ^= ((crc & 0xFF) << 5) & 0xffff;
			}

			crc &= 0xffff;
			return static_cast<uint16_t>(crc);
		}
	}
}
