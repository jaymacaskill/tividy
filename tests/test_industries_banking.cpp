#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "engine/connectors/ExternalConnector.h"
#include "industries/banking/BankingParts.h"

namespace tividy
{
	namespace banking
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

		TEST(CoreBankingVendorTest, FirstCreditReturnsLedger1)
		{
			CoreBankingVendor v;
			EXPECT_EQ(v.postProvisionalCredit("card-1", "100"), "LEDGER-1");
		}

		TEST(CoreBankingVendorTest, CounterIncrementsPerCall)
		{
			CoreBankingVendor v;
			EXPECT_EQ(v.postProvisionalCredit("card-1", "100"), "LEDGER-1");
			EXPECT_EQ(v.postProvisionalCredit("card-2", "200"), "LEDGER-2");
			EXPECT_EQ(v.postProvisionalCredit("card-3", "300"), "LEDGER-3");
		}

		TEST(CoreBankingVendorTest, RejectsZeroAmount)
		{
			CoreBankingVendor v;
			EXPECT_THROW(v.postProvisionalCredit("card-1", "0"), std::invalid_argument);
		}

		TEST(CoreBankingVendorTest, RejectsNegativeAmount)
		{
			CoreBankingVendor v;
			EXPECT_THROW(v.postProvisionalCredit("card-1", "-50"), std::invalid_argument);
		}

		TEST(CoreBankingVendorTest, RejectsNonNumericAmount)
		{
			CoreBankingVendor v;
			EXPECT_THROW(v.postProvisionalCredit("card-1", "abc"), std::invalid_argument);
		}

		TEST(CoreBankingVendorTest, FailNextCallsBlocksThenRecovers)
		{
			CoreBankingVendor v;
			v.failNextCalls(2);

			EXPECT_THROW(v.postProvisionalCredit("c", "100"), std::runtime_error);
			EXPECT_THROW(v.postProvisionalCredit("c", "100"), std::runtime_error);
			EXPECT_EQ(v.postProvisionalCredit("c", "100"), "LEDGER-1");   // counter only moved on success
		}

		TEST(CoreBankingVendorTest, ReverseRejectsMalformedRef)
		{
			CoreBankingVendor v;
			std::string ref = v.postProvisionalCredit("c", "100");
			EXPECT_THROW(v.reverse("not-a-ref"), std::invalid_argument);
			EXPECT_THROW(v.reverse(""),          std::invalid_argument);
			EXPECT_NO_THROW(v.reverse(ref));
		}

		TEST(CoreBankingVendorTest, FinaliseRejectsMalformedRef)
		{
			CoreBankingVendor v;
			std::string ref = v.postProvisionalCredit("c", "100");
			EXPECT_THROW(v.finalise("bogus"), std::invalid_argument);
			EXPECT_NO_THROW(v.finalise(ref));
		}

		TEST(CardSchemeVendorTest, FileChargebackReturnsCaseId)
		{
			CardSchemeVendor v;
			EXPECT_EQ(v.fileChargeback("txn-abc", "FRAUD"), "CASE-txn-abc");
		}

		TEST(CardSchemeVendorTest, FailNextCallsBlocksThenRecovers)
		{
			CardSchemeVendor v;
			v.failNextCalls(1);
			EXPECT_THROW(v.fileChargeback("txn-abc", "FRAUD"), std::runtime_error);
			EXPECT_EQ(v.fileChargeback("txn-abc", "FRAUD"), "CASE-txn-abc");
		}

