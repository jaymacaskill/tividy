#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/Clock.h"
#include "engine/events/Observer.h"
#include "engine/events/WorkflowEvent.h"
#include "engine/items/WorkContext.h"
#include "engine/items/WorkItem.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/items/WorkItemVisitor.h"
#include "engine/runtime/DeadlineMonitor.h"
#include "engine/runtime/WorkflowInstance.h"

/*
DeadlineMonitor only asks its instance for id(), root() and events(). WorkflowInstance has no implementation yet, so these tests replace
those three members at link time and walk a tree of fake items. The event channel is the real EventPublisher.
When WorkflowInstance.cpp exists, move these tests onto a real started instance.
*/

namespace tividy
{
    namespace
    {
        class FakeItem : public WorkItem
        {
            public:
                explicit FakeItem(const std::string& id) : s(id, "fake"), st(ItemStatus::Created), due(TimePoint()) { }
                const WorkItemSpec& spec() const { return s; }
                ItemStatus status() const { return st; }
                TimePoint dueAt() const { return due; }
                std::vector<WorkItem*> children() { return kids; }
                const Id& assignee() const { return none; }
                WorkItem* find(const Id&) { return 0; }
                void publish() { }
                void assign(const Id&) { }
                void start(const Participant&) { }
                void submit(const Participant&, const Outcome&) { }
                void complete(const Participant&, const Outcome&) { }
                void approve(const Participant&, const Outcome&) { }
                void reject(const Participant&, const std::string&) { }
                void escalate(const std::string&) { }
                void cancel(const std::string&) { }
                void skip(const std::string&) { }
                void expire(const std::string&, const std::string&) { }
                std::unique_ptr<WorkItem> clone() const { return std::unique_ptr<WorkItem>(); }
                void accept(WorkItemVisitor&) const { }
                std::unique_ptr<WorkItemIterator> createIterator() { return std::unique_ptr<WorkItemIterator>(); }
                void bind(WorkContext&) { }

                WorkItemSpec s;
                ItemStatus st;
                TimePoint due;
                std::vector<WorkItem*> kids;
                Id none;
        };

        class Recorder : public EventObserver
        {
            public:
                void onEvent(const WorkflowEvent& e) { seen.push_back(e); }
                std::vector<WorkflowEvent> seen;
        };

        FakeItem* the_root = 0;
        EventPublisher* the_events = 0;
        Id the_id = "inst-1";
        alignas(WorkflowInstance) unsigned char instance_storage[sizeof(WorkflowInstance)];
    }

    const Id& WorkflowInstance::id() const { return the_id; }
    WorkItem& WorkflowInstance::root() { return *the_root; }
    EventPublisher& WorkflowInstance::events() { return *the_events; }

    class DeadlineMonitorTest : public ::testing::Test
    {
        protected:
            DeadlineMonitorTest()
                : clock(TimePoint() + std::chrono::hours(1000)), root("root"), a("A"), b("B"), c("C"), group("G"), nested("N"),
                  monitor(*reinterpret_cast<WorkflowInstance*>(instance_storage), clock)
            {
                root.kids.push_back(&a);
                root.kids.push_back(&group);
                group.kids.push_back(&b);
                group.kids.push_back(&nested);
                nested.kids.push_back(&c);
                the_root = &root;
                the_events = &events;
                events.attach(&recorder);
            }
            ~DeadlineMonitorTest() { events.detach(&recorder); }

            TimePoint ago(long seconds) const { return clock.now() - std::chrono::seconds(seconds); }
            TimePoint ahead(long seconds) const { return clock.now() + std::chrono::seconds(seconds); }
            void lapse(FakeItem& item, ItemStatus st) { item.st = st; item.due = ago(5); }

            ManualClock clock;
            EventPublisher events;
            Recorder recorder;
            FakeItem root, a, b, c, group, nested;
            DeadlineMonitor monitor;
    };

    TEST_F(DeadlineMonitorTest, PublishesNothingWhenNoItemHasADeadline)
    {
        a.st = ItemStatus::InProgress;
        EXPECT_EQ(monitor.sweep(), 0u);
        EXPECT_TRUE(recorder.seen.empty());
    }

    TEST_F(DeadlineMonitorTest, AFutureDeadlineIsNotReported)
    {
        a.st = ItemStatus::InProgress;
        a.due = ahead(60);
        EXPECT_EQ(monitor.sweep(), 0u);
    }

    TEST_F(DeadlineMonitorTest, ADeadlineEqualToNowIsNotYetLapsed)
    {
        a.st = ItemStatus::InProgress;
        a.due = clock.now();
        EXPECT_EQ(monitor.sweep(), 0u);
    }

