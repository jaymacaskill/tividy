#include <gtest/gtest.h>

#include "engine/core/Types.h"
#include "engine/core/Outcome.h"
#include "engine/core/WorkflowData.h"
#include "engine/core/Participant.h"
#include "engine/core/Clock.h"
#include "engine/core/Errors.h"

namespace tividy
{
	TEST(TypesTest, ToStringCoversEveryItemStatus)
	{
		EXPECT_EQ(toString(ItemStatus::Created), "Created");
		EXPECT_EQ(toString(ItemStatus::Available), "Available");
		EXPECT_EQ(toString(ItemStatus::Assigned), "Assigned");
		EXPECT_EQ(toString(ItemStatus::InProgress), "InProgress");
		EXPECT_EQ(toString(ItemStatus::UnderReview), "UnderReview");
		EXPECT_EQ(toString(ItemStatus::Escalated), "Escalated");
		EXPECT_EQ(toString(ItemStatus::Completed), "Completed");
		EXPECT_EQ(toString(ItemStatus::Cancelled), "Cancelled");
		EXPECT_EQ(toString(ItemStatus::Skipped), "Skipped");
	}

	TEST(TypesTest, IsSettledAndIsTerminal)
	{
		EXPECT_TRUE(isSettled(ItemStatus::Completed));
		EXPECT_TRUE(isSettled(ItemStatus::Skipped));
		EXPECT_FALSE(isSettled(ItemStatus::Cancelled));
		EXPECT_FALSE(isSettled(ItemStatus::InProgress));

		EXPECT_TRUE(isTerminal(ItemStatus::Completed));
		EXPECT_TRUE(isTerminal(ItemStatus::Cancelled));
		EXPECT_TRUE(isTerminal(ItemStatus::Skipped));
		EXPECT_FALSE(isTerminal(ItemStatus::Assigned));
	}

	TEST(OutcomeTest, RoutingKeyUsesCodeWhenPresent)
	{
		Outcome o = Outcome::rejected("DOCS_INVALID", "reason");
		EXPECT_EQ(o.routingKey(), "DOCS_INVALID");
	}

	TEST(OutcomeTest, RoutingKeyFallsBackToKindName)
	{
		EXPECT_EQ(Outcome::rejected().routingKey(), "Rejected");
		EXPECT_EQ(Outcome::accepted().routingKey(), "Accepted");
		EXPECT_EQ(Outcome::escalated().routingKey(), "Escalated");
		EXPECT_EQ(Outcome::failed().routingKey(), "Failed");
	}

	TEST(WorkflowDataTest, GetThrowsOnMissingKey)
	{
		WorkflowData data;
		EXPECT_THROW(data.get("missing"), NotFoundError);
	}

	TEST(WorkflowDataTest, GetOrReturnsFallbackWhenAbsent)
	{
		WorkflowData data;
		EXPECT_EQ(data.getOr("missing", "default"), "default");
		data.set("present", "value");
		EXPECT_EQ(data.getOr("present", "default"), "value");
	}

	TEST(WorkflowDataTest, SetOverwrites)
	{
		WorkflowData data;
		data.set("k", "v1");
		data.set("k", "v2");
		EXPECT_EQ(data.get("k"), "v2");
	}

	TEST(ParticipantTest, HasRole)
	{
		std::set<std::string> roles{"hr", "admin"};
		Participant p("u1", "Alice", roles);
		EXPECT_TRUE(p.hasRole("hr"));
		EXPECT_FALSE(p.hasRole("it"));
	}

	TEST(ParticipantDirectoryTest, AddThrowsOnDuplicate)
	{
		ParticipantDirectory dir;
		dir.add(Participant("u1", "Alice", std::set<std::string>{"hr"}));
		EXPECT_THROW(
			dir.add(Participant("u1", "Impostor", std::set<std::string>{})),
			EngineError
		);
	}

	TEST(ParticipantDirectoryTest, WithRoleReturnsSortedById)
	{
		ParticipantDirectory dir;
		dir.add(Participant("u3", "Charlie", std::set<std::string>{"hr"}));
		dir.add(Participant("u1", "Alice",   std::set<std::string>{"hr"}));
		dir.add(Participant("u2", "Bob",     std::set<std::string>{"it"}));

		std::vector<const Participant*> hrs = dir.withRole("hr");
		ASSERT_EQ(hrs.size(), 2u);
		EXPECT_EQ(hrs[0]->id(), "u1");
		EXPECT_EQ(hrs[1]->id(), "u3");
	}

	TEST(ManualClockTest, AdvanceMovesClockForward)
	{
		TimePoint start;
		ManualClock clock(start);
		EXPECT_EQ(clock.now(), start);
		clock.advance(60);
		EXPECT_EQ(clock.now(), start + std::chrono::seconds(60));
	}

	TEST(ManualClockTest, AdvanceThrowsOnNegative)
	{
		ManualClock clock{TimePoint()};
		EXPECT_THROW(clock.advance(-1), EngineError);
	}
} // namespace tividy