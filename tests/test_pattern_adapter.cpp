/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 31 October 2026
*/

#include <gtest/gtest.h>
#include <memory>

#include "engine/connectors/ExternalConnector.h"
#include "industries/banking/BankingParts.h"
#include "industries/telecom/TelecomParts.h"

using namespace tividy;
using namespace tividy::banking;
using namespace tividy::telecom;

TEST(AdapterPattern, CoreBankingSuccess) {
	auto vendor = std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor());
	CoreBankingAdapter adapter(std::move(vendor));

	EXPECT_EQ(adapter.name(), "core-banking");

	ConnectorRequest req;
	req.operation = "post_provisional_credit";
	req.params["card_ref"] = "CARD-123";
	req.params["amount"] = "500.00";

	ConnectorResult res = adapter.invoke(req);
	
	EXPECT_TRUE(res.success);
	EXPECT_FALSE(res.retryable);
	EXPECT_EQ(res.payload["ledger_ref"], "LEDGER-1");
}

TEST(AdapterPattern, CoreBankingRetryableFailure) {
	auto vendor = std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor());
	CoreBankingAdapter adapter(std::move(vendor));

	ConnectorRequest req;
	req.operation = "post_provisional_credit";
	req.params["card_ref"] = "CARD-123";
	req.params["amount"] = "500.00";
	req.params["simulate_failures"] = "1"; 

	ConnectorResult res = adapter.invoke(req);
	
	EXPECT_FALSE(res.success);
	EXPECT_TRUE(res.retryable); 
	EXPECT_EQ(res.message, "core banking unavailable");
}

TEST(AdapterPattern, CoreBankingNonRetryableFailure) {
	auto vendor = std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor());
	CoreBankingAdapter adapter(std::move(vendor));

	ConnectorRequest req;
	req.operation = "post_provisional_credit";
	req.params["card_ref"] = "CARD-123";
	req.params["amount"] = "-50.00";

	ConnectorResult res = adapter.invoke(req);
	
	EXPECT_FALSE(res.success);
	EXPECT_FALSE(res.retryable); 
	EXPECT_EQ(res.message, "amount must be a positive number");
}

TEST(AdapterPattern, CardSchemeSuccess) {
	auto vendor = std::unique_ptr<CardSchemeVendor>(new CardSchemeVendor());
	CardSchemeAdapter adapter(std::move(vendor));

	EXPECT_EQ(adapter.name(), "card-scheme");

	ConnectorRequest req;
	req.operation = "file_chargeback";
	req.params["transaction_id"] = "TX-999";
	req.params["reason_code"] = "FRAUD";

	ConnectorResult res = adapter.invoke(req);
	
	EXPECT_TRUE(res.success);
	EXPECT_EQ(res.payload["scheme_case"], "CASE-TX-999");
}

TEST(AdapterPattern, CreditBureauSuccess) {
	auto vendor = std::unique_ptr<CreditBureauVendor>(new CreditBureauVendor());
	CreditBureauAdapter adapter(std::move(vendor));

	EXPECT_EQ(adapter.name(), "credit-bureau");

	ConnectorRequest req;
	req.operation = "score";
	req.params["id_number"] = "9001015000081"; 

	ConnectorResult res = adapter.invoke(req);
	
	EXPECT_TRUE(res.success);
	EXPECT_EQ(res.payload["score"], "750");
	EXPECT_EQ(res.payload["band"], "APPROVED");
}

TEST(AdapterPattern, NetworkProvisioningSuccess) {
	auto vendor = std::unique_ptr<NetworkVendor>(new NetworkVendor());
	NetworkProvisioningAdapter adapter(std::move(vendor));

	EXPECT_EQ(adapter.name(), "network-provisioning");

	ConnectorRequest req;
	req.operation = "provision";
	req.params["service_ref"] = "MSISDN-123";
	req.params["product"] = "Fibre";

	ConnectorResult res = adapter.invoke(req);
	
	EXPECT_TRUE(res.success);
	EXPECT_EQ(res.message, "network provisioned");
}

TEST(AdapterPattern, LoggingNotifier) {
	LoggingNotifier notifier("test-notifier");
	EXPECT_EQ(notifier.name(), "test-notifier");

	ConnectorRequest req;
	req.operation = "notify";
	req.params["recipient"] = "user@example.com";
	req.params["subject"] = "Test";
	req.params["body"] = "Hello";

	ConnectorResult res = notifier.invoke(req);
	EXPECT_TRUE(res.success);
	EXPECT_EQ(notifier.sent().size(), 1);
	EXPECT_EQ(notifier.sent()[0], "user@example.com | Test | Hello");
}

TEST(AdapterPattern, AllAdaptersShareOneInterfaceThroughTheRegistry) {
	ConnectorRegistry registry;
	registry.registerConnector(std::unique_ptr<ExternalConnector>(
		new CoreBankingAdapter(std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor()))));
	registry.registerConnector(std::unique_ptr<ExternalConnector>(
		new CreditBureauAdapter(std::unique_ptr<CreditBureauVendor>(new CreditBureauVendor()))));

	EXPECT_TRUE(registry.has("core-banking"));
	EXPECT_TRUE(registry.has("credit-bureau"));

	ExternalConnector& connector = registry.require("credit-bureau");
	ConnectorRequest req;
	req.operation = "score";
	req.params["id_number"] = "9001015000081";
	EXPECT_TRUE(connector.invoke(req).success);
}

TEST(AdapterPattern, VendorExceptionsBecomeUniformResults) {
	std::unique_ptr<CreditBureauVendor> bureau(new CreditBureauVendor());
	bureau->failNextCalls(1);
	CreditBureauAdapter credit(std::move(bureau));

	ConnectorRequest req;
	req.operation = "score";
	req.params["id_number"] = "9001015000081";
	ConnectorResult res = credit.invoke(req);
	EXPECT_FALSE(res.success);
	EXPECT_TRUE(res.retryable);
	EXPECT_EQ(res.message, "bureau unavailable");

	NetworkProvisioningAdapter network(std::unique_ptr<NetworkVendor>(new NetworkVendor()));
	ConnectorRequest prov;
	prov.operation = "provision";
	prov.params["service_ref"] = "MSISDN-1";
	prov.params["product"] = "Fibre";
	prov.params["simulate_failures"] = "1";
	res = network.invoke(prov);
	EXPECT_FALSE(res.success);
	EXPECT_TRUE(res.retryable);
	EXPECT_EQ(res.message, "OSS unavailable");
}

TEST(AdapterPattern, AnyConnectorCanStandInForAVendor) {
	struct FakeConnector : public ExternalConnector {
		std::string name() const override { return "card-scheme"; }
		ConnectorResult invoke(const ConnectorRequest&) override {
			std::map<std::string, std::string> payload;
			payload["scheme_case"] = "CASE-FAKE";
			return ConnectorResult::ok("fake", payload);
		}
	};

	ConnectorRegistry registry;
	registry.registerConnector(std::unique_ptr<ExternalConnector>(new FakeConnector()));

	ConnectorRequest req;
	req.operation = "file_chargeback";
	ConnectorResult res = registry.require("card-scheme").invoke(req);
	EXPECT_TRUE(res.success);
	EXPECT_EQ(res.payload["scheme_case"], "CASE-FAKE");
}