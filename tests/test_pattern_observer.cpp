#include <gtest/gtest.h>

#include "include/engine/events/Observer.h"
#include "include/engine/events/WorkflowEvent.h"

namespace tividy 
{
    namespace 
    {
        class FakeObserver : public EventObserver 
        {
            public:
                std::vector<WorkflowEvent> received;
                explicit FakeObserver(const std::string& n) : name_ref(n) { }
                std::string name const override { return name_ref; }
                void onEvent(const WorkflowEvent& event) { received.push_back(event); };

            private:
                std::string name_ref;
        };
    }

    TEST(SubjectTest, AttachedObserverIsNotified) {
        WorkflowEvent event = WorkflowEvent(EventType::InstanceStarted, "instance", "item", std::chrono::system_clock::now());
        EventPublisher subject = EventPublisher();
        FakeObserver observer;

        subject.attach(&obs);
        subject.notify(event);

        ASSERT_EQ(subject.observerCount(), 1);
        ASSERT_EQ(observer.received.size(), 1);
        EXPECT_EQ(observer.received[0].type(), EventType::InstanceStarted);
        EXPECT_EQ(observer.received[0].instanceId(), "instance");
        EXPECT_EQ(observer.received[0].itemId(), "item");
        EXPECT_EQ(observer.received[0].when(), std::chrono::system_clock::now());
    }

    TEST(SubjectTest, DetachedObserverIsNotNotified) {
        WorkflowEvent event = WorkflowEvent(EventType::InstanceStarted, "instance", "item", std::chrono::system_clock::now());
        EventPublisher subject = EventPublisher();
        FakeObserver observer;

        subject.attach(&observer);
        subject.detach(&observer);
        subject.notify(event);

        EXPECT_TRUE(observer.received.empty());
    }

    TEST(SubjectTest, AllObserversAreNotified) {
        WorkflowEvent event = WorkflowEvent(EventType::InstanceStarted, "instance", "item", std::chrono::system_clock::now());
        EventPublisher subject = EventPublisher();
        FakeObserver observerA, observerB;

        subject.attach(&observerA);
        subject.attach(&observerB);
        subject.notify(event);

        EXPECT_EQ(observerA.received.size(), 1);
        EXPECT_EQ(observerB.received.size(), 1);
    }
}