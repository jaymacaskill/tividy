#include <gtest/gtest.h>

#include <string>

#include "ui/ClientProfile.h"

namespace tividy
{
    TEST(ClientProfileTest, DefaultConstructorHasTividyIdentity)
    {
        ClientProfile p;
        EXPECT_EQ(p.product_name, "TIVIDY");
        EXPECT_EQ(p.tagline, "");
        EXPECT_EQ(p.accent, "\033[38;2;216;137;76m");
        EXPECT_TRUE(p.banner.empty());
        EXPECT_TRUE(p.vocabulary.empty());
    }

    TEST(ClientProfileTest, TermFallsBackToEngineWordWhenUnmapped)
    {
        ClientProfile p;
        EXPECT_EQ(p.term("instance"), "instance");
        EXPECT_EQ(p.term("item"), "item");
        EXPECT_EQ(p.term("workflow"), "workflow");
        EXPECT_EQ(p.term("participant"), "participant");
    }

    TEST(ClientProfileTest, TermReturnsOverrideWhenMapped)
    {
        ClientProfile p;
        p.vocabulary["instance"] = "Order";
        p.vocabulary["item"] = "Step";
        p.vocabulary["participant"] = "Agent";

        EXPECT_EQ(p.term("instance"), "Order");
        EXPECT_EQ(p.term("item"), "Step");
        EXPECT_EQ(p.term("participant"), "Agent");
    }

    TEST(ClientProfileTest, TermFallsBackPerKeyWhenOnlySomeAreMapped)
    {
        ClientProfile p;
        p.vocabulary["instance"] = "Order";

        EXPECT_EQ(p.term("instance"), "Order");
        EXPECT_EQ(p.term("item"),"item");
    }
} // namespace tividy