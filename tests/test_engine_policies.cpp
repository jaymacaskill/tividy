#include <gtest/gtest.h>
#include <memory>

#include "engine/policies/EscalationHandler.h"

/*
These tests are not exhaustive for the engine/policies folder,
at the time of writing tests, only EscalationHandler was available for testing due to missing WorkItem
*/

namespace tividy
{
    namespace
    {
        EscalationRequest make_request(unsigned level)
        {
            EscalationRequest r;
            r.instance_id = "inst-1";
            r.item_id = "task-1";
            r.reason = "overdue";
            r.level = level;
            r.data = 0;
            return r;
        }
    }

    TEST(EscalationHandlerTest, EmptyChainReturnsUnresolved)
    {
        RoleEscalationHandler h("senior-analyst", 1);
        EscalationResult r = h.handle(make_request(2));
        EXPECT_FALSE(r.resolved);
        EXPECT_TRUE(r.approver_role.empty());
        EXPECT_TRUE(r.handled_by.empty());
    }

    TEST(EscalationHandlerTest, SingleHandlerResolvesMatchingLevel)
    {
        RoleEscalationHandler h("senior-analyst", 1);
        EscalationResult r = h.handle(make_request(1));
        EXPECT_TRUE(r.resolved);
        EXPECT_EQ(r.approver_role, "senior-analyst");
        EXPECT_EQ(r.handled_by, "senior-analyst");
    }

    TEST(EscalationHandlerTest, ChainFallsThroughToMatchingLevel)
    {
        std::shared_ptr<const EscalationHandler> level2 =
            std::shared_ptr<const EscalationHandler>(new RoleEscalationHandler("compliance-officer", 2));
        std::shared_ptr<const EscalationHandler> chain =
            std::shared_ptr<const EscalationHandler>(new RoleEscalationHandler("senior-analyst", 1, level2));

        EscalationResult r = chain->handle(make_request(2));
        EXPECT_TRUE(r.resolved);
        EXPECT_EQ(r.approver_role, "compliance-officer");
    }

    TEST(EscalationHandlerTest, FirstMatchingHandlerWins)
    {
        std::shared_ptr<const EscalationHandler> level2 =
            std::shared_ptr<const EscalationHandler>(new RoleEscalationHandler("compliance-officer", 2));
        std::shared_ptr<const EscalationHandler> chain =
            std::shared_ptr<const EscalationHandler>(new RoleEscalationHandler("senior-analyst", 1, level2));

        EscalationResult r = chain->handle(make_request(1));
        EXPECT_TRUE(r.resolved);
        EXPECT_EQ(r.approver_role, "senior-analyst");
    }
} // namespace tividy