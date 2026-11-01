/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 29 October 2026
*/

#include <gtest/gtest.h>
#include <memory>
#include <type_traits>
#include <utility>

#include "engine/facade/IndustryPack.h"

static_assert(std::is_abstract<tividy::IndustryPack>::value,
	"Every industry must supply its identity and workflow director");
static_assert(std::has_virtual_destructor<tividy::IndustryPack>::value,
	"Industry packs must support destruction through their factory interface");

namespace
{
	class MinimalPack : public tividy::IndustryPack
	{
		public:
			std::string key() const override { return "minimal"; }
			std::string displayName() const override { return "Minimal industry"; }
			std::unique_ptr<tividy::WorkflowDirector> createDirector() const override
			{
				// Workflow construction is outside these default-product tests.
				return std::unique_ptr<tividy::WorkflowDirector>();
			}
	};

	class ProfileOnlyPack : public MinimalPack
	{
		public:
			tividy::ClientProfile createProfile() const override
			{
				tividy::ClientProfile profile = MinimalPack::createProfile();
				profile.product_name = "Client workspace";
				profile.vocabulary["instance"] = "Order";
				return profile;
			}
	};

	class CustomProductsPack : public MinimalPack
	{
		public:
			CustomProductsPack()
				: assignment(std::make_shared<tividy::DataDrivenAssignment>("owner_id",
					std::make_shared<tividy::RoleBasedAssignment>())),
				  escalation(std::make_shared<tividy::RoleEscalationHandler>("manager", 1)) {}

			std::vector<tividy::Participant> createParticipants() const override
			{
				return {tividy::Participant("minimal-worker", "Worker", {"worker"})};
			}
			std::vector<std::unique_ptr<tividy::ExternalConnector> > createConnectors() const override
			{
				std::vector<std::unique_ptr<tividy::ExternalConnector> > connectors;
				connectors.push_back(std::unique_ptr<tividy::ExternalConnector>(
					new tividy::LoggingNotifier("client-notifier")));
				return connectors;
			}
			std::shared_ptr<const tividy::AssignmentStrategy> createAssignmentStrategy() const override
			{
				return assignment;
			}
			std::shared_ptr<const tividy::EscalationHandler> createEscalationChain() const override
			{
				return escalation;
			}
			tividy::ClientProfile createProfile() const override
			{
				tividy::ClientProfile profile;
				profile.product_name = "Custom industry";
				return profile;
			}
			std::shared_ptr<const tividy::AssignmentStrategy> assignment;
			std::shared_ptr<const tividy::EscalationHandler> escalation;
	};

	tividy::ConnectorRequest notification()
	{
		tividy::ConnectorRequest request;
		request.operation = "notify";
		request.params = {{"recipient", "worker"}, {"subject", "Ready"}, {"body", "Work is available"}};
		return request;
	}
}

TEST(IndustryPackDefaults, MinimalPackInheritsAllDocumentedDefaultProducts)
{
	const MinimalPack pack;
	const tividy::IndustryPack& factory = pack;
	EXPECT_TRUE(factory.createParticipants().empty());
	EXPECT_FALSE(factory.createEscalationChain());
	const auto assignment = factory.createAssignmentStrategy();
	ASSERT_TRUE(assignment);
	EXPECT_NE(nullptr, dynamic_cast<const tividy::RoleBasedAssignment*>(assignment.get()));
	auto connectors = factory.createConnectors();
	ASSERT_EQ(1u, connectors.size());
	ASSERT_TRUE(connectors.front());
	EXPECT_EQ("notifier", connectors.front()->name());
	EXPECT_EQ("TIVIDY", factory.createProfile().product_name);
}

TEST(IndustryPackDefaults, DefaultNotifierPerformsNotificationsAndRejectsUnknownOperations)
{
	const MinimalPack pack;
	auto connectors = pack.createConnectors();
	ASSERT_EQ(1u, connectors.size());
	auto* notifier = dynamic_cast<tividy::LoggingNotifier*>(connectors.front().get());
	ASSERT_NE(nullptr, notifier);
	const auto result = notifier->invoke(notification());
	EXPECT_TRUE(result.success);
	ASSERT_EQ(1u, notifier->sent().size());
	EXPECT_EQ("worker | Ready | Work is available", notifier->sent().front());
	tividy::ConnectorRequest unsupported;
	unsupported.operation = "charge";
	const auto failure = notifier->invoke(unsupported);
	EXPECT_FALSE(failure.success);
	EXPECT_FALSE(failure.retryable);
	EXPECT_EQ(1u, notifier->sent().size());
}

TEST(IndustryPackDefaults, RepeatedConnectorFactoriesHaveIndependentNotificationOutboxes)
{
	const MinimalPack pack;
	auto first = pack.createConnectors();
	auto second = pack.createConnectors();
	ASSERT_EQ(1u, first.size());
	ASSERT_EQ(1u, second.size());
	EXPECT_NE(first.front().get(), second.front().get());
	auto* first_notifier = dynamic_cast<tividy::LoggingNotifier*>(first.front().get());
	auto* second_notifier = dynamic_cast<tividy::LoggingNotifier*>(second.front().get());
	ASSERT_NE(nullptr, first_notifier);
	ASSERT_NE(nullptr, second_notifier);
	ASSERT_TRUE(first_notifier->invoke(notification()).success);
	EXPECT_EQ(1u, first_notifier->sent().size());
	EXPECT_TRUE(second_notifier->sent().empty());
}

