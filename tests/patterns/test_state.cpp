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

test_state.cpp
*/

/**
 * @file test_state.cpp
 * @brief State pattern: the legal/illegal transition table of WorkItemState, one concrete state per ItemStatus, driven through TaskItem.
 *
 * The expected table below is written from the table in WorkItemState.h, NOT derived from the implementation, so a transition added or
 * removed in WorkItemStates.cpp makes a cell of the matrix fail. Only the real TaskItem/ManualTask/WorkContext/EventPublisher are used.
 * (Hook ordering, irreversible work and snapshots belong to test_task_item_work.cpp and test_task_irreversible_effect.cpp.)
 * The DISABLED_ test records a known gap: an unbound task changes state before its event fails; enable it once that is made atomic.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "engine/core/Clock.h"
#include "engine/core/Errors.h"
#include "engine/events/Observer.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkContext.h"

namespace tividy
{
	namespace
	{
		typedef ItemStatus S;
		enum Op { Publish, Assign, Start, Submit, Complete, Approve, Reject, Escalate, Cancel, Skip, Expire, OpCount };
		struct Row { S from; Op op; S to; int review; };   // review: -1 either, 0 plain only, 1 reviewed only

		const Row kTable[] = {
			{S::Created, Publish, S::Available, -1}, {S::Created, Cancel, S::Cancelled, -1}, {S::Created, Skip, S::Skipped, -1},
			{S::Available, Assign, S::Assigned, -1}, {S::Available, Cancel, S::Cancelled, -1}, {S::Available, Skip, S::Skipped, -1},
			{S::Assigned, Start, S::InProgress, -1}, {S::Assigned, Escalate, S::Escalated, -1}, {S::Assigned, Cancel, S::Cancelled, -1},
			{S::Assigned, Expire, S::Completed, -1}, {S::InProgress, Complete, S::Completed, 0}, {S::InProgress, Submit, S::UnderReview, 1},
			{S::InProgress, Escalate, S::Escalated, -1}, {S::InProgress, Cancel, S::Cancelled, -1}, {S::InProgress, Expire, S::Completed, -1},
			{S::UnderReview, Approve, S::Completed, -1}, {S::UnderReview, Reject, S::InProgress, -1}, {S::UnderReview, Escalate, S::Escalated, -1},
			{S::UnderReview, Cancel, S::Cancelled, -1}, {S::UnderReview, Expire, S::Completed, -1}, {S::Escalated, Assign, S::Escalated, -1},
			{S::Escalated, Approve, S::Completed, -1}, {S::Escalated, Cancel, S::Cancelled, -1}};

		const S kStatuses[] = {S::Created, S::Available, S::Assigned, S::InProgress, S::UnderReview, S::Escalated, S::Completed, S::Cancelled, S::Skipped};

		class Recorder : public EventObserver
		{
			public:
				void onEvent(const WorkflowEvent& event) override { seen.push_back(event); }
				std::vector<WorkflowEvent> seen;
		};

		class FlakyTask : public CloneableTask<FlakyTask>
		{
			public:
				explicit FlakyTask(const WorkItemSpec& spec) : CloneableTask<FlakyTask>(spec) { }
			protected:
				void performWork(const Participant&, Outcome&) override { throw ExternalFailure("connector down", true); }
		};

		class StateTest : public ::testing::Test
		{
			protected:
				StateTest() : clock(TimePoint() + std::chrono::hours(24 * 365 * 56)), context("inst-1", data, events, clock, connectors, directory),
							  worker("w1", "Worker", std::set<std::string>()), other("w2", "Other", std::set<std::string>()),
							  reviewer("r1", "Reviewer", std::set<std::string>()), approver("a1", "Approver", std::set<std::string>())
				{
					events.attach(&recorder);
				}

				std::unique_ptr<ManualTask> make(bool reviewed = false, unsigned retries = 2, long due = 0)
				{
					WorkItemSpec spec("task", "The task");
					spec.reviewRequired(reviewed).maxRetries(retries).dueAfter(due);
					if (reviewed) spec.reviewedBy("approver-role");
					std::unique_ptr<ManualTask> item(new ManualTask(spec));
					item->bind(context);
					return item;
				}

				void apply(TaskItem& item, Op op)
				{
					switch (op)
					{
						case Publish:  item.publish(); break;
						case Assign:   item.assign(item.status() == S::Escalated ? "a1" : "w1"); break;
						case Start:    item.start(worker); break;
						case Submit:   item.submit(worker, Outcome::accepted("DONE")); break;
						case Complete: item.complete(worker, Outcome::accepted("DONE")); break;
						case Approve:  item.approve(reviewer, Outcome::accepted("OK")); break;
						case Reject:   item.reject(reviewer, "not good enough"); break;
						case Escalate: item.escalate("too slow"); break;
						case Cancel:   item.cancel("stopped"); break;
						case Skip:     item.skip("not needed"); break;
						case Expire:   item.expire("LAPSED", "deadline lapsed"); break;
						default: break;
					}
				}

				void reach(TaskItem& item, S target, bool reviewed = false)
				{
					std::vector<Op> path;
					switch (target)
					{
						case S::Available:   path = {Publish}; break;
						case S::Assigned:    path = {Publish, Assign}; break;
						case S::Escalated:   path = {Publish, Assign, Escalate}; break;
						case S::InProgress:  path = {Publish, Assign, Start}; break;
						case S::UnderReview: path = {Publish, Assign, Start, Submit}; break;
						case S::Completed:   path = {Publish, Assign, Start, reviewed ? Submit : Complete}; if (reviewed) path.push_back(Approve); break;
						case S::Cancelled:   path = {Cancel}; break;
						case S::Skipped:     path = {Skip}; break;
						default: break;
					}
					for (const Op op : path) apply(item, op);
					ASSERT_EQ(item.status(), target);
					recorder.seen.clear();
				}

				void checkMatrix(bool reviewed)
				{
					for (const S from : kStatuses)
						for (int o = 0; o < OpCount; ++o)
						{
							if (from == S::UnderReview && !reviewed) continue;
							const Row* row = 0;
							for (const Row& r : kTable)
								if (r.from == from && r.op == o && (r.review < 0 || (r.review == 1) == reviewed)) row = &r;
							SCOPED_TRACE(std::string(toString(from)) + " + op " + std::to_string(o));
							std::unique_ptr<ManualTask> item = make(reviewed);
							reach(*item, from, reviewed);
							if (row)
							{
								EXPECT_NO_THROW(apply(*item, static_cast<Op>(o)));
								EXPECT_EQ(item->status(), row->to);
								EXPECT_EQ(recorder.seen.size(), 1u);
							}
							else
							{
								EXPECT_THROW(apply(*item, static_cast<Op>(o)), InvalidStateError);
								EXPECT_EQ(item->status(), from);
								EXPECT_TRUE(recorder.seen.empty());
							}
						}
				}

				ManualClock clock;
				WorkflowData data;
				EventPublisher events;
				ConnectorRegistry connectors;
				ParticipantDirectory directory;
				WorkContext context;
				Recorder recorder;
				Participant worker, other, reviewer, approver;
		};
	}

	TEST_F(StateTest, TransitionTableWithoutReview) { checkMatrix(false); }

	TEST_F(StateTest, TransitionTableWithReview) { checkMatrix(true); }

	TEST_F(StateTest, ALifecyclePublishesOneEventPerStepFromTheRightActor)
	{
		std::unique_ptr<ManualTask> item = make(true);
		for (const Op op : {Publish, Assign, Start, Submit, Approve}) apply(*item, op);
		const EventType types[] = {EventType::ItemAvailable, EventType::ItemAssigned, EventType::ItemStarted, EventType::ItemSubmitted, EventType::ItemCompleted};
		const char* const actors[] = {"system", "w1", "w1", "w1", "r1"};
		ASSERT_EQ(recorder.seen.size(), 5u);
		for (std::size_t i = 0; i < 5; ++i)
		{
			EXPECT_EQ(recorder.seen[i].type(), types[i]);
			EXPECT_EQ(recorder.seen[i].actorId(), actors[i]);
		}
		EXPECT_EQ(item->completedBy(), "r1");
	}

	TEST_F(StateTest, OnlyTheAssigneeMayStartCompleteOrSubmit)
	{
		std::unique_ptr<ManualTask> plain = make(false);
		reach(*plain, S::Assigned);
		EXPECT_THROW(plain->start(other), PermissionDeniedError);
		plain->start(worker);
		EXPECT_THROW(plain->complete(other, Outcome()), PermissionDeniedError);

		std::unique_ptr<ManualTask> reviewed = make(true);
		reach(*reviewed, S::InProgress, true);
		EXPECT_THROW(reviewed->submit(other, Outcome()), PermissionDeniedError);
		EXPECT_THROW(reviewed->complete(other, Outcome()), InvalidStateError);
		EXPECT_EQ(plain->status(), S::InProgress);
		EXPECT_EQ(reviewed->status(), S::InProgress);
	}

	TEST_F(StateTest, RejectionsAreAllowedUpToMaxRetriesThenTheNextOneCancels)
	{
		std::unique_ptr<ManualTask> item = make(true, 2);
		reach(*item, S::InProgress, true);
		for (unsigned round = 1; round <= 2; ++round)
		{
			apply(*item, Submit);
			apply(*item, Reject);
			EXPECT_EQ(item->status(), S::InProgress);
			EXPECT_EQ(item->rejections(), round);
			EXPECT_EQ(recorder.seen.back().actorId(), "r1");
			EXPECT_EQ(recorder.seen.back().outcome().code(), "REJECTED");
		}
		apply(*item, Submit);
		apply(*item, Reject);
		EXPECT_EQ(item->status(), S::Cancelled);
		EXPECT_EQ(recorder.seen.back().type(), EventType::ItemCancelled);

		std::unique_ptr<ManualTask> none = make(true, 0);
		reach(*none, S::UnderReview, true);
		apply(*none, Reject);
		EXPECT_EQ(none->status(), S::Cancelled);
	}

	TEST_F(StateTest, ApproveWithNoCodeKeepsTheSubmittedCode)
	{
		std::unique_ptr<ManualTask> item = make(true);
		reach(*item, S::UnderReview, true);
		item->approve(reviewer, Outcome());
		EXPECT_EQ(item->lastOutcome().code(), "DONE");
		EXPECT_EQ(recorder.seen.back().outcome().code(), "DONE");
	}

	TEST_F(StateTest, EscalationIsCountedFromEveryOpenStatus)
	{
		for (const S origin : {S::Assigned, S::InProgress, S::UnderReview})
		{
			std::unique_ptr<ManualTask> item = make(true);
			reach(*item, origin, true);
			item->escalate("late");
			EXPECT_EQ(item->escalations(), 1u);
			EXPECT_EQ(recorder.seen.back().type(), EventType::ItemEscalated);
			EXPECT_EQ(recorder.seen.back().count(), 1u);
		}
	}

	TEST_F(StateTest, TheEscalationApproverOverridesWithoutMovingTheDeadline)
	{
		std::unique_ptr<ManualTask> item = make(false, 2, 600);
		reach(*item, S::Escalated);
		const TimePoint assigned = item->assignedAt();
		clock.advance(300);
		item->assign("a1");
		EXPECT_EQ(item->status(), S::Escalated);
		EXPECT_EQ(item->assignee(), "a1");
		EXPECT_EQ(item->assignedAt(), assigned);
		EXPECT_EQ(item->dueAt(), assigned + std::chrono::seconds(600));

		item->approve(approver, Outcome::accepted("OVERRIDE"));
		EXPECT_EQ(item->status(), S::Completed);
		EXPECT_EQ(item->completedBy(), "a1");
	}

	TEST_F(StateTest, ExpireCompletesFromEveryOpenStatusAsTheSystemWithTheTimeoutOutcome)
	{
		for (const S origin : {S::Assigned, S::InProgress, S::UnderReview})
		{
			std::unique_ptr<ManualTask> item = make(true);
			reach(*item, origin, true);
			item->expire("DEFAULT_JUDGMENT", "deadline lapsed");
			EXPECT_EQ(item->status(), S::Completed);
			EXPECT_EQ(item->completedBy(), "system");
			EXPECT_EQ(item->lastOutcome().code(), "DEFAULT_JUDGMENT");
			EXPECT_EQ(recorder.seen.back().actorId(), "system");
		}
	}

	TEST_F(StateTest, ATechnicalFailureLeavesTheItemWhereItWas)
	{
		FlakyTask working(WorkItemSpec("task", "Flaky"));
		working.bind(context);
		working.publish();
		working.assign("w1");
		working.start(worker);
		EXPECT_NO_THROW(working.complete(worker, Outcome::accepted("DONE")));
		EXPECT_EQ(working.status(), S::InProgress);
		EXPECT_EQ(recorder.seen.back().type(), EventType::ItemFailed);

		FlakyTask escalated(WorkItemSpec("task", "Flaky"));
		escalated.bind(context);
		escalated.publish();
		escalated.assign("w1");
		escalated.escalate("late");
		EXPECT_NO_THROW(escalated.approve(approver, Outcome::accepted("OVERRIDE")));
		EXPECT_EQ(escalated.status(), S::Escalated);
	}

	TEST(StateGapTest, DISABLED_AnUnboundItemKeepsItsStatusWhenAnOperationFails)
	{
		ManualTask item((WorkItemSpec("task", "The task")));
		EXPECT_THROW(item.publish(), InvalidStateError);
		EXPECT_EQ(item.status(), S::Created);
	}
}