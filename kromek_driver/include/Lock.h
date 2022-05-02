#pragma once

#include "CriticalSection.h"


namespace kmk
{

	/** @brief A synchronisation mechanism using critical sections.

	Example:

		// declare critical section as class attribute somewhere...
		CriticalSection csObject; 


		void MyClass::ProtectedFunction ()
		{
			Lock protect (csObject);

			// work on data...

			// destructor releases at end of scope or on exception.
		}

	*/
	class Lock
	{
	public:

		Lock (CriticalSection &cs);
		~Lock ();

	private:

		CriticalSection &m_cs;

	};


} // namespace kmk
