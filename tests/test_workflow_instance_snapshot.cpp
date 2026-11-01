#include <gtest/gtest.h>
#include <limits>
#include <type_traits>
#include "support/WorkflowInstanceSnapshotFixtures.h"

static_assert(!std::is_default_constructible<tividy::InstanceMemento>::value,
	"Only the originator may construct an instance snapshot");
static_assert(!std::is_copy_constructible<tividy::InstanceMemento>::value,
	"Snapshots must remain uniquely owned");
static_assert(!std::is_copy_assignable<tividy::InstanceMemento>::value,
	"Snapshot contents must remain opaque");

class SnapshotEventRecorder : public tividy::EventObserver
{
	public:
		std::vector<tividy::WorkflowEvent> received;
		void onEvent(const tividy::WorkflowEvent& event) override { received.push_back(event); }
};

class SnapshotHistoryCommand : public tividy::Command
{
	public:
		tividy::CommandResult execute() override { return tividy::CommandResult::ok(); }
		tividy::CommandResult undo() override { return tividy::CommandResult::ok(); }
		bool isReversible() const override { return true; }
		std::string describe() const override { return "snapshot history marker"; }
};

class WorkflowInstanceSnapshotTest : public ::testing::Test
{
	protected:
		WorkflowInstanceSnapshotTest()
			: clock(tividy::TimePoint(std::chrono::seconds(1000))), services{clock, directory, connectors} {}
		void SetUp() override
		{
			ASSERT_EQ(0u, snapshotFixtureControls().live_item_mementos);
			snapshotFixtureControls() = InstanceSnapshotFixtureControls();
			selected.assignment.reset(new tividy::RoleBasedAssignment);
		}
		void TearDown() override
		{
			instance.reset();
			EXPECT_EQ(0u, snapshotFixtureControls().live_item_mementos);
		}
		void makeInstance(std::unique_ptr<tividy::WorkItem> root)
		{
			instance.reset(new tividy::WorkflowInstance(
				"instance-1", "Snapshot workflow", 3, initial, std::move(root), selected, services));
		}
		SnapshotFixtureTask& makeSingleTask()
		{
			std::unique_ptr<SnapshotFixtureStage> root(new SnapshotFixtureStage);
			std::unique_ptr<SnapshotFixtureTask> task(new SnapshotFixtureTask("leaf"));
			auto& result = *task;
			root->add(std::move(task));
			makeInstance(std::move(root));
			return result;
		}
		void seedStatus(tividy::InstanceStatus value)
		{
			if (value == tividy::InstanceStatus::Created) return;
			instance->start();
			if (value == tividy::InstanceStatus::Cancelled) instance->cancel("fixture cancellation");
			if (value == tividy::InstanceStatus::Completed || value == tividy::InstanceStatus::Failed)
			{
				const auto type = value == tividy::InstanceStatus::Completed
					? tividy::EventType::InstanceCompleted : tividy::EventType::InstanceFailed;
				instance->events().notify(tividy::WorkflowEvent(type, instance->id(), "", clock.now()));
			}
		}
		tividy::ManualClock clock;
		tividy::ParticipantDirectory directory;
		tividy::ConnectorRegistry connectors;
		tividy::Services services;
		tividy::DefinitionPolicies selected;
		tividy::WorkflowData initial;
		SnapshotEventRecorder recorder;
		std::unique_ptr<tividy::WorkflowInstance> instance;
};

TEST_F(WorkflowInstanceSnapshotTest, CapturesCreatedInstanceWithoutChangingLiveState)
{
	initial.set("value", "original");
	auto& task = makeSingleTask();
	const tividy::WorkflowInstance& observed = *instance;
	const auto data = observed.data().all();
	const auto snapshot = observed.captureSnapshot();
	ASSERT_TRUE(snapshot);
	EXPECT_EQ(tividy::InstanceStatus::Created, observed.status());
	EXPECT_EQ(tividy::ItemStatus::Created, task.status());
	EXPECT_EQ(data, observed.data().all());
	EXPECT_EQ(1u, task.captures);
	EXPECT_EQ(0u, task.restores);
	EXPECT_EQ(1u, task.bindings);
	EXPECT_EQ(1u, snapshotFixtureControls().live_item_mementos);
	EXPECT_TRUE(observed.audit().entries().empty());
}

