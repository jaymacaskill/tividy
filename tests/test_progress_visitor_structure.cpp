#include <gtest/gtest.h>
#include "engine/items/WorkItemVisitor.h"

namespace tividy
{
	TEST(ProgressVisitorStructure, StartsWithNoTasksAndZeroCounts)
	{
		const ProgressVisitor visitor;
		EXPECT_EQ(visitor.total(), 0u);
		for (auto status : {ItemStatus::Created, ItemStatus::Available, ItemStatus::Assigned,
			ItemStatus::InProgress, ItemStatus::UnderReview, ItemStatus::Escalated,
			ItemStatus::Completed, ItemStatus::Cancelled, ItemStatus::Skipped})
			EXPECT_EQ(visitor.count(status), 0u) << toString(status);
	}
}
