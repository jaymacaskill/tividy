#include <gtest/gtest.h>
#include <map>
#include <set>
#include <string>

#include "industries/banking/BankingPack.h"
#include "engine/facade/IndustryPack.h"

using namespace tividy;
using tividy::banking::BankingPack;

TEST(BankingPackTest, PublishesBankingIdentityAndDisputeDirector)
{
    BankingPack pack;

    EXPECT_EQ(pack.key(), "banking");
    EXPECT_EQ(pack.displayName(), "Banking: Transaction Dispute Resolution");

    auto director = pack.createDirector();
    ASSERT_NE(director.get(), nullptr);
    EXPECT_EQ(director->workflowName(), "transaction-dispute");
    EXPECT_EQ(director->version(), 1u);
    EXPECT_FALSE(director->description().empty());
}

TEST(BankingPackTest, ProvidesEveryRequiredParticipantWithUniqueIds)
{
    BankingPack pack;
    const std::map<std::string, std::string> expected = {
        {"banking-cust-1", "customer"},
        {"banking-agent-1", "dispute-agent"},
        {"banking-merchant-1", "merchant-rep"},
        {"banking-analyst-1", "dispute-analyst"},
        {"banking-senior-1", "senior-analyst"},
        {"banking-compliance-1", "compliance-officer"},
        {"banking-sys-1", "system"}
    };

    const auto participants = pack.createParticipants();
    ASSERT_EQ(participants.size(), expected.size());

    std::set<std::string> seen;
    for (const auto& participant : participants)
    {
        const auto match = expected.find(participant.id());
        ASSERT_NE(match, expected.end());
        EXPECT_TRUE(seen.insert(participant.id()).second);
        EXPECT_TRUE(participant.hasRole(match->second));
        EXPECT_FALSE(participant.displayName().empty());
    }
}

TEST(BankingPackTest, ProvidesAllConnectorsReadyForRegistration)
{
    BankingPack pack;
    auto connectors = pack.createConnectors();
    ASSERT_EQ(connectors.size(), 3u);

    ConnectorRegistry registry;
    for (auto& connector : connectors)
    {
        ASSERT_NE(connector.get(), nullptr);
        ASSERT_NO_THROW(registry.registerConnector(std::move(connector)));
    }

    EXPECT_TRUE(registry.has("core-banking"));
    EXPECT_TRUE(registry.has("card-scheme"));
    EXPECT_TRUE(registry.has("notifier"));
}

TEST(BankingPackTest, EscalatesToSeniorThenCompliance)
{
    BankingPack pack;
    const auto chain = pack.createEscalationChain();
    ASSERT_NE(chain.get(), nullptr);

    WorkflowData data;
    EscalationRequest request = {"case-1", "issue_provisional_credit", "deadline missed", 1u, &data};

    auto result = chain->handle(request);
    ASSERT_TRUE(result.resolved);
    EXPECT_EQ(result.approver_role, "senior-analyst");

    request.level = 2;
    result = chain->handle(request);
    ASSERT_TRUE(result.resolved);
    EXPECT_EQ(result.approver_role, "compliance-officer");

    request.level = 3;
    EXPECT_FALSE(chain->handle(request).resolved);
}

TEST(BankingPackTest, ProvidesBankingVocabularyAndFallback)
{
    BankingPack pack;
    const auto profile = pack.createProfile();

    EXPECT_EQ(profile.product_name, "TIVIDY Resolve");
    EXPECT_EQ(profile.term("instance"), "Dispute case");
    EXPECT_EQ(profile.term("item"), "Action");
    EXPECT_EQ(profile.term("workflow"), "Dispute process");
    EXPECT_EQ(profile.term("participant"), "Party");
    EXPECT_EQ(profile.term("escalate"), "Refer to supervisor");
    EXPECT_EQ(profile.term("unknown-term"), "unknown-term");
    EXPECT_EQ(profile.accent, "\033[38;2;255;191;0m");
}