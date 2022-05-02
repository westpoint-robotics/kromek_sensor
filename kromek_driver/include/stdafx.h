// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#ifdef _WINDOWS

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files:
#include <windows.h>

typedef __int64 int64_t;
#endif

#include <string>
#include <sstream>

//#define DSC_DEBUG

namespace kmk
{
	#ifdef DSC_DEBUG
		void Log(const std::wstring& s);

		#define DSC_LOG(x) { std::wstringstream ss; ss << x; Log(ss.str()); }
		#define DSC_TRACE(x) DSC_TRACE_T dscTrace(x);
		struct DSC_TRACE_T
		{
			DSC_TRACE_T(const std::wstring& s) : m_s(s)
			{
				DSC_LOG(m_s << " Entered");
			}
			virtual ~DSC_TRACE_T()
			{
				DSC_LOG(m_s << " Exited");
			}
		protected:
			std::wstring m_s;
		};

	#else
		#define DSC_LOG(x)
		#define DSC_TRACE(x)
	#endif
}


