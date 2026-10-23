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
 * @file WorkItemMemento.h
 * @brief Memento pattern (item level): opaque snapshot of one TaskItem's runtime state.
 */

#ifndef TIVIDY_ITEMS_WORKITEMMEMENTO_H
#define TIVIDY_ITEMS_WORKITEMMEMENTO_H

#include <string>

#include "engine/core/Types.h"

namespace tividy
{
	class TaskItem;

	/**
	 * @brief Memento. Narrow interface to everyone else, wide interface to its Originator only.
	 *
	 * The caretaker (InstanceMemento, held by a Command) can store and pass it back but cannot read
	 * or change its contents: every member is private and only TaskItem (the Originator) is a friend.
	 * It stores: status, assignee, assigned_at, rejection/failure/escalation counters, the last outcome and who completed the item.
	 * It does not store the item's description (spec), which never changes at runtime.
	 */
	class WorkItemMemento
	{
		public:
			~WorkItemMemento();
		private:
			friend class TaskItem;

			WorkItemMemento();

			ItemStatus status;
			Id assignee;
			TimePoint assigned_at;
			unsigned rejections;
			unsigned failures;
			unsigned escalations;
			OutcomeKind last_outcome_kind;
			std::string last_outcome_code;
			std::string last_outcome_note;
			Id completed_by;
	};
} // namespace tividy

#endif // TIVIDY_ITEMS_WORKITEMMEMENTO_H