#include "stdafx.h"
#include "Thread.h"
#include <memory.h>
#include "Lock.h"
#ifndef _WINDOWS
    #include <time.h>
#endif

namespace kmk
{
	

	Thread::Thread()
#ifdef _WINDOWS
		: m_handle(0)
		, m_id(0)
		, m_isRunning(false)
#else
		: m_thread(0)
		, m_isRunning(false)
#endif
	{
		memset(&_callbackArgs, 0, sizeof(CallbackArgs));
	}

	bool Thread::Start(ThreadProcFunc pFunction, void *pArg)
	{
		_callbackArgs.pThread = this;
		_callbackArgs.callbackProc = pFunction;
		_callbackArgs.pArg = pArg;

#ifdef _WINDOWS
		if (m_handle)
			CloseHandle(m_handle);
		m_handle = CreateThread(0,
			0,
			CallbackProc,
			&_callbackArgs,
			0,
			&m_id);
		return (m_handle != NULL);
#else
		if (m_thread)
		{
			pthread_detach(m_thread);
			m_thread = 0;
		}

        return (pthread_create(&m_thread, NULL, CallbackProc, &_callbackArgs) == 0);
#endif
	}


	Thread::~Thread ()
	{
#ifdef _WINDOWS
		if (m_handle)
			CloseHandle(m_handle);
#else
		if (m_thread)
			pthread_detach(m_thread);
#endif
	}

	int Thread::WaitForTermination()
	{
		if (!IsCurrentThread())
		{

#ifdef _WINDOWS
			return WaitForSingleObject(m_handle, INFINITE);
#else
			if (pthread_join(m_thread, NULL) == 0)
				m_thread = 0;
#endif

		}

		return 0;
	}

	bool Thread::IsRunning()
	{
		Lock l(m_critical);
		return m_isRunning;
	}

	bool Thread::IsCurrentThread()
	{
#ifdef _WINDOWS

		return (GetCurrentThreadId() == m_id);
#else
		return (pthread_self() == m_thread);
#endif
	}

#ifdef _WINDOWS
	DWORD WINAPI Thread::CallbackProc(void *pArg)
	{
		CallbackArgs *pArgs = (CallbackArgs*)pArg;

		{
			Lock lock(pArgs->pThread->m_critical);
			pArgs->pThread->m_isRunning = true;
		}

		if (pArgs->callbackProc != NULL)
		{ 
			return (*pArgs->callbackProc)(pArgs->pArg);
		}

		{
			Lock lock(pArgs->pThread->m_critical);
			pArgs->pThread->m_isRunning = false;
		}

		return 0;
	}
#else
	void *Thread::CallbackProc(void *pArg)
	{
		CallbackArgs *pArgs = (CallbackArgs*)pArg;
		
		{
			Lock lock(pArgs->pThread->m_critical);
			pArgs->pThread->m_isRunning = true;
		}

		if (pArgs->callbackProc != NULL)
		{
			return (void*)(*pArgs->callbackProc)(pArgs->pArg);
		}

		{
			Lock lock(pArgs->pThread->m_critical);
			pArgs->pThread->m_isRunning = true;
		}

		return (void*)0;
	}
#endif

void Thread::Sleep(int timeInMs)
{
#ifdef _WINDOWS
    ::Sleep(timeInMs);
#else
    struct timespec time;
    time.tv_sec = 0;
    time.tv_nsec = timeInMs * 1000000;
    nanosleep(&time, &time);
#endif
}

} // namespace kmk


