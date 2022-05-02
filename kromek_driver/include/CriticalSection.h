#pragma once

#ifdef _WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	#include <pthread.h>
#endif

namespace kmk
{

	/** @brief A wrapper around the windows critical section synch. mechanism.

		For an example see @ref Lock

	*/
	class CriticalSection
	{
		friend class Lock;

	public:
		
		CriticalSection ();
		~CriticalSection ();

	private:
		
		void Enter ();
		void Leave ();
		bool TryEnter ();

#ifdef _WINDOWS
		CRITICAL_SECTION m_cs;
#else
		pthread_mutex_t m_cs;		
#endif
	};


} // namespace kmk

