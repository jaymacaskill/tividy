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

Types.cpp
*/

#ifndef TIVIDY_CORE_TYPES_CPP
#define TIVIDY_CORE_TYPES_CPP

#include "engine/core/Types.h"

// these look like a "huge switch statement" but it's not replacing what could be a pattern, so it's fine, they're lookups

// unreachable returns at the end (prevents reached end of a non void function), if the switch doesn't find a case, this won't compile
// which is good because we will immediately know to update the lookup, so I have no default case here

namespace tividy
{
	bool isSettled(ItemStatus status)
	{
		return status == ItemStatus::Completed 
			|| status == ItemStatus::Skipped;
	}

	bool isTerminal(ItemStatus status)
	{
		return status == ItemStatus::Completed 
			|| status == ItemStatus::Cancelled 
			|| status == ItemStatus::Skipped;
	}

	std::string toString(ItemStatus status)
	{
		switch (status)
		{
			case ItemStatus::Created:
				return "Created";

			case ItemStatus::Available:
				return "Available";

			case ItemStatus::Assigned:
				return "Assigned";
			case ItemStatus::InProgress:
				return "InProgress";

			case ItemStatus::UnderReview:
				return "UnderReview";

			case ItemStatus::Escalated:
				return "Escalated";

			case ItemStatus::Completed:
				return "Completed";

			case ItemStatus::Cancelled:
				return "Cancelled";

			case ItemStatus::Skipped:
				return "Skipped";
		}

		return "ItemStatus lookup outdated";
	}

	std::string toString(InstanceStatus status)
	{
		switch (status)
		{
			case InstanceStatus::Created:
				return "Created";

			case InstanceStatus::Running:
				return "Running";

			case InstanceStatus::Completed:
				return "Completed";

			case InstanceStatus::Failed:
				return "Failed";

			case InstanceStatus::Cancelled:
				return "Cancelled";
		}

		return "InstanceStatus lookup outdated";
	}

	std::string toString(OutcomeKind kind)
	{
		switch (kind)
		{
			case OutcomeKind::Accepted:
				return "Accepted";

			case OutcomeKind::Escalated:
				return "Escalated";

			case OutcomeKind::Failed:
				return "Failed";

			case OutcomeKind::Rejected:
				return "Rejected";
		}

		return "OutcomeKind lookup outdated";
	}

	std::string toString(EventType type)
	{
		switch (type)
		{
			case EventType::DeadlineMissed:
				return "DeadlineMissed";

			case EventType::EngineNote:
				return "EngineNote";

			case EventType::ExternalSignal:
				return "ExternalSignal";

			case EventType::InstanceCancelled:
				return "InstanceCancelled";

			case EventType::InstanceCompleted:
				return "InstanceCompleted";

			case EventType::InstanceFailed:
				return "InstanceFailed";

			case EventType::InstanceStarted:
				return "InstanceStarted";

			case EventType::ItemAssigned:
				return "ItemAssigned";

			case EventType::ItemAvailable:
				return "ItemAvailable";

			case EventType::ItemCancelled:
				return "ItemCancelled";

			case EventType::ItemCompleted:
				return "ItemCompleted";

			case EventType::ItemEscalated:
				return "ItemEscalated";

			case EventType::ItemStarted:
				return "ItemStarted";

			case EventType::ItemSubmitted:
				return "ItemSubmitted";

			case EventType::ItemRejected:
				return "ItemRejected";

			case EventType::ItemSkipped:
				return "ItemSkipped";

			case EventType::ItemFailed:
				return "ItemFailed";

			case EventType::ItemReverted:
				return "ItemReverted";
		}

		return "EventType lookup outdated";
	}
} // namespace tividy

#endif // TIVIDY_CORE_TYPES_CPP