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
 * @file WorkItem.cpp
 * @brief Implements the default behaviour of the WorkItem base class (Composite pattern): no children, no snapshot, not reversible, until a subclass says otherwise.
 */

#ifndef TIVIDY_ITEMS_WORKITEM_CPP
#define TIVIDY_ITEMS_WORKITEM_CPP
 
#include "engine/items/WorkItem.h"
 
#include "engine/core/Errors.h"
#include "engine/items/WorkItemMemento.h"


namespace tividy
{
	WorkItem::~WorkItem() { }

	TimePoint WorkItem::assignedAt() const {
		return TimePoint();
	}

	TimePoint WorkItem::dueAt() const {
		return TimePoint();
	}

	std::vector<WorkItem*> WorkItem::children() {
		return std::vector<WorkItem*>();
	}

	WorkItem& WorkItem::add(std::unique_ptr<WorkItem> /* child */) {
		throw UnsupportedOperationError("Work item " + spec().id() + " cannot have children: add() is only supported by a StageItem");
	}
	
	std::unique_ptr<WorkItemMemento> WorkItem::createMemento() const {
		return std::unique_ptr<WorkItemMemento>();
	}
	
	void WorkItem::restore(const WorkItemMemento& /* memento */) {
		// Do nothing 
	}

	bool WorkItem::isReversible() const {
		return true;
	}


} // namespace tividy

#endif // TIVIDY_ITEMS_WORKITEM_CPP
