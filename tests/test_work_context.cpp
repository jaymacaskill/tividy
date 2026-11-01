#include <gtest/gtest.h>
#include <set>
#include "engine/items/WorkContext.h"

namespace tividy
{
	namespace
	{
		struct WorkContextTest : ::testing::Test
		{
			WorkflowData data;
			EventPublisher events;
			ManualClock clock{TimePoint()};
			ConnectorRegistry connectors;
			ParticipantDirectory directory;
			WorkContext context{"instance-1", data, events, clock, connectors, directory};
		};
		class CountEvents : public EventObserver
		{
			public:
				unsigned count = 0;
				void onEvent(const WorkflowEvent&) override { ++count; }
		};
	}

	TEST_F(WorkContextTest, ReferencesTheOriginalCollaborators)
	{
		EXPECT_EQ(&context.data(), &data);
		EXPECT_EQ(&context.events(), &events);
		EXPECT_EQ(&context.clock(), &clock);
		EXPECT_EQ(&context.connectors(), &connectors);
		EXPECT_EQ(&context.directory(), &directory);
	}
	TEST_F(WorkContextTest, OwnsACopyOfItsInstanceId)
	{
		Id id = "initial";
		WorkContext other(id, data, events, clock, connectors, directory);
		id = "changed";
		EXPECT_EQ(other.instanceId(), "initial");
		EXPECT_EQ(context.instanceId(), "instance-1");
	}
	TEST_F(WorkContextTest, ObservesChangesThroughSharedCollaborators)
	{
		context.data().set("key", "value");
		EXPECT_EQ(data.get("key"), "value");
		clock.advance(7);
		EXPECT_EQ(context.clock().now(), TimePoint() + std::chrono::seconds(7));
		context.directory().add(Participant("worker", "Worker", std::set<std::string>{"worker"}));
		EXPECT_EQ(directory.withRole("worker").size(), 1u);
	}
	TEST_F(WorkContextTest, IrreversibilityCountsAreLocalToEachContext)
	{
		WorkContext other("instance-2", data, events, clock, connectors, directory);
		EXPECT_EQ(context.irreversibleCount(), 0u);
		context.markIrreversible();
		context.markIrreversible();
		EXPECT_EQ(context.irreversibleCount(), 2u);
		EXPECT_EQ(other.irreversibleCount(), 0u);
	}
	TEST_F(WorkContextTest, InspectionAndBookkeepingDoNotPublishEvents)
	{
		CountEvents recorder;
		events.attach(&recorder);
		(void)context.instanceId();
		(void)context.data();
		context.markIrreversible();
		EXPECT_EQ(recorder.count, 0u);
		events.detach(&recorder);
	}
}
