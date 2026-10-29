#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Clock.h"
#include "engine/core/Errors.h"
#include "engine/core/Participant.h"
#include "engine/core/WorkflowData.h"
#include "engine/definition/WorkflowDefinition.h"
#include "engine/events/Observer.h"
#include "engine/events/WorkflowEvent.h"
#include "engine/items/StageItem.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/policies/AssignmentStrategy.h"
#include "engine/policies/EscalationHandler.h"
#include "engine/policies/RoutingStrategy.h"
#include "engine/runtime/FlowController.h"
#include "engine/runtime/WorkflowInstance.h"

namespace tividy
{
	namespace
	{
		class Harness
		{
			public:
				Harness() :
					clock(TimePoint() + std::chrono::seconds(1793190896)), directory(), connectors(),
					services{clock, directory, connectors}
				{
					directory.add(Participant("u1", "User One", {"clerk"}));
					directory.add(Participant("u2", "User Two", {"clerk"}));
					directory.add(Participant("boss", "Boss", {"approver"}));
					directory.add(Participant("system", "System", {"system"}));
				}

				std::unique_ptr<WorkflowInstance> make(std::unique_ptr<WorkItem> root, const DefinitionPolicies& policies)
				{
					return std::unique_ptr<WorkflowInstance>(new WorkflowInstance("inst-1", "demo", 1, WorkflowData(), std::move(root), policies, services));
				}

				void publish(WorkflowInstance& inst, EventType type, const Id& item_id, const std::string& message = "")
				{
					inst.events().notify(WorkflowEvent(type, inst.id(), item_id, clock.now()).withMessage(message));
				}

				ManualClock clock;
				ParticipantDirectory directory;
				ConnectorRegistry connectors;
				Services services;
		};

		class RecordingObserver : public EventObserver
		{
			public:
				void onEvent(const WorkflowEvent& event) override { received_ref.push_back(event); }
				const std::vector<WorkflowEvent>& received() const { return received_ref; }
			private:
				std::vector<WorkflowEvent> received_ref;
		};

		/** Throws ExternalFailure from performWork. remaining < 0 means it never succeeds. */
		class FailingTask : public CloneableTask<FailingTask>
		{
			public:
				FailingTask(const WorkItemSpec& spec, int remaining, bool retryable) :
					CloneableTask<FailingTask>(spec), remaining(remaining), retryable(retryable) { }
			protected:
				void performWork(const Participant&, Outcome&) override
				{
					if (remaining == 0) return;
					if (remaining > 0) remaining--;
					throw ExternalFailure("vendor down", retryable);
				}
			private:
				int remaining;
				bool retryable;
		};

		/** Throws an EngineError that is not an ExternalFailure. */
		class InvalidTask : public CloneableTask<InvalidTask>
		{
			public:
				explicit InvalidTask(const WorkItemSpec& spec) : CloneableTask<InvalidTask>(spec) { }
			protected:
				void performWork(const Participant&, Outcome&) override
				{
					throw ValidationError("bad data");
				}
		};

		DefinitionPolicies defaultPolicies()
		{
			DefinitionPolicies policies;
			policies.assignment = std::make_shared<RoleBasedAssignment>();
			return policies;
		}

		WorkItemSpec clerkSpec(const Id& id)
		{
			return WorkItemSpec(id, "Task " + id).requireRole("clerk");
		}

		WorkItemSpec automaticSpec(const Id& id)
		{
			return WorkItemSpec(id, "Task " + id).requireRole("system").automatic(true);
		}

		template <class T>
		T* addTask(StageItem& stage, T* task)
		{
			stage.add(std::unique_ptr<WorkItem>(task));
			return task;
		}

		std::unique_ptr<StageItem> makeRoot()
		{
			return std::unique_ptr<StageItem>(new StageItem(WorkItemSpec("root", "Root")));
		}

		std::size_t countType(const AuditLog& log, EventType type)
		{
			std::size_t n = 0;
			for (const AuditEntry& entry : log.entries())
			{
				if (entry.type == type) n++;
			}
			return n;
		}

