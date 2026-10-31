/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)
 
COS 214 (Software Modelling) TIVIDY
Last Modified: 31 October 2026
*/

/**
 * @file WorkItemMemento.cpp
 * @brief Implements WorkItemMemento (Memento pattern): the saved copy of one task's status, assignee and counters, used for undo.
 */

#ifndef TIVIDY_ITEMS_WORKITEMMEMENTO_CPP
#define TIVIDY_ITEMS_WORKITEMMEMENTO_CPP
 
#include "engine/items/WorkItemMemento.h"

namespace tividy
{
	WorkItemMemento::~WorkItemMemento() { }

	WorkItemMemento::WorkItemMemento()
		: status(ItemStatus::Created),
		  assignee(""),
		  assigned_at(TimePoint()),
		  rejections(0),
		  failures(0),
		  escalations(0),
		  last_outcome_kind(OutcomeKind::Accepted),
		  last_outcome_code(),
		  last_outcome_note(),
		  completed_by()
	{}
} // namespace tividy

#endif // TIVIDY_ITEMS_WORKITEMMEMENTO_CPP  
