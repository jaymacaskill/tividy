/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 2 November 2026
*/

/**
 * @file test_pattern_abstract_factory.cpp
 * @brief Factory families, client extensions and installation through the real facade.
 */

#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "engine/core/Errors.h"
#include "engine/facade/Tividy.h"
#include "engine/items/TaskItem.h"
#include "industries/banking/BankingPack.h"
#include "industries/demo/DemoPack.h"
#include "industries/telecom/TelecomPack.h"

static_assert(std::is_abstract<tividy::IndustryPack>::value, "IndustryPack is the abstract factory");
static_assert(std::has_virtual_destructor<tividy::IndustryPack>::value, "Factories support owned polymorphism");

namespace
{

	struct Family
	{
		std::unique_ptr<tividy::IndustryPack> (*make)();
		std::string key;
		std::string workflow;
		std::string first_item;
		std::set<std::string> connectors;
		std::vector<std::string> roles;
		std::string first_escalation;
		std::string second_escalation;
		std::string product_name;
		std::string instance_term;
	};

	template<class Pack>
	std::unique_ptr<tividy::IndustryPack> makePack()
	{
		return std::unique_ptr<tividy::IndustryPack>(new Pack());
	}

	const Family demo = {
		&makePack<tividy::demo::DemoPack>, "demo", "document-approval", "write_draft",
		{"notifier"}, {"author", "legal", "finance", "manager", "director", "publisher"},
		"manager", "director", "TIVIDY", "instance"
	};
	const Family banking = {
		&makePack<tividy::banking::BankingPack>, "banking", "transaction-dispute", "lodge_dispute",
		{"core-banking", "card-scheme", "notifier"},
		{"customer", "dispute-agent", "merchant-rep", "dispute-analyst", "senior-analyst", "compliance-officer", "system"},
		"senior-analyst", "compliance-officer", "TIVIDY Resolve", "Dispute case"
	};
	const Family telecom = {
		&makePack<tividy::telecom::TelecomPack>, "telecom", "service-activation", "capture_order",
		{"credit-bureau", "network-provisioning", "notifier"},
		{"sales-agent", "customer-care", "credit-analyst", "logistics", "field-technician", "network-engineer", "noc-manager", "system"},
		"network-engineer", "noc-manager", "TIVIDY Connect", "Order"
	};

	std::unique_ptr<tividy::ManualClock> clock()
	{
		return std::unique_ptr<tividy::ManualClock>(new tividy::ManualClock(tividy::TimePoint()));
	}

	std::set<std::string> connectorNames(const std::vector<std::unique_ptr<tividy::ExternalConnector> >& products)
	{
		std::set<std::string> names;
		for (const auto& product : products)
		{
			EXPECT_TRUE(product);
			if (product) EXPECT_TRUE(names.insert(product->name()).second) << product->name();
		}
		return names;
	}

	tividy::WorkflowData initialData()
	{
		tividy::WorkflowData data;
		data.set("title", "Factory integration");
		data.set("author", "Example author");
		data.set("transaction_id", "TX-1");
		data.set("amount", "100.00");
		data.set("reason_code", "DUPLICATE");
		data.set("card_ref", "CARD-1");
		data.set("merchant_name", "Example merchant");
		data.set("customer_ref", "SERVICE-1");
		data.set("customer_id_number", "1234567891");
		data.set("region", "gauteng");
		data.set("product", "fibre");
		data.set("install_type", "SELF_INSTALL");
		return data;
	}

	class AbstractFactoryPattern : public ::testing::TestWithParam<Family> {};

	TEST_P(AbstractFactoryPattern, OneAbstractInterfacePublishesAValidIndustryWorkflow)
	{
		auto pack = GetParam().make();
		const tividy::IndustryPack& factory = *pack;
		EXPECT_EQ(GetParam().key, factory.key());
		EXPECT_FALSE(factory.displayName().empty());
		auto director = factory.createDirector();
		ASSERT_TRUE(director);
		EXPECT_EQ(GetParam().workflow, director->workflowName());
		EXPECT_EQ(1u, director->version());
		tividy::StandardWorkflowBuilder builder;
		auto definition = director->build(builder);
		ASSERT_TRUE(definition);
		EXPECT_EQ(GetParam().workflow, definition->name());
		EXPECT_TRUE(definition->validationProblems().empty());
		EXPECT_NO_THROW(definition->validate());
	}

