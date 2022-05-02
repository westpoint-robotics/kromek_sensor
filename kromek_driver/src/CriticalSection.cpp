#include "stdafx.h"
#include "CriticalSection.h"
#include "errno.h"
namespace kmk
{

	CriticalSection::CriticalSection ()
	{
#ifdef _WINDOWS
		InitializeCriticalSection (&m_cs);
#else
		//create mutex attribute variable
		pthread_mutexattr_t mAttr;

        pthread_mutexattr_init(&mAttr);
		// setup recursive mutex for mutex attribute
		pthread_mutexattr_settype(&mAttr, PTHREAD_MUTEX_RECURSIVE_NP);

		// Use the mutex attribute to create the mutex
        int i = pthread_mutex_init(&m_cs, &mAttr);
        if (i != 0)
        {
            switch(i)
            {
            case EAGAIN:
                i = EAGAIN;
                break;
            case ENOMEM:
                i = ENOMEM;
                break;
            case EPERM:
                i = EPERM;
                break;
            case EBUSY:
                i = EBUSY;
                break;
            case EINVAL:
                i = EINVAL;
                break;


            }

            return;
        }

		// Mutex attribute can be destroy after initializing the mutex variable
		pthread_mutexattr_destroy(&mAttr);
#endif
	}

	CriticalSection::~CriticalSection ()
	{
#ifdef _WINDOWS
		DeleteCriticalSection (&m_cs);
#else
		pthread_mutex_destroy (&m_cs);
#endif
	}
		
	void CriticalSection::Enter ()
	{
#ifdef _WINDOWS
		EnterCriticalSection (&m_cs);
#else
		pthread_mutex_lock (&m_cs);
#endif
	}

	void CriticalSection::Leave ()
	{
#ifdef _WINDOWS
		LeaveCriticalSection (&m_cs);
#else
		pthread_mutex_unlock (&m_cs);
#endif
	}

	bool CriticalSection::TryEnter ()
	{
#ifdef _WINDOWS
		return (TryEnterCriticalSection (&m_cs) == TRUE);
#else
		return (pthread_mutex_trylock(&m_cs) == 0);
#endif
	}


} // namespace kmk


