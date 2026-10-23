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
 * @file Types.h
 * @brief Vocabulary types shared by every TIVIDY subsystem. Knows nothing about any industry.
 */

#ifndef TIVIDY_CORE_TYPES_H
#define TIVIDY_CORE_TYPES_H

#include <chrono>
#include <string>

namespace tividy
{

	/** Opaque identifier. Item ids are unique inside one definition; instance ids are unique per Tividy. */
	typedef std::string Id;

	/** Wall-clock instant. Always obtained from a Clock. */
	typedef std::chrono::system_clock::time_point TimePoint;

	/** Lifecycle of one work item (legal transitions: see WorkItemState). */
	enum class ItemStatus
	{
		Created,     /**< In the tree, dependencies not yet settled. */
		Available,   /**< Ready; nobody assigned. */
		Assigned,    /**< Has an assignee; not started. */
		InProgress,  /**< Assignee is working on it. */
		UnderReview, /**< Submitted; waiting for approve() or reject(). */
		Escalated,   /**< Taken off the normal path; an escalation approver must decide. */
		Completed,   /**< Finished successfully. Terminal. */
		Cancelled,   /**< Abandoned or terminated. Terminal. */
		Skipped      /**< A routing decision chose a different branch. Terminal. */
	};

	/** Lifecycle of a workflow instance. */
	enum class InstanceStatus { Created, Running, Completed, Failed, Cancelled };

	/** Business result category (not a lifecycle state): Accepted, Rejected, Escalated, Failed. */
	enum class OutcomeKind { Accepted, Rejected, Escalated, Failed };

	/** Every kind of fact published on an instance's EventPublisher. */
	enum class EventType
	{
		InstanceStarted, InstanceCompleted, InstanceFailed, InstanceCancelled,
		ItemAvailable, ItemAssigned, ItemStarted, ItemSubmitted, ItemCompleted, ItemRejected,
		ItemEscalated, ItemCancelled, ItemSkipped, ItemFailed, ItemReverted,
		DeadlineMissed,  /**< An item's due time passed (see DeadlineMonitor). */
		ExternalSignal,
		EngineNote   /**< Audit-only remark by the engine (ignored routing id, unresolved escalation). Triggers nothing. */
	};

	/** @return the enumerator's name as text, e.g. ItemStatus::UnderReview -> "UnderReview". Never empty. */
	std::string toString(ItemStatus status);

	std::string toString(InstanceStatus status);   /**< @copydoc toString(ItemStatus) */
	std::string toString(OutcomeKind kind);        /**< @copydoc toString(ItemStatus) */
	std::string toString(EventType type);          /**< @copydoc toString(ItemStatus) */

	/** @return true for Completed or Skipped: the item no longer blocks its dependants. */
	bool isSettled(ItemStatus status);

	/** @return true for Completed, Cancelled or Skipped: no further transition is legal. */
	bool isTerminal(ItemStatus status);
} // namespace tividy

#endif // TIVIDY_CORE_TYPES_H