TEST_F(WorkflowInstanceSnapshotTest, RestoresCreatedSnapshotAfterTheInstanceHasStarted)
{
	auto& task = makeSingleTask();
	auto snapshot = instance->captureSnapshot();
	instance->start();
	task.setRuntime(tividy::ItemStatus::Assigned, "later-actor");
	instance->data().set("later", "remove me");
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(tividy::InstanceStatus::Created, instance->status());
	EXPECT_EQ(tividy::ItemStatus::Created, task.status());
	EXPECT_TRUE(task.assignee().empty());
	EXPECT_TRUE(instance->data().all().empty());
}

TEST_F(WorkflowInstanceSnapshotTest, RestoresNestedItemsByIdEvenWhenTraversalOrderChanges)
{
	std::unique_ptr<SnapshotFixtureStage> root(new SnapshotFixtureStage);
	auto* root_view = root.get();
	std::unique_ptr<SnapshotFixtureStage> nested(new SnapshotFixtureStage("nested"));
	auto* nested_view = nested.get();
	std::unique_ptr<SnapshotFixtureTask> z(new SnapshotFixtureTask("z"));
	std::unique_ptr<SnapshotFixtureTask> a(new SnapshotFixtureTask("a"));
	std::unique_ptr<SnapshotFixtureTask> m(new SnapshotFixtureTask("m"));
	auto* z_view = z.get();
	auto* a_view = a.get();
	auto* m_view = m.get();
	root->add(std::move(z));
	nested->add(std::move(a));
	root->add(std::move(nested));
	root->add(std::move(m));
	makeInstance(std::move(root));
	z_view->setRuntime(tividy::ItemStatus::Assigned, "actor-z");
	a_view->setRuntime(tividy::ItemStatus::UnderReview, "actor-a");
	m_view->setRuntime(tividy::ItemStatus::Completed, "actor-m");
	auto snapshot = instance->captureSnapshot();
	EXPECT_EQ(3u, snapshotFixtureControls().live_item_mementos);
	EXPECT_EQ(1u, root_view->captures);
	EXPECT_EQ(1u, nested_view->captures);
	EXPECT_EQ(1u, root_view->iterators);
	root_view->reverseChildren();
	z_view->setRuntime(tividy::ItemStatus::Cancelled, "changed");
	a_view->setRuntime(tividy::ItemStatus::Skipped, "changed");
	m_view->setRuntime(tividy::ItemStatus::Created, "changed");
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(tividy::ItemStatus::Assigned, z_view->status());
	EXPECT_EQ("actor-z", z_view->assignee());
	EXPECT_EQ(tividy::ItemStatus::UnderReview, a_view->status());
	EXPECT_EQ("actor-a", a_view->assignee());
	EXPECT_EQ(tividy::ItemStatus::Completed, m_view->status());
	EXPECT_EQ("actor-m", m_view->assignee());
	EXPECT_EQ(0u, root_view->restores);
	EXPECT_EQ(0u, nested_view->restores);
	EXPECT_EQ(1u, z_view->restores);
	EXPECT_EQ(1u, a_view->restores);
	EXPECT_EQ(1u, m_view->restores);
}

TEST_F(WorkflowInstanceSnapshotTest, ReplacesTheDataBagWithAnIndependentSavedCopy)
{
	initial.set("saved", "original");
	initial.set("empty", "");
	makeSingleTask();
	const auto expected = instance->data().all();
	auto snapshot = instance->captureSnapshot();
	instance->data() = tividy::WorkflowData();
	instance->data().set("new", "discard");
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(expected, instance->data().all());
	instance->data().set("saved", "changed again");
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(expected, instance->data().all());
	EXPECT_EQ("original", initial.get("saved"));
}

TEST_F(WorkflowInstanceSnapshotTest, TheSameSnapshotCanBeRestoredRepeatedly)
{
	auto& task = makeSingleTask();
	instance->start();
	task.setRuntime(tividy::ItemStatus::Assigned, "saved");
	instance->data().set("value", "saved");
	auto snapshot = instance->captureSnapshot();
	for (unsigned attempt = 0; attempt < 3; ++attempt)
	{
		task.setRuntime(tividy::ItemStatus::Completed, "changed");
		instance->cancel("changed");
		instance->data().set("value", "changed");
		instance->restoreSnapshot(*snapshot);
		EXPECT_EQ(tividy::InstanceStatus::Running, instance->status());
		EXPECT_EQ(tividy::ItemStatus::Assigned, task.status());
		EXPECT_EQ("saved", task.assignee());
		EXPECT_EQ("saved", instance->data().get("value"));
	}
	EXPECT_EQ(3u, task.restores);
	EXPECT_EQ(1u, snapshotFixtureControls().live_item_mementos);
}

