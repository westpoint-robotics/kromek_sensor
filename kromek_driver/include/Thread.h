#pragma once

#include "CriticalSection.h"

#ifndef _WINDOWS
    #include <pthread.h>
#endif

namespace kmk
{

	typedef int (*ThreadProcFunc)(void *);

	/** @brief A wrapper around the windows thread API.
	*/
	class Thread
	{
	public:
	
		Thread();
		~Thread ();

        bool Start (ThreadProcFunc func, void *pArg);

        int WaitForTermination();

		bool IsCurrentThread();

        static void Sleep(int timeInMs);

		bool IsRunning();

	private:

		struct CallbackArgs
		{
			Thread *pThread;
			ThreadProcFunc callbackProc;
			void *pArg;
		};

		CallbackArgs _callbackArgs;

		CriticalSection m_critical;
		bool m_isRunning;

#ifdef _WINDOWS
		HANDLE m_handle;
		DWORD  m_id;
#else
        pthread_t m_thread;
#endif
		

#ifdef _WINDOWS
		static DWORD WINAPI CallbackProc(void *pArg);
#else
		static void *CallbackProc(void *pArg);

#endif
	};


} // namespace kmk
