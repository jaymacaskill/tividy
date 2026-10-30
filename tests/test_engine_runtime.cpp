#include <gtest/gtest.h>

/*
    Folder tests for engine/runtime: Observers.h, WorkflowInstance.h and FlowController.h.
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
} // namespace tividy