#include <gtest/gtest.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/Types.h"
#include "engine/items/StageItem.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItem.h"
#include "engine/items/WorkItemDecorator.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/items/WorkItemVisitor.h"

/*
	Visitor:          WorkItemVisitor (visitTask, visitStage).
	ConcreteVisitor:  ProgressVisitor, plus RecordingVisitor defined below to show that a new
	                  operation over the tree needs no change to the item classes.
	Elements:         WorkItem::accept(), implemented by TaskItem and StageItem. Decorators (e.g. SecureWorkItem)
	                  forward accept() to the wrapped item, so they are invisible to visitors.
*/

namespace tividy
{
	namespace
	{
		WorkItemSpec makeSpec(const Id& id)
		{
			return WorkItemSpec(id, id);
		}

		std::unique_ptr<WorkItem> makeTask(const Id& id)
		{
			return std::unique_ptr<WorkItem>(new ManualTask(makeSpec(id)));
		}

		std::unique_ptr<StageItem> makeStage(const Id& id)
		{
			return std::unique_ptr<StageItem>(new StageItem(makeSpec(id)));
		}

		// Test double: a task that reports a fixed status, so ProgressVisitor can be checked against
		// every status without binding the item or driving it through the state machine.
		class FixedStatusTask : public CloneableTask<FixedStatusTask>
		{
			public:
				FixedStatusTask(const WorkItemSpec& spec, ItemStatus status) : CloneableTask<FixedStatusTask>(spec), status_ref(status) { }
				ItemStatus status() const override { return status_ref; }
			protected:
				void performWork(const Participant&, Outcome&) override { }
			private:
				ItemStatus status_ref;
		};

		std::unique_ptr<WorkItem> makeTask(const Id& id, ItemStatus status)
		{
			return std::unique_ptr<WorkItem>(new FixedStatusTask(makeSpec(id), status));
		}

		// A ConcreteVisitor written only for these tests: records every visit as "task:<id>" or "stage:<id>".
		class RecordingVisitor : public WorkItemVisitor
		{
			public:
				void visitTask(const TaskItem& task) override { visits_ref.push_back("task:" + task.spec().id()); }
				void visitStage(const StageItem& stage) override { visits_ref.push_back("stage:" + stage.spec().id()); }
				const std::vector<std::string>& visits() const { return visits_ref; }
			private:
				std::vector<std::string> visits_ref;
		};

		// root
		//  |- a
		//  |- inner
		//  |   |- b
		//  |   |- c
		//  |- d
		std::unique_ptr<StageItem> makeNestedTree()
		{
			std::unique_ptr<StageItem> root = makeStage("root");
			root->add(makeTask("a"));
			std::unique_ptr<StageItem> inner = makeStage("inner");
			inner->add(makeTask("b"));
			inner->add(makeTask("c"));
			root->add(std::move(inner));
			root->add(makeTask("d"));
			return root;
		}
	}

	// --- Double dispatch and traversal (WorkItem::accept) ---

	TEST(VisitorPatternTest, TaskAcceptCallsVisitTaskOnce)
	{
		ManualTask task(makeSpec("a"));
		RecordingVisitor visitor;
		task.accept(visitor);

		ASSERT_EQ(visitor.visits().size(), 1u);
		EXPECT_EQ(visitor.visits()[0], "task:a");
	}

	TEST(VisitorPatternTest, EmptyStageAcceptCallsVisitStageOnce)
	{
		std::unique_ptr<StageItem> stage = makeStage("empty");
		RecordingVisitor visitor;
		stage->accept(visitor);

		ASSERT_EQ(visitor.visits().size(), 1u);
		EXPECT_EQ(visitor.visits()[0], "stage:empty");
	}

	TEST(VisitorPatternTest, AcceptThroughBaseReferenceDispatchesOnConcreteType)
	{
		std::unique_ptr<WorkItem> task = makeTask("a");
		std::unique_ptr<WorkItem> stage(makeStage("s").release());
		RecordingVisitor visitor;

		const WorkItem& task_ref = *task;
		const WorkItem& stage_ref = *stage;
		task_ref.accept(visitor);
		stage_ref.accept(visitor);

		ASSERT_EQ(visitor.visits().size(), 2u);
		EXPECT_EQ(visitor.visits()[0], "task:a");
		EXPECT_EQ(visitor.visits()[1], "stage:s");
	}

	TEST(VisitorPatternTest, StageIsVisitedBeforeChildrenInPreOrder)
	{
		std::unique_ptr<StageItem> root = makeNestedTree();
		RecordingVisitor visitor;
		root->accept(visitor);

		const std::vector<std::string> expected = {"stage:root", "task:a", "stage:inner", "task:b", "task:c", "task:d"};
		EXPECT_EQ(visitor.visits(), expected);
	}

	TEST(VisitorPatternTest, VisitingDoesNotChangeItemStatus)
	{
		std::unique_ptr<StageItem> root = makeNestedTree();
		const ItemStatus root_before = root->status();
		const ItemStatus task_before = root->find("b")->status();

		RecordingVisitor visitor;
		const StageItem& const_root = *root;
		const_root.accept(visitor);

		EXPECT_EQ(root->status(), root_before);
		EXPECT_EQ(root->find("b")->status(), task_before);
	}

	TEST(VisitorPatternTest, DifferentVisitorsOverSameTreeAreIndependent)
	{
		std::unique_ptr<StageItem> root = makeNestedTree();
		RecordingVisitor recorder;
		ProgressVisitor progress;

		root->accept(recorder);
		root->accept(progress);

		EXPECT_EQ(recorder.visits().size(), 6u);
		EXPECT_EQ(progress.total(), 4u);
	}

