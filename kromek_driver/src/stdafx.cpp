// stdafx.cpp : source file that includes just the standard includes
// KromekDriver.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include "Lock.h"

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file

namespace kmk
{
#ifdef _WINDOWS
	std::wofstream os("c:\\temp\\KromekDriver.log", std::ios::out);
	int gtc = GetTickCount();
	kmk::CriticalSection _logCS;

	void Log(const std::wstring& s)
	{
		int t = GetTickCount() - gtc;
		int sec = t / 1000;
		int msec = t % 1000;
		{
			std::wstringstream ss;
			ss << "0x" << std::hex << std::setw(4) << std::setfill(L'0') << GetCurrentThreadId() << " : " << std::dec
				<< std::setw(4) << std::setfill(L'0') << sec << "." << std::setw(4) << std::setfill(L'0') << msec
				<< " " << s.c_str() << "\n";

			std::wstringstream ns;
			ns << "c:\\temp\\KromekDriver." << std::hex << std::setw(4) << std::setfill(L'0') << GetCurrentThreadId() << ".log";
			std::wofstream ts(ns.str(), std::ios::out | std::ios::app);
			ts << ss.str() << std::flush;

			kmk::Lock lock(_logCS);
			os << ss.str() << std::flush;
		}
	}
#endif
}
