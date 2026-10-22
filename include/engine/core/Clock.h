/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 22 October 2026
*/

/**
 * @file Clock.h
 * @brief Time source abstraction so event timestamps are testable.
 */

#ifndef TIVIDY_CORE_CLOCK_H
#define TIVIDY_CORE_CLOCK_H

#include "engine/core/Types.h"

namespace tividy
{

	/** Abstract time source. No engine class calls std::chrono::system_clock::now() directly. */
	class Clock
	{
		public:
			virtual ~Clock();

			virtual TimePoint now() const = 0;
	};

	/** Real time. */
	class SystemClock : public Clock
	{
		public:
			TimePoint now() const override;
	};

	/** Test clock: returns the same instant until advance() is called. */
	class ManualClock : public Clock
	{
		public:
			explicit ManualClock(TimePoint start);

			TimePoint now() const override;

			/** Moves time forward by @p seconds. @throws EngineError if negative. */
			void advance(long seconds);
		private:
			TimePoint current;
	};
} // namespace tividy

#endif // TIVIDY_CORE_CLOCK_H