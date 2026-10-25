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
 * @file WorkflowEvent.h
 * @brief Immutable-after-construction fact published when something happens in an instance.
 */

#ifndef TIVIDY_EVENTS_WORKFLOWEVENT_H
#define TIVIDY_EVENTS_WORKFLOWEVENT_H

#include <string>

#include "engine/core/Outcome.h"
#include "engine/core/Types.h"

namespace tividy
{

	/**
	 * @brief Supporting class of the Observer collaboration (push model payload; NOT a GoF role).
	 *
	 * Every state change and every system reaction is described by one WorkflowEvent, so that
	 * AuditObserver can build a complete history from events alone.
	 *
	 * Field meaning by type:
	 *  - itemId():   the item concerned; empty for Instance* events.
	 *  - actorId():  the participant who caused it; "system" for engine-initiated changes.
	 *  - outcome():  meaningful for ItemSubmitted, ItemCompleted, ItemFailed; default Accepted otherwise.
	 *  - count():    ItemEscalated -> how many times this item has now been escalated (1 for the first);
	 *                ItemFailed -> how many technical failures this item has now had (1 for the first);
	 *                otherwise 0.
	 *  - message():  short human text for history and notifications ("Verify documents completed").
	 *  - ExternalSignal events carry the signal name in message() and are routed by SignalRule.
	 */
	class WorkflowEvent
	{
		public:
			WorkflowEvent(EventType type, const Id& instance_id, const Id& item_id, TimePoint when);

			/** Setters return *this so events can be built fluently: WorkflowEvent(...).byActor("u1").withMessage("..."). */
			WorkflowEvent& byActor(const Id& actor_id);
			WorkflowEvent& withOutcome(const Outcome& outcome);
			WorkflowEvent& withMessage(const std::string& message);
			WorkflowEvent& withCount(unsigned count);

			EventType type() const;
			const Id& instanceId() const;
			const Id& itemId() const;
			const Id& actorId() const;
			const Outcome& outcome() const;
			const std::string& message() const;
			unsigned count() const;
			TimePoint when() const;

		private:
			EventType type_ref;
			Id instance_id;
			Id item_id;
			Id actor_id;
			Outcome outcome_ref;
			std::string message_ref;
			unsigned count_ref;
			TimePoint when_ref;
	};
} // namespace tividy

#endif // TIVIDY_EVENTS_WORKFLOWEVENT_H