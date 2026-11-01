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
#include <stdexcept>
#include <string>
#include <vector>
#include "support/TaskItemWorkFixtures.h"

class TaskWorkStateAccess : public tividy::WorkItemState
{
	public:
		using WorkItemState::runWork;
		using WorkItemState::moveTo;
		using WorkItemState::setAssignee;
		using WorkItemState::recordResult;
		using WorkItemState::bumpRejections;
		using WorkItemState::bumpEscalations;
};

class WorkTestTask : public tividy::TaskItem
{
	public:
		WorkTestTask() : TaskItem(tividy::WorkItemSpec("task", "Task").dueAfter(30)), work_calls(0) {}
		std::unique_ptr<tividy::WorkItem> clone() const override
		{
			throw tividy::EngineError("unexpected clone");
		}
		tividy::WorkContext& boundContext() const { return context(); }
		unsigned work_calls;
	protected:
		void performWork(const tividy::Participant&, tividy::Outcome&) override { ++work_calls; }
};

class HookedWorkTestTask : public WorkTestTask
{
	public:
		mutable std::vector<std::string> calls;
		std::function<void(const tividy::Outcome&)> validation;
		std::function<void(const tividy::Participant&, tividy::Outcome&)> work;
		std::function<void(const tividy::Outcome&)> completion;
		bool irreversible = false;
	protected:
		void validateOutcome(const tividy::Outcome& outcome) const override
		{
			calls.push_back("validate");
			if (validation) validation(outcome);
		}
		void performWork(const tividy::Participant& actor, tividy::Outcome& outcome) override
		{
			calls.push_back("work");
			++work_calls;
			if (work) work(actor, outcome);
		}
		bool performsIrreversibleWork() const override
		{
			calls.push_back("irreversible-check");
			return irreversible;
		}
		void onCompleted(const tividy::Outcome& outcome) override
		{
			calls.push_back("completed");
			if (completion) completion(outcome);
		}
};

class TaskWorkEvents : public tividy::EventObserver
{
	public:
		std::vector<tividy::WorkflowEvent> values;
		void onEvent(const tividy::WorkflowEvent& event) override { values.push_back(event); }
};

class TaskItemWork : public ::testing::Test
{
	protected:
		TaskItemWork()
			: clock(tividy::TimePoint(std::chrono::seconds(1234))),
			  context("instance-1", data, publisher, clock, connectors, directory),
			  actor("worker", "Worker", {"worker"}), reviewer("reviewer", "Reviewer", {"reviewer"})
		{
			publisher.attach(&events);
		}

		void prepare(WorkTestTask& item)
		{
			item.bind(context);
			TaskWorkStateAccess::moveTo(item, tividy::AssignedState::instance());
			TaskWorkStateAccess::setAssignee(item, actor.id());
			TaskWorkStateAccess::moveTo(item, tividy::InProgressState::instance());
		}

		tividy::ManualClock clock;
		tividy::WorkflowData data;
		tividy::EventPublisher publisher;
		tividy::ConnectorRegistry connectors;
		tividy::ParticipantDirectory directory;
		tividy::WorkContext context;
		tividy::Participant actor;
		tividy::Participant reviewer;
		TaskWorkEvents events;
};

TEST_F(TaskItemWork, RejectsUnboundTasksBeforeCallingAnyHook)
{
	HookedWorkTestTask item;
	auto outcome = tividy::Outcome::accepted();
	EXPECT_THROW(TaskWorkStateAccess::runWork(item, actor, outcome), tividy::InvalidStateError);
	EXPECT_TRUE(item.calls.empty());
	EXPECT_EQ(0u, item.failures());
	EXPECT_TRUE(item.isReversible());
	EXPECT_TRUE(events.values.empty());
}