    TEST_F(DeadlineMonitorTest, ALapsedOpenItemIsReportedWithTheDocumentedEvent)
    {
        lapse(a, ItemStatus::Assigned);
        EXPECT_EQ(monitor.sweep(), 1u);
        ASSERT_EQ(recorder.seen.size(), 1u);
        const WorkflowEvent& e = recorder.seen[0];
        EXPECT_EQ(e.type(), EventType::DeadlineMissed);
        EXPECT_EQ(e.instanceId(), "inst-1");
        EXPECT_EQ(e.itemId(), "A");
        EXPECT_EQ(e.actorId(), "system");
        EXPECT_EQ(e.message(), "deadline passed");
        EXPECT_EQ(e.when(), clock.now());
    }

    TEST_F(DeadlineMonitorTest, EachLapseIsReportedOnlyOnce)
    {
        lapse(a, ItemStatus::InProgress);
        EXPECT_EQ(monitor.sweep(), 1u);
        EXPECT_EQ(monitor.sweep(), 0u);
        EXPECT_EQ(monitor.sweep(), 0u);
        EXPECT_EQ(recorder.seen.size(), 1u);
    }

    TEST_F(DeadlineMonitorTest, OnlyAssignedInProgressAndUnderReviewAreMonitored)
    {
        const ItemStatus watched[] = { ItemStatus::Assigned, ItemStatus::InProgress, ItemStatus::UnderReview };
        const ItemStatus ignored[] = { ItemStatus::Created, ItemStatus::Available, ItemStatus::Escalated,
                                       ItemStatus::Completed, ItemStatus::Cancelled, ItemStatus::Skipped };
        for (std::size_t i = 0; i < 3; ++i)
        {
            DeadlineMonitor fresh(*reinterpret_cast<WorkflowInstance*>(instance_storage), clock);
            lapse(a, watched[i]);
            EXPECT_EQ(fresh.sweep(), 1u);
        }
        for (std::size_t i = 0; i < 6; ++i)
        {
            DeadlineMonitor fresh(*reinterpret_cast<WorkflowInstance*>(instance_storage), clock);
            lapse(a, ignored[i]);
            EXPECT_EQ(fresh.sweep(), 0u);
        }
    }

    TEST_F(DeadlineMonitorTest, AnItemIsReportedAgainAfterLeavingAndReenteringAnOpenStatus)
    {
        lapse(a, ItemStatus::InProgress);
        EXPECT_EQ(monitor.sweep(), 1u);
        a.st = ItemStatus::Escalated;
        EXPECT_EQ(monitor.sweep(), 0u);
        a.st = ItemStatus::InProgress;
        EXPECT_EQ(monitor.sweep(), 1u);
        EXPECT_EQ(recorder.seen.size(), 2u);
    }

    TEST_F(DeadlineMonitorTest, CompositesAreNeverReportedOnlyTheirLeaves)
    {
        lapse(group, ItemStatus::InProgress);
        lapse(root, ItemStatus::InProgress);
        EXPECT_EQ(monitor.sweep(), 0u);
    }

    TEST_F(DeadlineMonitorTest, LeavesAtEveryDepthAreVisitedInDepthFirstOrder)
    {
        lapse(a, ItemStatus::Assigned);
        lapse(b, ItemStatus::InProgress);
        lapse(c, ItemStatus::UnderReview);
        EXPECT_EQ(monitor.sweep(), 3u);
        ASSERT_EQ(recorder.seen.size(), 3u);
        EXPECT_EQ(recorder.seen[0].itemId(), "A");
        EXPECT_EQ(recorder.seen[1].itemId(), "B");
        EXPECT_EQ(recorder.seen[2].itemId(), "C");
    }

    TEST_F(DeadlineMonitorTest, NeverChangesItemStatus)
    {
        lapse(a, ItemStatus::Assigned);
        lapse(b, ItemStatus::UnderReview);
        monitor.sweep();
        EXPECT_EQ(a.status(), ItemStatus::Assigned);
        EXPECT_EQ(b.status(), ItemStatus::UnderReview);
    }

    TEST_F(DeadlineMonitorTest, AdvancingTheClockMakesAnItemLapse)
    {
        a.st = ItemStatus::InProgress;
        a.due = ahead(30);
        EXPECT_EQ(monitor.sweep(), 0u);
        clock.advance(31);
        EXPECT_EQ(monitor.sweep(), 1u);
    }

    TEST_F(DeadlineMonitorTest, ReportsOnlyTheItemsThatHaveLapsed)
    {
        lapse(a, ItemStatus::Assigned);
        b.st = ItemStatus::Assigned;
        b.due = ahead(60);
        EXPECT_EQ(monitor.sweep(), 1u);
        ASSERT_EQ(recorder.seen.size(), 1u);
        EXPECT_EQ(recorder.seen[0].itemId(), "A");
    }
}