/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 27 October 2026

test_industry_telecom.cpp
*/

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "engine/core/Clock.h"
#include "engine/core/Errors.h"
#include "engine/core/Participant.h"
#include "engine/events/Observer.h"
#include "engine/items/WorkContext.h"
#include "industries/telecom/TelecomParts.h"

namespace tividy
{
	namespace telecom
	{
		namespace
		{
			// connector double: replies with whatever the test scripted for an operation and remembers every request it received
			class FakeConnector : public ExternalConnector
			{
				public:
					explicit FakeConnector(const std::string& connector_name) :
						name_ref(connector_name) { }

					void willReturn(const std::string& operation, const ConnectorResult& result)
					{
						replies[operation] = result;
					}

					std::string name() const override
					{
						return name_ref;
					}

					ConnectorResult invoke(const ConnectorRequest& request) override
					{
						calls.push_back(request);

						std::map<std::string, ConnectorResult>::const_iterator it = replies.find(request.operation);
						if (it == replies.end()) return ConnectorResult::failure("unknown operation: " + request.operation, false);
						return it->second;
					}

					std::vector<ConnectorRequest> calls;

				private:
					std::string name_ref;
					std::map<std::string, ConnectorResult> replies;
			};

			// everything a bound task needs, with fakes for both telecom connectors
			struct Fixture
			{
				WorkflowData data;
				EventPublisher events;
				SystemClock clock;
				ConnectorRegistry connectors;
				ParticipantDirectory directory;
				WorkContext context;
				FakeConnector* bureau;
				FakeConnector* network;
				Participant actor;
				Outcome outcome;

				Fixture() :
					context("inst-1", data, events, clock, connectors, directory), bureau(0), network(0),
					actor("sys-1", "System", std::set<std::string>()), outcome(Outcome::accepted("", ""))
				{
					std::unique_ptr<FakeConnector> bureau_fake(new FakeConnector("credit-bureau"));
					bureau = bureau_fake.get();
					connectors.registerConnector(std::move(bureau_fake));

					std::unique_ptr<FakeConnector> network_fake(new FakeConnector("network-provisioning"));
					network = network_fake.get();
					connectors.registerConnector(std::move(network_fake));
				}
			};

			// performWork and the hooks are protected, this makes them callable so a test can drive one step at a time
			template <class T>
			class Exposed : public T
			{
				public:
					template <class... Args>
					explicit Exposed(Args&&... args) :
						T(std::forward<Args>(args)...) { }

					using T::performWork;
					using T::performsIrreversibleWork;
			};

			ConnectorResult scoreReply(const std::string& score, const std::string& band)
			{
				std::map<std::string, std::string> payload;
				payload["score"] = score;
				payload["band"] = band;
				return ConnectorResult::ok("", payload);
			}

			template <class E, class F>
			std::string messageOf(F action)
			{
				try
				{
					action();
				}
				catch (const E& error)
				{
					return error.what();
				}
				return "<no exception>";
			}

			struct Failure
			{
				bool thrown;
				std::string message;
				bool retryable;
			};

			template <class F>
			Failure failureOf(F action)
			{
				Failure f;
				f.thrown = false;
				f.retryable = false;
				try
				{
					action();
				}
				catch (const ExternalFailure& error)
				{
					f.thrown = true;
					f.message = error.what();
					f.retryable = error.retryable();
				}
				return f;
			}

			WorkItemSpec spec(const std::string& id)
			{
				return WorkItemSpec(id, id);
			}
		} // anonymous namespace

		// CreditCheckTask

		TEST(CreditCheckTaskTest, AnApprovedScoreIsStoredAndTheOutcomeStaysAccepted)
		{
			Fixture f;
			f.data.set("customer_id_number", "8001015009087");
			f.bureau->willReturn("score", scoreReply("750", "APPROVED"));
			Exposed<CreditCheckTask> task(spec("credit_check"));
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			ASSERT_EQ(1u, f.bureau->calls.size());
			EXPECT_EQ("score", f.bureau->calls[0].operation);
			EXPECT_EQ("8001015009087", f.bureau->calls[0].params["id_number"]);
			EXPECT_EQ("750", f.data.get("credit_score"));
			EXPECT_EQ(OutcomeKind::Accepted, f.outcome.kind());
		}