TEST_F(TaskItemWork, RunsHooksInOrderAndReturnsTheRefinedOutcomeToTheCaller)
{
	HookedWorkTestTask item;
	prepare(item);
	item.validation = [](const tividy::Outcome& value) { EXPECT_EQ("INITIAL", value.code()); };
	item.work = [&](const tividy::Participant& supplied, tividy::Outcome& value) {
		EXPECT_EQ(&actor, &supplied);
		value = tividy::Outcome::rejected("REFINED", "Work refined the result");
	};
	item.completion = [](const tividy::Outcome& value) {
		EXPECT_EQ(tividy::OutcomeKind::Rejected, value.kind());
		EXPECT_EQ("REFINED", value.code());
	};
	auto outcome = tividy::Outcome::accepted("INITIAL");
	EXPECT_TRUE(TaskWorkStateAccess::runWork(item, actor, outcome));
	EXPECT_EQ((std::vector<std::string>{"validate", "work", "irreversible-check", "completed"}), item.calls);
	EXPECT_EQ("REFINED", outcome.code());
	EXPECT_EQ(tividy::ItemStatus::InProgress, item.status());
	EXPECT_TRUE(item.completedBy().empty());
	EXPECT_TRUE(item.lastOutcome().code().empty());
	EXPECT_TRUE(events.values.empty());
}

TEST_F(TaskItemWork, DefaultHooksAcceptTheOutcomeAndKeepOrdinaryWorkReversible)
{
	WorkTestTask item;
	prepare(item);
	auto outcome = tividy::Outcome::escalated("CUSTOM", "A business outcome");
	EXPECT_TRUE(TaskWorkStateAccess::runWork(item, actor, outcome));
	EXPECT_EQ(1u, item.work_calls);
	EXPECT_EQ("CUSTOM", outcome.code());
	EXPECT_TRUE(item.isReversible());
	EXPECT_EQ(0u, context.irreversibleCount());
	EXPECT_EQ(0u, item.failures());
	EXPECT_TRUE(events.values.empty());
}

TEST_F(TaskItemWork, ValidationFailureStopsBeforeWork)
{
	HookedWorkTestTask item;
	prepare(item);
	item.validation = [](const tividy::Outcome&) { throw tividy::ValidationError("invalid outcome"); };
	auto outcome = tividy::Outcome::accepted();
	EXPECT_THROW(TaskWorkStateAccess::runWork(item, actor, outcome), tividy::ValidationError);
	EXPECT_EQ((std::vector<std::string>{"validate"}), item.calls);
	EXPECT_EQ(0u, item.work_calls);
	EXPECT_EQ(0u, item.failures());
	EXPECT_EQ(tividy::ItemStatus::InProgress, item.status());
	EXPECT_TRUE(events.values.empty());
}

TEST_F(TaskItemWork, TechnicalFailuresPublishRetryabilityAndCompleteEventMetadata)
{
	for (const bool retryable : {false, true})
	{
		SCOPED_TRACE(retryable);
		events.values.clear();
		HookedWorkTestTask item;
		prepare(item);
		item.work = [retryable](const tividy::Participant&, tividy::Outcome&) {
			throw tividy::ExternalFailure("vendor unavailable", retryable);
		};
		auto outcome = tividy::Outcome::accepted("REPORTED");
		EXPECT_FALSE(TaskWorkStateAccess::runWork(item, actor, outcome));
		EXPECT_EQ((std::vector<std::string>{"validate", "work"}), item.calls);
		EXPECT_EQ(1u, item.failures());
		EXPECT_EQ(tividy::ItemStatus::InProgress, item.status());
		EXPECT_EQ(actor.id(), item.assignee());
		EXPECT_EQ("REPORTED", outcome.code());
		ASSERT_EQ(1u, events.values.size());
		const auto& event = events.values.front();
		EXPECT_EQ(tividy::EventType::ItemFailed, event.type());
		EXPECT_EQ("instance-1", event.instanceId());
		EXPECT_EQ("task", event.itemId());
		EXPECT_EQ(actor.id(), event.actorId());
		EXPECT_EQ(clock.now(), event.when());
		EXPECT_EQ(1u, event.count());
		EXPECT_EQ(tividy::OutcomeKind::Failed, event.outcome().kind());
		EXPECT_EQ(retryable ? "RETRYABLE" : "PERMANENT", event.outcome().code());
		EXPECT_EQ("vendor unavailable", event.outcome().note());
		EXPECT_FALSE(event.message().empty());
	}
}

