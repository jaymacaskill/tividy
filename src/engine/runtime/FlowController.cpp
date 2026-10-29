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

FlowController.cpp
*/

#ifndef TIVIDY_RUNTIME_FLOWCONTROLLER_CPP
#define TIVIDY_RUNTIME_FLOWCONTROLLER_CPP

#include "engine/runtime/FlowController.h"

#include "engine/core/Errors.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemVisitor.h"
#include "engine/runtime/WorkflowInstance.h"

namespace tividy
{
	namespace
	{
		// Reads the TaskItem behind an item, seeing through decorators (they forward accept()).
		class TaskReader : public WorkItemVisitor
		{
			public:
				TaskReader() : task(nullptr) { }

				void visitTask(const TaskItem& t) override
				{
					if (task == nullptr) task = &t;
				}

				void visitStage(const StageItem&) override { }

				const TaskItem* found() const { return task; }
			private:
				const TaskItem* task;
		};

		const TaskItem* taskBehind(const WorkItem& item)
		{
			TaskReader reader;
			item.accept(reader);
			return reader.found();
		}

		void publishNote(WorkflowInstance& instance, const Id& item_id, const std::string& message)
		{
			instance.events().notify(WorkflowEvent(EventType::EngineNote, instance.id(), item_id, instance.context().clock().now())
				.byActor("system")
				.withMessage(message));
		}

		bool isDeadlineStatus(ItemStatus status)
		{
			return status == ItemStatus::Assigned
				|| status == ItemStatus::InProgress
				|| status == ItemStatus::UnderReview;
		}
	}

	FlowController::FlowController(WorkflowInstance& instance, const DefinitionPolicies& policies) :
		instance(instance), policies(policies), pending(), draining(false) { }

	FlowController::~FlowController() { }

	void FlowController::onEvent(const WorkflowEvent& event)
	{
		pending.push_back(event);
		if (!draining) drain();
	}

	void FlowController::drain()
	{
		draining = true;
		while (!pending.empty())
		{
			WorkflowEvent event = pending.front();
			pending.pop_front();

			try
			{
				react(event);
			}
			catch (const EngineError& e)
			{
				publishNote(instance, event.itemId(), e.what());
			}
			catch (...)
			{
				draining = false;
				throw;
			}
		}
		draining = false;
	}

	void FlowController::react(const WorkflowEvent& event)
	{
		const EventType type = event.type();

		if (type == EventType::ItemCancelled)
		{
			if (instance.status() == InstanceStatus::Running)
				instance.finish(InstanceStatus::Failed, "item cancelled: " + event.itemId());
			return;
		}

		if (type == EventType::ExternalSignal)
		{
			if (policies.cancel_signals.count(event.message()) != 0)
				instance.cancel("signal: " + event.message());
			return;
		}

		if (instance.status() != InstanceStatus::Running) return;

		if (type == EventType::ItemSkipped)
		{
			releaseEligible();
			evaluateInstance();
			return;
		}

		if (type != EventType::ItemAvailable && type != EventType::ItemAssigned && type != EventType::ItemCompleted
			&& type != EventType::ItemFailed && type != EventType::ItemEscalated && type != EventType::DeadlineMissed)
			return;

		WorkItem* found = instance.findItem(event.itemId());
		if (found == nullptr)
		{
			publishNote(instance, event.itemId(), "unknown item '" + event.itemId() + "' in " + toString(type) + " event");
			return;
		}
		WorkItem& item = *found;

		switch (type)
		{
			case EventType::ItemAvailable:
				assignIfPossible(item);
				break;

			case EventType::ItemAssigned:
				if (item.spec().isAutomatic() && item.status() == ItemStatus::Assigned)
					runAutomatic(item);
				break;

			case EventType::ItemCompleted:
			{
				std::map<Id, std::shared_ptr<const RoutingStrategy> >::const_iterator it = policies.routing.find(item.spec().id());
				static const DependencyRouting dependency_routing;
				const RoutingStrategy& routing = (it != policies.routing.end() && it->second) ? *it->second : dependency_routing;

				const TaskItem* task = taskBehind(item);
				const Outcome outcome = (task != nullptr) ? task->lastOutcome() : event.outcome();

				applyDecision(routing.decide(item, outcome, instance.data()));
				releaseEligible();
				evaluateInstance();
				break;
			}

			case EventType::ItemFailed:
				if (event.outcome().code() == "RETRYABLE" && event.count() <= item.spec().maxRetries())
				{
					if (item.spec().isAutomatic()) runAutomatic(item);
				}
				else
				{
					item.escalate(event.message());
				}
				break;

			case EventType::ItemEscalated:
			{
				EscalationResult result;
				result.resolved = false;
				if (policies.escalation)
				{
					EscalationRequest request;
					request.instance_id = instance.id();
					request.item_id = item.spec().id();
					request.reason = event.message();
					request.level = event.count();
					request.data = &instance.data();
					result = policies.escalation->handle(request);
				}

				if (result.resolved)
				{
					const Id id = policies.assignment->chooseParticipant(item, result.approver_role,
						instance.context().directory(), instance.data());
					if (!id.empty()) item.assign(id);
				}
				else
				{
					publishNote(instance, item.spec().id(), "escalation unresolved for " + item.spec().id());
				}
				break;
			}

			case EventType::DeadlineMissed:
				if (isDeadlineStatus(item.status()))
				{
					if (!item.spec().timeoutCode().empty())
						item.expire(item.spec().timeoutCode(), "deadline lapsed");
					else
						item.escalate("deadline missed");
				}
				break;

			default:
				break;
		}
	}

	void FlowController::runAutomatic(WorkItem& item)
	{
		try
		{
			const Participant& actor = instance.context().directory().require(item.assignee());
			if (item.status() == ItemStatus::Assigned) item.start(actor);   // skipped on a retry: the item is still InProgress
			item.complete(actor, Outcome::accepted());
		}
		catch (const ExternalFailure&)
		{
			throw;   // TaskItem publishes ItemFailed for these itself
		}
		catch (const EngineError& e)
		{
			const TaskItem* task = taskBehind(item);
			const unsigned failures = (task != nullptr) ? task->failures() : 0;
			instance.events().notify(WorkflowEvent(EventType::ItemFailed, instance.id(), item.spec().id(), instance.context().clock().now())
				.byActor("system")
				.withOutcome(Outcome::failed("PERMANENT", e.what()))
				.withMessage(e.what())
				.withCount(failures + 1));
		}
	}
} // namespace tividy

#endif // TIVIDY_RUNTIME_FLOWCONTROLLER_CPP