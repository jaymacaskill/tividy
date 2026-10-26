/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 24 October 2026

Clock.cpp
*/

#ifndef TIVIDY_CORE_CLOCK_CPP
#define TIVIDY_CORE_CLOCK_CPP

#include "engine/core/Clock.h"
#include "engine/core/Errors.h"

namespace tividy
{
	Clock::~Clock() { } // empty default is fine, we just have this for polymorphism

	TimePoint SystemClock::now() const
	{
		return std::chrono::system_clock::now();
	}

	ManualClock::ManualClock(TimePoint start) :
		current(start) { }

	TimePoint ManualClock::now() const
	{
		return current;
	}

	void ManualClock::advance(long seconds)
	{
		if (seconds < 0) throw EngineError("Cannot advance the time by a negative value");
		current += std::chrono::seconds(seconds);
	}
} // namespace tividy

#endif // TIVIDY_CORE_CLOCK_CPP