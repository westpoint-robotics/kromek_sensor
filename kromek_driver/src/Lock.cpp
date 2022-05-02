#include "stdafx.h"
#include "Lock.h"

namespace kmk
{

	Lock::Lock (CriticalSection &cs)
    : m_cs (cs)
	{
		m_cs.Enter ();
	}

	Lock::~Lock ()
	{
		m_cs.Leave ();
	}


} // namespace kmk