		TEST(CreditCheckTaskTest, AReferScoreEscalatesForManualReview)
		{
			Fixture f;
			f.data.set("customer_id_number", "8001015009085");
			f.bureau->willReturn("score", scoreReply("550", "REFER"));
			Exposed<CreditCheckTask> task(spec("credit_check"));
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			EXPECT_EQ("550", f.data.get("credit_score"));
			EXPECT_EQ(OutcomeKind::Escalated, f.outcome.kind());
			EXPECT_EQ("CREDIT_REFER", f.outcome.code());
			EXPECT_EQ("score needs manual review", f.outcome.note());
		}

		TEST(CreditCheckTaskTest, ADeclinedScoreRejectsTheApplication)
		{
			Fixture f;
			f.data.set("customer_id_number", "8001015009080");
			f.bureau->willReturn("score", scoreReply("200", "DECLINED"));
			Exposed<CreditCheckTask> task(spec("credit_check"));
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			EXPECT_EQ("200", f.data.get("credit_score"));
			EXPECT_EQ(OutcomeKind::Rejected, f.outcome.kind());
			EXPECT_EQ("CREDIT_DECLINED", f.outcome.code());
			EXPECT_EQ("credit declined", f.outcome.note());
		}

		TEST(CreditCheckTaskTest, TheOutcomeCodesAreWhatTheRoutingBranchesOn)
		{
			Fixture f;
			f.data.set("customer_id_number", "1");
			Exposed<CreditCheckTask> task(spec("credit_check"));
			task.bind(f.context);

			f.bureau->willReturn("score", scoreReply("550", "REFER"));
			task.performWork(f.actor, f.outcome);
			EXPECT_EQ("CREDIT_REFER", f.outcome.routingKey());

			f.bureau->willReturn("score", scoreReply("200", "DECLINED"));
			task.performWork(f.actor, f.outcome);
			EXPECT_EQ("CREDIT_DECLINED", f.outcome.routingKey());
		}

		TEST(CreditCheckTaskTest, AnUnknownBandIsAPermanentFailureNotASilentApproval)
		{
			Fixture f;
			f.data.set("customer_id_number", "1");
			f.bureau->willReturn("score", scoreReply("700", "GOLD"));
			Exposed<CreditCheckTask> task(spec("credit_check"));
			task.bind(f.context);

			Failure failure = failureOf([&]() { task.performWork(f.actor, f.outcome); });

			EXPECT_TRUE(failure.thrown);
			EXPECT_FALSE(failure.retryable);
			EXPECT_EQ(OutcomeKind::Accepted, f.outcome.kind());
		}

		TEST(CreditCheckTaskTest, AReplyMissingTheBandIsAPermanentFailure)
		{
			Fixture f;
			f.data.set("customer_id_number", "1");
			std::map<std::string, std::string> payload;
			payload["score"] = "700";
			f.bureau->willReturn("score", ConnectorResult::ok("", payload));
			Exposed<CreditCheckTask> task(spec("credit_check"));
			task.bind(f.context);

			Failure failure = failureOf([&]() { task.performWork(f.actor, f.outcome); });

			EXPECT_TRUE(failure.thrown);
			EXPECT_FALSE(failure.retryable);
		}

		TEST(CreditCheckTaskTest, AMissingIdNumberIsAValidationErrorAndNothingIsCalled)
		{
			Fixture f;
			Exposed<CreditCheckTask> task(spec("credit_check"));
			task.bind(f.context);

			EXPECT_EQ("customer_id_number is required",
				messageOf<ValidationError>([&]() { task.performWork(f.actor, f.outcome); }));

			f.data.set("customer_id_number", "");
			EXPECT_EQ("customer_id_number is required",
				messageOf<ValidationError>([&]() { task.performWork(f.actor, f.outcome); }));

			EXPECT_TRUE(f.bureau->calls.empty());
		}

