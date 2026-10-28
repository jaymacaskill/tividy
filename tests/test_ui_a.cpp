#include <gtest/gtest.h>

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
} // namespace tividy
