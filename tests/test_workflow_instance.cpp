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
#include "engine/items/WorkItemDecorator.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/policies/AssignmentStrategy.h"
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
					directory.add(Participant("u3", "User Three", {"reviewer"}));
					directory.add(Participant("system", "System", {"system"}));
				}

				std::unique_ptr<WorkflowInstance> make(std::unique_ptr<WorkItem> root, const WorkflowData& data = WorkflowData())
				{
					DefinitionPolicies policies;
					policies.assignment = std::make_shared<RoleBasedAssignment>();
					return std::unique_ptr<WorkflowInstance>(new WorkflowInstance("inst-1", "demo", 3, data, std::move(root), policies, services));
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

		WorkItemSpec clerkSpec(const Id& id)
		{
			return WorkItemSpec(id, "Task " + id).requireRole("clerk");
		}

		ManualTask* addTask(StageItem& stage, const WorkItemSpec& spec)
		{
			ManualTask* task = new ManualTask(spec);
			stage.add(std::unique_ptr<WorkItem>(task));
			return task;
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

		void finishTask(Harness& h, WorkItem& item, const Id& participant_id)
		{
			const Participant& p = h.directory.require(participant_id);
			item.start(p);
			item.complete(p, Outcome::accepted());
		}
	}

	TEST(WorkflowInstanceTest, NewInstanceIsCreatedAndPublishesNothing)
	{
		Harness h;
		std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
		ManualTask* a = addTask(*root, clerkSpec("a"));
		WorkflowData data;
		data.set("k", "v");

		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root), data);
		data.set("k", "changed");

		EXPECT_EQ(inst->id(), "inst-1");
		EXPECT_EQ(inst->definitionName(), "demo");
		EXPECT_EQ(inst->definitionVersion(), 3u);
		EXPECT_EQ(inst->status(), InstanceStatus::Created);
		EXPECT_TRUE(inst->audit().entries().empty());
		EXPECT_EQ(inst->data().get("k"), "v");
		EXPECT_EQ(inst->events().observerCount(), 2u);
		EXPECT_EQ(a->status(), ItemStatus::Created);
	}

	TEST(WorkflowInstanceTest, StartRunsAndPublishesInstanceStarted)
	{
		Harness h;
		std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
		addTask(*root, clerkSpec("a"));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root));

		inst->start();

		EXPECT_EQ(inst->status(), InstanceStatus::Running);
		ASSERT_FALSE(inst->audit().entries().empty());
		const AuditEntry& first = inst->audit().entries()[0];
		EXPECT_EQ(first.type, EventType::InstanceStarted);
		EXPECT_EQ(first.message, "started from demo v3");
		EXPECT_EQ(first.actor_id, "system");
		EXPECT_EQ(first.item_id, "");
	}

	TEST(WorkflowInstanceTest, StartReleasesOnlyEligibleItems)
	{
		Harness h;
		std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
		ManualTask* a = addTask(*root, clerkSpec("a"));
		ManualTask* b = addTask(*root, clerkSpec("b").dependsOn("a"));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root));

		inst->start();

		EXPECT_EQ(a->status(), ItemStatus::Assigned);
		EXPECT_EQ(a->assignee(), "u1");
		EXPECT_EQ(b->status(), ItemStatus::Created);
	}

	TEST(WorkflowInstanceTest, StartTwiceThrows)
	{
		Harness h;
		std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
		addTask(*root, clerkSpec("a"));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root));
		inst->start();

		EXPECT_THROW(inst->start(), InvalidStateError);
		EXPECT_EQ(inst->status(), InstanceStatus::Running);
		EXPECT_EQ(countType(inst->audit(), EventType::InstanceStarted), 1u);
	}

	TEST(WorkflowInstanceTest, CancelBeforeStartThrows)
	{
		Harness h;
		std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
		addTask(*root, clerkSpec("a"));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root));

		EXPECT_THROW(inst->cancel("too early"), InvalidStateError);
		EXPECT_EQ(inst->status(), InstanceStatus::Created);
		EXPECT_TRUE(inst->audit().entries().empty());
	}

	TEST(WorkflowInstanceTest, CancelCancelsInstanceAndEveryItem)
	{
		Harness h;
		std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
		ManualTask* a = addTask(*root, clerkSpec("a"));
		ManualTask* b = addTask(*root, clerkSpec("b").dependsOn("a"));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root));
		inst->start();

		inst->cancel("customer withdrew");

		EXPECT_EQ(inst->status(), InstanceStatus::Cancelled);
		EXPECT_EQ(a->status(), ItemStatus::Cancelled);
		EXPECT_EQ(b->status(), ItemStatus::Cancelled);
		const AuditEntry* cancelled = firstOfType(inst->audit(), EventType::InstanceCancelled);
		ASSERT_NE(cancelled, nullptr);
		EXPECT_EQ(cancelled->message, "customer withdrew");
		EXPECT_EQ(cancelled->actor_id, "system");
		EXPECT_EQ(countType(inst->audit(), EventType::InstanceFailed), 0u);
		EXPECT_THROW(inst->cancel("again"), InvalidStateError);
	}

	TEST(WorkflowInstanceTest, FinishingAllWorkCompletesInstance)
	{
		Harness h;
		std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
		ManualTask* a = addTask(*root, clerkSpec("a"));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root));
		inst->start();

		finishTask(h, *a, "u1");

		EXPECT_EQ(inst->status(), InstanceStatus::Completed);
		const AuditEntry* completed = firstOfType(inst->audit(), EventType::InstanceCompleted);
		ASSERT_NE(completed, nullptr);
		EXPECT_EQ(completed->actor_id, "system");
		EXPECT_EQ(countType(inst->audit(), EventType::InstanceCompleted), 1u);
	}

	TEST(WorkflowInstanceTest, FindItemReturnsOutermostDecoratorOrNull)
	{
		Harness h;
		std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
		StageItem* root_ptr = root.get();
		SecureWorkItem* secure_a = new SecureWorkItem(std::unique_ptr<WorkItem>(new ManualTask(clerkSpec("a"))));
		root->add(std::unique_ptr<WorkItem>(secure_a));
		StageItem* s = new StageItem(WorkItemSpec("s", "Stage"));
		root->add(std::unique_ptr<WorkItem>(s));
		ManualTask* b = addTask(*s, clerkSpec("b"));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root));

		EXPECT_EQ(inst->findItem("root"), root_ptr);
		EXPECT_EQ(inst->findItem("a"), secure_a);
		EXPECT_EQ(inst->findItem("s"), s);
		EXPECT_EQ(inst->findItem("b"), b);
		EXPECT_EQ(inst->findItem("ghost"), nullptr);
	}

	TEST(WorkflowInstanceTest, ItemsForReturnsActiveTasksInDepthFirstOrder)
	{
		Harness h;
		std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
		ManualTask* a = addTask(*root, clerkSpec("a"));
		StageItem* s = new StageItem(WorkItemSpec("s", "Stage"));
		root->add(std::unique_ptr<WorkItem>(s));
		SecureWorkItem* secure_b = new SecureWorkItem(std::unique_ptr<WorkItem>(new ManualTask(clerkSpec("b"))));
		s->add(std::unique_ptr<WorkItem>(secure_b));
		ManualTask* c = addTask(*s, WorkItemSpec("c", "Task c").requireRole("reviewer"));
		ManualTask* d = addTask(*root, clerkSpec("d"));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root));
		inst->start();

		std::vector<WorkItem*> mine = inst->itemsFor("u1");
		ASSERT_EQ(mine.size(), 3u);
		EXPECT_EQ(mine[0], a);
		EXPECT_EQ(mine[1], secure_b);
		EXPECT_EQ(mine[2], d);

		std::vector<WorkItem*> reviewer = inst->itemsFor("u3");
		ASSERT_EQ(reviewer.size(), 1u);
		EXPECT_EQ(reviewer[0], c);

		EXPECT_TRUE(inst->itemsFor("nobody").empty());

		finishTask(h, *a, "u1");
		mine = inst->itemsFor("u1");
		ASSERT_EQ(mine.size(), 2u);
		EXPECT_EQ(mine[0], secure_b);
		EXPECT_EQ(mine[1], d);
	}

	TEST(WorkflowInstanceTest, AddedObserverSeesEveryEventAfterBuiltIns)
	{
		Harness h;
		std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
		addTask(*root, clerkSpec("a"));
		std::unique_ptr<WorkflowInstance> inst = h.make(std::move(root));
		RecordingObserver* recorder = new RecordingObserver();

		inst->addObserver(std::unique_ptr<EventObserver>(recorder));
		EXPECT_EQ(inst->events().observerCount(), 3u);

		inst->start();
		ASSERT_FALSE(recorder->received().empty());
		EXPECT_EQ(recorder->received()[0].type(), EventType::InstanceStarted);
		EXPECT_EQ(recorder->received().size(), inst->audit().entries().size());
	}

	TEST(WorkflowInstanceTest, InstancesDoNotShareData)
	{
		Harness h;
		WorkflowData initial;
		initial.set("k", "v");
		std::unique_ptr<StageItem> root1(new StageItem(WorkItemSpec("root", "Root")));
		addTask(*root1, clerkSpec("a"));
		std::unique_ptr<StageItem> root2(new StageItem(WorkItemSpec("root", "Root")));
		addTask(*root2, clerkSpec("a"));
		std::unique_ptr<WorkflowInstance> first = h.make(std::move(root1), initial);
		std::unique_ptr<WorkflowInstance> second = h.make(std::move(root2), initial);

		first->data().set("k", "first only");

		EXPECT_EQ(first->data().get("k"), "first only");
		EXPECT_EQ(second->data().get("k"), "v");
	}
} // namespace tividy