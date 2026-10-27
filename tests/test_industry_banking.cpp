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

test_industry_banking.cpp
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
#include "engine/definition/WorkflowBuilder.h"
#include "engine/events/Observer.h"
#include "engine/facade/IndustryPack.h"
#include "engine/items/WorkContext.h"
#include "engine/items/WorkItemDecorator.h"
#include "industries/banking/BankingPack.h"
#include "industries/banking/BankingParts.h"

namespace tividy
{
	namespace banking
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

			// everything a bound task needs, with fakes for both banking connectors
			struct Fixture
			{
				WorkflowData data;
				EventPublisher events;
				SystemClock clock;
				ConnectorRegistry connectors;
				ParticipantDirectory directory;
				WorkContext context;
				FakeConnector* core;
				FakeConnector* scheme;
				Participant actor;
				Outcome outcome;

				Fixture() :
					context("inst-1", data, events, clock, connectors, directory), core(0), scheme(0),
					actor("sys-1", "System", std::set<std::string>()), outcome(Outcome::accepted("", ""))
				{
					std::unique_ptr<FakeConnector> core_fake(new FakeConnector("core-banking"));
					core = core_fake.get();
					connectors.registerConnector(std::move(core_fake));

					std::unique_ptr<FakeConnector> scheme_fake(new FakeConnector("card-scheme"));
					scheme = scheme_fake.get();
					connectors.registerConnector(std::move(scheme_fake));
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
					using T::validateOutcome;
					using T::onCompleted;
					using T::performsIrreversibleWork;
			};