	TEST_P(AbstractFactoryPattern, ParticipantAndPolicyProductsAgreeOnIndustryRoles)
	{
		auto pack = GetParam().make();
		auto participants = pack->createParticipants();
		ASSERT_FALSE(participants.empty());
		tividy::ParticipantDirectory directory;
		std::set<tividy::Id> ids;
		for (const auto& person : participants)
		{
			EXPECT_EQ(0u, person.id().find(GetParam().key + "-"));
			EXPECT_TRUE(ids.insert(person.id()).second);
			directory.add(person);
		}
		auto assignment = pack->createAssignmentStrategy();
		ASSERT_TRUE(assignment);
		tividy::ManualTask item(tividy::WorkItemSpec("assignment-probe", "Assignment probe"));
		const auto data = initialData();
		for (const auto& role : GetParam().roles)
		{
			SCOPED_TRACE(role);
			const auto chosen = assignment->chooseParticipant(item, role, directory, data);
			ASSERT_FALSE(chosen.empty());
			const auto* person = directory.find(chosen);
			ASSERT_NE(nullptr, person);
			EXPECT_TRUE(person->hasRole(role));
		}
		auto escalation = pack->createEscalationChain();
		ASSERT_TRUE(escalation);
		const std::string expected[] = {GetParam().first_escalation, GetParam().second_escalation};
		for (unsigned level = 1; level <= 2; ++level)
		{
			const tividy::EscalationRequest request = {"instance", "item", "help", level, &data};
			const auto result = escalation->handle(request);
			EXPECT_TRUE(result.resolved);
			EXPECT_EQ(expected[level - 1], result.approver_role);
			EXPECT_FALSE(directory.withRole(result.approver_role).empty());
		}
		const tividy::EscalationRequest unhandled = {"instance", "item", "help", 3, &data};
		EXPECT_FALSE(escalation->handle(unhandled).resolved);
	}

	TEST_P(AbstractFactoryPattern, ConnectorAndProfileProductsBelongToTheSameFamily)
	{
		auto pack = GetParam().make();
		auto connectors = pack->createConnectors();
		EXPECT_EQ(GetParam().connectors, connectorNames(connectors));
		const auto profile = pack->createProfile();
		EXPECT_EQ(GetParam().product_name, profile.product_name);
		EXPECT_EQ(GetParam().instance_term, profile.term("instance"));
		EXPECT_EQ("extension-term", profile.term("extension-term"));
	}

	TEST_P(AbstractFactoryPattern, OwnedProductsRemainUsableAfterTheFactoryIsDestroyed)
	{
		auto pack = GetParam().make();
		auto connectors = pack->createConnectors();
		auto people = pack->createParticipants();
		auto assignment = pack->createAssignmentStrategy();
		auto escalation = pack->createEscalationChain();
		auto profile = pack->createProfile();
		pack.reset();
		EXPECT_EQ(GetParam().connectors, connectorNames(connectors));
		tividy::ParticipantDirectory directory;
		for (const auto& person : people) directory.add(person);
		ASSERT_TRUE(assignment);
		tividy::ManualTask item(tividy::WorkItemSpec("probe", "Probe"));
		const auto data = initialData();
		EXPECT_FALSE(assignment->chooseParticipant(item, GetParam().roles.front(), directory, data).empty());
		ASSERT_TRUE(escalation);
		const tividy::EscalationRequest request = {"instance", "item", "help", 1, &data};
		EXPECT_TRUE(escalation->handle(request).resolved);
		EXPECT_EQ(GetParam().instance_term, profile.term("instance"));
		// Directors retain a reference to their pack, so they are deliberately excluded here.
	}

	TEST_P(AbstractFactoryPattern, TheSameFacadeInstallsAnyFamilyWithoutIndustrySpecificCalls)
	{
		tividy::Tividy app(clock());
		app.installPack(GetParam().make());
		EXPECT_EQ(std::vector<std::string>{GetParam().key}, app.packKeys());
		EXPECT_EQ(std::vector<std::string>{GetParam().workflow}, app.workflowNames());
		EXPECT_EQ(GetParam().key, app.packOf(GetParam().workflow));
		EXPECT_EQ(GetParam().product_name, app.profile(GetParam().key).product_name);
		for (const auto& role : GetParam().roles) EXPECT_FALSE(app.participants().withRole(role).empty());
		for (const auto& name : GetParam().connectors) EXPECT_TRUE(app.connectors().has(name));
		EXPECT_TRUE(app.instanceIds().empty());
	}

	TEST_P(AbstractFactoryPattern, SeparateInstallationsOwnIndependentNotificationProducts)
	{
		tividy::Tividy first(clock());
		tividy::Tividy second(clock());
		first.installPack(GetParam().make());
		second.installPack(GetParam().make());
		auto* first_notifier = dynamic_cast<tividy::LoggingNotifier*>(&first.connectors().require("notifier"));
		auto* second_notifier = dynamic_cast<tividy::LoggingNotifier*>(&second.connectors().require("notifier"));
		ASSERT_NE(nullptr, first_notifier);
		ASSERT_NE(nullptr, second_notifier);
		EXPECT_NE(first_notifier, second_notifier);
		tividy::ConnectorRequest request;
		request.operation = "notify";
		request.params = {{"recipient", "client"}, {"subject", "Hello"}, {"body", "One installation"}};
		EXPECT_TRUE(first_notifier->invoke(request).success);
		EXPECT_EQ(1u, first_notifier->sent().size());
		EXPECT_TRUE(second_notifier->sent().empty());
	}