		TEST(CreditCheckTaskTest, ABureauOutageKeepsItsRetryableFlag)
		{
			Fixture f;
			f.data.set("customer_id_number", "1");
			f.bureau->willReturn("score", ConnectorResult::failure("bureau unavailable", true));
			Exposed<CreditCheckTask> task(spec("credit_check"));
			task.bind(f.context);

			Failure failure = failureOf([&]() { task.performWork(f.actor, f.outcome); });

			EXPECT_TRUE(failure.thrown);
			EXPECT_EQ("bureau unavailable", failure.message);
			EXPECT_TRUE(failure.retryable);
			EXPECT_FALSE(f.data.has("credit_score"));
		}

		TEST(CreditCheckTaskTest, APermanentBureauFailureIsNotRetryable)
		{
			Fixture f;
			f.data.set("customer_id_number", "not an id");
			f.bureau->willReturn("score", ConnectorResult::failure("id_number is invalid", false));
			Exposed<CreditCheckTask> task(spec("credit_check"));
			task.bind(f.context);

			Failure failure = failureOf([&]() { task.performWork(f.actor, f.outcome); });

			EXPECT_TRUE(failure.thrown);
			EXPECT_FALSE(failure.retryable);
		}

		TEST(CreditCheckTaskTest, IsNotIrreversibleAndClonesToTheSameType)
		{
			Exposed<CreditCheckTask> task(spec("credit_check"));
			EXPECT_FALSE(task.performsIrreversibleWork()); // asking the bureau changes nothing, so a revert is allowed

			CreditCheckTask original(spec("credit_check"));
			std::unique_ptr<WorkItem> copy = original.clone();
			ASSERT_TRUE(copy.get() != 0);
			EXPECT_TRUE(typeid(*copy) == typeid(CreditCheckTask));
			EXPECT_EQ("credit_check", copy->spec().id());
		}

		// ProvisionServiceTask

		TEST(ProvisionServiceTaskTest, ProvisionsThenActivatesAndMarksTheServiceActive)
		{
			Fixture f;
			f.data.set("customer_ref", "CUST-7");
			f.data.set("product", "fibre-100");
			f.network->willReturn("provision", ConnectorResult::ok());
			f.network->willReturn("activate", ConnectorResult::ok());
			Exposed<ProvisionServiceTask> task(spec("provision"));
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			ASSERT_EQ(2u, f.network->calls.size());
			EXPECT_EQ("provision", f.network->calls[0].operation);
			EXPECT_EQ("CUST-7", f.network->calls[0].params["service_ref"]);
			EXPECT_EQ("fibre-100", f.network->calls[0].params["product"]);
			EXPECT_EQ("activate", f.network->calls[1].operation);
			EXPECT_EQ("CUST-7", f.network->calls[1].params["service_ref"]);
			EXPECT_EQ("ACTIVE", f.data.get("service_status"));
		}

		TEST(ProvisionServiceTaskTest, TheDemoHookTravelsOnTheProvisionRequestOnly)
		{
			Fixture f;
			f.data.set("customer_ref", "CUST-7");
			f.data.set("product", "fibre-100");
			f.data.set("simulate_failures", "3");
			f.network->willReturn("provision", ConnectorResult::ok());
			f.network->willReturn("activate", ConnectorResult::ok());
			Exposed<ProvisionServiceTask> task(spec("provision"));
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			ASSERT_EQ(2u, f.network->calls.size());
			EXPECT_EQ("3", f.network->calls[0].params["simulate_failures"]);
			EXPECT_EQ(0u, f.network->calls[1].params.count("simulate_failures"));
		}

		TEST(ProvisionServiceTaskTest, WithoutTheDemoHookTheProvisionRequestAsksForZeroFailures)
		{
			Fixture f;
			f.data.set("customer_ref", "CUST-7");
			f.data.set("product", "fibre-100");
			f.network->willReturn("provision", ConnectorResult::ok());
			f.network->willReturn("activate", ConnectorResult::ok());
			Exposed<ProvisionServiceTask> task(spec("provision"));
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			EXPECT_EQ("0", f.network->calls[0].params["simulate_failures"]);
		}

