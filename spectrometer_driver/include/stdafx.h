// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#ifdef _WINDOWS

	#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
	// Windows Header Files:
	#include <windows.h>
	#include <tchar.h>
	
	typedef __int64 int64_t;
#else
	#include <stdint.h>
	#include <stdlib.h>

#endif


#define ERR TRACE

// TODO: reference additional headers your program requires here


