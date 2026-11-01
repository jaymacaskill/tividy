/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 29 October 2026
*/

/**
 * @file WorkflowInstanceSnapshot.cpp
 * @brief Instance-level mementos, silent restoration and deadline-monitor delegation.
 */

#include <memory>
#include <utility>

#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemMemento.h"
#include "engine/runtime/WorkflowInstance.h"

tividy::InstanceMemento::InstanceMemento() : status(tividy::InstanceStatus::Created) {}
tividy::InstanceMemento::~InstanceMemento() = default;

std::unique_ptr<tividy::InstanceMemento> tividy::WorkflowInstance::captureSnapshot() const
{
	std::unique_ptr<tividy::InstanceMemento> snapshot(new tividy::InstanceMemento);
	snapshot->data = data_ref;
	snapshot->status = status_ref;

	std::unique_ptr<tividy::WorkItemIterator> iterator = root_ref->createIterator();
	for (iterator->first(); !iterator->isDone(); iterator->next())
	{
		tividy::WorkItem& item = iterator->currentItem();
		std::unique_ptr<tividy::WorkItemMemento> item_snapshot = item.createMemento();
		if (item_snapshot)
		{
			snapshot->items[item.spec().id()] = std::move(item_snapshot);
		}
	}
	return snapshot;
}

void tividy::WorkflowInstance::restoreSnapshot(const tividy::InstanceMemento& snapshot)
{
	for (const auto& saved : snapshot.items)
	{
		if (tividy::WorkItem* item = root_ref->find(saved.first))
		{
			item->restore(*saved.second);
		}
	}
	data_ref = snapshot.data;
	status_ref = snapshot.status;
}

std::size_t tividy::WorkflowInstance::sweepDeadlines()
{
	return deadlines.sweep();
}
