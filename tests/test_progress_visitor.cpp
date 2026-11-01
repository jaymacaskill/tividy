#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "engine/items/StageItem.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemVisitor.h"

namespace tividy
{
namespace
{
class ProgressTask : public CloneableTask<ProgressTask>
{
public:
    ProgressTask(const Id& id, ItemStatus status)
        : CloneableTask<ProgressTask>(WorkItemSpec(id, id)), status_ref(status) {}
    ItemStatus status() const override { return status_ref; }
protected:
    void performWork(const Participant&, Outcome&) override {}
private:
    ItemStatus status_ref;
};

std::unique_ptr<WorkItem> progressTask(const Id& id, ItemStatus status)
{
    return std::unique_ptr<WorkItem>(new ProgressTask(id, status));
}
}

// Complements PR #58: every status contributes to the denominator, including
// Available, Assigned, UnderReview and Escalated, across more than one level.
TEST(ProgressVisitorAggregation, AllStatusesAcrossDeepAndEmptyStages)
{
    const ItemStatus statuses[] = {ItemStatus::Created, ItemStatus::Available,
        ItemStatus::Assigned, ItemStatus::InProgress, ItemStatus::UnderReview,
        ItemStatus::Escalated, ItemStatus::Completed, ItemStatus::Cancelled,
        ItemStatus::Skipped};
    StageItem root(WorkItemSpec("root", "root"));
    std::unique_ptr<StageItem> inner(new StageItem(WorkItemSpec("inner", "inner")));
    std::unique_ptr<StageItem> deep(new StageItem(WorkItemSpec("deep", "deep")));
    deep->add(std::unique_ptr<WorkItem>(new StageItem(WorkItemSpec("empty", "empty"))));
    for (std::size_t i = 0; i < 9; ++i)
    {
        StageItem& parent = i < 3 ? root : (i < 6 ? *inner : *deep);
        parent.add(progressTask("task-" + std::to_string(i), statuses[i]));
    }
    inner->add(std::move(deep));
    root.add(std::move(inner));
    ProgressVisitor visitor;
    root.accept(visitor);
    EXPECT_EQ(visitor.total(), 9u);
    for (ItemStatus status : statuses)
        EXPECT_EQ(visitor.count(status), 1u) << toString(status);
    EXPECT_NEAR(visitor.percentSettled(), 200.0 / 9.0, 1e-12);
}

TEST(ProgressVisitorAggregation, VisitsAccumulateAndQueriesDoNotChangeCounts)
{
    ProgressTask completed("completed", ItemStatus::Completed);
    ProgressTask skipped("skipped", ItemStatus::Skipped);
    ProgressTask pending("pending", ItemStatus::Available);
    ProgressVisitor visitor;
    completed.accept(visitor);
    EXPECT_DOUBLE_EQ(visitor.percentSettled(), 100.0);
    skipped.accept(visitor);
    EXPECT_DOUBLE_EQ(visitor.percentSettled(), 100.0);
    pending.accept(visitor);
    const ProgressVisitor& result = visitor;
    EXPECT_EQ(result.count(ItemStatus::Cancelled), 0u);
    EXPECT_EQ(result.count(ItemStatus::Cancelled), 0u);
    EXPECT_NEAR(result.percentSettled(), 200.0 / 3.0, 1e-12);
    pending.accept(visitor);
    EXPECT_EQ(result.total(), 4u);
    EXPECT_EQ(result.count(ItemStatus::Available), 2u);
    EXPECT_EQ(result.count(ItemStatus::Completed), 1u);
    EXPECT_EQ(result.count(ItemStatus::Skipped), 1u);
    EXPECT_DOUBLE_EQ(result.percentSettled(), 50.0);
}
}
