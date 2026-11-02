/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 2 November 2026
*/

/** @file test_pattern_memento.cpp
 * @brief Originator snapshots and command-caretaker undo through public interfaces.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "engine/commands/WorkItemCommands.h"
#include "engine/core/Errors.h"
#include "engine/items/StageItem.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemMemento.h"

static_assert(!std::is_default_constructible<tividy::WorkItemMemento>::value,
	"Only the task originator may create its memento");
static_assert(!std::is_default_constructible<tividy::InstanceMemento>::value,
	"Only the instance originator may create its memento");
static_assert(!std::is_copy_constructible<tividy::InstanceMemento>::value,
	"The instance memento has exclusive ownership");
static_assert(!std::is_copy_assignable<tividy::InstanceMemento>::value,
	"The caretaker must retain the owned instance memento");

enum class MementoWorkFailure { None, Validation, Technical };

// A client task injects data changes, failures and an observable external effect.
// TaskItem, its state machine, snapshots and command implementations remain real.
class MementoTask : public tividy::CloneableTask<MementoTask>
{
	public:
		MementoTask(const tividy::WorkItemSpec& spec, const std::string& result_key,
			std::shared_ptr<unsigned> effects, bool irreversible = false,
			MementoWorkFailure failure = MementoWorkFailure::None)
			: tividy::CloneableTask<MementoTask>(spec), result_key(result_key),
			  effects(std::move(effects)), irreversible(irreversible), failure(failure) {}
	protected:
		void performWork(const tividy::Participant& actor, tividy::Outcome&) override
		{
			context().data().set(result_key, actor.id());
			if (failure == MementoWorkFailure::Validation)
				throw tividy::ValidationError("planned failure after changing data");
			if (failure == MementoWorkFailure::Technical)
				throw tividy::ExternalFailure("temporary external failure", true);
			if (irreversible) ++*effects;
		}
		bool performsIrreversibleWork() const override { return irreversible; }
	private:
		std::string result_key;
		std::shared_ptr<unsigned> effects;
		bool irreversible;
		MementoWorkFailure failure;
};

class MementoEventRecorder : public tividy::EventObserver
{
	public:
		void onEvent(const tividy::WorkflowEvent& event) override { received.push_back(event); }
		std::vector<tividy::WorkflowEvent> received;
};

typedef std::tuple<tividy::ItemStatus, tividy::Id, tividy::TimePoint, tividy::TimePoint,
	unsigned, unsigned, unsigned, tividy::OutcomeKind, std::string, std::string, tividy::Id> MementoTaskView;

static MementoTaskView taskView(const tividy::TaskItem& task)
{
	return std::make_tuple(task.status(), task.assignee(), task.assignedAt(), task.dueAt(),
		task.rejections(), task.failures(), task.escalations(), task.lastOutcome().kind(),
		task.lastOutcome().code(), task.lastOutcome().note(), task.completedBy());
}

class MementoPatternTest : public ::testing::Test
{
	protected:
		MementoPatternTest()
			: clock(tividy::TimePoint(std::chrono::seconds(1000))),
			  services{clock, directory, connectors},
			  actor("worker", "Worker", std::set<std::string>{"worker"}),
			  reviewer("reviewer", "Reviewer", std::set<std::string>{"reviewer"}),
			  effects(std::make_shared<unsigned>(0)), work(nullptr), follow(nullptr), hold(nullptr) {}

		void make(bool irreversible = false, MementoWorkFailure failure = MementoWorkFailure::None,
			bool automatic_follow = false, bool reviewed = false)
		{
			directory.add(actor);
			directory.add(reviewer);
			directory.add(tividy::Participant("system", "System", std::set<std::string>{"system"}));
			std::unique_ptr<tividy::StageItem> root(new tividy::StageItem(tividy::WorkItemSpec("root", "Root")));
			std::unique_ptr<tividy::StageItem> group(new tividy::StageItem(tividy::WorkItemSpec("group", "Group")));
			auto spec = tividy::WorkItemSpec("work", "Work").requireRole("worker").dueAfter(90);
			if (reviewed) spec.reviewRequired(true).reviewedBy("reviewer");
			std::unique_ptr<MementoTask> owned_work(new MementoTask(spec, "work_result", effects, irreversible, failure));
			work = owned_work.get();
			group->add(std::move(owned_work));
			root->add(std::move(group));
			auto next_spec = tividy::WorkItemSpec("follow", "Follow").dependsOn("work")
				.requireRole(automatic_follow ? "system" : "worker").automatic(automatic_follow);
			std::unique_ptr<MementoTask> owned_follow(new MementoTask(next_spec, "follow_result", effects));
			follow = owned_follow.get();
			root->add(std::move(owned_follow));
			std::unique_ptr<tividy::ManualTask> owned_hold(new tividy::ManualTask(
				tividy::WorkItemSpec("hold", "Hold").dependsOn("follow").requireRole("worker")));
			hold = owned_hold.get();
			root->add(std::move(owned_hold));
			tividy::DefinitionPolicies policies;
			policies.assignment = std::make_shared<tividy::RoleBasedAssignment>();
			tividy::WorkflowData data;
			data.set("request", "original");
			instance.reset(new tividy::WorkflowInstance("memento-run", "Memento workflow", 1,
				data, std::move(root), policies, services));
			instance->events().attach(&recorder);
			instance->start();
		}

		tividy::CommandResult startWork()
		{
			return instance->actions().execute(std::unique_ptr<tividy::Command>(
				new tividy::StartCommand(*instance, "work", actor)));
		}
		tividy::CommandResult completeWork()
		{
			return instance->actions().execute(std::unique_ptr<tividy::Command>(
				new tividy::CompleteCommand(*instance, "work", actor, tividy::Outcome::accepted("DONE"))));
		}

		tividy::ManualClock clock;
		tividy::ParticipantDirectory directory;
		tividy::ConnectorRegistry connectors;
		tividy::Services services;
		tividy::Participant actor;
		tividy::Participant reviewer;
		MementoEventRecorder recorder;
		std::shared_ptr<unsigned> effects;
		MementoTask* work;
		MementoTask* follow;
		tividy::ManualTask* hold;
		std::unique_ptr<tividy::WorkflowInstance> instance;
};

TEST_F(MementoPatternTest, ItemMementoRestoresRuntimeFieldsSilentlyWithoutRestoringWorkflowData)
{
	make();
	const auto saved = taskView(*work);
	auto snapshot = work->createMemento();
	ASSERT_TRUE(snapshot);
	work->start(actor);
	work->complete(actor, tividy::Outcome::rejected("DOCS_INVALID", "missing document"));
	ASSERT_EQ(tividy::ItemStatus::Completed, work->status());
	const auto events = recorder.received.size();
	const auto audit = instance->audit().entries().size();
	work->restore(*snapshot);
	EXPECT_EQ(saved, taskView(*work));
	EXPECT_EQ(events, recorder.received.size());
	EXPECT_EQ(audit, instance->audit().entries().size());
	EXPECT_EQ("worker", instance->data().get("work_result"));
	EXPECT_EQ(work, instance->findItem("work"));
	EXPECT_NO_THROW(work->start(actor));
}

TEST_F(MementoPatternTest, InstanceRestoreReinstatesNestedItemsDataAndStatusAfterCancellation)
{
	make();
	const auto saved_work = taskView(*work), saved_follow = taskView(*follow), saved_hold = taskView(*hold);
	const auto data = instance->data().all();
	auto snapshot = instance->captureSnapshot();
	instance->data().set("request", "changed");
	instance->data().set("extra", "remove on restore");
	instance->cancel("withdrawn");
	ASSERT_EQ(tividy::InstanceStatus::Cancelled, instance->status());
	const auto events = recorder.received.size(), audit = instance->audit().entries().size();
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(tividy::InstanceStatus::Running, instance->status());
	EXPECT_EQ(saved_work, taskView(*work));
	EXPECT_EQ(saved_follow, taskView(*follow));
	EXPECT_EQ(saved_hold, taskView(*hold));
	EXPECT_EQ(data, instance->data().all());
	EXPECT_EQ(events, recorder.received.size());
	EXPECT_EQ(audit, instance->audit().entries().size());
}

TEST_F(MementoPatternTest, IndependentSnapshotsCanBeRestoredRepeatedly)
{
	make();
	const auto assigned = taskView(*work);
	auto first = instance->captureSnapshot();
	work->start(actor);
	instance->data().set("request", "second");
	const auto started = taskView(*work);
	auto second = instance->captureSnapshot();
	work->complete(actor, tividy::Outcome::accepted());
	instance->restoreSnapshot(*first);
	EXPECT_EQ(assigned, taskView(*work));
	EXPECT_EQ("original", instance->data().get("request"));
	instance->data().set("request", "later mutation");
	instance->restoreSnapshot(*second);
	EXPECT_EQ(started, taskView(*work));
	EXPECT_EQ("second", instance->data().get("request"));
	EXPECT_EQ(tividy::ItemStatus::Created, follow->status());
	instance->restoreSnapshot(*first);
	EXPECT_EQ(assigned, taskView(*work));
	EXPECT_EQ("original", instance->data().get("request"));
}

TEST_F(MementoPatternTest, ReviewSnapshotRestoresRecordedCountersOutcomeAndCompleter)
{
	make(false, MementoWorkFailure::None, false, true);
	work->start(actor);
	work->submit(actor, tividy::Outcome::accepted("REVIEW", "original note"));
	ASSERT_EQ(tividy::ItemStatus::UnderReview, work->status());
	const auto saved = taskView(*work);
	auto snapshot = instance->captureSnapshot();
	work->reject(reviewer, "revise");
	ASSERT_EQ(1u, work->rejections());
	work->escalate("needs help");
	ASSERT_EQ(1u, work->escalations());
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(saved, taskView(*work));
}

TEST_F(MementoPatternTest, SnapshotRestoresTechnicalFailureCountAndData)
{
	make(false, MementoWorkFailure::Technical);
	work->start(actor);
	const auto saved = taskView(*work);
	const auto data = instance->data().all();
	auto snapshot = instance->captureSnapshot();
	work->complete(actor, tividy::Outcome::accepted());
	ASSERT_EQ(1u, work->failures());
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(saved, taskView(*work));
	EXPECT_EQ(data, instance->data().all());
}

TEST_F(MementoPatternTest, RawRestorePreservesHistoryAuditObserversServicesAndElapsedTime)
{
	make();
	ASSERT_TRUE(startWork().success);
	auto snapshot = instance->captureSnapshot();
	const auto history = instance->actions().describeAll();
	auto* root = &instance->root();
	auto* context = &instance->context();
	const auto observers = instance->events().observerCount();
	work->escalate("later event");
	clock.advance(20);
	const auto now = clock.now();
	const auto events = recorder.received.size(), audit = instance->audit().entries().size();
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(history, instance->actions().describeAll());
	EXPECT_EQ(events, recorder.received.size());
	EXPECT_EQ(audit, instance->audit().entries().size());
	EXPECT_EQ(observers, instance->events().observerCount());
	EXPECT_EQ(root, &instance->root());
	EXPECT_EQ(context, &instance->context());
	EXPECT_EQ(work, instance->findItem("work"));
	EXPECT_EQ(&clock, &instance->context().clock());
	EXPECT_EQ(&directory, &instance->context().directory());
	EXPECT_EQ(&connectors, &instance->context().connectors());
	EXPECT_EQ(now, clock.now());
}

TEST_F(MementoPatternTest, CommandUndoRestoresAutomaticSuccessorEffectsAndSharedData)
{
	make(false, MementoWorkFailure::None, true);
	ASSERT_TRUE(startWork().success);
	const auto before_work = taskView(*work), before_follow = taskView(*follow), before_hold = taskView(*hold);
	const auto data = instance->data().all();
	ASSERT_TRUE(completeWork().success);
	ASSERT_EQ(tividy::ItemStatus::Completed, follow->status());
	ASSERT_EQ(tividy::ItemStatus::Assigned, hold->status());
	ASSERT_EQ("system", instance->data().get("follow_result"));
	ASSERT_TRUE(instance->actions().undoLast().success);
	EXPECT_EQ(before_work, taskView(*work));
	EXPECT_EQ(before_follow, taskView(*follow));
	EXPECT_EQ(before_hold, taskView(*hold));
	EXPECT_EQ(data, instance->data().all());
	EXPECT_EQ(1u, instance->actions().size());
}

TEST_F(MementoPatternTest, UndoPublishesOnlyOneRevertedEventAndKeepsPriorAuditEntries)
{
	make();
	const auto assigned = taskView(*work);
	ASSERT_TRUE(startWork().success);
	const auto events = recorder.received.size(), audit = instance->audit().entries().size();
	clock.advance(5);
	ASSERT_TRUE(instance->actions().undoLast().success);
	EXPECT_EQ(assigned, taskView(*work));
	ASSERT_EQ(events + 1, recorder.received.size());
	const auto& reverted = recorder.received.back();
	EXPECT_EQ(tividy::EventType::ItemReverted, reverted.type());
	EXPECT_EQ("work", reverted.itemId());
	EXPECT_EQ(instance->id(), reverted.instanceId());
	EXPECT_EQ(clock.now(), reverted.when());
	EXPECT_EQ(audit + 1, instance->audit().entries().size());
	EXPECT_EQ(0u, instance->actions().size());
}

TEST_F(MementoPatternTest, FailedCommandRollsBackPartialDataChangesWithoutEnteringHistory)
{
	make(false, MementoWorkFailure::Validation);
	ASSERT_TRUE(startWork().success);
	const auto saved = taskView(*work);
	const auto data = instance->data().all();
	const auto events = recorder.received.size(), audit = instance->audit().entries().size();
	const auto result = completeWork();
	EXPECT_FALSE(result.success);
	EXPECT_NE(std::string::npos, result.message.find("planned failure"));
	EXPECT_EQ(saved, taskView(*work));
	EXPECT_EQ(data, instance->data().all());
	EXPECT_EQ(1u, instance->actions().size());
	EXPECT_EQ(events, recorder.received.size());
	EXPECT_EQ(audit, instance->audit().entries().size());
}

TEST_F(MementoPatternTest, IrreversibleCommandBlocksUndoAndRawRestoreCannotEraseItsEffect)
{
	make(true);
	ASSERT_TRUE(startWork().success);
	const auto saved = taskView(*work);
	auto snapshot = instance->captureSnapshot();
	ASSERT_TRUE(completeWork().success);
	ASSERT_EQ(1u, *effects);
	ASSERT_EQ(1u, instance->context().irreversibleCount());
	const auto history = instance->actions().describeAll();
	const auto result = instance->actions().undoLast();
	EXPECT_FALSE(result.success);
	EXPECT_EQ(history, instance->actions().describeAll());
	EXPECT_EQ(tividy::ItemStatus::Completed, work->status());
	instance->restoreSnapshot(*snapshot);
	EXPECT_EQ(saved, taskView(*work));
	EXPECT_EQ(1u, *effects);
	EXPECT_EQ(1u, instance->context().irreversibleCount());
	EXPECT_FALSE(work->isReversible());
}

TEST_F(MementoPatternTest, AReversibleActionAfterAnIrreversibleBarrierCanStillBeUndone)
{
	make(true);
	ASSERT_TRUE(startWork().success);
	ASSERT_TRUE(completeWork().success);
	const auto assigned = taskView(*follow);
	auto result = instance->actions().execute(std::unique_ptr<tividy::Command>(
		new tividy::StartCommand(*instance, "follow", actor)));
	ASSERT_TRUE(result.success);
	ASSERT_EQ(3u, instance->actions().size());
	ASSERT_TRUE(instance->actions().undoLast().success);
	EXPECT_EQ(assigned, taskView(*follow));
	EXPECT_EQ(2u, instance->actions().size());
	EXPECT_FALSE(instance->actions().undoLast().success);
	EXPECT_EQ(2u, instance->actions().size());
	EXPECT_EQ(1u, instance->context().irreversibleCount());
	EXPECT_EQ(1u, *effects);
}
