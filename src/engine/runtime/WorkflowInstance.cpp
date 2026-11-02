/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 02 November 2026
*/

/**
 * @file WorkflowInstance.cpp
 * @brief Implements WorkflowInstance, one running copy of a workflow.
 * @details Holds the item tree, workflow data, event publisher, audit log,
 * action history and shared context, and implements lookup of items for a
 * participant plus the start, cancel and finish transitions.
 */

#ifndef TIVIDY_RUNTIME_WORKFLOWINSTANCE_CPP
#define TIVIDY_RUNTIME_WORKFLOWINSTANCE_CPP

#include "engine/runtime/WorkflowInstance.h"

#include "engine/core/Errors.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemVisitor.h"

namespace tividy
{
	namespace
	{
		// Decides whether ONE item is a task: TaskItem::accept() calls visitTask first, StageItem::accept() calls visitStage
		// first, and decorators forward accept(), so a decorated task is still reported as a task.
		class TaskProbe : public WorkItemVisitor
		{
			public:
				TaskProbe() : decided(false), task(false) { }

				void visitTask(const TaskItem&) override
				{
					if (!decided)
					{
						decided = true;
						task = true;
					}
				}

				void visitStage(const StageItem&) override
				{
					if (!decided)
					{
						decided = true;
						task = false;
					}
				}

				bool isTask() const { return task; }
			private:
				bool decided;
				bool task;
		};

		bool isTask(const WorkItem& item)
		{
			TaskProbe probe;
			item.accept(probe);
			return probe.isTask();
		}

		bool isActiveFor(ItemStatus status)
		{
			return status == ItemStatus::Assigned
				|| status == ItemStatus::InProgress
				|| status == ItemStatus::UnderReview
				|| status == ItemStatus::Escalated;
		}
	}

	WorkflowInstance::WorkflowInstance(const Id& id, const std::string& definition_name, unsigned definition_version,
									   const WorkflowData& data, std::unique_ptr<WorkItem> root, const DefinitionPolicies& policies,
									   Services& services) :
		id_ref(id),
		definition_name(definition_name),
		definition_version(definition_version),
		data_ref(data),
		events_ref(),
		audit_ref(),
		audit_observer(audit_ref),
		root_ref(std::move(root)),
		context_ref(id, data_ref, events_ref, services.clock, services.connectors, services.directory),
		flow(*this, policies),
		deadlines(*this, services.clock),
		actions_ref(*this),
		extra_observers()
	{
		root_ref->bind(context_ref);

		events_ref.attach(&audit_observer);
		events_ref.attach(&flow);

		status_ref = InstanceStatus::Created;
	}

	WorkflowInstance::~WorkflowInstance() { }

	const Id& WorkflowInstance::id() const
	{
		return id_ref;
	}

	const std::string& WorkflowInstance::definitionName() const
	{
		return definition_name;
	}

	unsigned WorkflowInstance::definitionVersion() const
	{
		return definition_version;
	}

	InstanceStatus WorkflowInstance::status() const
	{
		return status_ref;
	}

	WorkflowData& WorkflowInstance::data()
	{
		return data_ref;
	}

	const WorkflowData& WorkflowInstance::data() const
	{
		return data_ref;
	}

	WorkItem& WorkflowInstance::root()
	{
		return *root_ref;
	}

	const WorkItem& WorkflowInstance::root() const
	{
		return *root_ref;
	}

	WorkItem* WorkflowInstance::findItem(const Id& id)
	{
		return root_ref->find(id);
	}

	std::vector<WorkItem*> WorkflowInstance::itemsFor(const Id& participant_id)
	{
		std::vector<WorkItem*> items;

		std::unique_ptr<WorkItemIterator> it = root_ref->createIterator();
		for (it->first(); !it->isDone(); it->next())
		{
			WorkItem& item = it->currentItem();
			if (!isTask(item)) continue;
			if (item.assignee() != participant_id) continue;
			if (!isActiveFor(item.status())) continue;
			items.push_back(&item);
		}

		return items;
	}

	EventPublisher& WorkflowInstance::events()
	{
		return events_ref;
	}

	AuditLog& WorkflowInstance::audit()
	{
		return audit_ref;
	}

	const AuditLog& WorkflowInstance::audit() const
	{
		return audit_ref;
	}

	ActionHistory& WorkflowInstance::actions()
	{
		return actions_ref;
	}

	WorkContext& WorkflowInstance::context()
	{
		return context_ref;
	}

		void WorkflowInstance::addObserver(std::unique_ptr<EventObserver> observer)
	{
		if (!observer)
			throw InvalidStateError("cannot add a null observer to instance " + id_ref);

		events_ref.attach(observer.get());
		extra_observers.push_back(std::move(observer));
	}

	void WorkflowInstance::start()
	{
		if (status_ref != InstanceStatus::Created)
			throw InvalidStateError("cannot start instance " + id_ref + ": it is " + toString(status_ref) + ", not Created");

		status_ref = InstanceStatus::Running;
		events_ref.notify(WorkflowEvent(EventType::InstanceStarted, id_ref, "", context_ref.clock().now())
			.byActor("system")
			.withMessage("started from " + definition_name + " v" + std::to_string(definition_version)));
		flow.releaseEligible();
	}

	void WorkflowInstance::cancel(const std::string& reason)
	{
		if (status_ref != InstanceStatus::Running)
			throw InvalidStateError("cannot cancel instance " + id_ref + ": it is " + toString(status_ref) + ", not Running");

		status_ref = InstanceStatus::Cancelled;
		events_ref.notify(WorkflowEvent(EventType::InstanceCancelled, id_ref, "", context_ref.clock().now())
			.byActor("system")
			.withMessage(reason));
		root_ref->cancel(reason);
	}

	void WorkflowInstance::finish(InstanceStatus final_status, const std::string& message)
	{
		if (status_ref != InstanceStatus::Running) return;

		status_ref = final_status;
		EventType type = (final_status == InstanceStatus::Completed) ? EventType::InstanceCompleted : EventType::InstanceFailed;
		events_ref.notify(WorkflowEvent(type, id_ref, "", context_ref.clock().now())
			.byActor("system")
			.withMessage(message));
	}
} // namespace tividy

#endif // TIVIDY_RUNTIME_WORKFLOWINSTANCE_CPP