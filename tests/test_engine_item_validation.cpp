#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "engine/core/Errors.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkContext.h"
#include "engine/items/WorkItemDecorator.h"

namespace tividy
{
namespace
{
    using Rules = std::vector<std::shared_ptr<const ValidationRule>>;

    // Records rule invocation order without changing the item or its data.
    class RecordingRule : public ValidationRule
    {
    public:
        RecordingRule(std::vector<int>& calls, int id,
                      const std::string& message)
            : calls_(calls), id_(id), message_(message)
        {
        }

        std::string check(const WorkItem&, const WorkflowData&,
                          const Outcome&) const override
        {
            calls_.push_back(id_);
            return message_;
        }

    private:
        std::vector<int>& calls_;
        int id_;
        std::string message_;
    };

    class RecordingObserver : public EventObserver
    {
    public:
        std::vector<WorkflowEvent> received;

        void onEvent(const WorkflowEvent& event) override
        {
            received.push_back(event);
        }
    };

    // false: complete(); true: submit().
    class WorkItemValidationTest : public ::testing::TestWithParam<bool>
    {
    protected:
        WorkflowData data;
        RecordingObserver observer;
        EventPublisher publisher;
        ManualClock clock{
            TimePoint() + std::chrono::seconds(100)
        };
        ConnectorRegistry connectors;
        ParticipantDirectory directory;
        WorkContext context{
            "instance", data, publisher, clock, connectors, directory
        };

        Participant actor{"worker", "Worker", std::set<std::string>{}};
        Outcome outcome = Outcome::rejected("DECLINED", "Document mismatch");

        std::vector<int> calls;
        ManualTask* component = nullptr; // Owned by the decorator.

        void SetUp() override
        {
            directory.add(actor);
            publisher.attach(&observer);
        }

        std::shared_ptr<const ValidationRule> rule(
            int id, const std::string& message = "")
        {
            return std::make_shared<RecordingRule>(calls, id, message);
        }

        std::unique_ptr<ValidatedWorkItem> makeItem(const Rules& rules)
        {
            WorkItemSpec spec("verify_documents", "Verify documents");
            spec.reviewRequired(GetParam())
                .reviewedBy("reviewer")
                .dueAfter(60);

            std::unique_ptr<WorkItem> task(new ManualTask(spec));
            component = static_cast<ManualTask*>(task.get());

            return std::unique_ptr<ValidatedWorkItem>(
                new ValidatedWorkItem(std::move(task), rules));
        }

        void startItem(WorkItem& item, WorkContext& boundContext)
        {
            item.bind(boundContext);
            item.publish();
            item.assign(actor.id());
            item.start(actor);
        }

        void finish(WorkItem& item)
        {
            if (GetParam())
                item.submit(actor, outcome);
            else
                item.complete(actor, outcome);
        }

        ItemStatus expectedStatus() const
        {
            return GetParam()
                ? ItemStatus::UnderReview
                : ItemStatus::Completed;
        }

