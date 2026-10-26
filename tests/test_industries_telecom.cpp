#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "engine/connectors/ExternalConnector.h"
#include "industries/telecom/TelecomParts.h"

namespace tividy
{
	namespace telecom
	{
		namespace
		{
			ConnectorRequest make_request(const std::string& operation)
			{
				ConnectorRequest r;
				r.operation = operation;
				return r;
			}
		}

        /*
		TEST(CreditBureauVendorTest, IdEndingInZeroReturns200)
		{
			CreditBureauVendor v;
			EXPECT_EQ(v.score("12340"), 200);
		}

		TEST(CreditBureauVendorTest, IdEndingInFiveReturns550)
		{
			CreditBureauVendor v;
			EXPECT_EQ(v.score("12345"), 550);
		}

		TEST(CreditBureauVendorTest, OtherIdsReturn750)
		{
			CreditBureauVendor v;
			EXPECT_EQ(v.score("12346"), 750);
			EXPECT_EQ(v.score("99999"), 750);
			EXPECT_EQ(v.score("abcdef"), 750);
		}

		TEST(CreditBureauVendorTest, FailNextCallsBlocksThenRecovers)
		{
			CreditBureauVendor v;
			v.failNextCalls(2);

			EXPECT_THROW(v.score("12345"), std::runtime_error);
			EXPECT_THROW(v.score("12345"), std::runtime_error);
			EXPECT_EQ(v.score("12345"), 550);
		}

		TEST(CreditBureauVendorTest, EmptyIdThrowsInvalidArgument)
		{
			CreditBureauVendor v;
			EXPECT_THROW(v.score(""), std::invalid_argument);
		}

		TEST(CreditBureauAdapterTest, NameIsCreditBureau)
		{
			CreditBureauAdapter a(std::unique_ptr<CreditBureauVendor>(new CreditBureauVendor()));
			EXPECT_EQ(a.name(), "credit-bureau");
		}

		TEST(CreditBureauAdapterTest, HighScoreReturnsApprovedBand)
		{
			CreditBureauAdapter a(std::unique_ptr<CreditBureauVendor>(new CreditBureauVendor()));

			ConnectorRequest req = make_request("score");
			req.params["id_number"] = "12346";

			ConnectorResult r = a.invoke(req);
			EXPECT_TRUE(r.success);
			EXPECT_FALSE(r.retryable);
			EXPECT_EQ(r.payload.at("score"), "750");
			EXPECT_EQ(r.payload.at("band"),  "APPROVED");
		}

		TEST(CreditBureauAdapterTest, MidScoreReturnsReferBand)
		{
			CreditBureauAdapter a(std::unique_ptr<CreditBureauVendor>(new CreditBureauVendor()));

			ConnectorRequest req = make_request("score");
			req.params["id_number"] = "12345";

			ConnectorResult r = a.invoke(req);
			EXPECT_TRUE(r.success);
			EXPECT_EQ(r.payload.at("score"), "550");
			EXPECT_EQ(r.payload.at("band"),  "REFER");
		}

		TEST(CreditBureauAdapterTest, LowScoreReturnsDeclinedBand)
		{
			CreditBureauAdapter a(std::unique_ptr<CreditBureauVendor>(new CreditBureauVendor()));

			ConnectorRequest req = make_request("score");
			req.params["id_number"] = "12340";

			ConnectorResult r = a.invoke(req);
			EXPECT_TRUE(r.success);
			EXPECT_EQ(r.payload.at("score"), "200");
			EXPECT_EQ(r.payload.at("band"),  "DECLINED");
		}

		TEST(CreditBureauAdapterTest, MissingIdNumberNonRetryable)
		{
			CreditBureauAdapter a(std::unique_ptr<CreditBureauVendor>(new CreditBureauVendor()));

			ConnectorRequest req = make_request("score");

			ConnectorResult r = a.invoke(req);
			EXPECT_FALSE(r.success);
			EXPECT_FALSE(r.retryable);
			EXPECT_NE(r.message.find("id_number"), std::string::npos);
		}

		TEST(CreditBureauAdapterTest, UnknownOperationNonRetryable)
		{
			CreditBureauAdapter a(std::unique_ptr<CreditBureauVendor>(new CreditBureauVendor()));

			ConnectorRequest req = make_request("check_wealth");
			ConnectorResult r = a.invoke(req);
			EXPECT_FALSE(r.success);
			EXPECT_FALSE(r.retryable);
			EXPECT_NE(r.message.find("unknown operation"), std::string::npos);
		}

		TEST(CreditBureauAdapterTest, EmptyIdNumberNonRetryable)
		{
			CreditBureauAdapter a(std::unique_ptr<CreditBureauVendor>(new CreditBureauVendor()));

			ConnectorRequest req = make_request("score");
			req.params["id_number"] = "";

			ConnectorResult r = a.invoke(req);
			EXPECT_FALSE(r.success);
			EXPECT_FALSE(r.retryable);
			EXPECT_FALSE(r.message.empty());
		}
        */

        TEST(NetworkVendorTest, FreshVendorHasNoActiveService)
        {
            NetworkVendor v;
            EXPECT_FALSE(v.isActive("ref-1"));
            EXPECT_FALSE(v.isActive("anything"));
        }

        TEST(NetworkVendorTest, ActivateMarksServiceActive)
        {
            NetworkVendor v;
            v.activate("ref-1");
            EXPECT_TRUE(v.isActive("ref-1"));
            EXPECT_FALSE(v.isActive("ref-2"));
        }

        TEST(NetworkVendorTest, SecondActivateReplacesFirst)
        {
            NetworkVendor v;
            v.activate("ref-1");
            v.activate("ref-2");
            EXPECT_TRUE(v.isActive("ref-2"));
            EXPECT_FALSE(v.isActive("ref-1"));
        }

