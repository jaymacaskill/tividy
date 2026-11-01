#include <gtest/gtest.h>

/*
    Folder tests for engine/runtime: Observers.h, WorkflowInstance.h, FlowController.h and DeadlineMonitor.h.
    These are "is this folder broken?" checks (happy paths plus illegal uses), not pattern tests.
*/

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Clock.h"
#include "engine/core/Errors.h"
#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"
#include "engine/core/WorkflowData.h"
#include "engine/definition/WorkflowDefinition.h"
#include "engine/events/WorkflowEvent.h"
#include "engine/items/StageItem.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/policies/AssignmentStrategy.h"
#include "engine/policies/EscalationHandler.h"
#include "engine/policies/RoutingStrategy.h"
#include "engine/runtime/FlowController.h"
#include "engine/runtime/Observers.h"
#include "engine/runtime/WorkflowInstance.h"

namespace tividy
{
    namespace
    {
        // Records every invoke() call so tests can assert on operation, params and order.
        class RecordingConnector : public ExternalConnector
        {
            public:
                std::string name() const override { return "notifier"; }
                ConnectorResult invoke(const ConnectorRequest& req) override
                {
                    calls.push_back(req);
                    return ConnectorResult::ok("");
                }
                std::vector<ConnectorRequest> calls;
        };

        bool hasEntry(const AuditLog& log, EventType type)
        {
            for (const AuditEntry& entry : log.entries())
            {
                if (entry.type == type)
                {
                    return true;
                }
            }
            return false;
        }

        std::size_t countEntries(const AuditLog& log, EventType type)
        {
            std::size_t count = 0;
            for (const AuditEntry& entry : log.entries())
            {
                if (entry.type == type)
                {
                    ++count;
                }
            }
            return count;
        }

        bool hasNote(const AuditLog& log, const std::string& message)
        {
            for (const AuditEntry& entry : log.entries())
            {
                if (entry.type == EventType::EngineNote && entry.message == message)
                {
                    return true;
                }
            }
            return false;
        }

        // Test double: throws ExternalFailure on its first failures_before_success runs, then succeeds.
        class FlakyTask : public CloneableTask<FlakyTask>
        {
            public:
                FlakyTask(const WorkItemSpec& spec, unsigned failures_before_success, bool retryable)
                    : CloneableTask<FlakyTask>(spec), remaining(failures_before_success), retryable_ref(retryable), runs_ref(0) { }
                unsigned runs() const { return runs_ref; }
            protected:
                void performWork(const Participant&, Outcome&) override
                {
                    ++runs_ref;
                    if (remaining > 0)
                    {
                        --remaining;
                        throw ExternalFailure("vendor timeout", retryable_ref);
                    }
                }
            private:
                unsigned remaining;
                bool retryable_ref;
                unsigned runs_ref;
        };

        // Test double: an engine error that is not an ExternalFailure (bad data inside the work).
        class BrokenTask : public CloneableTask<BrokenTask>
        {
            public:
                explicit BrokenTask(const WorkItemSpec& spec) : CloneableTask<BrokenTask>(spec) { }
            protected:
                void performWork(const Participant&, Outcome&) override { throw ValidationError("bad data"); }
        };

        // Shared collaborators for one instance: a clerk "u1", a "system" participant for automatic items,
        // and RoleBasedAssignment (the clerk role always resolves to u1).
        class RuntimeTest : public ::testing::Test
        {
            protected:
                RuntimeTest() : clock(TimePoint()), services{clock, directory, connectors}
                {
                    directory.add(Participant("u1", "Una", {"clerk"}));
                    directory.add(Participant("system", "System", {"system"}));
                    policies.assignment = std::make_shared<RoleBasedAssignment>();
                }