	TEST_P(AbstractFactoryPattern, OneFactoryDefinitionCreatesIndependentRuntimeProducts)
	{
		tividy::Tividy app(clock());
		app.installPack(GetParam().make());
		auto seed = initialData();
		const auto first_id = app.startWorkflow(GetParam().workflow, seed);
		const auto second_id = app.startWorkflow(GetParam().workflow, seed);
		ASSERT_NE(first_id, second_id);
		auto& first = app.instance(first_id);
		auto& second = app.instance(second_id);
		EXPECT_EQ(GetParam().workflow, first.definitionName());
		EXPECT_EQ(GetParam().workflow, second.definitionName());
		EXPECT_EQ(tividy::InstanceStatus::Running, first.status());
		EXPECT_EQ(tividy::InstanceStatus::Running, second.status());
		ASSERT_NE(nullptr, first.findItem(GetParam().first_item));
		ASSERT_NE(nullptr, second.findItem(GetParam().first_item));
		EXPECT_NE(first.findItem(GetParam().first_item), second.findItem(GetParam().first_item));
		first.data().set("title", "First instance only");
		seed.set("title", "Caller only");
		EXPECT_EQ("Factory integration", second.data().get("title"));
		EXPECT_EQ("First instance only", first.data().get("title"));
		EXPECT_EQ((std::vector<tividy::Id>{first_id, second_id}), app.instanceIds());
	}

	INSTANTIATE_TEST_SUITE_P(IndustryFamilies, AbstractFactoryPattern, ::testing::Values(demo, banking, telecom));

	template<class Pack>
	class ClientPolicies : public Pack
	{
		public:
			ClientPolicies()
				: assignment(new tividy::RoleBasedAssignment()),
				  escalation(new tividy::RoleEscalationHandler("client-reviewer", 1)) {}
			std::shared_ptr<const tividy::AssignmentStrategy> createAssignmentStrategy() const override { return assignment; }
			std::shared_ptr<const tividy::EscalationHandler> createEscalationChain() const override { return escalation; }
			std::shared_ptr<const tividy::AssignmentStrategy> assignment;
			std::shared_ptr<const tividy::EscalationHandler> escalation;
	};

	template<class Pack>
	void expectClientPolicies()
	{
		ClientPolicies<Pack> client;
		const tividy::IndustryPack& factory = client;
		auto director = factory.createDirector();
		ASSERT_TRUE(director);
		tividy::StandardWorkflowBuilder builder;
		auto definition = director->build(builder);
		ASSERT_TRUE(definition);
		EXPECT_EQ(client.assignment, definition->policies().assignment);
		EXPECT_EQ(client.escalation, definition->policies().escalation);
	}

	TEST(AbstractFactoryCustomization, InheritedDemoDirectorUsesClientPolicyFactories)
	{
		expectClientPolicies<tividy::demo::DemoPack>();
	}

	TEST(AbstractFactoryCustomization, InheritedBankingDirectorUsesClientPolicyFactories)
	{
		expectClientPolicies<tividy::banking::BankingPack>();
	}

	TEST(AbstractFactoryCustomization, InheritedTelecomDirectorUsesClientPolicyFactories)
	{
		expectClientPolicies<tividy::telecom::TelecomPack>();
	}

	class ClientDemo : public tividy::demo::DemoPack
	{
		public:
			explicit ClientDemo(bool& destroyed) : destroyed(destroyed)
			{
				profile.product_name = "Client approval";
				profile.vocabulary["instance"] = "Submission";
			}
			~ClientDemo() override { destroyed = true; }
			tividy::ClientProfile createProfile() const override { return profile; }
			tividy::ClientProfile profile;
		private:
			bool& destroyed;
	};

	TEST(AbstractFactoryCustomization, OneProfileOverrideRetainsTheWorkflowAndFacadeOwnsTheClientPack)
	{
		bool destroyed = false;
		{
			tividy::Tividy app(clock());
			std::unique_ptr<ClientDemo> client(new ClientDemo(destroyed));
			auto* observed = client.get();
			app.installPack(std::move(client));
			EXPECT_FALSE(destroyed);
			EXPECT_EQ("demo", app.packOf("document-approval"));
			EXPECT_EQ("Client approval", app.profile("demo").product_name);
			EXPECT_EQ("Submission", app.profile("demo").term("instance"));
			observed->profile.product_name = "Later factory edit";
			observed->profile.vocabulary["instance"] = "Later label";
			EXPECT_EQ("Client approval", app.profile("demo").product_name);
			EXPECT_EQ("Submission", app.profile("demo").term("instance"));
		}
		EXPECT_TRUE(destroyed);
	}

	TEST(AbstractFactoryInstallation, DuplicatePackIdentityDoesNotPublishASecondWorkflow)
	{
		tividy::Tividy app(clock());
		app.installPack(demo.make());
		EXPECT_THROW(app.installPack(demo.make()), tividy::EngineError);
		EXPECT_EQ(std::vector<std::string>{"demo"}, app.packKeys());
		EXPECT_EQ(std::vector<std::string>{"document-approval"}, app.workflowNames());
		EXPECT_EQ("demo", app.packOf("document-approval"));
	}

} // namespace