TEST_F(WorkflowInstanceSnapshotTest, SnapshotsTakenAtDifferentTimesRemainIndependent)
{
	auto& task = makeSingleTask();
	task.setRuntime(tividy::ItemStatus::Assigned, "first");
	instance->data().set("value", "first");
	auto first = instance->captureSnapshot();
	instance->start();
	task.setRuntime(tividy::ItemStatus::UnderReview, "second");
	instance->data().set("value", "second");
	auto second = instance->captureSnapshot();
	task.setRuntime(tividy::ItemStatus::Completed, "third");
	instance->data().set("value", "third");
	instance->restoreSnapshot(*first);
	EXPECT_EQ(tividy::InstanceStatus::Created, instance->status());
	EXPECT_EQ(tividy::ItemStatus::Assigned, task.status());
	EXPECT_EQ("first", task.assignee());
	EXPECT_EQ("first", instance->data().get("value"));
	instance->restoreSnapshot(*second);
	EXPECT_EQ(tividy::InstanceStatus::Running, instance->status());
	EXPECT_EQ(tividy::ItemStatus::UnderReview, task.status());
	EXPECT_EQ("second", task.assignee());
	EXPECT_EQ("second", instance->data().get("value"));
}

TEST_F(WorkflowInstanceSnapshotTest, RoundTripsEveryInstanceStatus)
{
	for (const auto value : {tividy::InstanceStatus::Created, tividy::InstanceStatus::Running,
		tividy::InstanceStatus::Completed, tividy::InstanceStatus::Failed, tividy::InstanceStatus::Cancelled})
	{
		makeSingleTask();
		seedStatus(value);
		ASSERT_EQ(value, instance->status());
		auto snapshot = instance->captureSnapshot();
		if (value == tividy::InstanceStatus::Cancelled)
			instance->events().notify(tividy::WorkflowEvent(tividy::EventType::InstanceFailed, instance->id(), "", clock.now()));
		else instance->cancel("mutate fixture status");
		ASSERT_NE(value, instance->status());
		instance->restoreSnapshot(*snapshot);
		EXPECT_EQ(value, instance->status());
	}
}

TEST_F(WorkflowInstanceSnapshotTest, DelegatesSnapshotsForEveryTaskStatusAndSavedTimestamp)
{
	auto& task = makeSingleTask();
	for (const auto value : {tividy::ItemStatus::Created, tividy::ItemStatus::Available,
		tividy::ItemStatus::Assigned, tividy::ItemStatus::InProgress, tividy::ItemStatus::UnderReview,
		tividy::ItemStatus::Escalated, tividy::ItemStatus::Completed, tividy::ItemStatus::Cancelled,
		tividy::ItemStatus::Skipped})
	{
		task.setRuntime(value, "saved");
		const auto assigned = task.assignedAt();
		auto snapshot = instance->captureSnapshot();
		clock.advance(10);
		task.setRuntime(value == tividy::ItemStatus::Assigned ? tividy::ItemStatus::Created : tividy::ItemStatus::Assigned, "changed");
		instance->restoreSnapshot(*snapshot);
		EXPECT_EQ(value, task.status());
		EXPECT_EQ("saved", task.assignee());
		EXPECT_EQ(assigned, task.assignedAt());
	}
}

TEST_F(WorkflowInstanceSnapshotTest, SkipsEmptyMementosIncludingACompletelyEmptySnapshot)
{
	auto& task = makeSingleTask();
	task.omit_snapshot = true;
	instance->data().set("value", "saved");
	auto snapshot = instance->captureSnapshot();
	ASSERT_TRUE(snapshot);
	EXPECT_EQ(0u, snapshotFixtureControls().live_item_mementos);
	instance->start();
	task.setRuntime(tividy::ItemStatus::Completed, "later");
	instance->data().set("value", "later");
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(tividy::InstanceStatus::Created, instance->status());
	EXPECT_EQ("saved", instance->data().get("value"));
	EXPECT_EQ(tividy::ItemStatus::Completed, task.status());
	EXPECT_EQ(0u, task.restores);
}

TEST_F(WorkflowInstanceSnapshotTest, SupportsATaskAtTheRootWithoutAssumingAStage)
{
	std::unique_ptr<SnapshotFixtureTask> root(new SnapshotFixtureTask("root-task"));
	auto* task = root.get();
	makeInstance(std::move(root));
	task->setRuntime(tividy::ItemStatus::InProgress, "saved");
	auto snapshot = instance->captureSnapshot();
	task->setRuntime(tividy::ItemStatus::Completed, "later");
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(tividy::ItemStatus::InProgress, task->status());
	EXPECT_EQ("saved", task->assignee());
	EXPECT_EQ(1u, task->captures);
	EXPECT_EQ(1u, task->restores);
}