                // root
                //  |- a  (clerk)
                //  |- b  (clerk, depends on a)
                static std::unique_ptr<StageItem> twoStepFlow()
                {
                    std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
                    root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("a", "A").requireRole("clerk"))));
                    root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("b", "B").requireRole("clerk").dependsOn("a"))));
                    return root;
                }

                std::unique_ptr<WorkflowInstance> makeInstance(std::unique_ptr<StageItem> root)
                {
                    return std::unique_ptr<WorkflowInstance>(new WorkflowInstance("inst-1", "flow", 1, WorkflowData(),
                        std::unique_ptr<WorkItem>(root.release()), policies, services));
                }

                static WorkItem& item(WorkflowInstance& instance, const Id& id)
                {
                    WorkItem* found = instance.findItem(id);
                    if (found == nullptr)
                    {
                        throw std::runtime_error("test setup: no item " + id);
                    }
                    return *found;
                }

                const Participant& clerk() const { return directory.require("u1"); }

                static TaskItem& task(WorkflowInstance& instance, const Id& id)
                {
                    return dynamic_cast<TaskItem&>(item(instance, id));
                }

                // root
                //  |- decide  (clerk)
                //  |- approve (clerk, depends on decide)
                //  |- reject  (clerk, depends on decide)
                //  |- close   (clerk, depends on approve and reject)
                static std::unique_ptr<StageItem> branchFlow()
                {
                    std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
                    root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("decide", "Decide").requireRole("clerk"))));
                    root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("approve", "Approve").requireRole("clerk").dependsOn("decide"))));
                    root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("reject", "Reject").requireRole("clerk").dependsOn("decide"))));
                    root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("close", "Close").requireRole("clerk")
                        .dependsOn("approve").dependsOn("reject"))));
                    return root;
                }

                // root
                //  |- choose (clerk)
                //  |- ship   (clerk, depends on choose)
                //  |- visit  (clerk, depends on choose)
                static std::unique_ptr<StageItem> installFlow()
                {
                    std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
                    root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("choose", "Choose").requireRole("clerk"))));
                    root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("ship", "Ship").requireRole("clerk").dependsOn("choose"))));
                    root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("visit", "Visit").requireRole("clerk").dependsOn("choose"))));
                    return root;
                }

                // install_type: SELF_INSTALL -> ship (skip visit); TECHNICIAN and default -> visit (skip ship).
                static std::shared_ptr<DataValueRouting> installRouting()
                {
                    std::shared_ptr<DataValueRouting> routing = std::make_shared<DataValueRouting>("install_type");
                    routing->addRule("SELF_INSTALL", {}, {"visit"});
                    routing->addRule("TECHNICIAN", {}, {"ship"});
                    routing->setDefault({}, {"ship"});
                    return routing;
                }

                // A single task under the root, so one item can be driven without dependencies.
                static std::unique_ptr<StageItem> single(std::unique_ptr<WorkItem> child)
                {
                    std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
                    root->add(std::move(child));
                    return root;
                }

                void doTask(WorkflowInstance& instance, const Id& id, const Outcome& outcome = Outcome::accepted())
                {
                    item(instance, id).start(clerk());
                    item(instance, id).complete(clerk(), outcome);
                }

                ManualClock clock;
                ParticipantDirectory directory;
                ConnectorRegistry connectors;
                Services services;
                DefinitionPolicies policies;
        };
    }

    TEST(AuditLogTest, AppendAndRetrieve)
    {
        AuditLog log;
        AuditEntry e;
        e.when     = TimePoint();
        e.type     = EventType::ItemCompleted;
        e.item_id  = "task-1";
        e.actor_id = "u1";
        e.message  = "done";
        log.append(e);

        ASSERT_EQ(log.entries().size(), 1u);
        EXPECT_EQ(log.entries()[0].type, EventType::ItemCompleted);
        EXPECT_EQ(log.entries()[0].item_id, "task-1");
    }

    TEST(AuditLogTest, FormatContainsTimestampAndFields)
    {
        AuditEntry e;
        e.when = TimePoint();   // epoch
        e.type = EventType::ItemCompleted;
        e.item_id = "task-1";
        e.actor_id = "u1";
        e.message = "done";

        std::string line = AuditLog::format(e);
        EXPECT_NE(line.find("1970-01-01 00:00:00"), std::string::npos);
        EXPECT_NE(line.find("ItemCompleted"), std::string::npos);
        EXPECT_NE(line.find("u1"), std::string::npos);
        EXPECT_NE(line.find("task-1"), std::string::npos);
        EXPECT_NE(line.find("done"), std::string::npos);
    }

    // --- AuditObserver ---

    TEST(AuditObserverTest, AppendsEntryOnEvent)
    {
        AuditLog log;
        AuditObserver obs(log);
        WorkflowEvent ev(EventType::ItemCompleted, "inst-1", "task-1", TimePoint());
        obs.onEvent(ev);

        ASSERT_EQ(log.entries().size(), 1u);
        EXPECT_EQ(log.entries()[0].item_id,  "task-1");
        EXPECT_EQ(log.entries()[0].actor_id, "system"); // default
        EXPECT_EQ(log.entries()[0].message,  "ItemCompleted"); // default to toString(type)
    }

    TEST(AuditObserverTest, PreservesActorAndMessageWhenProvided)
    {
        AuditLog log;
        AuditObserver obs(log);
        WorkflowEvent ev(EventType::ItemCompleted, "inst-1", "task-1", TimePoint());
        ev.byActor("u1").withMessage("completed");
        obs.onEvent(ev);

        ASSERT_EQ(log.entries().size(), 1u);
        EXPECT_EQ(log.entries()[0].actor_id, "u1");
        EXPECT_EQ(log.entries()[0].message,  "completed");
    }

    TEST(NotificationObserverTest, SendsOnItemAssigned)
    {
        ConnectorRegistry reg;
        RecordingConnector* rec = new RecordingConnector();
        reg.registerConnector(std::unique_ptr<ExternalConnector>(rec));
        NotificationObserver obs(reg);

        WorkflowEvent ev(EventType::ItemAssigned, "inst-1", "task-1", TimePoint());
        ev.byActor("u1").withMessage("assigned");
        obs.onEvent(ev);

        ASSERT_EQ(rec->calls.size(), 1u);
        EXPECT_EQ(rec->calls[0].operation,                "notify");
        EXPECT_EQ(rec->calls[0].params.at("recipient"),   "u1");
        EXPECT_EQ(rec->calls[0].params.at("subject"),     "ItemAssigned");
        EXPECT_EQ(rec->calls[0].params.at("body"),        "assigned");
    }

    TEST(NotificationObserverTest, IgnoresUnrelatedEvents)
    {
        ConnectorRegistry reg;
        RecordingConnector* rec = new RecordingConnector();
        reg.registerConnector(std::unique_ptr<ExternalConnector>(rec));
        NotificationObserver obs(reg);

        obs.onEvent(WorkflowEvent(EventType::ItemStarted, "inst-1", "task-1", TimePoint()));
        obs.onEvent(WorkflowEvent(EventType::ItemCompleted, "inst-1", "task-1", TimePoint()));

        EXPECT_TRUE(rec->calls.empty());
    }

    TEST(NotificationObserverTest, DoesNothingWhenConnectorMissing)
    {
        ConnectorRegistry reg;   // "notifier" not registered
        NotificationObserver obs(reg);

        // Must not throw — notifications are best-effort.
        EXPECT_NO_THROW(obs.onEvent(WorkflowEvent(EventType::InstanceCompleted, "i1", "", TimePoint())));
    }

    TEST(NotificationObserverTest, RecipientsForEscalationAndOwner)
    {
        ConnectorRegistry reg;
        RecordingConnector* rec = new RecordingConnector();
        reg.registerConnector(std::unique_ptr<ExternalConnector>(rec));
        NotificationObserver obs(reg);

        obs.onEvent(WorkflowEvent(EventType::ItemEscalated,     "i1", "t1", TimePoint()));
        obs.onEvent(WorkflowEvent(EventType::ItemFailed,        "i1", "t1", TimePoint()));
        obs.onEvent(WorkflowEvent(EventType::InstanceCompleted, "i1", "",   TimePoint()));
        obs.onEvent(WorkflowEvent(EventType::InstanceFailed,    "i1", "",   TimePoint()));

        ASSERT_EQ(rec->calls.size(), 4u);
        EXPECT_EQ(rec->calls[0].params.at("recipient"), "escalation");
        EXPECT_EQ(rec->calls[1].params.at("recipient"), "escalation");
        EXPECT_EQ(rec->calls[2].params.at("recipient"), "owner");
        EXPECT_EQ(rec->calls[3].params.at("recipient"), "owner");
    }

    // --- WorkflowInstance ---

    TEST_F(RuntimeTest, NewInstanceIsCreatedAndHasNoHistory)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());

        EXPECT_EQ(instance->id(), "inst-1");
        EXPECT_EQ(instance->definitionName(), "flow");
        EXPECT_EQ(instance->definitionVersion(), 1u);
        EXPECT_EQ(instance->status(), InstanceStatus::Created);
        EXPECT_TRUE(instance->audit().entries().empty());
    }

    TEST_F(RuntimeTest, StartRunsInstanceAndPublishesInstanceStarted)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();

        EXPECT_EQ(instance->status(), InstanceStatus::Running);
        ASSERT_FALSE(instance->audit().entries().empty());
        EXPECT_EQ(instance->audit().entries()[0].type, EventType::InstanceStarted);
        EXPECT_EQ(instance->audit().entries()[0].message, "started from flow v1");
    }

    TEST_F(RuntimeTest, StartTwiceThrowsInvalidState)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();

        EXPECT_THROW(instance->start(), InvalidStateError);
        EXPECT_EQ(instance->status(), InstanceStatus::Running);
    }

    TEST_F(RuntimeTest, CancelBeforeStartThrowsInvalidState)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());

        EXPECT_THROW(instance->cancel("too early"), InvalidStateError);
        EXPECT_EQ(instance->status(), InstanceStatus::Created);
    }

    TEST_F(RuntimeTest, CancelStopsInstanceAndCancelsItems)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();
        instance->cancel("stopped");

        EXPECT_EQ(instance->status(), InstanceStatus::Cancelled);
        EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Cancelled);
        EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Cancelled);
        EXPECT_TRUE(hasEntry(instance->audit(), EventType::InstanceCancelled));
        EXPECT_FALSE(hasEntry(instance->audit(), EventType::InstanceFailed));
    }

    TEST_F(RuntimeTest, FindItemReturnsItemOrNull)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());

        ASSERT_NE(instance->findItem("b"), nullptr);
        EXPECT_EQ(instance->findItem("b")->spec().id(), "b");
        EXPECT_EQ(instance->findItem("root"), &instance->root());
        EXPECT_EQ(instance->findItem("ghost"), nullptr);
    }

    TEST_F(RuntimeTest, ItemsForListsOnlyThatParticipantsActiveWork)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();

        std::vector<WorkItem*> mine = instance->itemsFor("u1");
        ASSERT_EQ(mine.size(), 1u);
        EXPECT_EQ(mine[0]->spec().id(), "a");
        EXPECT_TRUE(instance->itemsFor("nobody").empty());
    }

    // --- FlowController ---

    TEST_F(RuntimeTest, StartReleasesAndAssignsOnlyItemsWithSettledDependencies)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();

        EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*instance, "a").assignee(), "u1");
        EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Created);
    }

    TEST_F(RuntimeTest, CompletingItemReleasesItsDependant)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();

        item(*instance, "a").start(clerk());
        item(*instance, "a").complete(clerk(), Outcome::accepted());

        EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Completed);
        EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*instance, "b").assignee(), "u1");
        EXPECT_EQ(instance->status(), InstanceStatus::Running);
    }

    TEST_F(RuntimeTest, CompletingLastItemCompletesInstance)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();

        item(*instance, "a").start(clerk());
        item(*instance, "a").complete(clerk(), Outcome::accepted());
        item(*instance, "b").start(clerk());
        item(*instance, "b").complete(clerk(), Outcome::accepted());

        EXPECT_EQ(instance->status(), InstanceStatus::Completed);
        EXPECT_TRUE(hasEntry(instance->audit(), EventType::InstanceCompleted));
    }

    TEST_F(RuntimeTest, AutomaticItemIsStartedAndCompletedByTheController)
    {
        std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("gate", "Gate").requireRole("system").automatic(true))));
        std::unique_ptr<WorkflowInstance> instance = makeInstance(std::move(root));
        instance->start();

        EXPECT_EQ(item(*instance, "gate").status(), ItemStatus::Completed);
        EXPECT_EQ(instance->status(), InstanceStatus::Completed);
    }

    TEST_F(RuntimeTest, CancelledItemFailsRunningInstance)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();

        item(*instance, "a").cancel("dropped");

        EXPECT_EQ(instance->status(), InstanceStatus::Failed);
        EXPECT_TRUE(hasEntry(instance->audit(), EventType::InstanceFailed));
    }

    TEST_F(RuntimeTest, CancelSignalCancelsInstance)
    {
        policies.cancel_signals.insert("abort");
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();

        instance->events().notify(WorkflowEvent(EventType::ExternalSignal, "inst-1", "", clock.now()).withMessage("abort"));

        EXPECT_EQ(instance->status(), InstanceStatus::Cancelled);
    }

    TEST_F(RuntimeTest, UnknownRoutingIdIsNotedAndNeverThrows)
    {
        std::shared_ptr<OutcomeTableRouting> routing = std::make_shared<OutcomeTableRouting>();
        routing->setDefault({"ghost"}, {});
        policies.routing["a"] = routing;
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();

        item(*instance, "a").start(clerk());
        EXPECT_NO_THROW(item(*instance, "a").complete(clerk(), Outcome::accepted()));

        EXPECT_TRUE(hasEntry(instance->audit(), EventType::EngineNote));
        EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Assigned);
        EXPECT_EQ(instance->status(), InstanceStatus::Running);
    }

    // --- Failure handling (TaskItem::executeWork + FlowController ItemFailed / ItemEscalated) ---

    TEST_F(RuntimeTest, RetryableFailureOnAutomaticItemIsRetriedUntilItSucceeds)
    {
        FlakyTask* flaky = new FlakyTask(WorkItemSpec("sync", "Sync").requireRole("system").automatic(true), 1, true);
        std::unique_ptr<WorkflowInstance> instance = makeInstance(single(std::unique_ptr<WorkItem>(flaky)));
        instance->start();

        EXPECT_EQ(flaky->runs(), 2u);
        EXPECT_EQ(flaky->failures(), 1u);
        EXPECT_EQ(item(*instance, "sync").status(), ItemStatus::Completed);
        EXPECT_EQ(countEntries(instance->audit(), EventType::ItemFailed), 1u);
        EXPECT_EQ(instance->status(), InstanceStatus::Completed);
    }

    TEST_F(RuntimeTest, RetryableFailuresBeyondMaxRetriesEscalate)
    {
        FlakyTask* flaky = new FlakyTask(WorkItemSpec("sync", "Sync").requireRole("system").automatic(true).maxRetries(1), 100, true);
        std::unique_ptr<WorkflowInstance> instance = makeInstance(single(std::unique_ptr<WorkItem>(flaky)));
        instance->start();

        // failure 1 is within maxRetries and is retried; failure 2 is not, so the item escalates
        EXPECT_EQ(flaky->runs(), 2u);
        EXPECT_EQ(item(*instance, "sync").status(), ItemStatus::Escalated);
        EXPECT_EQ(countEntries(instance->audit(), EventType::ItemFailed), 2u);
        EXPECT_TRUE(hasEntry(instance->audit(), EventType::ItemEscalated));
        EXPECT_EQ(instance->status(), InstanceStatus::Running);
    }

    TEST_F(RuntimeTest, PermanentFailureEscalatesWithoutRetry)
    {
        FlakyTask* flaky = new FlakyTask(WorkItemSpec("sync", "Sync").requireRole("system").automatic(true), 100, false);
        std::unique_ptr<WorkflowInstance> instance = makeInstance(single(std::unique_ptr<WorkItem>(flaky)));
        instance->start();

        EXPECT_EQ(flaky->runs(), 1u);
        EXPECT_EQ(item(*instance, "sync").status(), ItemStatus::Escalated);
        EXPECT_EQ(countEntries(instance->audit(), EventType::ItemFailed), 1u);
    }

    TEST_F(RuntimeTest, NonExternalErrorInAutomaticItemIsTreatedAsPermanentFailure)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(single(std::unique_ptr<WorkItem>(
            new BrokenTask(WorkItemSpec("sync", "Sync").requireRole("system").automatic(true)))));
        EXPECT_NO_THROW(instance->start());

        EXPECT_TRUE(hasEntry(instance->audit(), EventType::ItemFailed));
        EXPECT_EQ(item(*instance, "sync").status(), ItemStatus::Escalated);
    }

    TEST_F(RuntimeTest, RetryableFailureOnHumanItemWaitsForTheUserToRetry)
    {
        FlakyTask* flaky = new FlakyTask(WorkItemSpec("call", "Call").requireRole("clerk"), 1, true);
        std::unique_ptr<WorkflowInstance> instance = makeInstance(single(std::unique_ptr<WorkItem>(flaky)));
        instance->start();

        item(*instance, "call").start(clerk());
        item(*instance, "call").complete(clerk(), Outcome::accepted());

        EXPECT_EQ(item(*instance, "call").status(), ItemStatus::InProgress);
        EXPECT_EQ(flaky->failures(), 1u);
        EXPECT_FALSE(hasEntry(instance->audit(), EventType::ItemEscalated));

        item(*instance, "call").complete(clerk(), Outcome::accepted());
        EXPECT_EQ(item(*instance, "call").status(), ItemStatus::Completed);
        EXPECT_EQ(instance->status(), InstanceStatus::Completed);
    }

    TEST_F(RuntimeTest, ResolvedEscalationReassignsToTheApproverAndStaysEscalated)
    {
        directory.add(Participant("s1", "Sipho", {"supervisor"}));
        policies.escalation = std::make_shared<RoleEscalationHandler>("supervisor", 1);
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();

        item(*instance, "a").escalate("stuck");

        EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Escalated);
        EXPECT_EQ(item(*instance, "a").assignee(), "s1");
    }

    TEST_F(RuntimeTest, UnresolvedEscalationIsNotedAndItemStaysEscalated)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());   // no escalation chain
        instance->start();

        item(*instance, "a").escalate("stuck");

        EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Escalated);
        EXPECT_EQ(item(*instance, "a").assignee(), "u1");
        EXPECT_TRUE(hasNote(instance->audit(), "escalation unresolved for a"));
    }

    // --- Deadlines (DeadlineMonitor::sweep + FlowController DeadlineMissed) ---
    // The fixture clock starts at the epoch, which TaskItem::dueAt() reads as "never assigned", so each
    // deadline test moves the clock forward before the items are assigned.

    TEST_F(RuntimeTest, SweepBeforeDueTimeReportsNothing)
    {
        clock.advance(60);
        std::unique_ptr<WorkflowInstance> instance = makeInstance(single(std::unique_ptr<WorkItem>(
            new ManualTask(WorkItemSpec("a", "A").requireRole("clerk").dueAfter(100)))));
        instance->start();

        clock.advance(100);   // exactly at the due time: not yet late
        EXPECT_EQ(instance->sweepDeadlines(), 0u);
        EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Assigned);
        EXPECT_FALSE(hasEntry(instance->audit(), EventType::DeadlineMissed));
    }

    TEST_F(RuntimeTest, MissedDeadlineWithoutTimeoutCodeEscalates)
    {
        clock.advance(60);
        std::unique_ptr<WorkflowInstance> instance = makeInstance(single(std::unique_ptr<WorkItem>(
            new ManualTask(WorkItemSpec("a", "A").requireRole("clerk").dueAfter(100)))));
        instance->start();

        clock.advance(101);
        EXPECT_EQ(instance->sweepDeadlines(), 1u);

        EXPECT_TRUE(hasEntry(instance->audit(), EventType::DeadlineMissed));
        EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Escalated);
    }

    TEST_F(RuntimeTest, MissedDeadlineWithTimeoutCodeCompletesWithDefaultJudgment)
    {
        clock.advance(60);
        std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("a", "A").requireRole("clerk").dueAfter(100).onTimeout("TIMED_OUT"))));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("b", "B").requireRole("clerk").dependsOn("a"))));
        std::unique_ptr<WorkflowInstance> instance = makeInstance(std::move(root));
        instance->start();

        clock.advance(101);
        instance->sweepDeadlines();

        EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Completed);
        EXPECT_EQ(task(*instance, "a").lastOutcome().kind(), OutcomeKind::Rejected);
        EXPECT_EQ(task(*instance, "a").lastOutcome().code(), "TIMED_OUT");
        EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Assigned);
    }

    TEST_F(RuntimeTest, EachLapseIsReportedOnlyOnce)
    {
        clock.advance(60);
        std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("late", "Late").requireRole("clerk").dueAfter(100))));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("other", "Other").requireRole("clerk"))));
        std::unique_ptr<WorkflowInstance> instance = makeInstance(std::move(root));
        instance->start();

        // Cancelling "other" fails the instance, so FlowController stops reacting and "late" stays Assigned (still overdue).
        item(*instance, "other").cancel("dropped");
        ASSERT_EQ(instance->status(), InstanceStatus::Failed);

        clock.advance(101);
        EXPECT_EQ(instance->sweepDeadlines(), 1u);
        clock.advance(500);
        EXPECT_EQ(instance->sweepDeadlines(), 0u);

        EXPECT_EQ(item(*instance, "late").status(), ItemStatus::Assigned);
        EXPECT_EQ(countEntries(instance->audit(), EventType::DeadlineMissed), 1u);
    }

    TEST_F(RuntimeTest, ItemsWithoutDeadlineOrAlreadyFinishedAreNeverReported)
    {
        clock.advance(60);
        std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("no_deadline", "No deadline").requireRole("clerk"))));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("done", "Done").requireRole("clerk").dueAfter(100))));
        std::unique_ptr<WorkflowInstance> instance = makeInstance(std::move(root));
        instance->start();
        doTask(*instance, "done");

        clock.advance(10000);
        EXPECT_EQ(instance->sweepDeadlines(), 0u);
        EXPECT_EQ(item(*instance, "no_deadline").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*instance, "done").status(), ItemStatus::Completed);
    }

    // --- Skip routing (OutcomeTableRouting skip lists + FlowController::applyDecision) ---

    TEST_F(RuntimeTest, SkipListSkipsTheBranchNotTaken)
    {
        std::shared_ptr<OutcomeTableRouting> routing = std::make_shared<OutcomeTableRouting>();
        routing->addRule("APPROVED", {}, {"reject"});
        routing->addRule("REJECTED", {}, {"approve"});
        policies.routing["decide"] = routing;
        std::unique_ptr<WorkflowInstance> instance = makeInstance(branchFlow());
        instance->start();

        doTask(*instance, "decide", Outcome::accepted("APPROVED"));

        EXPECT_EQ(item(*instance, "reject").status(), ItemStatus::Skipped);
        EXPECT_EQ(item(*instance, "approve").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*instance, "close").status(), ItemStatus::Created);
        EXPECT_TRUE(hasEntry(instance->audit(), EventType::ItemSkipped));
    }

    TEST_F(RuntimeTest, SkippedItemCountsAsSettledForDependants)
    {
        std::shared_ptr<OutcomeTableRouting> routing = std::make_shared<OutcomeTableRouting>();
        routing->addRule("REJECTED", {}, {"approve"});
        policies.routing["decide"] = routing;
        std::unique_ptr<WorkflowInstance> instance = makeInstance(branchFlow());
        instance->start();

        doTask(*instance, "decide", Outcome::rejected("REJECTED"));
        doTask(*instance, "reject");

        // close depends on approve (Skipped) and reject (Completed)
        EXPECT_EQ(item(*instance, "close").status(), ItemStatus::Assigned);

        doTask(*instance, "close");
        EXPECT_EQ(instance->status(), InstanceStatus::Completed);
    }

    TEST_F(RuntimeTest, SkippingEveryRemainingItemCompletesTheInstance)
    {
        std::shared_ptr<OutcomeTableRouting> routing = std::make_shared<OutcomeTableRouting>();
        routing->setDefault({}, {"approve", "reject", "close"});
        policies.routing["decide"] = routing;
        std::unique_ptr<WorkflowInstance> instance = makeInstance(branchFlow());
        instance->start();

        doTask(*instance, "decide");

        EXPECT_EQ(item(*instance, "approve").status(), ItemStatus::Skipped);
        EXPECT_EQ(item(*instance, "reject").status(), ItemStatus::Skipped);
        EXPECT_EQ(item(*instance, "close").status(), ItemStatus::Skipped);
        EXPECT_EQ(instance->status(), InstanceStatus::Completed);
    }

    TEST_F(RuntimeTest, SkipOfAnItemThatAlreadyStartedIsNotedAndIgnored)
    {
        std::shared_ptr<OutcomeTableRouting> routing = std::make_shared<OutcomeTableRouting>();
        routing->setDefault({}, {"y"});
        policies.routing["x"] = routing;
        std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("x", "X").requireRole("clerk"))));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("y", "Y").requireRole("clerk"))));
        std::unique_ptr<WorkflowInstance> instance = makeInstance(std::move(root));
        instance->start();

        // y is already Assigned (no dependencies), so it can no longer be skipped
        EXPECT_NO_THROW(doTask(*instance, "x"));

        EXPECT_EQ(item(*instance, "y").status(), ItemStatus::Assigned);
        EXPECT_TRUE(hasNote(instance->audit(), "routing ignored y"));
        EXPECT_EQ(instance->status(), InstanceStatus::Running);
    }

    TEST_F(RuntimeTest, DirectSkipReleasesDependantsAndCanFinishTheInstance)
    {
        std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("a", "A").requireRole("clerk"))));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("b", "B").requireRole("clerk").dependsOn("a"))));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("c", "C").requireRole("nobody").dependsOn("b"))));
        std::unique_ptr<WorkflowInstance> instance = makeInstance(std::move(root));
        instance->start();
        doTask(*instance, "a");
        doTask(*instance, "b");
        ASSERT_EQ(item(*instance, "c").status(), ItemStatus::Available);   // no participant has role "nobody"

        item(*instance, "c").skip("not needed");

        EXPECT_EQ(item(*instance, "c").status(), ItemStatus::Skipped);
        EXPECT_EQ(instance->status(), InstanceStatus::Completed);
    }

    TEST_F(RuntimeTest, DirectSkipOfAPredecessorReleasesItsDependant)
    {
        std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("a", "A").requireRole("clerk"))));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("b", "B").requireRole("clerk").dependsOn("a"))));
        root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("c", "C").requireRole("clerk").dependsOn("b"))));
        std::unique_ptr<WorkflowInstance> instance = makeInstance(std::move(root));
        instance->start();

        item(*instance, "b").skip("not needed");   // b is still Created

        EXPECT_EQ(item(*instance, "c").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Assigned);
        EXPECT_EQ(instance->status(), InstanceStatus::Running);
    }

    // --- Data value routing (DataValueRouting) ---

    TEST_F(RuntimeTest, DataValueRoutingFollowsTheMatchingRule)
    {
        policies.routing["choose"] = installRouting();
        WorkflowData data;
        data.set("install_type", "SELF_INSTALL");
        std::unique_ptr<WorkflowInstance> instance(new WorkflowInstance("inst-1", "flow", 1, data,
            std::unique_ptr<WorkItem>(installFlow().release()), policies, services));
        instance->start();

        doTask(*instance, "choose");

        EXPECT_EQ(item(*instance, "ship").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*instance, "visit").status(), ItemStatus::Skipped);
    }

    TEST_F(RuntimeTest, DataValueRoutingReadsTheDataWhenTheItemCompletes)
    {
        policies.routing["choose"] = installRouting();
        std::unique_ptr<WorkflowInstance> instance = makeInstance(installFlow());
        instance->start();

        instance->data().set("install_type", "TECHNICIAN");   // set after start, before choose completes
        doTask(*instance, "choose");

        EXPECT_EQ(item(*instance, "visit").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*instance, "ship").status(), ItemStatus::Skipped);
    }

    TEST_F(RuntimeTest, DataValueRoutingIgnoresTheOutcome)
    {
        policies.routing["choose"] = installRouting();
        std::unique_ptr<WorkflowInstance> instance = makeInstance(installFlow());
        instance->start();

        instance->data().set("install_type", "SELF_INSTALL");
        doTask(*instance, "choose", Outcome::rejected("TECHNICIAN"));

        EXPECT_EQ(item(*instance, "ship").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*instance, "visit").status(), ItemStatus::Skipped);
    }

    TEST_F(RuntimeTest, DataValueRoutingUsesTheDefaultForMissingOrUnknownValues)
    {
        policies.routing["choose"] = installRouting();
        std::unique_ptr<WorkflowInstance> missing = makeInstance(installFlow());
        missing->start();
        doTask(*missing, "choose");   // install_type never set

        EXPECT_EQ(item(*missing, "visit").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*missing, "ship").status(), ItemStatus::Skipped);

        std::unique_ptr<WorkflowInstance> unknown = makeInstance(installFlow());
        unknown->start();
        unknown->data().set("install_type", "DRONE");
        doTask(*unknown, "choose");

        EXPECT_EQ(item(*unknown, "visit").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*unknown, "ship").status(), ItemStatus::Skipped);
    }

    TEST_F(RuntimeTest, DataValueRoutingWithoutMatchOrDefaultReleasesByDependencies)
    {
        std::shared_ptr<DataValueRouting> routing = std::make_shared<DataValueRouting>("install_type");
        routing->addRule("SELF_INSTALL", {}, {"visit"});
        policies.routing["choose"] = routing;
        std::unique_ptr<WorkflowInstance> instance = makeInstance(installFlow());
        instance->start();

        instance->data().set("install_type", "DRONE");
        doTask(*instance, "choose");

        EXPECT_EQ(item(*instance, "ship").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*instance, "visit").status(), ItemStatus::Assigned);
    }

    // --- Snapshot and restore (WorkflowInstance::captureSnapshot / restoreSnapshot) ---

    TEST_F(RuntimeTest, RestoreSnapshotBringsBackItemsDataAndStatus)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();
        instance->data().set("note", "before");
        std::unique_ptr<InstanceMemento> snapshot = instance->captureSnapshot();

        doTask(*instance, "a", Outcome::rejected("DOCS_INVALID"));
        instance->data().set("note", "after");
        instance->data().set("extra", "x");
        ASSERT_EQ(item(*instance, "b").status(), ItemStatus::Assigned);

        instance->restoreSnapshot(*snapshot);

        EXPECT_EQ(instance->status(), InstanceStatus::Running);
        EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*instance, "a").assignee(), "u1");
        EXPECT_TRUE(task(*instance, "a").completedBy().empty());
        EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Created);
        EXPECT_EQ(instance->data().get("note"), "before");
        EXPECT_FALSE(instance->data().has("extra"));
    }

    TEST_F(RuntimeTest, RestoreSnapshotUndoesAnInstanceCancel)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();
        std::unique_ptr<InstanceMemento> snapshot = instance->captureSnapshot();

        instance->cancel("oops");
        ASSERT_EQ(instance->status(), InstanceStatus::Cancelled);

        instance->restoreSnapshot(*snapshot);

        EXPECT_EQ(instance->status(), InstanceStatus::Running);
        EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Assigned);
        EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Created);
    }

    TEST_F(RuntimeTest, RestoreSnapshotRestoresTaskCounters)
    {
        FlakyTask* flaky = new FlakyTask(WorkItemSpec("call", "Call").requireRole("clerk"), 1, true);
        std::unique_ptr<WorkflowInstance> instance = makeInstance(single(std::unique_ptr<WorkItem>(flaky)));
        instance->start();
        item(*instance, "call").start(clerk());
        std::unique_ptr<InstanceMemento> snapshot = instance->captureSnapshot();

        item(*instance, "call").complete(clerk(), Outcome::accepted());   // fails once
        ASSERT_EQ(flaky->failures(), 1u);

        instance->restoreSnapshot(*snapshot);

        EXPECT_EQ(flaky->failures(), 0u);
        EXPECT_EQ(item(*instance, "call").status(), ItemStatus::InProgress);
    }

    TEST_F(RuntimeTest, RestoreSnapshotPublishesNothing)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();
        std::unique_ptr<InstanceMemento> snapshot = instance->captureSnapshot();
        doTask(*instance, "a");

        const std::size_t entries_before = instance->audit().entries().size();
        instance->restoreSnapshot(*snapshot);

        EXPECT_EQ(instance->audit().entries().size(), entries_before);
    }

    TEST_F(RuntimeTest, RestoredInstanceCanCarryOnToCompletion)
    {
        std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());
        instance->start();
        std::unique_ptr<InstanceMemento> snapshot = instance->captureSnapshot();
        doTask(*instance, "a");
        instance->restoreSnapshot(*snapshot);

        doTask(*instance, "a");
        EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Assigned);
        doTask(*instance, "b");

        EXPECT_EQ(instance->status(), InstanceStatus::Completed);
    }
} // namespace tividy