		TEST(CoreBankingAdapterTest, NameIsCoreBanking)
		{
			CoreBankingAdapter a(std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor()));
			EXPECT_EQ(a.name(), "core-banking");
		}

		TEST(CoreBankingAdapterTest, PostProvisionalCreditSuccess)
		{
			CoreBankingAdapter a(std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor()));

			ConnectorRequest req = make_request("post_provisional_credit");
			req.params["card_ref"] = "card-1";
			req.params["amount"]   = "100";

			ConnectorResult r = a.invoke(req);
			EXPECT_TRUE(r.success);
			EXPECT_FALSE(r.retryable);
			EXPECT_EQ(r.payload.at("ledger_ref"), "LEDGER-1");
		}

		TEST(CoreBankingAdapterTest, MissingCardRefReturnsNonRetryableFailure)
		{
			CoreBankingAdapter a(std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor()));

			ConnectorRequest req = make_request("post_provisional_credit");
			req.params["amount"] = "100";

			ConnectorResult r = a.invoke(req);
			EXPECT_FALSE(r.success);
			EXPECT_FALSE(r.retryable);
			EXPECT_NE(r.message.find("card_ref"), std::string::npos);
		}

		TEST(CoreBankingAdapterTest, InvalidAmountBecomesNonRetryableFailure)
		{
			CoreBankingAdapter a(std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor()));

			ConnectorRequest req = make_request("post_provisional_credit");
			req.params["card_ref"] = "card-1";
			req.params["amount"]   = "-5";

			ConnectorResult r = a.invoke(req);
			EXPECT_FALSE(r.success);
			EXPECT_FALSE(r.retryable);
		}

		TEST(CoreBankingAdapterTest, SimulateFailuresParamTriggersFailure)
		{
			CoreBankingAdapter a(std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor()));

			ConnectorRequest req = make_request("post_provisional_credit");
			req.params["card_ref"]         = "card-1";
			req.params["amount"]           = "100";
			req.params["simulate_failures"] = "1";

			ConnectorResult r1 = a.invoke(req);
			EXPECT_FALSE(r1.success);
			EXPECT_TRUE(r1.retryable);

			// Second call (no simulate_failures) should succeed
			ConnectorRequest req2 = make_request("post_provisional_credit");
			req2.params["card_ref"] = "card-1";
			req2.params["amount"]   = "100";

			ConnectorResult r2 = a.invoke(req2);
			EXPECT_TRUE(r2.success);
		}

		TEST(CoreBankingAdapterTest, ReverseCreditSuccess)
		{
			CoreBankingAdapter a(std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor()));

			ConnectorRequest post = make_request("post_provisional_credit");
			post.params["card_ref"] = "card-1";
			post.params["amount"]   = "100";
			ConnectorResult posted = a.invoke(post);
			ASSERT_TRUE(posted.success);

			ConnectorRequest req = make_request("reverse_credit");
			req.params["ledger_ref"] = posted.payload.at("ledger_ref");

			EXPECT_TRUE(a.invoke(req).success);
		}

		TEST(CoreBankingAdapterTest, ReverseCreditBadRefNonRetryable)
		{
			CoreBankingAdapter a(std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor()));

			ConnectorRequest req = make_request("reverse_credit");
			req.params["ledger_ref"] = "garbage";

			ConnectorResult r = a.invoke(req);
			EXPECT_FALSE(r.success);
			EXPECT_FALSE(r.retryable);
		}

		TEST(CoreBankingAdapterTest, FinaliseCreditSuccess)
		{
			CoreBankingAdapter a(std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor()));

			ConnectorRequest post = make_request("post_provisional_credit");
			post.params["card_ref"] = "card-1";
			post.params["amount"]   = "100";
			ConnectorResult posted = a.invoke(post);
			ASSERT_TRUE(posted.success);

			ConnectorRequest req = make_request("finalise_credit");
			req.params["ledger_ref"] = posted.payload.at("ledger_ref");

			EXPECT_TRUE(a.invoke(req).success);
		}

		TEST(CoreBankingAdapterTest, UnknownOperationNonRetryable)
		{
			CoreBankingAdapter a(std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor()));

			ConnectorRequest req = make_request("send_money_to_switzerland");
			ConnectorResult r = a.invoke(req);
			EXPECT_FALSE(r.success);
			EXPECT_FALSE(r.retryable);
			EXPECT_NE(r.message.find("unknown operation"), std::string::npos);
		}

		TEST(CardSchemeAdapterTest, NameIsCardScheme)
		{
			CardSchemeAdapter a(std::unique_ptr<CardSchemeVendor>(new CardSchemeVendor()));
			EXPECT_EQ(a.name(), "card-scheme");
		}

		TEST(CardSchemeAdapterTest, FileChargebackSuccess)
		{
			CardSchemeAdapter a(std::unique_ptr<CardSchemeVendor>(new CardSchemeVendor()));

			ConnectorRequest req = make_request("file_chargeback");
			req.params["transaction_id"] = "txn-42";
			req.params["reason_code"]    = "FRAUD";

			ConnectorResult r = a.invoke(req);
			EXPECT_TRUE(r.success);
			EXPECT_EQ(r.payload.at("scheme_case"), "CASE-txn-42");
		}

		TEST(CardSchemeAdapterTest, MissingTransactionIdNonRetryable)
		{
			CardSchemeAdapter a(std::unique_ptr<CardSchemeVendor>(new CardSchemeVendor()));

			ConnectorRequest req = make_request("file_chargeback");
			req.params["reason_code"] = "FRAUD";

			ConnectorResult r = a.invoke(req);
			EXPECT_FALSE(r.success);
			EXPECT_FALSE(r.retryable);
			EXPECT_NE(r.message.find("transaction_id"), std::string::npos);
		}

		TEST(CardSchemeAdapterTest, MissingReasonCodeNonRetryable)
		{
			CardSchemeAdapter a(std::unique_ptr<CardSchemeVendor>(new CardSchemeVendor()));

			ConnectorRequest req = make_request("file_chargeback");
			req.params["transaction_id"] = "txn-42";

			ConnectorResult r = a.invoke(req);
			EXPECT_FALSE(r.success);
			EXPECT_FALSE(r.retryable);
		}

		TEST(CardSchemeAdapterTest, UnknownOperationNonRetryable)
		{
			CardSchemeAdapter a(std::unique_ptr<CardSchemeVendor>(new CardSchemeVendor()));

			ConnectorRequest req = make_request("refund_transaction");
			ConnectorResult r = a.invoke(req);
			EXPECT_FALSE(r.success);
			EXPECT_FALSE(r.retryable);
		}

		TEST(CoreBankingVendorTest, AcceptsPositiveDecimalAmount)
		{
			CoreBankingVendor v;
			EXPECT_NO_THROW(v.postProvisionalCredit("card-1", "0.50"));
		}

		TEST(CoreBankingVendorTest, AcceptsDecimalAmountVariants)
		{
			CoreBankingVendor v;
			EXPECT_NO_THROW(v.postProvisionalCredit("c", "0.01"));
			EXPECT_NO_THROW(v.postProvisionalCredit("c", "100.00"));
			EXPECT_NO_THROW(v.postProvisionalCredit("c", "1.5"));
		}

		TEST(CoreBankingVendorTest, RejectsTrailingGarbageInAmount)
		{
			CoreBankingVendor v;
			EXPECT_THROW(v.postProvisionalCredit("card-1", "100garbage"), std::invalid_argument);
		}

		TEST(CoreBankingVendorTest, RejectsLeadingGarbageInAmount)
		{
			CoreBankingVendor v;
			EXPECT_THROW(v.postProvisionalCredit("card-1", "garbage100"), std::invalid_argument);
		}

		TEST(CoreBankingVendorTest, ReverseRejectsPrefixOnlyLedgerRef)
		{
			CoreBankingVendor v;
			EXPECT_THROW(v.reverse("LEDGER-"), std::invalid_argument);
		}

		TEST(CoreBankingVendorTest, ReverseRejectsNonNumericLedgerRef)
		{
			CoreBankingVendor v;
			EXPECT_THROW(v.reverse("LEDGER-abc"), std::invalid_argument);
		}

		TEST(CoreBankingVendorTest, ReverseRejectsUnissuedLedgerRef)
		{
			CoreBankingVendor v;
			v.postProvisionalCredit("c", "100");   // counter = 1
			v.postProvisionalCredit("c", "200");   // counter = 2
			EXPECT_THROW(v.reverse("LEDGER-999"), std::invalid_argument);
		}

		TEST(CoreBankingVendorTest, ReverseAcceptsIssuedLedgerRef)
		{
			CoreBankingVendor v;
			std::string ref = v.postProvisionalCredit("c", "100");
			EXPECT_NO_THROW(v.reverse(ref));
		}

		TEST(CoreBankingVendorTest, FinaliseRejectsPrefixOnlyLedgerRef)
		{
			CoreBankingVendor v;
			EXPECT_THROW(v.finalise("LEDGER-"), std::invalid_argument);
		}

		TEST(CoreBankingVendorTest, FinaliseRejectsUnissuedLedgerRef)
		{
			CoreBankingVendor v;
			v.postProvisionalCredit("c", "100");   // counter = 1
			EXPECT_THROW(v.finalise("LEDGER-999"), std::invalid_argument);
		}
	} // namespace banking
} // namespace tividy