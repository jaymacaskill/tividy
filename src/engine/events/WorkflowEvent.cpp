/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 25 October 2026

WorkflowEvent.cpp
*/

#ifndef TIVIDY_EVENTS_WORKFLOWEVENT_CPP
#define TIVIDY_EVENTS_WORKFLOWEVENT_CPP

#include "engine/events/WorkflowEvent.h"

namespace tividy
{
	WorkflowEvent::WorkflowEvent(EventType type, const Id& instance_id, const Id& item_id, TimePoint when) :
	type_ref(type), instance_id(instance_id), item_id(item_id), count_ref(0), when_ref(when) { }

	WorkflowEvent& WorkflowEvent::byActor(const Id& actor_id)
	{
		this->actor_id = actor_id;
		return *this;
	}

	WorkflowEvent& WorkflowEvent::withOutcome(const Outcome& outcome)
	{
		outcome_ref = outcome;
		return *this;
	}
	
	WorkflowEvent& WorkflowEvent::withMessage(const std::string& message)
	{
		message_ref = message;
		return *this;
	}

	WorkflowEvent& WorkflowEvent::withCount(unsigned count)
	{
		count_ref = count;
		return *this;
	}

	EventType WorkflowEvent::type() const
	{
		return type_ref;
	}

	const Id& WorkflowEvent::instanceId() const
	{
		return instance_id;
	}

	const Id& WorkflowEvent::itemId() const
	{
		return item_id;
	}

	const Id& WorkflowEvent::actorId() const
	{
		return actor_id;
	}

	const Outcome& WorkflowEvent::outcome() const
	{
		return outcome_ref;
	}

	const std::string& WorkflowEvent::message() const
	{
		return message_ref;
	}

	unsigned WorkflowEvent::count() const
	{
		return count_ref;
	}

	TimePoint WorkflowEvent::when() const
	{
		return when_ref;
	}
} // namespace tividy

#endif // TIVIDY_EVENTS_WORKFLOWEVENT_CPP