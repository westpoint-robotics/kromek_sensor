#pragma once

#ifdef _WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cmath>

#include "types.h"

// NOTE: EPOCH = January 1, 1601 

#define NS_IN_SECONDS 1000000000

// Time length of a tick in nanoseconds
#define TICK_TIME_NS 100L

#define MS_TO_TICKS(ms) ((ms * 1000000) / TICK_TIME_NS)

// Some older versions of librarys do not include CLOCK_BOOTTIME - in this case fall back to monotonic
#ifdef CLOCK_BOOTTIME
#define CLOCK_TYPE CLOCK_BOOTTIME
#else
#define CLOCK_TYPE CLOCK_MONOTONIC
#endif

// Helper class / static methods for time
namespace kmk
{
	class Time
	{
	private:
	#ifdef _WINDOWS
		static LARGE_INTEGER GetPerformanceFreq()
		{
			LARGE_INTEGER freq;
			QueryPerformanceFrequency(&freq);
			return freq;
		}
		#endif
	public:

		// Get time from performance counter in ticks
		static int64_t GetTime()
		{
		#ifdef _WINDOWS
			static LARGE_INTEGER freq = GetPerformanceFreq();
			LARGE_INTEGER now;
			QueryPerformanceCounter(&now);
			 
			return (int64_t)(now.QuadPart * (double(NS_IN_SECONDS / TICK_TIME_NS) / (double)freq.QuadPart)); 
		#else
			timespec now;
			clock_gettime(CLOCK_TYPE, &now);
			return static_cast<int64_t>(now.tv_nsec) + SecondsToTicks(now.tv_sec);
		#endif
		}

		// Get time from the performance counter in milliseconds
		static int64_t GetTimeMs()
		{
		#ifdef _WINDOWS
			static LARGE_INTEGER freq = GetPerformanceFreq();
			LARGE_INTEGER now;
			
			QueryPerformanceCounter(&now);
			return (int64_t)(now.QuadPart * (1000.0 / freq.QuadPart));
		#else
			timespec now;
			clock_gettime(CLOCK_TYPE, &now);
			return (static_cast<int64_t>(now.tv_nsec) / 1000000) + (now.tv_sec * 1000);
		#endif
		}

		// Return the system time in ticks (UTC)
		static int64_t GetSystemTime()
		{
		#ifdef _WINDOWS
			FILETIME time;
			::GetSystemTimeAsFileTime(&time);

			return ((int64_t)time.dwHighDateTime << 32) + (int64_t)time.dwLowDateTime;
		#else
			timespec now;
			clock_gettime(CLOCK_REALTIME, &now); // TODO: Check this is UTC?
			return static_cast<int64_t>(now.tv_nsec) + SecondsToTicks(now.tv_sec);
		#endif
		}

		static double TicksToSeconds(int64_t ticks)
		{
			return (double)(ticks * TICK_TIME_NS) / (double)NS_IN_SECONDS;
		}

		static int64_t SecondsToTicks(double seconds)
		{
			return (int64_t)(seconds * NS_IN_SECONDS) /  TICK_TIME_NS;
		}

		static int64_t TicksToMs(int64_t ticks)
		{
			return (ticks * TICK_TIME_NS) / 1000000;
		}
	};
}