		TEST(ProvisionServiceTaskTest, AFailedProvisionNeverReachesActivation)
		{
			Fixture f;
			f.data.set("customer_ref", "CUST-7");
			f.data.set("product", "fibre-100");
			f.network->willReturn("provision", ConnectorResult::failure("OSS unavailable", true));
			f.network->willReturn("activate", ConnectorResult::ok());
			Exposed<ProvisionServiceTask> task(spec("provision"));
			task.bind(f.context);

			Failure failure = failureOf([&]() { task.performWork(f.actor, f.outcome); });

			EXPECT_TRUE(failure.thrown);
			EXPECT_EQ("OSS unavailable", failure.message);
			EXPECT_TRUE(failure.retryable);
			ASSERT_EQ(1u, f.network->calls.size());
			EXPECT_EQ("provision", f.network->calls[0].operation);
			EXPECT_FALSE(f.data.has("service_status"));
		}

		TEST(ProvisionServiceTaskTest, AFailedActivationFailsTheTaskAndTheServiceIsNotMarkedActive)
		{
			Fixture f;
			f.data.set("customer_ref", "CUST-7");
			f.data.set("product", "fibre-100");
			f.network->willReturn("provision", ConnectorResult::ok());
			f.network->willReturn("activate", ConnectorResult::failure("OSS unavailable", true));
			Exposed<ProvisionServiceTask> task(spec("provision"));
			task.bind(f.context);

			Failure failure = failureOf([&]() { task.performWork(f.actor, f.outcome); });

			EXPECT_TRUE(failure.thrown);
			EXPECT_TRUE(failure.retryable);
			EXPECT_EQ(2u, f.network->calls.size());
			EXPECT_FALSE(f.data.has("service_status"));
		}

		TEST(ProvisionServiceTaskTest, ARetryAfterAFailedActivationRepeatsBothCalls)
		{
			// the vendor is idempotent, so the retry simply provisions again and then activates
			Fixture f;
			f.data.set("customer_ref", "CUST-7");
			f.data.set("product", "fibre-100");
			f.network->willReturn("provision", ConnectorResult::ok());
			f.network->willReturn("activate", ConnectorResult::failure("OSS unavailable", true));
			Exposed<ProvisionServiceTask> task(spec("provision"));
			task.bind(f.context);
			failureOf([&]() { task.performWork(f.actor, f.outcome); });

			f.network->willReturn("activate", ConnectorResult::ok());
			task.performWork(f.actor, f.outcome);

			ASSERT_EQ(4u, f.network->calls.size());
			EXPECT_EQ("provision", f.network->calls[2].operation);
			EXPECT_EQ("activate", f.network->calls[3].operation);
			EXPECT_EQ("ACTIVE", f.data.get("service_status"));
		}

		TEST(ProvisionServiceTaskTest, MissingCustomerRefOrProductIsAValidationErrorAndNothingIsCalled)
		{
			Fixture f;
			Exposed<ProvisionServiceTask> task(spec("provision"));
			task.bind(f.context);

			f.data.set("product", "fibre-100");
			EXPECT_EQ("customer_ref and product are required",
				messageOf<ValidationError>([&]() { task.performWork(f.actor, f.outcome); }));

			f.data.set("customer_ref", "CUST-7");
			f.data.set("product", "");
			EXPECT_EQ("customer_ref and product are required",
				messageOf<ValidationError>([&]() { task.performWork(f.actor, f.outcome); }));

			EXPECT_TRUE(f.network->calls.empty());
		}

		TEST(ProvisionServiceTaskTest, IsIrreversibleAndClonesToTheSameType)
		{
			Exposed<ProvisionServiceTask> task(spec("provision"));
			EXPECT_TRUE(task.performsIrreversibleWork()); // the customer's service is live once it succeeds

			ProvisionServiceTask original(spec("provision"));
			std::unique_ptr<WorkItem> copy = original.clone();
			ASSERT_TRUE(copy.get() != 0);
			EXPECT_TRUE(typeid(*copy) == typeid(ProvisionServiceTask));
			EXPECT_EQ("provision", copy->spec().id());
		}
	} // namespace telecom
} // namespace tividy