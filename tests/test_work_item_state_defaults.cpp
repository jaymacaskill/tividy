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

#include <gtest/gtest.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "support/WorkItemStateDefaultsFixtures.h"

class DefaultsOnlyState : public tividy::WorkItemState
{
	public:
		explicit DefaultsOnlyState(tividy::ItemStatus status) : status_ref(status) {}
		tividy::ItemStatus status() const override { return status_ref; }
	private:
		tividy::ItemStatus status_ref;
};

class DefaultsTestTask : public tividy::TaskItem
{
	public:
		explicit DefaultsTestTask(const tividy::WorkItemSpec& spec) : TaskItem(spec) {}
		std::unique_ptr<tividy::WorkItem> clone() const override
		{
			throw tividy::EngineError("unexpected clone");
		}
	protected:
		void performWork(const tividy::Participant&, tividy::Outcome&) override
		{
			throw tividy::EngineError("unexpected work execution");
		}
};

struct TaskObservations
{
	tividy::ItemStatus status;
	tividy::Id assignee;
	tividy::TimePoint assigned_at;
	tividy::TimePoint due_at;
	unsigned rejections;
	unsigned failures;
	unsigned escalations;
	tividy::Outcome outcome;
	tividy::Id completed_by;
	bool reversible;
	tividy::WorkItemSpec spec;
};

static TaskObservations observe(const tividy::TaskItem& item)
{
	return {item.status(), item.assignee(), item.assignedAt(), item.dueAt(),
		item.rejections(), item.failures(), item.escalations(), item.lastOutcome(),
		item.completedBy(), item.isReversible(), item.spec()};
}

static void expectUnchanged(const tividy::TaskItem& item, const TaskObservations& before)
{
	EXPECT_EQ(before.status, item.status());
	EXPECT_EQ(before.assignee, item.assignee());
	EXPECT_EQ(before.assigned_at, item.assignedAt());
	EXPECT_EQ(before.due_at, item.dueAt());
	EXPECT_EQ(before.rejections, item.rejections());
	EXPECT_EQ(before.failures, item.failures());
	EXPECT_EQ(before.escalations, item.escalations());
	EXPECT_EQ(before.outcome.kind(), item.lastOutcome().kind());
	EXPECT_EQ(before.outcome.code(), item.lastOutcome().code());
	EXPECT_EQ(before.outcome.note(), item.lastOutcome().note());
	EXPECT_EQ(before.completed_by, item.completedBy());
	EXPECT_EQ(before.reversible, item.isReversible());
	EXPECT_EQ(before.spec.id(), item.spec().id());
	EXPECT_EQ(before.spec.name(), item.spec().name());
	EXPECT_EQ(before.spec.requiredRole(), item.spec().requiredRole());
	EXPECT_EQ(before.spec.reviewerRole(), item.spec().reviewerRole());
	EXPECT_EQ(before.spec.dependencies(), item.spec().dependencies());
	EXPECT_EQ(before.spec.requiresReview(), item.spec().requiresReview());
	EXPECT_EQ(before.spec.isAutomatic(), item.spec().isAutomatic());
	EXPECT_EQ(before.spec.maxRetries(), item.spec().maxRetries());
	EXPECT_EQ(before.spec.dueAfterSeconds(), item.spec().dueAfterSeconds());
	EXPECT_EQ(before.spec.timeoutCode(), item.spec().timeoutCode());
}

struct DefaultOperation
{
	std::string name;
	std::function<void(const tividy::WorkItemState&, tividy::TaskItem&,
		const tividy::Participant&, const tividy::Outcome&)> invoke;
};

