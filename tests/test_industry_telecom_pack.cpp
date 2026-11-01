#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>

#include "industries/telecom/TelecomPack.h"
#include "industries/telecom/TelecomParts.h"

namespace tividy
{
namespace telecom
{
    TEST(TelecomPackTest, HasCorrectIdentity)
    {
        TelecomPack pack;

        EXPECT_EQ(pack.key(), "telecom");
        EXPECT_EQ(pack.displayName(),
                  "Telecommunications: Service Provisioning & Activation");
    }

    TEST(TelecomPackTest, CreatesServiceActivationDirector)
    {
        TelecomPack pack;
        auto director = pack.createDirector();

        ASSERT_NE(director.get(), nullptr);
        EXPECT_EQ(director->workflowName(), "service-activation");
        EXPECT_EQ(director->version(), 1u);
    }

    TEST(TelecomPackTest, CreatesExpectedParticipantsAndRegions)
    {
        TelecomPack pack;
        const auto participants = pack.createParticipants();

        const std::map<std::string, std::string> expected = {
            {"telecom-sales-1", "sales-agent"},
            {"telecom-care-1", "customer-care"},
            {"telecom-credit-1", "credit-analyst"},
            {"telecom-logistics-1", "logistics"},
            {"telecom-tech-gp-1", "field-technician"},
            {"telecom-tech-wc-1", "field-technician"},
            {"telecom-neteng-1", "network-engineer"},
            {"telecom-noc-1", "noc-manager"},
            {"telecom-sys-1", "system"}
        };

        ASSERT_EQ(participants.size(), expected.size());

        std::set<std::string> ids;
        ParticipantDirectory directory;

        for (const auto& participant : participants)
        {
            SCOPED_TRACE(participant.id());

            // Check uniqueness before adding to the directory.
            ASSERT_TRUE(ids.insert(participant.id()).second);

            const auto entry = expected.find(participant.id());
            ASSERT_NE(entry, expected.end());
            EXPECT_TRUE(participant.hasRole(entry->second));

            directory.add(participant);
        }

        EXPECT_EQ(directory.require("telecom-tech-gp-1").attribute("region"), "gauteng");
        EXPECT_EQ(directory.require("telecom-tech-wc-1").attribute("region"), "western-cape");
    }

    TEST(TelecomPackTest, CreatesExpectedConnectors)
    {
        TelecomPack pack;
        auto connectors = pack.createConnectors();

        ASSERT_EQ(connectors.size(), 3u);

        std::set<std::string> names;
        for (const auto& connector : connectors)
        {
            ASSERT_NE(connector.get(), nullptr);
            EXPECT_TRUE(names.insert(connector->name()).second);
        }

        const std::set<std::string> expected = {"credit-bureau", "network-provisioning", "notifier"};

        EXPECT_EQ(names, expected);
    }

    TEST(TelecomPackTest, CreatesRegionalAssignmentStrategy)
    {
        TelecomPack pack;
        auto strategy = pack.createAssignmentStrategy();

        ASSERT_NE(strategy.get(), nullptr);
        EXPECT_NE(dynamic_cast<const RegionalTechnicianAssignment*>(strategy.get()), nullptr);
    }

    TEST(TelecomPackTest, EscalationChainRoutesSupportedLevels)
    {
        TelecomPack pack;
        auto chain = pack.createEscalationChain();
        ASSERT_NE(chain.get(), nullptr);

        struct Expected
        {
            unsigned level;
            bool resolved;
            const char* role;
        };

        const Expected cases[] = {
            {0u, false, ""},
            {1u, true, "network-engineer"},
            {2u, true, "noc-manager"},
            {3u, false, ""}
        };

        WorkflowData data;

        for (const auto& testCase : cases)
        {
            SCOPED_TRACE(testCase.level);

            EscalationRequest request{"order-1", "install_cpe", "Needs review", testCase.level, &data };

            const auto result = chain->handle(request);

            EXPECT_EQ(result.resolved, testCase.resolved);
            EXPECT_EQ(result.approver_role, testCase.role);
            EXPECT_EQ(result.handled_by, testCase.role);
        }
    }

    TEST(TelecomPackTest, CreatesTelecomProfile)
    {
        TelecomPack pack;
        const auto profile = pack.createProfile();

        EXPECT_EQ(profile.product_name, "TIVIDY Connect");
        EXPECT_EQ(profile.accent, "\033[34m");
        EXPECT_EQ(profile.term("instance"), "Order");
        EXPECT_EQ(profile.term("item"), "Step");
        EXPECT_EQ(profile.term("workflow"), "Service journey");
        EXPECT_EQ(profile.term("participant"), "Agent");
        EXPECT_EQ(profile.term("escalate"), "Raise to NOC");
    }
}
}