		const AuditEntry* firstOfType(const AuditLog& log, EventType type)
		{
			for (const AuditEntry& entry : log.entries())
			{
				if (entry.type == type) return &entry;
			}
			return nullptr;
		}

		std::vector<WorkflowEvent> eventsOfType(const RecordingObserver& recorder, EventType type)
		{
			std::vector<WorkflowEvent> found;
			for (const WorkflowEvent& event : recorder.received())
			{
				if (event.type() == type) found.push_back(event);
			}
			return found;
		}

		void finishTask(Harness& h, WorkItem& item, const Id& participant_id, const Outcome& outcome = Outcome::accepted())
		{
			const Participant& p = h.directory.require(participant_id);
			item.start(p);
			item.complete(p, outcome);
		}
	}

	TEST(FlowControllerTest, AvailableItemIsAssignedByStrategy)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(clerkSpec("a")));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());

		inst->start();

		EXPECT_EQ(a->status(), ItemStatus::Assigned);
		EXPECT_EQ(a->assignee(), "u1");
	}

	TEST(FlowControllerTest, NoEligibleParticipantLeavesItemAvailable)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(WorkItemSpec("a", "Task a").requireRole("nobody-has-this")));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());

		inst->start();

		EXPECT_EQ(a->status(), ItemStatus::Available);
		EXPECT_EQ(a->assignee(), "");
	}

	TEST(FlowControllerTest, AutomaticItemRunsToCompletion)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(automaticSpec("a")));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());

		inst->start();

		EXPECT_EQ(a->status(), ItemStatus::Completed);
		EXPECT_EQ(a->completedBy(), "system");
		EXPECT_EQ(inst->status(), InstanceStatus::Completed);

		std::vector<EventType> item_events;
		for (const AuditEntry& entry : inst->audit().entries())
		{
			if (entry.item_id == "a") item_events.push_back(entry.type);
		}
		ASSERT_EQ(item_events.size(), 4u);
		EXPECT_EQ(item_events[0], EventType::ItemAvailable);
		EXPECT_EQ(item_events[1], EventType::ItemAssigned);
		EXPECT_EQ(item_events[2], EventType::ItemStarted);
		EXPECT_EQ(item_events[3], EventType::ItemCompleted);
	}

	TEST(FlowControllerTest, CompletionReleasesDependants)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(clerkSpec("a")));
		ManualTask* b = addTask(*root, new ManualTask(clerkSpec("b").dependsOn("a")));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());
		inst->start();
		EXPECT_EQ(b->status(), ItemStatus::Created);

		finishTask(h, *a, "u1");
		EXPECT_EQ(b->status(), ItemStatus::Assigned);
		EXPECT_EQ(inst->status(), InstanceStatus::Running);

		finishTask(h, *b, "u1");
		EXPECT_EQ(inst->status(), InstanceStatus::Completed);
	}

	TEST(FlowControllerTest, RoutingStrategyActivatesAndSkipsBranches)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(clerkSpec("a")));
		ManualTask* fix = addTask(*root, new ManualTask(clerkSpec("fix").dependsOn("a")));
		ManualTask* ok = addTask(*root, new ManualTask(clerkSpec("ok").dependsOn("a")));
		DefinitionPolicies policies = defaultPolicies();
		std::shared_ptr<OutcomeTableRouting> table = std::make_shared<OutcomeTableRouting>();
		table->addRule("DOCS_INVALID", {"fix"}, {"ok"});
		policies.routing["a"] = table;
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), policies);
		inst->start();

		finishTask(h, *a, "u1", Outcome::rejected("DOCS_INVALID"));

		EXPECT_EQ(fix->status(), ItemStatus::Assigned);
		EXPECT_EQ(ok->status(), ItemStatus::Skipped);
		EXPECT_EQ(inst->status(), InstanceStatus::Running);
	}

	TEST(FlowControllerTest, WithoutRoutingRuleBothBranchesAreReleased)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(clerkSpec("a")));
		ManualTask* fix = addTask(*root, new ManualTask(clerkSpec("fix").dependsOn("a")));
		ManualTask* ok = addTask(*root, new ManualTask(clerkSpec("ok").dependsOn("a")));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());
		inst->start();

		finishTask(h, *a, "u1", Outcome::rejected("DOCS_INVALID"));

		EXPECT_EQ(fix->status(), ItemStatus::Assigned);
		EXPECT_EQ(ok->status(), ItemStatus::Assigned);
	}

	TEST(FlowControllerTest, RetryableFailureIsRetried)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		FailingTask* a = addTask(*root, new FailingTask(automaticSpec("a"), 1, true));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());
		RecordingObserver* recorder = new RecordingObserver();
		inst->addObserver(std::unique_ptr<EventObserver>(recorder));

		inst->start();

		EXPECT_EQ(a->status(), ItemStatus::Completed);
		EXPECT_EQ(a->failures(), 1u);
		std::vector<WorkflowEvent> failed = eventsOfType(*recorder, EventType::ItemFailed);
		ASSERT_EQ(failed.size(), 1u);
		EXPECT_EQ(failed[0].outcome().code(), "RETRYABLE");
		EXPECT_EQ(failed[0].count(), 1u);
		EXPECT_EQ(inst->status(), InstanceStatus::Completed);
	}

	TEST(FlowControllerTest, RetryableFailureBeyondMaxRetriesEscalates)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		FailingTask* a = addTask(*root, new FailingTask(automaticSpec("a").maxRetries(1), -1, true));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());

		inst->start();

		EXPECT_EQ(a->status(), ItemStatus::Escalated);
		EXPECT_EQ(a->failures(), 2u);
		EXPECT_EQ(countType(inst->audit(), EventType::ItemFailed), 2u);
		EXPECT_EQ(inst->status(), InstanceStatus::Running);
	}

	TEST(FlowControllerTest, PermanentExternalFailureEscalatesWithoutRetry)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		FailingTask* a = addTask(*root, new FailingTask(automaticSpec("a"), -1, false));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());

		inst->start();

		EXPECT_EQ(a->status(), ItemStatus::Escalated);
		EXPECT_EQ(a->failures(), 1u);
		EXPECT_EQ(countType(inst->audit(), EventType::ItemFailed), 1u);
	}

	TEST(FlowControllerTest, OtherEngineErrorBecomesPermanentItemFailed)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		InvalidTask* a = addTask(*root, new InvalidTask(automaticSpec("a")));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());
		RecordingObserver* recorder = new RecordingObserver();
		inst->addObserver(std::unique_ptr<EventObserver>(recorder));

		inst->start();

		std::vector<WorkflowEvent> failed = eventsOfType(*recorder, EventType::ItemFailed);
		ASSERT_EQ(failed.size(), 1u);
		EXPECT_EQ(failed[0].itemId(), "a");
		EXPECT_EQ(failed[0].actorId(), "system");
		EXPECT_EQ(failed[0].outcome().kind(), OutcomeKind::Failed);
		EXPECT_EQ(failed[0].outcome().code(), "PERMANENT");
		EXPECT_EQ(failed[0].outcome().note(), "bad data");
		EXPECT_EQ(failed[0].message(), "bad data");
		EXPECT_EQ(failed[0].count(), 1u);
		EXPECT_EQ(a->failures(), 0u);
		EXPECT_EQ(a->status(), ItemStatus::Escalated);
	}

	TEST(FlowControllerTest, ResolvedEscalationAssignsApprover)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(clerkSpec("a").dueAfter(60)));
		DefinitionPolicies policies = defaultPolicies();
		policies.escalation = std::make_shared<RoleEscalationHandler>("approver", 1);
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), policies);
		inst->start();

		h.clock.advance(61);
		EXPECT_EQ(inst->sweepDeadlines(), 1u);

		EXPECT_EQ(a->status(), ItemStatus::Escalated);
		EXPECT_EQ(a->assignee(), "boss");
		EXPECT_EQ(countType(inst->audit(), EventType::EngineNote), 0u);
	}

	TEST(FlowControllerTest, UnresolvedEscalationPublishesEngineNote)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(clerkSpec("a").dueAfter(60)));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());
		inst->start();

		h.clock.advance(61);
		inst->sweepDeadlines();

		EXPECT_EQ(a->status(), ItemStatus::Escalated);
		const AuditEntry* note = firstOfType(inst->audit(), EventType::EngineNote);
		ASSERT_NE(note, nullptr);
		EXPECT_EQ(note->message, "escalation unresolved for a");
	}

	TEST(FlowControllerTest, DeadlineWithTimeoutCodeExpiresItem)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(clerkSpec("a").dueAfter(60).onTimeout("NO_RESPONSE")));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());
		inst->start();

		h.clock.advance(61);
		inst->sweepDeadlines();

		EXPECT_EQ(a->status(), ItemStatus::Completed);
		EXPECT_EQ(a->lastOutcome().kind(), OutcomeKind::Rejected);
		EXPECT_EQ(a->lastOutcome().code(), "NO_RESPONSE");
		EXPECT_EQ(inst->status(), InstanceStatus::Completed);
	}

	TEST(FlowControllerTest, CancelledItemFailsInstance)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(clerkSpec("a")));
		addTask(*root, new ManualTask(clerkSpec("b")));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());
		inst->start();

		a->cancel("lost paperwork");

		EXPECT_EQ(inst->status(), InstanceStatus::Failed);
		const AuditEntry* failed = firstOfType(inst->audit(), EventType::InstanceFailed);
		ASSERT_NE(failed, nullptr);
		EXPECT_EQ(failed->message, "item cancelled: a");
	}

	TEST(FlowControllerTest, CancelSignalCancelsInstance)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(clerkSpec("a")));
		DefinitionPolicies policies = defaultPolicies();
		policies.cancel_signals.insert("customer_cancel");
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), policies);
		inst->start();

		h.publish(*inst, EventType::ExternalSignal, "", "something_else");
		EXPECT_EQ(inst->status(), InstanceStatus::Running);

		h.publish(*inst, EventType::ExternalSignal, "", "customer_cancel");
		EXPECT_EQ(inst->status(), InstanceStatus::Cancelled);
		EXPECT_EQ(a->status(), ItemStatus::Cancelled);
		EXPECT_EQ(countType(inst->audit(), EventType::InstanceFailed), 0u);
	}

	TEST(FlowControllerTest, UnknownItemPublishesEngineNote)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		addTask(*root, new ManualTask(clerkSpec("a")));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());
		inst->start();

		h.publish(*inst, EventType::ItemAvailable, "ghost");

		const AuditEntry* note = firstOfType(inst->audit(), EventType::EngineNote);
		ASSERT_NE(note, nullptr);
		EXPECT_EQ(note->item_id, "ghost");
		EXPECT_EQ(note->message, "unknown item 'ghost' in ItemAvailable event");
	}

	TEST(FlowControllerTest, ReactionsAreIgnoredWhenInstanceIsNotRunning)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		addTask(*root, new ManualTask(clerkSpec("a")));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), defaultPolicies());

		h.publish(*inst, EventType::ItemAvailable, "ghost");

		EXPECT_EQ(countType(inst->audit(), EventType::EngineNote), 0u);
		EXPECT_EQ(inst->status(), InstanceStatus::Created);
	}

	TEST(FlowControllerTest, EngineErrorInReactionBecomesNoteAndProcessingContinues)
	{
		Harness h;
		std::unique_ptr<StageItem> root = makeRoot();
		ManualTask* a = addTask(*root, new ManualTask(clerkSpec("a")));
		DefinitionPolicies policies = defaultPolicies();
		policies.cancel_signals.insert("stop");
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), policies);

		EXPECT_NO_THROW(h.publish(*inst, EventType::ExternalSignal, "", "stop"));
		EXPECT_EQ(countType(inst->audit(), EventType::EngineNote), 1u);
		EXPECT_EQ(inst->status(), InstanceStatus::Created);

		inst->start();
		EXPECT_EQ(a->status(), ItemStatus::Assigned);
	}
} // namespace tividy