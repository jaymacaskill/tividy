// Independently runnable vocabulary/spec tests used by the future routing tests.
// These exercise existing implemented APIs; they do not claim to exercise FlowController.
#include <gtest/gtest.h>
#include "engine/core/Types.h"
#include "engine/items/WorkItemSpec.h"

namespace tividy
{
	struct StatusExpectation { ItemStatus status; bool settled; bool terminal; };
	class RoutingStatusContract : public ::testing::TestWithParam<StatusExpectation> {};

	TEST_P(RoutingStatusContract, DistinguishesDependencySettlementFromTermination)
	{
		const auto expected = GetParam();
		EXPECT_EQ(isSettled(expected.status), expected.settled) << toString(expected.status);
		EXPECT_EQ(isTerminal(expected.status), expected.terminal) << toString(expected.status);
	}

	INSTANTIATE_TEST_SUITE_P(AllStatuses, RoutingStatusContract, ::testing::Values(
		StatusExpectation{ItemStatus::Created, false, false},
		StatusExpectation{ItemStatus::Available, false, false},
		StatusExpectation{ItemStatus::Assigned, false, false},
		StatusExpectation{ItemStatus::InProgress, false, false},
		StatusExpectation{ItemStatus::UnderReview, false, false},
		StatusExpectation{ItemStatus::Escalated, false, false},
		StatusExpectation{ItemStatus::Completed, true, true},
		StatusExpectation{ItemStatus::Cancelled, false, true},
		StatusExpectation{ItemStatus::Skipped, true, true}));

	TEST(RoutingDependencySpecContract, PreservesDeclaredOrderAndPermittedRepeatedIds)
	{
		WorkItemSpec spec("leaf", "Leaf");
		spec.dependsOn("b").dependsOn("a").dependsOn("b");
		EXPECT_EQ(spec.dependencies(), (std::vector<Id>{"b", "a", "b"}));
	}
}
