/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 23 October 2026
*/

/**
 * @file DeadlineMonitor.h
 * @brief Detects lapsed deadlines and publishes DeadlineMissed events.
 */

#ifndef TIVIDY_RUNTIME_DEADLINEMONITOR_H
#define TIVIDY_RUNTIME_DEADLINEMONITOR_H

#include <cstddef>
#include <set>

#include "engine/core/Clock.h"
#include "engine/core/Types.h"

namespace tividy
{
	class WorkflowInstance;

	/**
	 * @brief Polled monitor (not an Observer): Tividy::advanceTime() (or a real timer in production) calls sweep().
	 *
	 * sweep(): walk every non-composite item of the instance (DepthFirstIterator). For each item whose status is Assigned, InProgress or
	 * UnderReview, with dueAt() != TimePoint(), with clock.now() > dueAt(), and not already reported: publish
	 * DeadlineMissed(item_id, actor "system", message "deadline passed") and remember the id. An id is forgotten as soon as a sweep finds
	 * that item outside those three statuses, so each lapse is reported once.
	 * It never changes item status itself; FlowController reacts to the event (expire or escalate).
	 */
	class DeadlineMonitor
	{
		public:
			DeadlineMonitor(WorkflowInstance& instance, const Clock& clock);

			/** @return the number of DeadlineMissed events published by this call. */
			std::size_t sweep();
		private:
			WorkflowInstance& instance;
			const Clock& clock;
			std::set<Id> reported;
	};
} // namespace tividy

#endif // TIVIDY_RUNTIME_DEADLINEMONITOR_H