TEST_F(TaskItemWork, CountsRepeatedFailuresWithoutResettingTheCountOnSuccess)
{
	HookedWorkTestTask item;
	prepare(item);
	item.work = [](const tividy::Participant&, tividy::Outcome&) { throw tividy::ExternalFailure("retry", true); };
	auto outcome = tividy::Outcome::accepted();
	EXPECT_FALSE(TaskWorkStateAccess::runWork(item, actor, outcome));
	EXPECT_FALSE(TaskWorkStateAccess::runWork(item, actor, outcome));
	ASSERT_EQ(2u, events.values.size());
	EXPECT_EQ(1u, events.values[0].count());
	EXPECT_EQ(2u, events.values[1].count());
	item.work = [](const tividy::Participant&, tividy::Outcome& value) { value = tividy::Outcome::accepted("RECOVERED"); };
	EXPECT_TRUE(TaskWorkStateAccess::runWork(item, actor, outcome));
	EXPECT_EQ(2u, item.failures());
	EXPECT_EQ("RECOVERED", outcome.code());
	EXPECT_EQ(2u, events.values.size());
}

TEST_F(TaskItemWork, OtherWorkExceptionsPropagateWithoutBecomingTechnicalFailures)
{
	HookedWorkTestTask item;
	prepare(item);
	item.work = [](const tividy::Participant&, tividy::Outcome&) { throw std::logic_error("work bug"); };
	auto outcome = tividy::Outcome::accepted();
	EXPECT_THROW(TaskWorkStateAccess::runWork(item, actor, outcome), std::logic_error);
	EXPECT_EQ((std::vector<std::string>{"validate", "work"}), item.calls);
	EXPECT_EQ(0u, item.failures());
	EXPECT_TRUE(events.values.empty());
}

TEST_F(TaskItemWork, ExternalFailuresFromValidationAreNotCaughtAsWorkFailures)
{
	HookedWorkTestTask item;
	prepare(item);
	item.validation = [](const tividy::Outcome&) { throw tividy::ExternalFailure("validation hook", false); };
	auto outcome = tividy::Outcome::accepted();
	EXPECT_THROW(TaskWorkStateAccess::runWork(item, actor, outcome), tividy::ExternalFailure);
	EXPECT_EQ(0u, item.work_calls);
	EXPECT_EQ(0u, item.failures());
	EXPECT_TRUE(events.values.empty());
}

TEST_F(TaskItemWork, ExternalFailuresFromCompletionAreNotCaughtAsWorkFailures)
{
	HookedWorkTestTask item;
	prepare(item);
	item.completion = [](const tividy::Outcome&) { throw tividy::ExternalFailure("completion hook", false); };
	auto outcome = tividy::Outcome::accepted();
	EXPECT_THROW(TaskWorkStateAccess::runWork(item, actor, outcome), tividy::ExternalFailure);
	EXPECT_EQ(1u, item.work_calls);
	EXPECT_EQ(0u, item.failures());
	EXPECT_EQ(tividy::ItemStatus::InProgress, item.status());
	EXPECT_TRUE(events.values.empty());
}

TEST_F(TaskItemWork, MarksSuccessfulIrreversibleWorkBeforeTheCompletionHook)
{
	HookedWorkTestTask item;
	prepare(item);
	item.irreversible = true;
	item.completion = [&](const tividy::Outcome&) {
		EXPECT_FALSE(item.isReversible());
		EXPECT_EQ(1u, context.irreversibleCount());
	};
	auto outcome = tividy::Outcome::accepted();
	EXPECT_TRUE(TaskWorkStateAccess::runWork(item, actor, outcome));
	EXPECT_FALSE(item.isReversible());
	EXPECT_EQ(1u, context.irreversibleCount());
}

