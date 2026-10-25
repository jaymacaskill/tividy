#include <gtest/gtest.h>
#include <stdexcept>

#include "engine/events/Observer.h"
#include "engine/events/WorkflowEvent.h"

namespace tividy
{
	namespace
	{
		class RecordingObserver : public EventObserver
		{
			public:
				std::vector<EventType> received;
				void onEvent(const WorkflowEvent& e) override { received.push_back(e.type()); }
		};
	}

	TEST(EventPublisherTest, NotifiesAttachedObservers)
	{
		EventPublisher pub;
		RecordingObserver a, b;
		pub.attach(&a);
		pub.attach(&b);

		pub.notify(WorkflowEvent(EventType::InstanceStarted, "inst-1", "", TimePoint()));

		EXPECT_EQ(a.received.size(), 1u);
		EXPECT_EQ(b.received.size(), 1u);
	}

	TEST(EventPublisherTest, DetachRemovesObserver)
	{
		EventPublisher pub;
		RecordingObserver a;
		pub.attach(&a);
		pub.detach(&a);

		pub.notify(WorkflowEvent(EventType::ItemCompleted, "inst-1", "task-1", TimePoint()));

		EXPECT_EQ(a.received.size(), 0u);
		EXPECT_EQ(pub.observerCount(), 0u);
	}

	TEST(EventPublisherTest, AttachIsIdempotent)
	{
		EventPublisher pub;
		RecordingObserver a;
		pub.attach(&a);
		pub.attach(&a);

		EXPECT_EQ(pub.observerCount(), 1u);
	}

	TEST(WorkflowEventTest, FluentBuilderSetsFields)
	{
		WorkflowEvent ev(EventType::ItemCompleted, "inst-1", "task-1", TimePoint());
		ev.byActor("u1").withMessage("done").withCount(2);

		EXPECT_EQ(ev.actorId(), "u1");
		EXPECT_EQ(ev.message(), "done");
		EXPECT_EQ(ev.count(), 2u);
	}
} // namespace tividy