			ConnectorResult okWith(const std::string& key, const std::string& value)
			{
				std::map<std::string, std::string> payload;
				payload[key] = value;
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

		// IssueProvisionalCreditTask

		TEST(IssueProvisionalCreditTaskTest, PostsTheCreditAndStoresTheLedgerReference)
		{
			Fixture f;
			f.data.set("card_ref", "CARD-1");
			f.data.set("amount", "250.00");
			f.core->willReturn("post_provisional_credit", okWith("ledger_ref", "LEDGER-1"));
			Exposed<IssueProvisionalCreditTask> task(spec("issue"));
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			ASSERT_EQ(1u, f.core->calls.size());
			EXPECT_EQ("post_provisional_credit", f.core->calls[0].operation);
			EXPECT_EQ("CARD-1", f.core->calls[0].params["card_ref"]);
			EXPECT_EQ("250.00", f.core->calls[0].params["amount"]);
			EXPECT_EQ("LEDGER-1", f.data.get("ledger_ref"));
		}

		TEST(IssueProvisionalCreditTaskTest, OnTheFirstAttemptTheDemoHookIsForwardedToTheVendor)
		{
			Fixture f;
			f.data.set("card_ref", "CARD-1");
			f.data.set("amount", "10");
			f.data.set("simulate_failures", "2");
			f.core->willReturn("post_provisional_credit", okWith("ledger_ref", "LEDGER-1"));
			Exposed<IssueProvisionalCreditTask> task(spec("issue"));
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			EXPECT_EQ("2", f.core->calls[0].params["simulate_failures"]);
		}

		TEST(IssueProvisionalCreditTaskTest, WithoutTheDemoHookTheRequestAsksForZeroFailures)
		{
			Fixture f;
			f.data.set("card_ref", "CARD-1");
			f.data.set("amount", "10");
			f.core->willReturn("post_provisional_credit", okWith("ledger_ref", "LEDGER-1"));
			Exposed<IssueProvisionalCreditTask> task(spec("issue"));
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			EXPECT_EQ("0", f.core->calls[0].params["simulate_failures"]);
		}

		TEST(IssueProvisionalCreditTaskTest, MissingCardRefOrAmountIsAValidationErrorAndNothingIsCalled)
		{
			Fixture f;
			Exposed<IssueProvisionalCreditTask> task(spec("issue"));
			task.bind(f.context);

			f.data.set("amount", "10");
			EXPECT_EQ("card_ref and amount are required",
				messageOf<ValidationError>([&]() { task.performWork(f.actor, f.outcome); }));

			f.data.set("card_ref", "CARD-1");
			f.data.set("amount", "");
			EXPECT_EQ("card_ref and amount are required",
				messageOf<ValidationError>([&]() { task.performWork(f.actor, f.outcome); }));

			EXPECT_TRUE(f.core->calls.empty());
		}

		TEST(IssueProvisionalCreditTaskTest, ARetryableConnectorFailureBecomesARetryableExternalFailure)
		{
			Fixture f;
			f.data.set("card_ref", "CARD-1");
			f.data.set("amount", "10");
			f.core->willReturn("post_provisional_credit", ConnectorResult::failure("core banking unavailable", true));
			Exposed<IssueProvisionalCreditTask> task(spec("issue"));
			task.bind(f.context);

			Failure failure = failureOf([&]() { task.performWork(f.actor, f.outcome); });

			EXPECT_TRUE(failure.thrown);
			EXPECT_EQ("core banking unavailable", failure.message);
			EXPECT_TRUE(failure.retryable);
			EXPECT_FALSE(f.data.has("ledger_ref"));
		}

		TEST(IssueProvisionalCreditTaskTest, APermanentConnectorFailureIsNotRetryable)
		{
			Fixture f;
			f.data.set("card_ref", "CARD-1");
			f.data.set("amount", "-5");
			f.core->willReturn("post_provisional_credit", ConnectorResult::failure("amount must be positive", false));
			Exposed<IssueProvisionalCreditTask> task(spec("issue"));
			task.bind(f.context);

			Failure failure = failureOf([&]() { task.performWork(f.actor, f.outcome); });

			EXPECT_TRUE(failure.thrown);
			EXPECT_FALSE(failure.retryable);
		}

		TEST(IssueProvisionalCreditTaskTest, ASuccessfulReplyWithoutALedgerRefIsAPermanentFailure)
		{
			Fixture f;
			f.data.set("card_ref", "CARD-1");
			f.data.set("amount", "10");
			f.core->willReturn("post_provisional_credit", ConnectorResult::ok());
			Exposed<IssueProvisionalCreditTask> task(spec("issue"));
			task.bind(f.context);

			Failure failure = failureOf([&]() { task.performWork(f.actor, f.outcome); });

			EXPECT_TRUE(failure.thrown);
			EXPECT_FALSE(failure.retryable);
			EXPECT_FALSE(f.data.has("ledger_ref"));
		}

		TEST(IssueProvisionalCreditTaskTest, IsIrreversibleAndClonesToTheSameType)
		{
			Exposed<IssueProvisionalCreditTask> task(spec("issue"));
			EXPECT_TRUE(task.performsIrreversibleWork());

			IssueProvisionalCreditTask original(spec("issue"));
			std::unique_ptr<WorkItem> copy = original.clone();
			ASSERT_TRUE(copy.get() != 0);
			EXPECT_TRUE(typeid(*copy) == typeid(IssueProvisionalCreditTask));
			EXPECT_EQ("issue", copy->spec().id());
		}

		// FileChargebackTask

		TEST(FileChargebackTaskTest, FilesTheChargebackAndStoresTheSchemeCase)
		{
			Fixture f;
			f.data.set("transaction_id", "TX-9");
			f.data.set("reason_code", "4837");
			f.scheme->willReturn("file_chargeback", okWith("scheme_case", "CASE-TX-9"));
			Exposed<FileChargebackTask> task(spec("chargeback"));
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			ASSERT_EQ(1u, f.scheme->calls.size());
			EXPECT_EQ("file_chargeback", f.scheme->calls[0].operation);
			EXPECT_EQ("TX-9", f.scheme->calls[0].params["transaction_id"]);
			EXPECT_EQ("4837", f.scheme->calls[0].params["reason_code"]);
			EXPECT_EQ("CASE-TX-9", f.data.get("scheme_case"));
		}

		TEST(FileChargebackTaskTest, ForwardsTheDemoHookOnTheFirstAttempt)
		{
			Fixture f;
			f.data.set("transaction_id", "TX-9");
			f.data.set("reason_code", "4837");
			f.data.set("simulate_failures", "1");
			f.scheme->willReturn("file_chargeback", okWith("scheme_case", "CASE-TX-9"));
			Exposed<FileChargebackTask> task(spec("chargeback"));
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			EXPECT_EQ("1", f.scheme->calls[0].params["simulate_failures"]);
		}

		TEST(FileChargebackTaskTest, MissingTransactionIdOrReasonCodeIsAValidationError)
		{
			Fixture f;
			Exposed<FileChargebackTask> task(spec("chargeback"));
			task.bind(f.context);

			f.data.set("reason_code", "4837");
			EXPECT_EQ("transaction_id and reason_code are required",
				messageOf<ValidationError>([&]() { task.performWork(f.actor, f.outcome); }));

			f.data.set("transaction_id", "TX-9");
			f.data.set("reason_code", "");
			EXPECT_EQ("transaction_id and reason_code are required",
				messageOf<ValidationError>([&]() { task.performWork(f.actor, f.outcome); }));

			EXPECT_TRUE(f.scheme->calls.empty());
		}

		TEST(FileChargebackTaskTest, ASchemeFailureKeepsItsRetryableFlag)
		{
			Fixture f;
			f.data.set("transaction_id", "TX-9");
			f.data.set("reason_code", "4837");
			f.scheme->willReturn("file_chargeback", ConnectorResult::failure("scheme unavailable", true));
			Exposed<FileChargebackTask> task(spec("chargeback"));
			task.bind(f.context);

			Failure failure = failureOf([&]() { task.performWork(f.actor, f.outcome); });

			EXPECT_TRUE(failure.thrown);
			EXPECT_EQ("scheme unavailable", failure.message);
			EXPECT_TRUE(failure.retryable);
			EXPECT_FALSE(f.data.has("scheme_case"));
		}

		TEST(FileChargebackTaskTest, IsIrreversible)
		{
			Exposed<FileChargebackTask> task(spec("chargeback"));
			EXPECT_TRUE(task.performsIrreversibleWork());
		}

		// MerchantResponseTask

		TEST(MerchantResponseTaskTest, AcceptsEvidenceWithASummary)
		{
			Exposed<MerchantResponseTask> task(spec("response"));
			EXPECT_NO_THROW(task.validateOutcome(Outcome::accepted("EVIDENCE_SUBMITTED", "signed delivery receipt")));
		}

		TEST(MerchantResponseTaskTest, AcceptsAnAdmissionOfLiabilityWithoutANote)
		{
			Exposed<MerchantResponseTask> task(spec("response"));
			EXPECT_NO_THROW(task.validateOutcome(Outcome::accepted("ACCEPT_LIABILITY", "")));
		}

		TEST(MerchantResponseTaskTest, RejectsEvidenceWithoutASummary)
		{
			Exposed<MerchantResponseTask> task(spec("response"));
			EXPECT_EQ("evidence summary required",
				messageOf<ValidationError>([&]() { task.validateOutcome(Outcome::accepted("EVIDENCE_SUBMITTED", "")); }));
		}

		TEST(MerchantResponseTaskTest, RejectsAnyOtherCode)
		{
			Exposed<MerchantResponseTask> task(spec("response"));
			EXPECT_EQ("choose EVIDENCE_SUBMITTED or ACCEPT_LIABILITY",
				messageOf<ValidationError>([&]() { task.validateOutcome(Outcome::accepted("MAYBE", "a note")); }));
			EXPECT_EQ("choose EVIDENCE_SUBMITTED or ACCEPT_LIABILITY",
				messageOf<ValidationError>([&]() { task.validateOutcome(Outcome::accepted("", "a note")); }));
			// NO_RESPONSE is set by the system when the deadline lapses, a person can never choose it
			EXPECT_EQ("choose EVIDENCE_SUBMITTED or ACCEPT_LIABILITY",
				messageOf<ValidationError>([&]() { task.validateOutcome(Outcome::rejected("NO_RESPONSE", "x")); }));
		}

		TEST(MerchantResponseTaskTest, PerformWorkLeavesTheOutcomeUnchanged)
		{
			Fixture f;
			Exposed<MerchantResponseTask> task(spec("response"));
			task.bind(f.context);
			Outcome chosen = Outcome::accepted("EVIDENCE_SUBMITTED", "receipt");

			task.performWork(f.actor, chosen);

			EXPECT_EQ(OutcomeKind::Accepted, chosen.kind());
			EXPECT_EQ("EVIDENCE_SUBMITTED", chosen.code());
			EXPECT_EQ("receipt", chosen.note());
		}

		TEST(MerchantResponseTaskTest, EvidenceIsStoredInTheInstanceData)
		{
			Fixture f;
			Exposed<MerchantResponseTask> task(spec("response"));
			task.bind(f.context);

			task.onCompleted(Outcome::accepted("EVIDENCE_SUBMITTED", "signed delivery receipt"));

			EXPECT_EQ("signed delivery receipt", f.data.get("merchant_evidence"));
		}

		TEST(MerchantResponseTaskTest, AdmittingLiabilityStoresNoEvidence)
		{
			Fixture f;
			Exposed<MerchantResponseTask> task(spec("response"));
			task.bind(f.context);

			task.onCompleted(Outcome::accepted("ACCEPT_LIABILITY", "we concede"));

			EXPECT_FALSE(f.data.has("merchant_evidence"));
		}

		TEST(MerchantResponseTaskTest, IsNotIrreversible)
		{
			Exposed<MerchantResponseTask> task(spec("response"));
			EXPECT_FALSE(task.performsIrreversibleWork());
		}

		// DecisionTask

		TEST(DecisionTaskTest, AcceptsEitherDecisionWithReasoning)
		{
			Exposed<DecisionTask> task(spec("decision"));
			EXPECT_NO_THROW(task.validateOutcome(Outcome::accepted("CUSTOMER_WINS", "the receipt is unsigned")));
			EXPECT_NO_THROW(task.validateOutcome(Outcome::accepted("MERCHANT_WINS", "the receipt is signed")));
		}

		TEST(DecisionTaskTest, RejectsAnythingElseAsTheDecision)
		{
			Exposed<DecisionTask> task(spec("decision"));
			EXPECT_EQ("decision must be CUSTOMER_WINS or MERCHANT_WINS",
				messageOf<ValidationError>([&]() { task.validateOutcome(Outcome::accepted("DRAW", "reasoning")); }));
			EXPECT_EQ("decision must be CUSTOMER_WINS or MERCHANT_WINS",
				messageOf<ValidationError>([&]() { task.validateOutcome(Outcome::accepted("", "reasoning")); }));
		}

		TEST(DecisionTaskTest, RequiresReasoning)
		{
			Exposed<DecisionTask> task(spec("decision"));
			EXPECT_EQ("reasoning required",
				messageOf<ValidationError>([&]() { task.validateOutcome(Outcome::accepted("CUSTOMER_WINS", "")); }));
		}

		TEST(DecisionTaskTest, TheCodeIsCheckedBeforeTheReasoning)
		{
			Exposed<DecisionTask> task(spec("decision"));
			EXPECT_EQ("decision must be CUSTOMER_WINS or MERCHANT_WINS",
				messageOf<ValidationError>([&]() { task.validateOutcome(Outcome::accepted("DRAW", "")); }));
		}

		TEST(DecisionTaskTest, TheDecisionAndReasoningAreStoredInTheInstanceData)
		{
			Fixture f;
			Exposed<DecisionTask> task(spec("decision"));
			task.bind(f.context);

			task.onCompleted(Outcome::accepted("MERCHANT_WINS", "evidence is conclusive"));

			EXPECT_EQ("MERCHANT_WINS", f.data.get("decision"));
			EXPECT_EQ("evidence is conclusive", f.data.get("decision_reasoning"));
		}

		TEST(DecisionTaskTest, AnAppealDecisionOverwritesTheFirstOne)
		{
			Fixture f;
			Exposed<DecisionTask> first(spec("decision"));
			Exposed<DecisionTask> appeal(spec("appeal_review"));
			first.bind(f.context);
			appeal.bind(f.context);

			first.onCompleted(Outcome::accepted("MERCHANT_WINS", "first reasoning"));
			appeal.onCompleted(Outcome::accepted("CUSTOMER_WINS", "overturned"));

			EXPECT_EQ("CUSTOMER_WINS", f.data.get("decision"));
			EXPECT_EQ("overturned", f.data.get("decision_reasoning"));
		}

		// SettleDisputeTask

		TEST(SettleDisputeTaskTest, ACustomerWinMakesTheProvisionalCreditPermanent)
		{
			Fixture f;
			f.data.set("ledger_ref", "LEDGER-1");
			f.core->willReturn("finalise_credit", ConnectorResult::ok());
			Exposed<SettleDisputeTask> task(spec("settle_for_customer"), Winner::Customer);
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			ASSERT_EQ(1u, f.core->calls.size());
			EXPECT_EQ("finalise_credit", f.core->calls[0].operation);
			EXPECT_EQ("LEDGER-1", f.core->calls[0].params["ledger_ref"]);
			EXPECT_EQ("CUSTOMER", f.data.get("settlement"));
		}

		TEST(SettleDisputeTaskTest, AMerchantWinReversesTheProvisionalCredit)
		{
			Fixture f;
			f.data.set("ledger_ref", "LEDGER-1");
			f.core->willReturn("reverse_credit", ConnectorResult::ok());
			Exposed<SettleDisputeTask> task(spec("settle_for_merchant"), Winner::Merchant);
			task.bind(f.context);

			task.performWork(f.actor, f.outcome);

			ASSERT_EQ(1u, f.core->calls.size());
			EXPECT_EQ("reverse_credit", f.core->calls[0].operation);
			EXPECT_EQ("LEDGER-1", f.core->calls[0].params["ledger_ref"]);
			EXPECT_EQ("MERCHANT", f.data.get("settlement"));
		}

		TEST(SettleDisputeTaskTest, WithoutAProvisionalCreditOnFileNothingIsCalled)
		{
			Fixture f;
			Exposed<SettleDisputeTask> task(spec("settle_for_customer"), Winner::Customer);
			task.bind(f.context);

			EXPECT_EQ("no provisional credit on file",
				messageOf<ValidationError>([&]() { task.performWork(f.actor, f.outcome); }));
			EXPECT_TRUE(f.core->calls.empty());
			EXPECT_FALSE(f.data.has("settlement"));
		}

		TEST(SettleDisputeTaskTest, ALedgerFailureKeepsItsRetryableFlagAndRecordsNoSettlement)
		{
			Fixture f;
			f.data.set("ledger_ref", "LEDGER-1");
			f.core->willReturn("finalise_credit", ConnectorResult::failure("core banking unavailable", true));
			Exposed<SettleDisputeTask> task(spec("settle_for_customer"), Winner::Customer);
			task.bind(f.context);

			Failure failure = failureOf([&]() { task.performWork(f.actor, f.outcome); });

			EXPECT_TRUE(failure.thrown);
			EXPECT_EQ("core banking unavailable", failure.message);
			EXPECT_TRUE(failure.retryable);
			EXPECT_FALSE(f.data.has("settlement"));
		}

		TEST(SettleDisputeTaskTest, IsIrreversible)
		{
			Exposed<SettleDisputeTask> task(spec("settle_for_customer"), Winner::Customer);
			EXPECT_TRUE(task.performsIrreversibleWork());
		}

		TEST(SettleDisputeTaskTest, TheWinnerSurvivesCopying)
		{
			// the winner is a member the Prototype copy has to carry over, a copy that forgot it would settle for the wrong party
			Fixture f;
			f.data.set("ledger_ref", "LEDGER-1");
			f.core->willReturn("reverse_credit", ConnectorResult::ok());
			f.core->willReturn("finalise_credit", ConnectorResult::ok());

			Exposed<SettleDisputeTask> merchant(spec("settle_for_merchant"), Winner::Merchant);
			Exposed<SettleDisputeTask> copy(merchant);
			copy.bind(f.context);

			copy.performWork(f.actor, f.outcome);

			ASSERT_EQ(1u, f.core->calls.size());
			EXPECT_EQ("reverse_credit", f.core->calls[0].operation);
			EXPECT_EQ("MERCHANT", f.data.get("settlement"));
		}

		TEST(SettleDisputeTaskTest, CloneProducesAnIndependentTaskOfTheSameType)
		{
			SettleDisputeTask original(spec("settle_for_merchant"), Winner::Merchant);
			std::unique_ptr<WorkItem> copy = original.clone();

			ASSERT_TRUE(copy.get() != 0);
			EXPECT_TRUE(copy.get() != &original);
			EXPECT_TRUE(typeid(*copy) == typeid(SettleDisputeTask));
			EXPECT_EQ("settle_for_merchant", copy->spec().id());
		}

		// DisputeDirector

		namespace
		{
			// builder double: remembers every call so a test can read the structure the Director described
			class RecordingBuilder : public WorkflowBuilder
			{
				public:
					RecordingBuilder() : resets(0), ends(0) { }

					void reset(const std::string& name, unsigned version, const std::string& description) override
					{
						++resets;
						recorded_name = name;
						recorded_version = version;
						recorded_description = description;
					}

					void beginStage(const WorkItemSpec& stage_spec) override
					{
						stage_order.push_back(stage_spec.id());
						stage_specs.push_back(stage_spec);
						tasks_in[stage_spec.id()] = std::vector<Id>();
						current = stage_spec.id();
					}

					void endStage() override
					{
						++ends;
					}

					void addTask(std::unique_ptr<WorkItem> task) override
					{
						const Id id = task->spec().id();
						task_order.push_back(id);
						tasks_in[current].push_back(id);
						items[id] = std::move(task);
					}

					void setAssignment(std::shared_ptr<const AssignmentStrategy> strategy) override
					{
						assignment = strategy;
					}

					void setRouting(const Id& source_item_id, std::shared_ptr<const RoutingStrategy> strategy) override
					{
						routing[source_item_id] = strategy;
					}

					void setEscalationChain(std::shared_ptr<const EscalationHandler> head) override
					{
						escalation = head;
					}

					void addCancelSignal(const std::string& signal_name) override
					{
						cancel_signals.push_back(signal_name);
					}

					std::unique_ptr<WorkflowDefinition> getResult() override
					{
						return std::unique_ptr<WorkflowDefinition>();
					}

					const WorkItemSpec& specOf(const Id& id) const
					{
						return items.at(id)->spec();
					}

					unsigned resets;
					unsigned ends;
					std::string recorded_name;
					unsigned recorded_version;
					std::string recorded_description;
					std::vector<Id> stage_order;
					std::vector<WorkItemSpec> stage_specs;
					std::vector<Id> task_order;
					std::map<Id, std::vector<Id> > tasks_in;
					std::map<Id, std::unique_ptr<WorkItem> > items;
					std::map<Id, std::shared_ptr<const RoutingStrategy> > routing;
					std::shared_ptr<const AssignmentStrategy> assignment;
					std::shared_ptr<const EscalationHandler> escalation;
					std::vector<std::string> cancel_signals;

				private:
					Id current;
			};

			// the Director only calls two factory methods on its pack, so a tiny pack with recognisable products is enough
			class TestPack : public IndustryPack
			{
				public:
					TestPack() :
						assignment(new RoleBasedAssignment),
						escalation(new RoleEscalationHandler("senior-analyst", 1)) { }

					std::string key() const override { return "test-bank"; }
					std::string displayName() const override { return "Test bank"; }
					std::unique_ptr<WorkflowDirector> createDirector() const override { return std::unique_ptr<WorkflowDirector>(); }
					std::shared_ptr<const AssignmentStrategy> createAssignmentStrategy() const override { return assignment; }
					std::shared_ptr<const EscalationHandler> createEscalationChain() const override { return escalation; }

					std::shared_ptr<const AssignmentStrategy> assignment;
					std::shared_ptr<const EscalationHandler> escalation;
			};

			std::set<Id> idSet(const std::vector<Id>& ids)
			{
				return std::set<Id>(ids.begin(), ids.end());
			}

			// the nine items that come after intake, all skipped together when the dispute is ineligible
			std::set<Id> allLater()
			{
				const char* ids[] = {"issue_provisional_credit", "file_chargeback", "merchant_response", "analyst_decision",
					"merchant_appeal", "customer_appeal", "appeal_review", "settle_for_customer", "settle_for_merchant"};
				return std::set<Id>(ids, ids + 9);
			}

			struct Branch
			{
				std::set<Id> activated;
				std::set<Id> skipped;
			};

			typedef std::vector<std::pair<Id, std::string> > Steps;

			Steps steps(const Id& a, const std::string& a_code)
			{
				Steps s;
				s.push_back(std::make_pair(a, a_code));
				return s;
			}

			Steps then(Steps s, const Id& source, const std::string& code)
			{
				s.push_back(std::make_pair(source, code));
				return s;
			}

			struct DirectorFixture
			{
				TestPack pack;
				DisputeDirector director;
				RecordingBuilder builder;

				DirectorFixture() :
					director(pack)
				{
					director.construct(builder);
				}

				RoutingDecision decide(const Id& source, const std::string& code)
				{
					std::map<Id, std::shared_ptr<const RoutingStrategy> >::const_iterator it = builder.routing.find(source);
					EXPECT_TRUE(it != builder.routing.end()) << "no routing registered for " << source;
					if (it == builder.routing.end()) return RoutingDecision();

					WorkflowData data;
					return it->second->decide(*builder.items.begin()->second, Outcome::accepted(code, ""), data);
				}

				// follows one path through the routing tables and collects everything activated or skipped along it
				Branch walk(const Steps& path)
				{
					Branch branch;
					for (Steps::const_iterator step = path.begin(); step != path.end(); ++step)
					{
						RoutingDecision d = decide(step->first, step->second);
						branch.activated.insert(d.activate.begin(), d.activate.end());
						branch.skipped.insert(d.skip.begin(), d.skip.end());
					}
					return branch;
				}
			};
		} // anonymous namespace

		TEST(DisputeDirectorTest, DescribesTheTransactionDisputeWorkflow)
		{
			TestPack pack;
			DisputeDirector director(pack);

			EXPECT_EQ("transaction-dispute", director.workflowName());
			EXPECT_EQ(1u, director.version());
			EXPECT_FALSE(director.description().empty());
		}

		TEST(DisputeDirectorTest, ConstructNeverResetsTheBuilderOrFetchesTheResult)
		{
			DirectorFixture f;
			EXPECT_EQ(0u, f.builder.resets); // reset and getResult belong to WorkflowDirector::build
		}

		TEST(DisputeDirectorTest, BuildsTheFourStagesInOrder)
		{
			DirectorFixture f;

			const char* expected[] = {"intake", "provisional", "contest", "settlement"};
			EXPECT_EQ(std::vector<Id>(expected, expected + 4), f.builder.stage_order);
			EXPECT_EQ(4u, f.builder.ends); // every stage that was opened was closed
		}

		TEST(DisputeDirectorTest, PutsEveryItemInItsStage)
		{
			DirectorFixture f;

			const char* intake[] = {"lodge_dispute", "assess_eligibility", "close_ineligible"};
			const char* provisional[] = {"issue_provisional_credit", "file_chargeback"};
			const char* contest[] = {"merchant_response", "analyst_decision", "merchant_appeal", "customer_appeal", "appeal_review"};
			const char* settlement[] = {"settle_for_customer", "settle_for_merchant"};

			EXPECT_EQ(std::vector<Id>(intake, intake + 3), f.builder.tasks_in["intake"]);
			EXPECT_EQ(std::vector<Id>(provisional, provisional + 2), f.builder.tasks_in["provisional"]);
			EXPECT_EQ(std::vector<Id>(contest, contest + 5), f.builder.tasks_in["contest"]);
			EXPECT_EQ(std::vector<Id>(settlement, settlement + 2), f.builder.tasks_in["settlement"]);
			EXPECT_EQ(12u, f.builder.task_order.size());
		}

		TEST(DisputeDirectorTest, TheProvisionalAndContestStagesWaitForTheStepBeforeThem)
		{
			DirectorFixture f;

			const std::vector<Id>& provisional = f.builder.stage_specs[1].dependencies();
			ASSERT_EQ(2u, provisional.size());
			EXPECT_EQ((std::set<Id>{"assess_eligibility", "close_ineligible"}), idSet(provisional));

			const std::vector<Id>& contest = f.builder.stage_specs[2].dependencies();
			ASSERT_EQ(1u, contest.size());
			EXPECT_EQ("file_chargeback", contest[0]);

			EXPECT_TRUE(f.builder.stage_specs[0].dependencies().empty());
			EXPECT_TRUE(f.builder.stage_specs[3].dependencies().empty());
		}

		TEST(DisputeDirectorTest, ItemDependenciesFollowTheDocumentedOrder)
		{
			DirectorFixture f;

			EXPECT_TRUE(f.builder.specOf("lodge_dispute").dependencies().empty());
			EXPECT_EQ(std::vector<Id>(1, "lodge_dispute"), f.builder.specOf("assess_eligibility").dependencies());
			EXPECT_EQ(std::vector<Id>(1, "assess_eligibility"), f.builder.specOf("close_ineligible").dependencies());
			EXPECT_TRUE(f.builder.specOf("issue_provisional_credit").dependencies().empty());
			EXPECT_EQ(std::vector<Id>(1, "issue_provisional_credit"), f.builder.specOf("file_chargeback").dependencies());
			EXPECT_TRUE(f.builder.specOf("merchant_response").dependencies().empty());
			EXPECT_EQ(std::vector<Id>(1, "merchant_response"), f.builder.specOf("analyst_decision").dependencies());
			EXPECT_EQ(std::vector<Id>(1, "analyst_decision"), f.builder.specOf("merchant_appeal").dependencies());
			EXPECT_EQ(std::vector<Id>(1, "analyst_decision"), f.builder.specOf("customer_appeal").dependencies());

			const char* appeals[] = {"merchant_appeal", "customer_appeal"};
			EXPECT_EQ(idSet(std::vector<Id>(appeals, appeals + 2)), idSet(f.builder.specOf("appeal_review").dependencies()));

			const char* all_three[] = {"merchant_appeal", "customer_appeal", "appeal_review"};
			EXPECT_EQ(idSet(std::vector<Id>(all_three, all_three + 3)), idSet(f.builder.specOf("settle_for_customer").dependencies()));
			EXPECT_EQ(idSet(std::vector<Id>(all_three, all_three + 3)), idSet(f.builder.specOf("settle_for_merchant").dependencies()));
		}

		TEST(DisputeDirectorTest, EachItemNeedsTheRoleTheDocumentationGivesIt)
		{
			DirectorFixture f;

			EXPECT_EQ("customer", f.builder.specOf("lodge_dispute").requiredRole());
			EXPECT_EQ("dispute-agent", f.builder.specOf("assess_eligibility").requiredRole());
			EXPECT_EQ("dispute-agent", f.builder.specOf("close_ineligible").requiredRole());
			EXPECT_EQ("dispute-agent", f.builder.specOf("issue_provisional_credit").requiredRole());
			EXPECT_EQ("system", f.builder.specOf("file_chargeback").requiredRole());
			EXPECT_EQ("merchant-rep", f.builder.specOf("merchant_response").requiredRole());
			EXPECT_EQ("dispute-analyst", f.builder.specOf("analyst_decision").requiredRole());
			EXPECT_EQ("merchant-rep", f.builder.specOf("merchant_appeal").requiredRole());
			EXPECT_EQ("customer", f.builder.specOf("customer_appeal").requiredRole());
			EXPECT_EQ("senior-analyst", f.builder.specOf("appeal_review").requiredRole());
			EXPECT_EQ("system", f.builder.specOf("settle_for_customer").requiredRole());
			EXPECT_EQ("system", f.builder.specOf("settle_for_merchant").requiredRole());
		}

		TEST(DisputeDirectorTest, OnlyTheSystemItemsAreAutomaticAndTheyHaveNoDeadline)
		{
			DirectorFixture f;

			const char* automatic[] = {"file_chargeback", "settle_for_customer", "settle_for_merchant"};
			for (unsigned i = 0; i < 3; ++i)
			{
				EXPECT_TRUE(f.builder.specOf(automatic[i]).isAutomatic()) << automatic[i];
				EXPECT_EQ(0, f.builder.specOf(automatic[i]).dueAfterSeconds()) << automatic[i];
			}

			for (std::vector<Id>::const_iterator id = f.builder.task_order.begin(); id != f.builder.task_order.end(); ++id)
			{
				if (*id == "file_chargeback" || *id == "settle_for_customer" || *id == "settle_for_merchant") continue;
				EXPECT_FALSE(f.builder.specOf(*id).isAutomatic()) << *id;
			}
		}

		TEST(DisputeDirectorTest, DeadlinesAndLapseBehaviourMatchTheDocumentation)
		{
			DirectorFixture f;
			const long day = 24 * 60 * 60;

			// escalates on lapse: no timeout code
			EXPECT_EQ(10 * day, f.builder.specOf("issue_provisional_credit").dueAfterSeconds());
			EXPECT_EQ("", f.builder.specOf("issue_provisional_credit").timeoutCode());
			EXPECT_EQ(30 * day, f.builder.specOf("analyst_decision").dueAfterSeconds());
			EXPECT_EQ("", f.builder.specOf("analyst_decision").timeoutCode());

			// a default judgment: the item completes with a code the routing can branch on
			EXPECT_EQ(14 * day, f.builder.specOf("merchant_response").dueAfterSeconds());
			EXPECT_EQ("NO_RESPONSE", f.builder.specOf("merchant_response").timeoutCode());
			EXPECT_EQ(7 * day, f.builder.specOf("merchant_appeal").dueAfterSeconds());
			EXPECT_EQ("NO_APPEAL", f.builder.specOf("merchant_appeal").timeoutCode());
			EXPECT_EQ(7 * day, f.builder.specOf("customer_appeal").dueAfterSeconds());
			EXPECT_EQ("NO_APPEAL", f.builder.specOf("customer_appeal").timeoutCode());

			// no deadline at all
			EXPECT_EQ(0, f.builder.specOf("lodge_dispute").dueAfterSeconds());
			EXPECT_EQ(0, f.builder.specOf("assess_eligibility").dueAfterSeconds());
			EXPECT_EQ(0, f.builder.specOf("appeal_review").dueAfterSeconds());
		}

		TEST(DisputeDirectorTest, OnlyTheAnalystDecisionNeedsAFourEyesReview)
		{
			DirectorFixture f;

			EXPECT_TRUE(f.builder.specOf("analyst_decision").requiresReview());
			EXPECT_EQ("senior-analyst", f.builder.specOf("analyst_decision").reviewerRole());

			for (std::vector<Id>::const_iterator id = f.builder.task_order.begin(); id != f.builder.task_order.end(); ++id)
			{
				if (*id == "analyst_decision") continue;
				EXPECT_FALSE(f.builder.specOf(*id).requiresReview()) << *id;
			}
		}

		TEST(DisputeDirectorTest, HumanItemsAreSecuredAndAutomaticOnesAreNot)
		{
			DirectorFixture f;

			const char* human[] = {"lodge_dispute", "assess_eligibility", "close_ineligible", "issue_provisional_credit",
				"merchant_response", "analyst_decision", "merchant_appeal", "customer_appeal", "appeal_review"};
			for (unsigned i = 0; i < 9; ++i)
				EXPECT_TRUE(dynamic_cast<SecureWorkItem*>(f.builder.items[human[i]].get()) != 0) << human[i];

			const char* automatic[] = {"file_chargeback", "settle_for_customer", "settle_for_merchant"};
			for (unsigned i = 0; i < 3; ++i)
				EXPECT_TRUE(dynamic_cast<SecureWorkItem*>(f.builder.items[automatic[i]].get()) == 0) << automatic[i];
		}

		TEST(DisputeDirectorTest, UsesThePacksPoliciesAndTheWithdrawalSignal)
		{
			DirectorFixture f;

			EXPECT_EQ(f.pack.assignment, f.builder.assignment);
			EXPECT_EQ(f.pack.escalation, f.builder.escalation);
			ASSERT_EQ(1u, f.builder.cancel_signals.size());
			EXPECT_EQ("customer-withdrew", f.builder.cancel_signals[0]);
		}

		TEST(DisputeDirectorTest, RegistersRoutingForExactlyTheSixDecisionPoints)
		{
			DirectorFixture f;

			std::set<Id> sources;
			for (std::map<Id, std::shared_ptr<const RoutingStrategy> >::const_iterator it = f.builder.routing.begin();
				it != f.builder.routing.end(); ++it)
				sources.insert(it->first);

			const char* expected[] = {"assess_eligibility", "merchant_response", "analyst_decision", "merchant_appeal",
				"customer_appeal", "appeal_review"};
			EXPECT_EQ(std::set<Id>(expected, expected + 6), sources);
		}

		TEST(DisputeDirectorTest, EveryRoutingSourceIsAnItemTheDirectorAdded)
		{
			DirectorFixture f;

			for (std::map<Id, std::shared_ptr<const RoutingStrategy> >::const_iterator it = f.builder.routing.begin();
				it != f.builder.routing.end(); ++it)
				EXPECT_EQ(1u, f.builder.items.count(it->first)) << it->first;
		}

		TEST(DisputeDirectorTest, AnIneligibleDisputeClosesAndSkipsEverythingAfterIntake)
		{
			DirectorFixture f;

			RoutingDecision d = f.decide("assess_eligibility", "INELIGIBLE");

			EXPECT_EQ(std::set<Id>{"close_ineligible"}, idSet(d.activate));
			EXPECT_EQ(allLater(), idSet(d.skip));
		}

		TEST(DisputeDirectorTest, AnEligibleDisputeSkipsOnlyTheClosingItem)
		{
			DirectorFixture f;

			RoutingDecision d = f.decide("assess_eligibility", "ELIGIBLE");

			EXPECT_TRUE(d.activate.empty());
			EXPECT_EQ(std::set<Id>{"close_ineligible"}, idSet(d.skip));
		}

		TEST(DisputeDirectorTest, SubmittedEvidenceNeedsNoRoutingBecauseTheAnalystIsReleasedByDependency)
		{
			DirectorFixture f;

			RoutingDecision d = f.decide("merchant_response", "EVIDENCE_SUBMITTED");

			EXPECT_TRUE(d.activate.empty());
			EXPECT_TRUE(d.skip.empty());
		}

		TEST(DisputeDirectorTest, AMerchantWhoConcedesOrStaysSilentLosesByDefault)
		{
			DirectorFixture f;
			const char* skipped[] = {"analyst_decision", "merchant_appeal", "customer_appeal", "appeal_review", "settle_for_merchant"};

			const char* codes[] = {"ACCEPT_LIABILITY", "NO_RESPONSE"};
			for (unsigned i = 0; i < 2; ++i)
			{
				RoutingDecision d = f.decide("merchant_response", codes[i]);
				EXPECT_EQ(std::set<Id>{"settle_for_customer"}, idSet(d.activate)) << codes[i];
				EXPECT_EQ(std::set<Id>(skipped, skipped + 5), idSet(d.skip)) << codes[i];
			}
		}

		TEST(DisputeDirectorTest, OnlyTheLosingPartyGetsToAppeal)
		{
			DirectorFixture f;

			RoutingDecision customer = f.decide("analyst_decision", "CUSTOMER_WINS");
			EXPECT_EQ(std::set<Id>{"merchant_appeal"}, idSet(customer.activate));
			EXPECT_EQ(std::set<Id>{"customer_appeal"}, idSet(customer.skip));

			RoutingDecision merchant = f.decide("analyst_decision", "MERCHANT_WINS");
			EXPECT_EQ(std::set<Id>{"customer_appeal"}, idSet(merchant.activate));
			EXPECT_EQ(std::set<Id>{"merchant_appeal"}, idSet(merchant.skip));
		}

		TEST(DisputeDirectorTest, AMerchantAppealGoesToReviewAnythingElseSettlesForTheCustomer)
		{
			DirectorFixture f;

			RoutingDecision appeal = f.decide("merchant_appeal", "APPEAL");
			EXPECT_EQ(std::set<Id>{"appeal_review"}, idSet(appeal.activate));
			EXPECT_TRUE(appeal.skip.empty());

			// accepting the decision and the 7 day timeout (NO_APPEAL) both fall into the default rule
			const char* codes[] = {"NO_APPEAL", "ACCEPT"};
			for (unsigned i = 0; i < 2; ++i)
			{
				RoutingDecision d = f.decide("merchant_appeal", codes[i]);
				EXPECT_EQ(std::set<Id>{"settle_for_customer"}, idSet(d.activate)) << codes[i];
				EXPECT_EQ((std::set<Id>{"appeal_review", "settle_for_merchant"}), idSet(d.skip)) << codes[i];
			}
		}

		TEST(DisputeDirectorTest, ACustomerAppealGoesToReviewAnythingElseSettlesForTheMerchant)
		{
			DirectorFixture f;

			RoutingDecision appeal = f.decide("customer_appeal", "APPEAL");
			EXPECT_EQ(std::set<Id>{"appeal_review"}, idSet(appeal.activate));
			EXPECT_TRUE(appeal.skip.empty());

			const char* codes[] = {"NO_APPEAL", "ACCEPT"};
			for (unsigned i = 0; i < 2; ++i)
			{
				RoutingDecision d = f.decide("customer_appeal", codes[i]);
				EXPECT_EQ(std::set<Id>{"settle_for_merchant"}, idSet(d.activate)) << codes[i];
				EXPECT_EQ((std::set<Id>{"appeal_review", "settle_for_customer"}), idSet(d.skip)) << codes[i];
			}
		}

		TEST(DisputeDirectorTest, TheAppealDecisionSettlesForTheWinnerAndSkipsTheOther)
		{
			DirectorFixture f;

			RoutingDecision customer = f.decide("appeal_review", "CUSTOMER_WINS");
			EXPECT_EQ(std::set<Id>{"settle_for_customer"}, idSet(customer.activate));
			EXPECT_EQ(std::set<Id>{"settle_for_merchant"}, idSet(customer.skip));

			RoutingDecision merchant = f.decide("appeal_review", "MERCHANT_WINS");
			EXPECT_EQ(std::set<Id>{"settle_for_merchant"}, idSet(merchant.activate));
			EXPECT_EQ(std::set<Id>{"settle_for_customer"}, idSet(merchant.skip));
		}

		TEST(DisputeDirectorTest, EveryPathThroughTheWorkflowSettlesExactlyOnce)
		{
			struct Path
			{
				const char* name;
				Steps steps;
				const char* winner;
				const char* loser;
			};

			Steps evidence = steps("merchant_response", "EVIDENCE_SUBMITTED");
			Steps customer_wins = then(evidence, "analyst_decision", "CUSTOMER_WINS");
			Steps merchant_wins = then(evidence, "analyst_decision", "MERCHANT_WINS");

			Path paths[] = {
				{"merchant concedes", steps("merchant_response", "ACCEPT_LIABILITY"), "settle_for_customer", "settle_for_merchant"},
				{"merchant silent", steps("merchant_response", "NO_RESPONSE"), "settle_for_customer", "settle_for_merchant"},
				{"customer wins, merchant accepts", then(customer_wins, "merchant_appeal", "ACCEPT"), "settle_for_customer", "settle_for_merchant"},
				{"customer wins, merchant too late", then(customer_wins, "merchant_appeal", "NO_APPEAL"), "settle_for_customer", "settle_for_merchant"},
				{"customer wins, appeal upheld", then(then(customer_wins, "merchant_appeal", "APPEAL"), "appeal_review", "CUSTOMER_WINS"), "settle_for_customer", "settle_for_merchant"},
				{"customer wins, appeal overturns", then(then(customer_wins, "merchant_appeal", "APPEAL"), "appeal_review", "MERCHANT_WINS"), "settle_for_merchant", "settle_for_customer"},
				{"merchant wins, customer accepts", then(merchant_wins, "customer_appeal", "ACCEPT"), "settle_for_merchant", "settle_for_customer"},
				{"merchant wins, customer too late", then(merchant_wins, "customer_appeal", "NO_APPEAL"), "settle_for_merchant", "settle_for_customer"},
				{"merchant wins, appeal upheld", then(then(merchant_wins, "customer_appeal", "APPEAL"), "appeal_review", "MERCHANT_WINS"), "settle_for_merchant", "settle_for_customer"},
				{"merchant wins, appeal overturns", then(then(merchant_wins, "customer_appeal", "APPEAL"), "appeal_review", "CUSTOMER_WINS"), "settle_for_customer", "settle_for_merchant"}
			};

			DirectorFixture f;
			for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
			{
				SCOPED_TRACE(paths[i].name);
				Branch branch = f.walk(paths[i].steps);

				EXPECT_EQ(1u, branch.activated.count(paths[i].winner));
				EXPECT_EQ(0u, branch.activated.count(paths[i].loser));
				EXPECT_EQ(1u, branch.skipped.count(paths[i].loser));
				EXPECT_EQ(0u, branch.skipped.count(paths[i].winner));
			}
		}

		TEST(DisputeDirectorTest, TheFinishedDefinitionPassesValidation)
		{
			// the real builder runs WorkflowDefinition::validate() in getResult(): a dependency cycle, a missing reviewer role,
			// a timeout without a deadline or a duplicate id in the Director would all surface here
			TestPack pack;
			DisputeDirector director(pack);
			StandardWorkflowBuilder builder;

			std::unique_ptr<WorkflowDefinition> definition;
			ASSERT_NO_THROW(definition = director.build(builder));
			ASSERT_TRUE(definition.get() != 0);

			EXPECT_EQ("transaction-dispute", definition->name());
			EXPECT_EQ(1u, definition->version());
			EXPECT_TRUE(definition->validationProblems().empty());
			EXPECT_TRUE(definition->policies().cancel_signals.count("customer-withdrew") == 1);
			EXPECT_EQ(6u, definition->policies().routing.size());
		}
	} // namespace banking
} // namespace tividy