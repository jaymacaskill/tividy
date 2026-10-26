#include <gtest/gtest.h>

/*
    currently this only tests Observers.h from engine/runtime. At the time of testing only Observers was implemented.
*/

#include <memory>
#include <string>

#include "engine/connectors/ExternalConnector.h"
#include "engine/events/WorkflowEvent.h"
#include "engine/runtime/Observers.h"

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
} // namespace tividy