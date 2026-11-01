/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 1 November 2026
*/

/**
 * @file BankingDirector.cpp
 * @brief Implements DisputeDirector (Builder pattern): assembles the "transaction-dispute" workflow, its deadlines and its routing rules.
 */

#ifndef TIVIDY_BANKING_BANKINGDIRECTOR_CPP
#define TIVIDY_BANKING_BANKINGDIRECTOR_CPP

#include <memory>
#include <utility>
#include <vector>

#include "industries/banking/BankingPack.h"
#include "industries/banking/BankingParts.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemDecorator.h"
#include "engine/policies/RoutingStrategy.h"

namespace tividy
{
	namespace banking
	{
		namespace
		{
			// the windows are illustrative configuration (see BankingPack.h), they only live here so they are easy to change
			const long DAY = 24 * 60 * 60;
			const long PROVISIONAL_CREDIT_WINDOW = 10 * DAY;
			const long MERCHANT_RESPONSE_WINDOW = 14 * DAY;
			const long ANALYST_DECISION_WINDOW = 30 * DAY;
			const long APPEAL_WINDOW = 7 * DAY;

			// the same few wrappers on nearly every item, named so construct() stays readable
			std::unique_ptr<WorkItem> secure(std::unique_ptr<WorkItem> item)
			{
				return std::unique_ptr<WorkItem>(new SecureWorkItem(std::move(item)));
			}

			std::unique_ptr<WorkItem> manual(const WorkItemSpec& spec)
			{
				return std::unique_ptr<WorkItem>(new ManualTask(spec));
			}

			std::unique_ptr<WorkItem> secureManual(const WorkItemSpec& spec)
			{
				return secure(manual(spec));
			}

			std::unique_ptr<WorkItem> secureDecision(const WorkItemSpec& spec)
			{
				return secure(std::unique_ptr<WorkItem>(new DecisionTask(spec)));
			}
		} // anonymous namespace

		DisputeDirector::DisputeDirector(const IndustryPack& pack) :
			pack(pack) { }

		std::string DisputeDirector::workflowName() const
		{
			return "transaction-dispute";
		}

		unsigned DisputeDirector::version() const
		{
			return 1;
		}

		std::string DisputeDirector::description() const
		{
			return "Cardholder transaction dispute: eligibility, provisional credit, chargeback, merchant response, "
				   "analyst decision with appeals, and ledger settlement.";
		}

