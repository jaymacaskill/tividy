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
	} // namespace telecom
} // namespace tividy