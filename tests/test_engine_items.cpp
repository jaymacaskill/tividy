#include <gtest/gtest.h>

#include "engine/items/WorkItemSpec.h"

/*
These tests are not exhaustive for the engine/tests folder,
at the time of writing tests, only WorkItemSpec was available for testing
*/

namespace tividy
{
    TEST(WorkItemSpecTest, ConstructorSetsIdAndName)
    {
        WorkItemSpec spec("task-1", "Verify documents");
        EXPECT_EQ(spec.id(), "task-1");
        EXPECT_EQ(spec.name(), "Verify documents");
    }

    TEST(WorkItemSpecTest, DefaultsAreSensible)
    {
        WorkItemSpec spec("task-1", "Task");
        EXPECT_TRUE(spec.requiredRole().empty());
        EXPECT_TRUE(spec.reviewerRole().empty());
        EXPECT_TRUE(spec.dependencies().empty());
        EXPECT_FALSE(spec.requiresReview());
        EXPECT_FALSE(spec.isAutomatic());
        EXPECT_EQ(spec.maxRetries(), 2u);
        EXPECT_EQ(spec.dueAfterSeconds(), 0);
        EXPECT_TRUE(spec.timeoutCode().empty());
    }

    TEST(WorkItemSpecTest, RequireRoleSetsRole)
    {
        WorkItemSpec spec("task-1", "Task");
        spec.requireRole("hr-coordinator");
        EXPECT_EQ(spec.requiredRole(), "hr-coordinator");
    }

    TEST(WorkItemSpecTest, ReviewedBySetsRole)
    {
        WorkItemSpec spec("task-1", "Task");
        spec.reviewedBy("hr-head");
        EXPECT_EQ(spec.reviewerRole(), "hr-head");
    }

    TEST(WorkItemSpecTest, DependsOnAccumulates)
    {
        WorkItemSpec spec("task-1", "Task");
        spec.dependsOn("a");
        spec.dependsOn("b");
        ASSERT_EQ(spec.dependencies().size(), 2u);
        EXPECT_EQ(spec.dependencies()[0], "a");
        EXPECT_EQ(spec.dependencies()[1], "b");
    }

    TEST(WorkItemSpecTest, ReviewAndAutomaticFlags)
    {
        WorkItemSpec spec("task-1", "Task");
        EXPECT_FALSE(spec.requiresReview());
        EXPECT_FALSE(spec.isAutomatic());

        spec.reviewRequired(true);
        EXPECT_TRUE(spec.requiresReview());

        spec.automatic(true);
        EXPECT_TRUE(spec.isAutomatic());
    }

    TEST(WorkItemSpecTest, DueAfterAndTimeoutCode)
    {
        WorkItemSpec spec("task-1", "Task");
        spec.dueAfter(86400);
        spec.onTimeout("BREACHED");

        EXPECT_EQ(spec.dueAfterSeconds(), 86400);
        EXPECT_EQ(spec.timeoutCode(), "BREACHED");
    }

    TEST(WorkItemSpecTest, MaxRetriesOverrides)
    {
        WorkItemSpec spec("task-1", "Task");
        EXPECT_EQ(spec.maxRetries(), 2u);

        spec.maxRetries(5);
        EXPECT_EQ(spec.maxRetries(), 5u);
    }

    TEST(WorkItemSpecTest, FluentChaining)
    {
        WorkItemSpec spec("verify_docs", "Verify documents");
        spec.requireRole("hr-coordinator")
            .reviewedBy("hr-head")
            .reviewRequired(true)
            .dependsOn("sign_contract")
            .dependsOn("candidate_accepts")
            .dueAfter(172800)
            .onTimeout("STALLED")
            .maxRetries(3);

        EXPECT_EQ(spec.id(), "verify_docs");
        EXPECT_EQ(spec.requiredRole(), "hr-coordinator");
        EXPECT_EQ(spec.reviewerRole(), "hr-head");
        EXPECT_TRUE(spec.requiresReview());
        EXPECT_EQ(spec.dependencies().size(), 2u);
        EXPECT_EQ(spec.dueAfterSeconds(), 172800);
        EXPECT_EQ(spec.timeoutCode(), "STALLED");
        EXPECT_EQ(spec.maxRetries(), 3u);
    }
} // namespace tividy