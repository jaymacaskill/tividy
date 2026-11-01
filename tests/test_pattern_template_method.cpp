#include <gtest/gtest.h>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/core/Errors.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkContext.h"

namespace tividy
{
namespace
{
    class RecordingObserver : public EventObserver
    {
    public:
        std::vector<WorkflowEvent> received;

        void onEvent(const WorkflowEvent& event) override
        {
            received.push_back(event);
        }
    };

    // Overrides the hooks, leaving the real Template Method unchanged.
    class ProbeTask : public CloneableTask<ProbeTask>
    {
    public:
        enum class Failure { None, Retryable, Permanent, Unexpected };

        explicit ProbeTask(const WorkItemSpec& spec)
            : CloneableTask<ProbeTask>(spec) {}

        mutable std::vector<std::string> calls;
        bool rejectOutcome = false;
        bool refineOutcome = false;
        bool irreversible = false;
        Failure failure = Failure::None;
        Outcome completedOutcome;
        unsigned irreversibleAtCompletion = 0;

    protected:
        void validateOutcome(const Outcome&) const override
        {
            calls.push_back("validate");

            if (rejectOutcome)
                throw ValidationError("invalid outcome");
        }

        void performWork(const Participant&, Outcome& outcome) override
        {
            calls.push_back("perform");

            if (failure == Failure::Retryable)
                throw ExternalFailure("offline", true);

            if (failure == Failure::Permanent)
                throw ExternalFailure("offline", false);

            if (failure == Failure::Unexpected)
                throw std::logic_error("unexpected error");

            if (refineOutcome)
                outcome = Outcome::rejected("ISSUES_FOUND", "needs review");
        }

        bool performsIrreversibleWork() const override
        {
            return irreversible;
        }

        void onCompleted(const Outcome& outcome) override
        {
            calls.push_back("completed");
            completedOutcome = outcome;
            irreversibleAtCompletion = context().irreversibleCount();
        }
    };

    class TemplateMethodTest : public ::testing::Test
    {
    protected:
        RecordingObserver recorder;
        EventPublisher events;
        WorkflowData data;
        ManualClock clock{TimePoint()};
        ConnectorRegistry connectors;
        ParticipantDirectory directory;

        WorkContext context{
            "inst-1", data, events, clock, connectors, directory
        };

        Participant actor{
            "u1", "Alice", std::set<std::string>{"operator"}
        };

        ProbeTask task{WorkItemSpec("task-1", "Template task")};

        void SetUp() override
        {
            directory.add(actor);
            events.attach(&recorder);
        }

        void start(TaskItem& item)
        {
            item.bind(context);
            item.publish();
            item.assign(actor.id());
            item.start(actor);

            // Assertions below concern events produced by completing work.
            recorder.received.clear();
        }

        void checkExternalFailure(bool retryable)
        {
            task.failure = retryable ? ProbeTask::Failure::Retryable
                                     : ProbeTask::Failure::Permanent;
            task.irreversible = true;
            start(task);

            for (unsigned attempt = 1; attempt <= 2; ++attempt)
            {
                ASSERT_NO_THROW(
                    task.complete(actor, Outcome::accepted()));

                EXPECT_EQ(task.status(), ItemStatus::InProgress);
                EXPECT_EQ(task.failures(), attempt);
                EXPECT_TRUE(task.completedBy().empty());
                EXPECT_EQ(context.irreversibleCount(), 0u);
                EXPECT_TRUE(task.isReversible());

                ASSERT_EQ(recorder.received.size(), attempt);
                const WorkflowEvent& event = recorder.received.back();

                EXPECT_EQ(event.type(), EventType::ItemFailed);
                EXPECT_EQ(event.count(), attempt);
                EXPECT_EQ(event.outcome().kind(), OutcomeKind::Failed);
                EXPECT_EQ(
                    event.outcome().code(),
                    retryable ? "RETRYABLE" : "PERMANENT");
                EXPECT_EQ(event.outcome().note(), "offline");
            }

            EXPECT_EQ(task.calls, (std::vector<std::string>{
                "validate", "perform", "validate", "perform"
            }));
        }
    };

    TEST_F(TemplateMethodTest, RunsHooksInOrderAndCompletes)
    {
        start(task);

        ASSERT_NO_THROW(
            task.complete(actor, Outcome::accepted("OK")));

        EXPECT_EQ(task.calls, (std::vector<std::string>{
            "validate", "perform", "completed"
        }));

        EXPECT_EQ(task.status(), ItemStatus::Completed);
        EXPECT_EQ(task.completedBy(), actor.id());
        EXPECT_EQ(task.lastOutcome().code(), "OK");
        EXPECT_EQ(task.completedOutcome.code(), "OK");
        EXPECT_EQ(task.failures(), 0u);
        EXPECT_TRUE(task.isReversible());
        EXPECT_EQ(context.irreversibleCount(), 0u);

        ASSERT_EQ(recorder.received.size(), 1u);
        EXPECT_EQ(recorder.received[0].type(), EventType::ItemCompleted);
        EXPECT_EQ(recorder.received[0].outcome().code(), "OK");
    }