static const std::vector<DefaultOperation> default_operations = {
	{"publish", [](const tividy::WorkItemState& state, tividy::TaskItem& item,
		const tividy::Participant&, const tividy::Outcome&) { state.publish(item); }},
	{"assign", [](const tividy::WorkItemState& state, tividy::TaskItem& item,
		const tividy::Participant&, const tividy::Outcome&) { state.assign(item, "new-worker"); }},
	{"start", [](const tividy::WorkItemState& state, tividy::TaskItem& item,
		const tividy::Participant& actor, const tividy::Outcome&) { state.start(item, actor); }},
	{"submit", [](const tividy::WorkItemState& state, tividy::TaskItem& item,
		const tividy::Participant& actor, const tividy::Outcome& outcome) { state.submit(item, actor, outcome); }},
	{"complete", [](const tividy::WorkItemState& state, tividy::TaskItem& item,
		const tividy::Participant& actor, const tividy::Outcome& outcome) { state.complete(item, actor, outcome); }},
	{"approve", [](const tividy::WorkItemState& state, tividy::TaskItem& item,
		const tividy::Participant& actor, const tividy::Outcome& outcome) { state.approve(item, actor, outcome); }},
	{"reject", [](const tividy::WorkItemState& state, tividy::TaskItem& item,
		const tividy::Participant& actor, const tividy::Outcome&) { state.reject(item, actor, "review rejected"); }},
	{"escalate", [](const tividy::WorkItemState& state, tividy::TaskItem& item,
		const tividy::Participant&, const tividy::Outcome&) { state.escalate(item, "needs attention"); }},
	{"cancel", [](const tividy::WorkItemState& state, tividy::TaskItem& item,
		const tividy::Participant&, const tividy::Outcome&) { state.cancel(item, "cancelled"); }},
	{"skip", [](const tividy::WorkItemState& state, tividy::TaskItem& item,
		const tividy::Participant&, const tividy::Outcome&) { state.skip(item, "not needed"); }},
	{"expire", [](const tividy::WorkItemState& state, tividy::TaskItem& item,
		const tividy::Participant&, const tividy::Outcome&) { state.expire(item, "EXPIRED", "deadline lapsed"); }}
};

class WorkItemStateDefaults : public ::testing::TestWithParam<DefaultOperation> {};

TEST_P(WorkItemStateDefaults, RejectsEveryStatusWithAClearMessageAndNoTaskMutation)
{
	const tividy::ItemStatus statuses[] = {
		tividy::ItemStatus::Created, tividy::ItemStatus::Available, tividy::ItemStatus::Assigned,
		tividy::ItemStatus::InProgress, tividy::ItemStatus::UnderReview, tividy::ItemStatus::Escalated,
		tividy::ItemStatus::Completed, tividy::ItemStatus::Cancelled, tividy::ItemStatus::Skipped
	};
	const auto& operation = GetParam();
	for (const auto status : statuses)
	{
		SCOPED_TRACE(tividy::toString(status));
		DefaultsOnlyState state(status);
		DefaultsTestTask item(tividy::WorkItemSpec("task", "Original task").requireRole("worker")
			.reviewedBy("reviewer").reviewRequired(true).dependsOn("predecessor")
			.maxRetries(5).dueAfter(20).onTimeout("EXPIRED"));
		const auto before = observe(item);
		const tividy::Participant actor("new-worker", "Worker", {"worker", "reviewer"});
		const tividy::Outcome outcome = tividy::Outcome::accepted("NEW_RESULT", "New result");
		try
		{
			operation.invoke(state, item, actor, outcome);
			ADD_FAILURE() << "The default operation should throw InvalidStateError";
		}
		catch (const tividy::InvalidStateError& error)
		{
			EXPECT_EQ(operation.name + " is not allowed while " + tividy::toString(status), error.what());
		}
		expectUnchanged(item, before);
		EXPECT_EQ(status, state.status());
		EXPECT_EQ(tividy::OutcomeKind::Accepted, outcome.kind());
		EXPECT_EQ("NEW_RESULT", outcome.code());
		EXPECT_EQ("New result", outcome.note());
		EXPECT_EQ("new-worker", actor.id());
	}
}

INSTANTIATE_TEST_SUITE_P(AllDefaultOperations, WorkItemStateDefaults,
	::testing::ValuesIn(default_operations),
	[](const ::testing::TestParamInfo<DefaultOperation>& info) { return info.param.name; });

TEST(WorkItemStateDefaultsLifetime, DeletesDerivedStatesThroughTheBase)
{
	class ObservedState : public DefaultsOnlyState
	{
		public:
			explicit ObservedState(unsigned& destroyed)
				: DefaultsOnlyState(tividy::ItemStatus::Completed), destroyed(destroyed) {}
			~ObservedState() override { ++destroyed; }
		private:
			unsigned& destroyed;
	};
	unsigned destroyed = 0;
	std::unique_ptr<tividy::WorkItemState> value(new ObservedState(destroyed));
	value.reset();
	EXPECT_EQ(1u, destroyed);
}