TEST_F(WorkflowInstanceSnapshotTest, CapturesAndRestoresThroughTheOutermostDecorator)
{
	std::unique_ptr<SnapshotFixtureStage> root(new SnapshotFixtureStage);
	std::unique_ptr<SnapshotFixtureTask> task(new SnapshotFixtureTask("decorated"));
	auto* task_view = task.get();
	std::unique_ptr<SnapshotFixtureWrapper> wrapper(new SnapshotFixtureWrapper(std::move(task)));
	auto* wrapper_view = wrapper.get();
	root->add(std::move(wrapper));
	makeInstance(std::move(root));
	task_view->setRuntime(tividy::ItemStatus::Escalated, "saved");
	auto snapshot = instance->captureSnapshot();
	EXPECT_EQ(1u, snapshotFixtureControls().live_item_mementos);
	EXPECT_EQ(1u, wrapper_view->captures);
	EXPECT_EQ(1u, task_view->captures);
	task_view->setRuntime(tividy::ItemStatus::Cancelled, "changed");
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(1u, wrapper_view->restores);
	EXPECT_EQ(1u, task_view->restores);
	EXPECT_EQ(tividy::ItemStatus::Escalated, task_view->status());
	EXPECT_EQ("saved", task_view->assignee());
}

TEST_F(WorkflowInstanceSnapshotTest, PreservesEventsAuditHistoryAndObserverSubscriptions)
{
	makeSingleTask();
	instance->start();
	instance->events().attach(&recorder);
	instance->events().notify(tividy::WorkflowEvent(tividy::EventType::EngineNote, instance->id(), "", clock.now()).withMessage("before"));
	ASSERT_TRUE(instance->actions().execute(std::unique_ptr<tividy::Command>(new SnapshotHistoryCommand)).success);
	auto snapshot = instance->captureSnapshot();
	EXPECT_EQ(1u, recorder.received.size());
	EXPECT_EQ(1u, instance->audit().entries().size());
	instance->events().notify(tividy::WorkflowEvent(tividy::EventType::EngineNote, instance->id(), "", clock.now()).withMessage("after"));
	ASSERT_TRUE(instance->actions().execute(std::unique_ptr<tividy::Command>(new SnapshotHistoryCommand)).success);
	const auto flow_events = snapshotFixtureControls().flow_events;
	const auto subscribers = instance->events().observerCount();
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(2u, recorder.received.size());
	ASSERT_EQ(2u, instance->audit().entries().size());
	EXPECT_EQ("before", instance->audit().entries()[0].message);
	EXPECT_EQ("after", instance->audit().entries()[1].message);
	EXPECT_EQ(2u, instance->actions().size());
	EXPECT_EQ(flow_events, snapshotFixtureControls().flow_events);
	EXPECT_EQ(subscribers, instance->events().observerCount());
}

TEST_F(WorkflowInstanceSnapshotTest, PreservesObjectIdentityServicesClockAndIrreversibleCount)
{
	auto& task = makeSingleTask();
	const auto* data = &instance->data();
	const auto* root = &instance->root();
	const auto* context = &instance->context();
	const auto* events = &instance->events();
	const auto* actions = &instance->actions();
	instance->context().markIrreversible();
	auto snapshot = instance->captureSnapshot();
	instance->context().markIrreversible();
	clock.advance(70);
	const auto time = clock.now();
	directory.add(tividy::Participant("new", "New participant", {"reviewer"}));
	connectors.registerConnector(std::unique_ptr<tividy::ExternalConnector>(new tividy::LoggingNotifier("later")));
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(data, &instance->data());
	EXPECT_EQ(root, &instance->root());
	EXPECT_EQ(context, &instance->context());
	EXPECT_EQ(events, &instance->events());
	EXPECT_EQ(actions, &instance->actions());
	EXPECT_EQ(context, &task.boundContext());
	EXPECT_EQ(1u, task.bindings);
	EXPECT_EQ(2u, instance->context().irreversibleCount());
	EXPECT_EQ(time, clock.now());
	EXPECT_EQ(&clock, &instance->context().clock());
	EXPECT_EQ(&directory, &instance->context().directory());
	EXPECT_EQ(&connectors, &instance->context().connectors());
	EXPECT_NE(nullptr, directory.find("new"));
	EXPECT_TRUE(connectors.has("later"));
	EXPECT_EQ("instance-1", instance->id());
	EXPECT_EQ("Snapshot workflow", instance->definitionName());
	EXPECT_EQ(3u, instance->definitionVersion());
}

