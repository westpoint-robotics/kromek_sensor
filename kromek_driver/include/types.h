#pragma once

#ifdef _WINDOWS
	typedef __int64 int64_t;
	typedef __int16 int16_t;
	typedef unsigned int uint32_t;
	typedef unsigned short uint16_t;
	typedef unsigned char uint8_t;
	typedef signed char int8_t;
#else
	#include <stdint.h>
#endif

#ifndef BYTE
	typedef unsigned char BYTE;
#endif

#include <string>

#ifdef _UNICODE
	typedef std::wstring String;
#else
	typedef std::string String;
#endif

typedef int VID;
typedef int PID;