TEST_F(TaskItemWork, DoesNotMarkIrreversibleWorkWhenThePrimitiveFails)
{
	HookedWorkTestTask item;
	prepare(item);
	item.irreversible = true;
	item.work = [](const tividy::Participant&, tividy::Outcome&) { throw tividy::ExternalFailure("failed", true); };
	auto outcome = tividy::Outcome::accepted();
	EXPECT_FALSE(TaskWorkStateAccess::runWork(item, actor, outcome));
	EXPECT_TRUE(item.isReversible());
	EXPECT_EQ(0u, context.irreversibleCount());
	EXPECT_EQ((std::vector<std::string>{"validate", "work"}), item.calls);
}

TEST_F(TaskItemWork, CompletionHookFailurePreservesTheIrreversibleEffect)
{
	HookedWorkTestTask item;
	prepare(item);
	item.irreversible = true;
	item.completion = [](const tividy::Outcome&) { throw std::logic_error("completion failed"); };
	auto outcome = tividy::Outcome::accepted();
	EXPECT_THROW(TaskWorkStateAccess::runWork(item, actor, outcome), std::logic_error);
	EXPECT_FALSE(item.isReversible());
	EXPECT_EQ(1u, context.irreversibleCount());
	EXPECT_EQ(0u, item.failures());
	EXPECT_TRUE(events.values.empty());
}

TEST_F(TaskItemWork, SnapshotsRoundTripEveryStatusAndAllStoredRuntimeValuesWithoutEvents)
{
	const tividy::WorkItemState* states[] = {
		&tividy::CreatedState::instance(), &tividy::AvailableState::instance(),
		&tividy::AssignedState::instance(), &tividy::InProgressState::instance(),
		&tividy::UnderReviewState::instance(), &tividy::EscalatedState::instance(),
		&tividy::CompletedState::instance(), &tividy::CancelledState::instance(), &tividy::SkippedState::instance()
	};
	for (const auto* state : states)
	{
		SCOPED_TRACE(tividy::toString(state->status()));
		HookedWorkTestTask item;
		prepare(item);
		item.work = [](const tividy::Participant&, tividy::Outcome&) { throw tividy::ExternalFailure("retry", true); };
		auto outcome = tividy::Outcome::accepted();
		EXPECT_FALSE(TaskWorkStateAccess::runWork(item, actor, outcome));
		EXPECT_FALSE(TaskWorkStateAccess::runWork(item, actor, outcome));
		TaskWorkStateAccess::bumpRejections(item);
		TaskWorkStateAccess::bumpEscalations(item);
		TaskWorkStateAccess::bumpEscalations(item);
		TaskWorkStateAccess::recordResult(item, reviewer, tividy::Outcome::rejected("SAVED", "Saved note"));
		TaskWorkStateAccess::moveTo(item, *state);
		const auto assigned_at = item.assignedAt();
		const auto due_at = item.dueAt();
		const auto* spec = &item.spec();
		events.values.clear();
		const auto snapshot = item.createMemento();
		ASSERT_TRUE(snapshot);
		EXPECT_TRUE(events.values.empty());

		clock.advance(60);
		TaskWorkStateAccess::moveTo(item, tividy::AssignedState::instance());
		TaskWorkStateAccess::setAssignee(item, "different-worker");
		TaskWorkStateAccess::bumpRejections(item);
		TaskWorkStateAccess::bumpEscalations(item);
		TaskWorkStateAccess::recordResult(item, actor, tividy::Outcome::failed("CHANGED", "Changed note"));
		EXPECT_FALSE(TaskWorkStateAccess::runWork(item, actor, outcome));
		TaskWorkStateAccess::moveTo(item, tividy::CancelledState::instance());
		data.set("outside-task", "preserve this change");
		events.values.clear();
		const auto now = clock.now();
		item.restore(*snapshot);

		EXPECT_EQ(state->status(), item.status());
		EXPECT_EQ(actor.id(), item.assignee());
		EXPECT_EQ(assigned_at, item.assignedAt());
		EXPECT_EQ(due_at, item.dueAt());
		EXPECT_EQ(1u, item.rejections());
		EXPECT_EQ(2u, item.failures());
		EXPECT_EQ(2u, item.escalations());
		EXPECT_EQ(tividy::OutcomeKind::Rejected, item.lastOutcome().kind());
		EXPECT_EQ("SAVED", item.lastOutcome().code());
		EXPECT_EQ("Saved note", item.lastOutcome().note());
		EXPECT_EQ(reviewer.id(), item.completedBy());
		EXPECT_EQ(spec, &item.spec());
		EXPECT_EQ(&context, &item.boundContext());
		EXPECT_EQ("preserve this change", data.get("outside-task"));
		EXPECT_EQ(now, clock.now());
		EXPECT_TRUE(events.values.empty());
	}
}