		void DisputeDirector::construct(WorkflowBuilder& builder) const
		{
			// policies first, they do not depend on the items. going through the pack means an overridden policy applies here too
			builder.setAssignment(pack.createAssignmentStrategy());
			builder.setEscalationChain(pack.createEscalationChain());
			builder.addCancelSignal("customer-withdrew");

			// intake
			builder.beginStage(WorkItemSpec("intake", "Intake"));

			// secure on the outside so the role is checked before the data rules run
			std::vector<std::shared_ptr<const ValidationRule> > lodge_rules;
			lodge_rules.push_back(std::shared_ptr<const ValidationRule>(
				new RequiredDataRule(std::vector<std::string>{"transaction_id", "amount", "reason_code"})));

			builder.addTask(secure(std::unique_ptr<WorkItem>(new ValidatedWorkItem(
				manual(WorkItemSpec("lodge_dispute", "Lodge dispute").requireRole("customer")), lodge_rules))));

			builder.addTask(secureManual(WorkItemSpec("assess_eligibility", "Assess eligibility")
				.requireRole("dispute-agent").dependsOn("lodge_dispute")));

			builder.addTask(secureManual(WorkItemSpec("close_ineligible", "Close ineligible dispute")
				.requireRole("dispute-agent").dependsOn("assess_eligibility")));

			builder.endStage();

			// provisional: the stage's own dependencies gate everything inside it
			builder.beginStage(WorkItemSpec("provisional", "Provisional credit")
				.dependsOn("assess_eligibility").dependsOn("close_ineligible"));

			// no onTimeout on purpose: a late provisional credit is a regulatory breach, so it escalates instead of defaulting
			builder.addTask(secure(std::unique_ptr<WorkItem>(new IssueProvisionalCreditTask(
				WorkItemSpec("issue_provisional_credit", "Issue provisional credit")
					.requireRole("dispute-agent").dueAfter(PROVISIONAL_CREDIT_WINDOW)))));

			builder.addTask(std::unique_ptr<WorkItem>(new FileChargebackTask(
				WorkItemSpec("file_chargeback", "File chargeback")
					.requireRole("system").automatic(true).dependsOn("issue_provisional_credit"))));

			builder.endStage();

			// contest
			builder.beginStage(WorkItemSpec("contest", "Contest").dependsOn("file_chargeback"));

			// silence from the merchant is a default judgment against them, hence the timeout code
			builder.addTask(secure(std::unique_ptr<WorkItem>(new MerchantResponseTask(
				WorkItemSpec("merchant_response", "Merchant response")
					.requireRole("merchant-rep").dueAfter(MERCHANT_RESPONSE_WINDOW).onTimeout("NO_RESPONSE")))));

			// four-eyes: the analyst submits, a senior analyst approves
			builder.addTask(secureDecision(WorkItemSpec("analyst_decision", "Analyst decision")
				.requireRole("dispute-analyst").reviewRequired(true).reviewedBy("senior-analyst")
				.dependsOn("merchant_response").dueAfter(ANALYST_DECISION_WINDOW)));

			builder.addTask(secureManual(WorkItemSpec("merchant_appeal", "Merchant appeal")
				.requireRole("merchant-rep").dependsOn("analyst_decision")
				.dueAfter(APPEAL_WINDOW).onTimeout("NO_APPEAL")));

			builder.addTask(secureManual(WorkItemSpec("customer_appeal", "Customer appeal")
				.requireRole("customer").dependsOn("analyst_decision")
				.dueAfter(APPEAL_WINDOW).onTimeout("NO_APPEAL")));

			builder.addTask(secureDecision(WorkItemSpec("appeal_review", "Appeal review")
				.requireRole("senior-analyst").dependsOn("merchant_appeal").dependsOn("customer_appeal")));

			builder.endStage();

			// settlement
			builder.beginStage(WorkItemSpec("settlement", "Settlement"));

			builder.addTask(std::unique_ptr<WorkItem>(new SettleDisputeTask(
				WorkItemSpec("settle_for_customer", "Settle for customer")
					.requireRole("system").automatic(true)
					.dependsOn("merchant_appeal").dependsOn("customer_appeal").dependsOn("appeal_review"),
				Winner::Customer)));

			builder.addTask(std::unique_ptr<WorkItem>(new SettleDisputeTask(
				WorkItemSpec("settle_for_merchant", "Settle for merchant")
					.requireRole("system").automatic(true)
					.dependsOn("merchant_appeal").dependsOn("customer_appeal").dependsOn("appeal_review"),
				Winner::Merchant)));

			builder.endStage();

			// routing. exactly one settlement runs, everything on the branches not taken is skipped so the instance can complete
			const std::vector<Id> none;

			// every item after intake, skipped as a group if the dispute turns out to be ineligible
			std::vector<Id> all_later;
			all_later.push_back("issue_provisional_credit");
			all_later.push_back("file_chargeback");
			all_later.push_back("merchant_response");
			all_later.push_back("analyst_decision");
			all_later.push_back("merchant_appeal");
			all_later.push_back("customer_appeal");
			all_later.push_back("appeal_review");
			all_later.push_back("settle_for_customer");
			all_later.push_back("settle_for_merchant");

			std::shared_ptr<OutcomeTableRouting> eligibility(new OutcomeTableRouting);
			eligibility->setDefault(none, std::vector<Id>{"close_ineligible"});
			eligibility->addRule("INELIGIBLE", std::vector<Id>{"close_ineligible"}, all_later);
			builder.setRouting("assess_eligibility", eligibility);

			// evidence submitted needs no rule, analyst_decision is released by its dependency
			std::vector<Id> merchant_concedes_skip;
			merchant_concedes_skip.push_back("analyst_decision");
			merchant_concedes_skip.push_back("merchant_appeal");
			merchant_concedes_skip.push_back("customer_appeal");
			merchant_concedes_skip.push_back("appeal_review");
			merchant_concedes_skip.push_back("settle_for_merchant");

			std::shared_ptr<OutcomeTableRouting> response(new OutcomeTableRouting);
			response->addRule("ACCEPT_LIABILITY", std::vector<Id>{"settle_for_customer"}, merchant_concedes_skip);
			response->addRule("NO_RESPONSE", std::vector<Id>{"settle_for_customer"}, merchant_concedes_skip);
			builder.setRouting("merchant_response", response);

			// only the losing party can appeal, so the other appeal item is skipped
			std::shared_ptr<OutcomeTableRouting> decision(new OutcomeTableRouting);
			decision->addRule("CUSTOMER_WINS", std::vector<Id>{"merchant_appeal"}, std::vector<Id>{"customer_appeal"});
			decision->addRule("MERCHANT_WINS", std::vector<Id>{"customer_appeal"}, std::vector<Id>{"merchant_appeal"});
			builder.setRouting("analyst_decision", decision);

			// no appeal (or accepting the decision) settles for whoever won. the merchant only appeals when the customer won, and vice versa
			std::shared_ptr<OutcomeTableRouting> merchant_appeal(new OutcomeTableRouting);
			merchant_appeal->addRule("APPEAL", std::vector<Id>{"appeal_review"}, none);
			merchant_appeal->setDefault(std::vector<Id>{"settle_for_customer"},
				std::vector<Id>{"appeal_review", "settle_for_merchant"});
			builder.setRouting("merchant_appeal", merchant_appeal);

			std::shared_ptr<OutcomeTableRouting> customer_appeal(new OutcomeTableRouting);
			customer_appeal->addRule("APPEAL", std::vector<Id>{"appeal_review"}, none);
			customer_appeal->setDefault(std::vector<Id>{"settle_for_merchant"},
				std::vector<Id>{"appeal_review", "settle_for_customer"});
			builder.setRouting("customer_appeal", customer_appeal);

			// the senior analyst's final decision on appeal
			std::shared_ptr<OutcomeTableRouting> review(new OutcomeTableRouting);
			review->addRule("CUSTOMER_WINS", std::vector<Id>{"settle_for_customer"}, std::vector<Id>{"settle_for_merchant"});
			review->addRule("MERCHANT_WINS", std::vector<Id>{"settle_for_merchant"}, std::vector<Id>{"settle_for_customer"});
			builder.setRouting("appeal_review", review);
		}
	} // namespace banking
} // namespace tividy

#endif // TIVIDY_BANKING_BANKINGDIRECTOR_CPP