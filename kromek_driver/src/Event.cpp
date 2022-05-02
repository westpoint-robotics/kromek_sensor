#include "stdafx.h"
#include "Event.h"
#include <assert.h>

#ifndef _WINDOWS
#include <sys/time.h> // Linux header
#endif

namespace kmk
{

	Event::Event ()
    {
		Construct (true, false, L"");
    }

	Event::Event (bool autoReset, bool signalled, const String& name)
    {
		Construct (autoReset, signalled, name);
    }

    Event::~Event ()
    {
#ifdef _WINDOWS
        CloseHandle (m_event);
#else
        pthread_cond_destroy(&m_condition);
        pthread_mutex_destroy(&m_mutex);
#endif
    }

	void Event::Construct (bool autoReset, bool signalled, const String& name)
	{
#ifdef _WINDOWS
		m_event = CreateEvent (0, 
								autoReset ? FALSE : TRUE,
								signalled ? TRUE : FALSE, 
								name.empty () ? 0 : name.c_str ());
#else
        pthread_mutex_init(&m_mutex, NULL);
        pthread_cond_init(&m_condition, NULL);
        m_signalled = signalled;
#endif
	}


    // put into signaled state
    void Event::Signal () 
	{ 
#ifdef _WINDOWS
		SetEvent (m_event); 
#else
        pthread_mutex_lock(&m_mutex);
        m_signalled = true;
        pthread_cond_broadcast(&m_condition);
        pthread_mutex_unlock(&m_mutex);
#endif
	}

	void Event::Reset ()
	{
#ifdef _WINDOWS
		ResetEvent (m_event);
#else
        pthread_mutex_lock(&m_mutex);
        m_signalled = false;
        pthread_mutex_unlock(&m_mutex);
#endif
	}

    bool Event::Wait (uint32_t timeOut /* = INFINITE */)
    {
		bool signalled = false;

#ifdef _WINDOWS
        DWORD result = WaitForSingleObject (m_event, timeOut);

		signalled = (result == WAIT_OBJECT_0);
#else
        pthread_mutex_lock(&m_mutex);

        if (m_signalled)
        {
            signalled = m_signalled;
        }
        else
        {
            if (timeOut == INFINITE)
            {
                while (!m_signalled)
                    pthread_cond_wait(&m_condition, &m_mutex);
            }
            else
            {
                struct timeval now;
                struct timespec timeout;

                /*gettimeofday(&tv, NULL);
                    ts.tv_sec = time(NULL) + timeInMs / 1000;
                    ts.tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * (timeInMs % 1000);
                    ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
                    ts.tv_nsec %= (1000 * 1000 * 1000);*/

                // Calculate the time to expire the wait
                gettimeofday(&now, NULL);
                int64_t timeUSec = (now.tv_usec * 1000) + (timeOut * 1000000);
                timeout.tv_sec = now.tv_sec + (timeUSec / 1000000000L);
                timeout.tv_nsec = (timeUSec % 1000000000L);

                pthread_cond_timedwait(&m_condition, &m_mutex, &timeout);
                signalled = m_signalled;
            }
        }

        pthread_mutex_unlock(&m_mutex);
#endif

		return signalled;
    }

#ifdef _WINDOWS
	// conversion operator for APIs
    Event::operator HANDLE () 
	{ 
		return m_event; 
	}
#endif

} // namespace kmk