TEST_F(WorkflowInstanceSnapshotTest, FailedCaptureReleasesPreviouslyCreatedItemMementos)
{
	std::unique_ptr<SnapshotFixtureStage> root(new SnapshotFixtureStage);
	std::unique_ptr<SnapshotFixtureTask> first(new SnapshotFixtureTask("first"));
	std::unique_ptr<SnapshotFixtureTask> second(new SnapshotFixtureTask("second"));
	auto* first_view = first.get();
	auto* second_view = second.get();
	second->fail_capture = true;
	root->add(std::move(first));
	root->add(std::move(second));
	makeInstance(std::move(root));
	EXPECT_THROW(instance->captureSnapshot(), tividy::EngineError);
	EXPECT_EQ(1u, first_view->captures);
	EXPECT_EQ(0u, snapshotFixtureControls().live_item_mementos);
	EXPECT_EQ(tividy::InstanceStatus::Created, instance->status());
	second_view->fail_capture = false;
	auto recovered = instance->captureSnapshot();
	EXPECT_EQ(2u, snapshotFixtureControls().live_item_mementos);
}

TEST_F(WorkflowInstanceSnapshotTest, IteratorCreationFailurePropagatesWithoutChangingTheInstance)
{
	std::unique_ptr<SnapshotFixtureStage> root(new SnapshotFixtureStage);
	root->fail_iterator = true;
	root->add(std::unique_ptr<tividy::WorkItem>(new SnapshotFixtureTask("leaf")));
	makeInstance(std::move(root));
	instance->data().set("value", "unchanged");
	EXPECT_THROW(instance->captureSnapshot(), tividy::EngineError);
	EXPECT_EQ("unchanged", instance->data().get("value"));
	EXPECT_EQ(tividy::InstanceStatus::Created, instance->status());
	EXPECT_EQ(0u, snapshotFixtureControls().live_item_mementos);
}

TEST_F(WorkflowInstanceSnapshotTest, AStoredSnapshotCanOutliveItsOriginatingInstance)
{
	makeSingleTask();
	auto snapshot = instance->captureSnapshot();
	instance.reset();
	EXPECT_EQ(1u, snapshotFixtureControls().live_item_mementos);
	snapshot.reset();
	EXPECT_EQ(0u, snapshotFixtureControls().live_item_mementos);
}

TEST_F(WorkflowInstanceSnapshotTest, DeadlineWrapperCallsItsMonitorAndReturnsTheExactCount)
{
	makeSingleTask();
	auto& control = snapshotFixtureControls();
	EXPECT_EQ(0u, instance->sweepDeadlines());
	instance->start();
	control.sweep_result = 3;
	EXPECT_EQ(3u, instance->sweepDeadlines());
	instance->cancel("fixture state");
	control.sweep_result = std::numeric_limits<std::size_t>::max();
	EXPECT_EQ(control.sweep_result, instance->sweepDeadlines());
	EXPECT_EQ(3u, control.sweep_calls);
	EXPECT_EQ(instance.get(), control.swept_instance);
}

TEST_F(WorkflowInstanceSnapshotTest, DeadlineWrapperPreservesMonitorEventsAndExceptions)
{
	makeSingleTask();
	instance->events().attach(&recorder);
	auto& control = snapshotFixtureControls();
	control.sweep_emits = true;
	control.sweep_result = 1;
	EXPECT_EQ(1u, instance->sweepDeadlines());
	ASSERT_EQ(1u, recorder.received.size());
	const auto& event = recorder.received.front();
	EXPECT_EQ(tividy::EventType::DeadlineMissed, event.type());
	EXPECT_EQ(instance->id(), event.instanceId());
	EXPECT_EQ("leaf", event.itemId());
	EXPECT_EQ("system", event.actorId());
	EXPECT_EQ(clock.now(), event.when());
	EXPECT_EQ("deadline passed", event.message());
	control.sweep_throws = true;
	EXPECT_THROW(instance->sweepDeadlines(), tividy::EngineError);
	EXPECT_EQ(2u, control.sweep_calls);
	EXPECT_EQ(1u, recorder.received.size());
}