    TEST_F(TemplateMethodTest, ValidationFailureStopsBeforeWork)
    {
        task.rejectOutcome = true;
        task.irreversible = true;
        start(task);

        EXPECT_THROW(
            task.complete(actor, Outcome::accepted()),
            ValidationError);

        EXPECT_EQ(task.calls, (std::vector<std::string>{"validate"}));
        EXPECT_EQ(task.status(), ItemStatus::InProgress);
        EXPECT_EQ(task.failures(), 0u);
        EXPECT_TRUE(task.completedBy().empty());
        EXPECT_TRUE(task.isReversible());
        EXPECT_EQ(context.irreversibleCount(), 0u);
        EXPECT_TRUE(recorder.received.empty());
    }

    TEST_F(TemplateMethodTest, RefinedOutcomeReachesHookResultAndEvent)
    {
        task.refineOutcome = true;
        start(task);
        const Outcome original = Outcome::accepted("INITIAL");

        ASSERT_NO_THROW(task.complete(actor, original));

        EXPECT_EQ(task.status(), ItemStatus::Completed);
        EXPECT_EQ(original.kind(), OutcomeKind::Accepted);
        EXPECT_EQ(original.code(), "INITIAL");

        EXPECT_EQ(task.completedOutcome.kind(), OutcomeKind::Rejected);
        EXPECT_EQ(task.completedOutcome.code(), "ISSUES_FOUND");
        EXPECT_EQ(task.lastOutcome().kind(), OutcomeKind::Rejected);
        EXPECT_EQ(task.lastOutcome().code(), "ISSUES_FOUND");
        EXPECT_EQ(task.lastOutcome().note(), "needs review");

        ASSERT_EQ(recorder.received.size(), 1u);
        EXPECT_EQ(recorder.received[0].type(), EventType::ItemCompleted);
        EXPECT_EQ(
            recorder.received[0].outcome().kind(),
            OutcomeKind::Rejected);
        EXPECT_EQ(
            recorder.received[0].outcome().code(),
            "ISSUES_FOUND");
    }

    TEST_F(TemplateMethodTest, MarksIrreversibleBeforeCompletionHook)
    {
        task.irreversible = true;
        start(task);

        ASSERT_NO_THROW(task.complete(actor, Outcome::accepted()));

        EXPECT_EQ(task.status(), ItemStatus::Completed);
        EXPECT_EQ(context.irreversibleCount(), 1u);
        EXPECT_EQ(task.irreversibleAtCompletion, 1u);
        EXPECT_FALSE(task.isReversible());
    }

    TEST_F(TemplateMethodTest, RetryableFailuresAreCountedAndReported)
    {
        checkExternalFailure(true);
    }

    TEST_F(TemplateMethodTest, PermanentFailuresAreCountedAndReported)
    {
        checkExternalFailure(false);
    }

    TEST_F(TemplateMethodTest, OtherExceptionsPropagateWithoutTechnicalFailure)
    {
        task.failure = ProbeTask::Failure::Unexpected;
        task.irreversible = true;
        start(task);

        EXPECT_THROW(
            task.complete(actor, Outcome::accepted()),
            std::logic_error);

        EXPECT_EQ(task.calls, (std::vector<std::string>{
            "validate", "perform"
        }));
        EXPECT_EQ(task.status(), ItemStatus::InProgress);
        EXPECT_EQ(task.failures(), 0u);
        EXPECT_EQ(context.irreversibleCount(), 0u);
        EXPECT_TRUE(task.isReversible());
        EXPECT_TRUE(recorder.received.empty());
    }

    TEST_F(TemplateMethodTest, DefaultHooksAllowManualTaskToComplete)
    {
        ManualTask manual(WorkItemSpec("manual-1", "Manual task"));
        start(manual);

        ASSERT_NO_THROW(
            manual.complete(actor, Outcome::accepted("DONE")));

        EXPECT_EQ(manual.status(), ItemStatus::Completed);
        EXPECT_EQ(manual.lastOutcome().code(), "DONE");
        EXPECT_TRUE(manual.isReversible());
        EXPECT_EQ(context.irreversibleCount(), 0u);

        ASSERT_EQ(recorder.received.size(), 1u);
        EXPECT_EQ(recorder.received[0].type(), EventType::ItemCompleted);
    }
} // namespace
} // namespace tividy