TEST(IndustryPackDefaults, ConnectorOwnershipTransfersToTheRegistryAndOutlivesThePack)
{
	tividy::ConnectorRegistry registry;
	{
		const MinimalPack pack;
		auto connectors = pack.createConnectors();
		ASSERT_EQ(1u, connectors.size());
		auto* supplied = connectors.front().get();
		registry.registerConnector(std::move(connectors.front()));
		EXPECT_FALSE(connectors.front());
		EXPECT_EQ(supplied, &registry.require("notifier"));
	}
	ASSERT_TRUE(registry.has("notifier"));
	EXPECT_TRUE(registry.require("notifier").invoke(notification()).success);
}

TEST(IndustryPackDefaults, ProfileUsesClientProfileDefaultsAndPreservesVocabularyFallback)
{
	const MinimalPack pack;
	const auto profile = pack.createProfile();
	const tividy::ClientProfile defaults;
	EXPECT_EQ(defaults.product_name, profile.product_name);
	EXPECT_EQ(defaults.tagline, profile.tagline);
	EXPECT_EQ(defaults.banner, profile.banner);
	EXPECT_EQ(defaults.accent, profile.accent);
	EXPECT_EQ(defaults.vocabulary, profile.vocabulary);
	for (const std::string term : {"workflow", "instance", "item", "participant", "start", "submit",
		"complete", "approve", "reject", "escalate", "cancel", "history", "work-list", "client-term", ""})
	{
		SCOPED_TRACE(term);
		EXPECT_EQ(term, profile.term(term));
	}
}

TEST(IndustryPackDefaults, EditingAReturnedProfileDoesNotChangeFutureProfilesOrOtherPacks)
{
	const MinimalPack pack;
	const MinimalPack other;
	auto edited = pack.createProfile();
	edited.product_name = "Changed";
	edited.tagline = "Tagline";
	edited.banner = "Banner";
	edited.accent = "custom";
	edited.vocabulary["instance"] = "Order";
	const auto fresh = pack.createProfile();
	const auto independent = other.createProfile();
	EXPECT_EQ("TIVIDY", fresh.product_name);
	EXPECT_TRUE(fresh.tagline.empty());
	EXPECT_TRUE(fresh.banner.empty());
	EXPECT_EQ(tividy::ClientProfile().accent, fresh.accent);
	EXPECT_TRUE(fresh.vocabulary.empty());
	EXPECT_EQ("instance", fresh.term("instance"));
	EXPECT_EQ(fresh.product_name, independent.product_name);
	EXPECT_EQ(fresh.vocabulary, independent.vocabulary);
}

TEST(IndustryPackDefaults, AssignmentStrategyHasSharedOwnershipIndependentOfThePack)
{
	std::shared_ptr<const tividy::AssignmentStrategy> strategy;
	{
		const MinimalPack pack;
		strategy = pack.createAssignmentStrategy();
	}
	ASSERT_TRUE(strategy);
	EXPECT_NE(nullptr, dynamic_cast<const tividy::RoleBasedAssignment*>(strategy.get()));
	std::weak_ptr<const tividy::AssignmentStrategy> lifetime = strategy;
	auto retained = strategy;
	strategy.reset();
	EXPECT_FALSE(lifetime.expired());
	EXPECT_EQ(retained, lifetime.lock());
}

TEST(IndustryPackDefaults, ClientCanOverrideOnlyItsProfileAndInheritOtherDefaultProducts)
{
	const ProfileOnlyPack pack;
	const tividy::IndustryPack& factory = pack;
	const auto profile = factory.createProfile();
	EXPECT_EQ("Client workspace", profile.product_name);
	EXPECT_EQ("Order", profile.term("instance"));
	EXPECT_EQ("workflow", profile.term("workflow"));
	EXPECT_TRUE(factory.createParticipants().empty());
	EXPECT_FALSE(factory.createEscalationChain());
	const auto assignment = factory.createAssignmentStrategy();
	EXPECT_NE(nullptr, dynamic_cast<const tividy::RoleBasedAssignment*>(assignment.get()));
	auto connectors = factory.createConnectors();
	ASSERT_EQ(1u, connectors.size());
	EXPECT_EQ("notifier", connectors.front()->name());
}

TEST(IndustryPackDefaults, EveryDefaultFactoryCanBeOverriddenThroughTheBaseInterface)
{
	const CustomProductsPack pack;
	const tividy::IndustryPack& factory = pack;
	const auto participants = factory.createParticipants();
	ASSERT_EQ(1u, participants.size());
	EXPECT_EQ("minimal-worker", participants.front().id());
	auto connectors = factory.createConnectors();
	ASSERT_EQ(1u, connectors.size());
	EXPECT_EQ("client-notifier", connectors.front()->name());
	EXPECT_EQ(pack.assignment, factory.createAssignmentStrategy());
	EXPECT_EQ(pack.escalation, factory.createEscalationChain());
	EXPECT_EQ("Custom industry", factory.createProfile().product_name);
}

TEST(IndustryPackDefaults, DestructionThroughTheFactoryInterfaceReleasesTheConcretePack)
{
	class TrackedPack : public MinimalPack
	{
		public:
			explicit TrackedPack(bool& destroyed) : destroyed(destroyed) {}
			~TrackedPack() override { destroyed = true; }
		private:
			bool& destroyed;
	};
	bool destroyed = false;
	{
		std::unique_ptr<tividy::IndustryPack> pack(new TrackedPack(destroyed));
		EXPECT_FALSE(destroyed);
	}
	EXPECT_TRUE(destroyed);
}
