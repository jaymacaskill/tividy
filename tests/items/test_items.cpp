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

test_items.cpp
*/

/**
 * @file test_items.cpp
 * @brief TaskItem and ManualTask as the Composite leaf and Prototype of the item tree: leaf behaviour, deadline, clone, visitor and binding.
 *
 * Lifecycle transitions are covered by tests/patterns/test_state.cpp, work hooks by test_task_item_work.cpp and WorkItemSpec by
 * test_engine_items.cpp. Only the real TaskItem, ManualTask, WorkContext and iterator are used.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <set>
#include <string>

#include "engine/core/Clock.h"
#include "engine/core/Errors.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkContext.h"
#include "engine/items/WorkItemIterator.h"

namespace tividy
{
	namespace
	{
		class NoteTask : public CloneableTask<NoteTask>
		{
			public:
				NoteTask(const WorkItemSpec& spec, const std::string& text) : CloneableTask<NoteTask>(spec), note(text) { }
				std::string note;
			protected:
				void performWork(const Participant&, Outcome&) override { }
		};

		class ItemTest : public ::testing::Test
		{
			protected:
				ItemTest() : clock(TimePoint() + std::chrono::hours(1000)), context("inst-1", data, events, clock, connectors, directory),
							 another("inst-2", otherData, otherEvents, clock, connectors, directory),
							 worker("w1", "Worker", std::set<std::string>()), reviewer("r1", "Reviewer", std::set<std::string>()) { }

				ManualClock clock;
				WorkflowData data, otherData;
				EventPublisher events, otherEvents;
				ConnectorRegistry connectors;
				ParticipantDirectory directory;
				WorkContext context, another;
				Participant worker, reviewer;
		};
	}

	TEST_F(ItemTest, ATaskIsALeafThatOnlyFindsItself)
	{
		ManualTask task(WorkItemSpec("t1", "Task"));
		EXPECT_EQ(task.find("t1"), &task);
		EXPECT_EQ(task.find("other"), static_cast<WorkItem*>(0));
		EXPECT_TRUE(task.children().empty());
		EXPECT_THROW(task.add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("t2", "Other")))), UnsupportedOperationError);

		std::unique_ptr<WorkItemIterator> it = task.createIterator();
		it->first();
		ASSERT_FALSE(it->isDone());
		EXPECT_EQ(&it->currentItem(), &task);
		it->next();
		EXPECT_TRUE(it->isDone());
	}

	TEST_F(ItemTest, TheDeadlineNeedsBothAnAssignmentAndADueTime)
	{
		ManualTask timed(WorkItemSpec("t1", "Timed").dueAfter(120));
		ManualTask open(WorkItemSpec("t2", "Open"));
		timed.bind(context);
		open.bind(context);
		EXPECT_EQ(timed.dueAt(), TimePoint());
		timed.publish();
		timed.assign("w1");
		open.publish();
		open.assign("w1");
		EXPECT_EQ(timed.dueAt(), clock.now() + std::chrono::seconds(120));
		EXPECT_EQ(open.dueAt(), TimePoint());
	}

	TEST_F(ItemTest, ACloneIsAFreshUnboundCopyOfTheTemplateOnly)
	{
		ManualTask original(WorkItemSpec("t1", "Task").requireRole("clerk").dueAfter(60).reviewRequired(true).reviewedBy("boss"));
		original.bind(context);
		original.publish();
		original.assign("w1");
		original.start(worker);
		original.submit(worker, Outcome::accepted("DONE"));
		original.reject(reviewer, "again");

		std::unique_ptr<WorkItem> copy = original.clone();
		ASSERT_TRUE(dynamic_cast<ManualTask*>(copy.get()) != 0);
		EXPECT_NE(copy.get(), &original);
		EXPECT_EQ(copy->spec().requiredRole(), "clerk");
		EXPECT_EQ(copy->spec().dueAfterSeconds(), 60);
		EXPECT_EQ(copy->status(), ItemStatus::Created);
		EXPECT_EQ(copy->assignee(), "");
		EXPECT_EQ(copy->assignedAt(), TimePoint());
		EXPECT_EQ(static_cast<TaskItem*>(copy.get())->rejections(), 0u);
		EXPECT_NO_THROW(copy->bind(another));
		EXPECT_EQ(original.status(), ItemStatus::InProgress);
		EXPECT_EQ(original.rejections(), 1u);
	}

	TEST_F(ItemTest, ACloneKeepsTheSubclassAndItsOwnFields)
	{
		NoteTask original(WorkItemSpec("t1", "Task"), "remember me");
		std::unique_ptr<WorkItem> copy = original.clone();
		NoteTask* typed = dynamic_cast<NoteTask*>(copy.get());
		ASSERT_TRUE(typed != 0);
		EXPECT_EQ(typed->note, "remember me");
		typed->note = "changed";
		EXPECT_EQ(original.note, "remember me");
	}

	TEST_F(ItemTest, BindingTwiceToTheSameContextIsHarmlessButAnotherContextIsRefused)
	{
		ManualTask task(WorkItemSpec("t1", "Task"));
		task.bind(context);
		EXPECT_NO_THROW(task.bind(context));
		EXPECT_THROW(task.bind(another), InvalidStateError);
	}

	TEST_F(ItemTest, ManualWorkLeavesTheReportedOutcomeUntouched)
	{
		ManualTask task(WorkItemSpec("t1", "Task"));
		task.bind(context);
		task.publish();
		task.assign("w1");
		task.start(worker);
		task.complete(worker, Outcome::rejected("DOCS_INVALID", "blurry"));
		EXPECT_EQ(task.lastOutcome().kind(), OutcomeKind::Rejected);
		EXPECT_EQ(task.lastOutcome().code(), "DOCS_INVALID");
		EXPECT_EQ(task.lastOutcome().note(), "blurry");
	}
}