        void expectValidationFailure(
            WorkItem& item, const std::string& expectedMessage)
        {
            try
            {
                finish(item);
                FAIL() << "Expected ValidationError";
            }
            catch (const ValidationError& error)
            {
                EXPECT_EQ(std::string(error.what()), expectedMessage);
            }
        }
    };
}

TEST(RequiredDataRuleTest, ReportsMissingAndEmptyKeysInConfiguredOrder)
{
    ManualTask item(WorkItemSpec("task", "Task"));
    WorkflowData data;
    data.set("present", "value");
    data.set("a_empty", "");

    RequiredDataRule rule({"z_missing", "present", "a_empty"});

    EXPECT_EQ(rule.check(item, data, Outcome()), "missing required data: z_missing, a_empty");
}

TEST(RequiredDataRuleTest, AcceptsPopulatedRequiredKeys)
{
    ManualTask item(WorkItemSpec("task", "Task"));
    WorkflowData data;
    data.set("document", "passport");
    data.set("reference", "123");

    RequiredDataRule rule({"document", "reference"});

    EXPECT_TRUE(rule.check(item, data, Outcome()).empty());
}

TEST(RequiredDataRuleTest, AcceptsAnEmptyRequiredKeyList)
{
    ManualTask item(WorkItemSpec("task", "Task"));
    WorkflowData data;
    RequiredDataRule rule(std::vector<std::string>{});

    EXPECT_TRUE(rule.check(item, data, Outcome()).empty());
}

TEST(WorkItemValidationTest, RunsRulesInOrderAndStopsAtFirstFailure)
{
    auto item = makeItem({rule(1), rule(2, "first failure"), rule(3, "later failure")});
    startItem(*item, context);
    observer.received.clear();

    expectValidationFailure(*item, "first failure");

    EXPECT_EQ(calls, std::vector<int>({1, 2}));
    EXPECT_EQ(component->status(), ItemStatus::InProgress);
    EXPECT_TRUE(observer.received.empty());
}

TEST(WorkItemValidationTest, PassingValidationForwardsActorAndOutcome)
{
    data.set("document", "passport");

    auto item = makeItem({rule(1), std::make_shared<RequiredDataRule>(std::vector<std::string>{"document"}), rule(2)});
    startItem(*item, context);
    observer.received.clear();

    ASSERT_NO_THROW(finish(*item));

    EXPECT_EQ(calls, std::vector<int>({1, 2}));
    EXPECT_EQ(component->status(), expectedStatus());
    EXPECT_EQ(component->lastOutcome().kind(), outcome.kind());
    EXPECT_EQ(component->lastOutcome().code(), outcome.code());
    EXPECT_EQ(component->lastOutcome().note(), outcome.note());

    ASSERT_EQ(observer.received.size(), 1u);
    const WorkflowEvent& event = observer.received.front();

    EXPECT_EQ(event.type(), GetParam() ? EventType::ItemSubmitted : EventType::ItemCompleted);
    EXPECT_EQ(event.actorId(), actor.id());
    EXPECT_EQ(event.itemId(), item->spec().id());
    EXPECT_EQ(event.outcome().kind(), outcome.kind());
    EXPECT_EQ(event.outcome().code(), outcome.code());
    EXPECT_EQ(event.outcome().note(), outcome.note());
}

TEST(WorkItemValidationTest, AnEmptyRuleListStillForwards)
{
    auto item = makeItem({});
    startItem(*item, context);
    observer.received.clear();

    ASSERT_NO_THROW(finish(*item));

    EXPECT_EQ(component->status(), expectedStatus());
    ASSERT_EQ(observer.received.size(), 1u);
}

TEST(WorkItemValidationTest, RequiredDataFailureLeavesStateUnchanged)
{
    data.set("empty", "");
    data.set("unrelated", "keep this");

    auto item = makeItem({std::make_shared<RequiredDataRule>(std::vector<std::string>{"missing", "empty"})});
    startItem(*item, context);
    observer.received.clear();

    const auto beforeData = data.all();
    const TimePoint beforeAssigned = component->assignedAt();
    const TimePoint beforeDue = component->dueAt();
    const Outcome beforeOutcome = component->lastOutcome();
    const Id beforeCompletedBy = component->completedBy();

    expectValidationFailure(*item, "missing required data: missing, empty");

    EXPECT_EQ(component->status(), ItemStatus::InProgress);
    EXPECT_EQ(component->assignee(), actor.id());
    EXPECT_EQ(component->assignedAt(), beforeAssigned);
    EXPECT_EQ(component->dueAt(), beforeDue);
    EXPECT_EQ(component->lastOutcome().kind(), beforeOutcome.kind());
    EXPECT_EQ(component->lastOutcome().code(), beforeOutcome.code());
    EXPECT_EQ(component->lastOutcome().note(), beforeOutcome.note());
    EXPECT_EQ(component->completedBy(), beforeCompletedBy);
    EXPECT_EQ(component->rejections(), 0u);
    EXPECT_EQ(component->failures(), 0u);
    EXPECT_EQ(component->escalations(), 0u);
    EXPECT_TRUE(component->isReversible());
    EXPECT_EQ(context.irreversibleCount(), 0u);
    EXPECT_EQ(data.all(), beforeData);
    EXPECT_TRUE(observer.received.empty());

    // Correcting the data allows the same item to proceed.
    data.set("missing", "supplied");
    data.set("empty", "now populated");

    ASSERT_NO_THROW(finish(*item));
    EXPECT_EQ(component->status(), expectedStatus());
}

TEST(WorkItemValidationTest, ClonePreservesRulesAndIndependentComponent)
{
    auto original = makeItem({rule(1), std::make_shared<RequiredDataRule>(std::vector<std::string>{"document"}), rule(3)});

    // Clone the unbound template, as required by the WorkItem contract.
    auto copy = original->clone();

    ASSERT_NE(copy.get(), nullptr);
    ASSERT_NE(dynamic_cast<ValidatedWorkItem*>(copy.get()), nullptr);
    EXPECT_NE(&copy->spec(), &original->spec());
    EXPECT_EQ(copy->spec().id(), original->spec().id());
    EXPECT_EQ(copy->spec().name(), original->spec().name());
    EXPECT_EQ(copy->spec().requiresReview(), GetParam());
    EXPECT_EQ(copy->spec().dueAfterSeconds(), 60);
    EXPECT_EQ(copy->status(), ItemStatus::Created);
    EXPECT_TRUE(copy->assignee().empty());
    EXPECT_THROW(finish(*copy), InvalidStateError);

    data.set("document", "original document");
    startItem(*original, context);

    WorkflowData copyData;
    WorkContext copyContext("copy_instance", copyData, publisher, clock, connectors, directory);
    startItem(*copy, copyContext);

    observer.received.clear();
    calls.clear();

    // The clone must validate its own data using the preserved rules.
    expectValidationFailure(*copy, "missing required data: document");

    EXPECT_EQ(calls, std::vector<int>({1}));
    EXPECT_EQ(copy->status(), ItemStatus::InProgress);
    EXPECT_EQ(original->status(), ItemStatus::InProgress);
    EXPECT_TRUE(observer.received.empty());

    // Advancing the clone must leave the original component untouched.
    copyData.set("document", "cloned document");
    calls.clear();

    ASSERT_NO_THROW(finish(*copy));

    EXPECT_EQ(calls, std::vector<int>({1, 3}));
    EXPECT_EQ(copy->status(), expectedStatus());
    EXPECT_EQ(original->status(), ItemStatus::InProgress);
    EXPECT_EQ(data.get("document"), "original document");
}
} // namespace tividy