	TEST(VisitorPatternTest, SecureWrappedTaskIsVisitedAsTheTask)
	{
		SecureWorkItem secured(makeTask("a"));
		RecordingVisitor visitor;
		secured.accept(visitor);

		// Exactly one visitTask for the wrapped task; the decorator adds no visit of its own.
		ASSERT_EQ(visitor.visits().size(), 1u);
		EXPECT_EQ(visitor.visits()[0], "task:a");
	}

	TEST(VisitorPatternTest, SecureWrappedTaskInsideTreeKeepsOrderAndRealStatus)
	{
		std::unique_ptr<StageItem> root = makeStage("root");
		root->add(makeTask("a"));
		root->add(std::unique_ptr<WorkItem>(new SecureWorkItem(makeTask("secured", ItemStatus::Completed))));
		root->add(makeTask("d"));

		RecordingVisitor recorder;
		ProgressVisitor progress;
		root->accept(recorder);
		root->accept(progress);

		const std::vector<std::string> expected = {"stage:root", "task:a", "task:secured", "task:d"};
		EXPECT_EQ(recorder.visits(), expected);
		EXPECT_EQ(progress.total(), 3u);
		EXPECT_EQ(progress.count(ItemStatus::Completed), 1u);   // the status of the task behind the decorator
	}

	// --- ProgressVisitor (ConcreteVisitor) ---

	TEST(ProgressVisitorTest, FreshVisitorHasNoTasksAndIsFullySettled)
	{
		const ProgressVisitor visitor;
		EXPECT_EQ(visitor.total(), 0u);
		for (ItemStatus status : {ItemStatus::Created, ItemStatus::Available, ItemStatus::Assigned,
								  ItemStatus::InProgress, ItemStatus::UnderReview, ItemStatus::Escalated,
								  ItemStatus::Completed, ItemStatus::Cancelled, ItemStatus::Skipped})
		{
			EXPECT_EQ(visitor.count(status), 0u) << toString(status);
		}
		EXPECT_DOUBLE_EQ(visitor.percentSettled(), 100.0);
	}

	TEST(ProgressVisitorTest, EmptyStageCountsNoTasks)
	{
		std::unique_ptr<StageItem> stage = makeStage("empty");
		ProgressVisitor visitor;
		stage->accept(visitor);

		EXPECT_EQ(visitor.total(), 0u);
		EXPECT_DOUBLE_EQ(visitor.percentSettled(), 100.0);
	}

	TEST(ProgressVisitorTest, CountsTasksAndIgnoresStages)
	{
		std::unique_ptr<StageItem> root = makeNestedTree();
		ProgressVisitor visitor;
		root->accept(visitor);

		EXPECT_EQ(visitor.total(), 4u);
	}

	TEST(ProgressVisitorTest, UnboundTaskIsCountedAsCreated)
	{
		ManualTask task(makeSpec("a"));
		ProgressVisitor visitor;
		task.accept(visitor);

		EXPECT_EQ(visitor.total(), 1u);
		EXPECT_EQ(visitor.count(ItemStatus::Created), 1u);
		EXPECT_DOUBLE_EQ(visitor.percentSettled(), 0.0);
	}

	TEST(ProgressVisitorTest, CountsEachStatusAcrossNestedStages)
	{
		std::unique_ptr<StageItem> root = makeStage("root");
		root->add(makeTask("done_1", ItemStatus::Completed));
		std::unique_ptr<StageItem> inner = makeStage("inner");
		inner->add(makeTask("done_2", ItemStatus::Completed));
		inner->add(makeTask("skipped", ItemStatus::Skipped));
		inner->add(makeTask("working", ItemStatus::InProgress));
		root->add(std::move(inner));
		root->add(makeTask("dropped", ItemStatus::Cancelled));

		ProgressVisitor visitor;
		root->accept(visitor);

		EXPECT_EQ(visitor.total(), 5u);
		EXPECT_EQ(visitor.count(ItemStatus::Completed), 2u);
		EXPECT_EQ(visitor.count(ItemStatus::Skipped), 1u);
		EXPECT_EQ(visitor.count(ItemStatus::InProgress), 1u);
		EXPECT_EQ(visitor.count(ItemStatus::Cancelled), 1u);
		EXPECT_EQ(visitor.count(ItemStatus::Created), 0u);
	}

	TEST(ProgressVisitorTest, PercentSettledCountsCompletedAndSkippedOnly)
	{
		std::unique_ptr<StageItem> root = makeStage("root");
		root->add(makeTask("done_1", ItemStatus::Completed));
		root->add(makeTask("done_2", ItemStatus::Completed));
		root->add(makeTask("skipped", ItemStatus::Skipped));
		root->add(makeTask("working", ItemStatus::InProgress));
		root->add(makeTask("dropped", ItemStatus::Cancelled));

		ProgressVisitor visitor;
		root->accept(visitor);

		// 100 * (2 Completed + 1 Skipped) / 5 tasks
		EXPECT_DOUBLE_EQ(visitor.percentSettled(), 60.0);
	}

	TEST(ProgressVisitorTest, CancelledTasksAreNotSettled)
	{
		std::unique_ptr<StageItem> root = makeStage("root");
		root->add(makeTask("dropped_1", ItemStatus::Cancelled));
		root->add(makeTask("dropped_2", ItemStatus::Cancelled));

		ProgressVisitor visitor;
		root->accept(visitor);

		EXPECT_EQ(visitor.total(), 2u);
		EXPECT_DOUBLE_EQ(visitor.percentSettled(), 0.0);
	}
} // namespace tividy