        TEST(NetworkVendorTest, ProvisionIsIdempotent)
        {
            NetworkVendor v;
            EXPECT_NO_THROW(v.provision("ref-1", "fibre"));
            EXPECT_NO_THROW(v.provision("ref-1", "fibre"));
        }

        TEST(NetworkVendorTest, FailNextCallsBlocksProvisionThenRecovers)
        {
            NetworkVendor v;
            v.failNextCalls(2);

            EXPECT_THROW(v.provision("ref-1", "fibre"), std::runtime_error);
            EXPECT_THROW(v.provision("ref-1", "fibre"), std::runtime_error);
            EXPECT_NO_THROW(v.provision("ref-1", "fibre"));
        }

        TEST(NetworkVendorTest, FailNextCallsBlocksActivateThenRecovers)
        {
            NetworkVendor v;
            v.failNextCalls(1);

            EXPECT_THROW(v.activate("ref-1"), std::runtime_error);
            EXPECT_NO_THROW(v.activate("ref-1"));
            EXPECT_TRUE(v.isActive("ref-1"));
        }

        TEST(NetworkProvisioningAdapterTest, NameIsNetworkProvisioning)
        {
            NetworkProvisioningAdapter a(std::unique_ptr<NetworkVendor>(new NetworkVendor()));
            EXPECT_EQ(a.name(), "network-provisioning");
        }

        TEST(NetworkProvisioningAdapterTest, ProvisionSuccess)
        {
            NetworkProvisioningAdapter a(std::unique_ptr<NetworkVendor>(new NetworkVendor()));

            ConnectorRequest req = make_request("provision");
            req.params["service_ref"] = "ref-1";
            req.params["product"]     = "fibre-100";

            ConnectorResult r = a.invoke(req);
            EXPECT_TRUE(r.success);
            EXPECT_FALSE(r.retryable);
            EXPECT_TRUE(r.payload.empty());
        }

        TEST(NetworkProvisioningAdapterTest, ProvisionMissingServiceRefNonRetryable)
        {
            NetworkProvisioningAdapter a(std::unique_ptr<NetworkVendor>(new NetworkVendor()));

            ConnectorRequest req = make_request("provision");
            req.params["product"] = "fibre-100";

            ConnectorResult r = a.invoke(req);
            EXPECT_FALSE(r.success);
            EXPECT_FALSE(r.retryable);
            EXPECT_NE(r.message.find("service_ref"), std::string::npos);
        }

        TEST(NetworkProvisioningAdapterTest, ProvisionMissingProductNonRetryable)
        {
            NetworkProvisioningAdapter a(std::unique_ptr<NetworkVendor>(new NetworkVendor()));

            ConnectorRequest req = make_request("provision");
            req.params["service_ref"] = "ref-1";

            ConnectorResult r = a.invoke(req);
            EXPECT_FALSE(r.success);
            EXPECT_FALSE(r.retryable);
            EXPECT_NE(r.message.find("product"), std::string::npos);
        }

        TEST(NetworkProvisioningAdapterTest, ProvisionSimulateFailuresTriggersRetryable)
        {
            NetworkProvisioningAdapter a(std::unique_ptr<NetworkVendor>(new NetworkVendor()));

            ConnectorRequest req = make_request("provision");
            req.params["service_ref"]        = "ref-1";
            req.params["product"]            = "fibre-100";
            req.params["simulate_failures"]  = "1";

            ConnectorResult r = a.invoke(req);
            EXPECT_FALSE(r.success);
            EXPECT_TRUE(r.retryable);
        }

        TEST(NetworkProvisioningAdapterTest, ProvisionRecoversAfterSimulatedFailure)
        {
            NetworkProvisioningAdapter a(std::unique_ptr<NetworkVendor>(new NetworkVendor()));

            ConnectorRequest fail = make_request("provision");
            fail.params["service_ref"]       = "ref-1";
            fail.params["product"]           = "fibre-100";
            fail.params["simulate_failures"] = "1";
            EXPECT_FALSE(a.invoke(fail).success);

            ConnectorRequest retry = make_request("provision");
            retry.params["service_ref"] = "ref-1";
            retry.params["product"]     = "fibre-100";
            EXPECT_TRUE(a.invoke(retry).success);
        }

        TEST(NetworkProvisioningAdapterTest, ActivateSuccess)
        {
            NetworkProvisioningAdapter a(std::unique_ptr<NetworkVendor>(new NetworkVendor()));

            ConnectorRequest req = make_request("activate");
            req.params["service_ref"] = "ref-1";

            ConnectorResult r = a.invoke(req);
            EXPECT_TRUE(r.success);
            EXPECT_TRUE(r.payload.empty());
        }

        TEST(NetworkProvisioningAdapterTest, ActivateMissingServiceRefNonRetryable)
        {
            NetworkProvisioningAdapter a(std::unique_ptr<NetworkVendor>(new NetworkVendor()));

            ConnectorRequest req = make_request("activate");

            ConnectorResult r = a.invoke(req);
            EXPECT_FALSE(r.success);
            EXPECT_FALSE(r.retryable);
            EXPECT_NE(r.message.find("service_ref"), std::string::npos);
        }

        TEST(NetworkProvisioningAdapterTest, UnknownOperationNonRetryable)
        {
            NetworkProvisioningAdapter a(std::unique_ptr<NetworkVendor>(new NetworkVendor()));

            ConnectorRequest req = make_request("decommission");
            ConnectorResult r = a.invoke(req);
            EXPECT_FALSE(r.success);
            EXPECT_FALSE(r.retryable);
            EXPECT_NE(r.message.find("unknown operation"), std::string::npos);
        }
	} // namespace telecom
} // namespace tividy