TEST_F(TaskItemWork, SnapshotsAreIndependentAndCanBeRestoredRepeatedly)
{
	WorkTestTask item;
	prepare(item);
	const auto first = item.createMemento();
	TaskWorkStateAccess::moveTo(item, tividy::AssignedState::instance());
	TaskWorkStateAccess::setAssignee(item, "second-worker");
	TaskWorkStateAccess::recordResult(item, reviewer, tividy::Outcome::accepted("SECOND"));
	const auto second = item.createMemento();
	item.restore(*first);
	EXPECT_EQ(actor.id(), item.assignee());
	EXPECT_TRUE(item.lastOutcome().code().empty());
	item.restore(*second);
	EXPECT_EQ("second-worker", item.assignee());
	EXPECT_EQ("SECOND", item.lastOutcome().code());
	item.restore(*first);
	EXPECT_EQ(tividy::ItemStatus::InProgress, item.status());
	EXPECT_EQ(actor.id(), item.assignee());
	EXPECT_TRUE(item.lastOutcome().code().empty());
	EXPECT_TRUE(events.values.empty());
}

TEST_F(TaskItemWork, InitialSnapshotRestoresCreatedDefaultsAndKeepsTheLaterBinding)
{
	HookedWorkTestTask item;
	const auto initial = item.createMemento();
	ASSERT_TRUE(initial);
	EXPECT_TRUE(item.calls.empty());
	prepare(item);
	TaskWorkStateAccess::bumpRejections(item);
	TaskWorkStateAccess::bumpEscalations(item);
	TaskWorkStateAccess::recordResult(item, actor, tividy::Outcome::accepted("LATER"));
	item.restore(*initial);
	EXPECT_EQ(tividy::ItemStatus::Created, item.status());
	EXPECT_TRUE(item.assignee().empty());
	EXPECT_EQ(tividy::TimePoint(), item.assignedAt());
	EXPECT_EQ(tividy::TimePoint(), item.dueAt());
	EXPECT_EQ(0u, item.rejections());
	EXPECT_EQ(0u, item.failures());
	EXPECT_EQ(0u, item.escalations());
	EXPECT_EQ(tividy::OutcomeKind::Accepted, item.lastOutcome().kind());
	EXPECT_TRUE(item.lastOutcome().code().empty());
	EXPECT_TRUE(item.lastOutcome().note().empty());
	EXPECT_TRUE(item.completedBy().empty());
	EXPECT_EQ(&context, &item.boundContext());
	EXPECT_TRUE(events.values.empty());
}

TEST_F(TaskItemWork, RestoringASnapshotCannotMakeIrreversibleWorkReversible)
{
	HookedWorkTestTask item;
	prepare(item);
	const auto snapshot = item.createMemento();
	item.irreversible = true;
	auto outcome = tividy::Outcome::accepted();
	ASSERT_TRUE(TaskWorkStateAccess::runWork(item, actor, outcome));
	item.restore(*snapshot);
	EXPECT_FALSE(item.isReversible());
	EXPECT_EQ(1u, context.irreversibleCount());
	EXPECT_TRUE(events.values.empty());
}
