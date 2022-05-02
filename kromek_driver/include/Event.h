#pragma once

#include "types.h"

#ifndef _WINDOWS
    #include <pthread.h>
    #define INFINITE 0xFFFFFFFF
#endif

namespace kmk
{

    /** @brief A wrapper around the windows/linux event mechanism.
	*/
	class Event
	{
	public:
		
		Event ();
		Event (bool autoReset, bool signalled, const String& name);
		~Event ();

		void Signal ();
		void Reset ();
        bool Wait (uint32_t timeOut = INFINITE);

#ifdef _WINDOWS
		operator HANDLE (); 
#endif

    private:

		void Construct (bool autoReset, bool signalled, const String& name);

#ifdef _WINDOWS
		HANDLE m_event;
#else
        // Linux
        bool m_signalled;
        pthread_cond_t m_condition;
        pthread_mutex_t m_mutex;
#endif
	};


} // namespace kmk
