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
 * @file BankingTasks.cpp
 * @brief Implements the five banking dispute tasks (Template Method): provisional credit, chargeback, merchant response, decision and settlement.
 */

#ifndef TIVIDY_BANKING_BANKINGTASKS_CPP
#define TIVIDY_BANKING_BANKINGTASKS_CPP

#include "industries/banking/BankingParts.h"
#include "engine/core/Errors.h"
#include "engine/items/WorkContext.h"

namespace tividy
{
	namespace banking
	{
		namespace
		{
			// every task here talks to a vendor the same way: call the connector, and turn a failed reply into the exception the Template Method expects
			ConnectorResult callConnector(WorkContext& context, const std::string& connector, const ConnectorRequest& request)
			{
				ConnectorResult result = context.connectors().require(connector).invoke(request);
				if (!result.success) throw ExternalFailure(result.message, result.retryable);

				return result;
			}

			// a connector that reports success but leaves out the value we need is a broken contract, retrying will not fix it
			std::string requirePayload(const ConnectorResult& result, const std::string& key)
			{
				std::map<std::string, std::string>::const_iterator it = result.payload.find(key);
				if (it == result.payload.end()) throw ExternalFailure("connector reply is missing " + key, false);

				return it->second;
			}

			bool isBlank(const WorkflowData& data, const std::string& key)
			{
				return data.getOr(key, "") == "";
			}
		} // anonymous namespace

		// IssueProvisionalCreditTask

		IssueProvisionalCreditTask::IssueProvisionalCreditTask(const WorkItemSpec& spec) :
			CloneableTask<IssueProvisionalCreditTask>(spec) { }

		void IssueProvisionalCreditTask::performWork(const Participant& /* actor */, Outcome& /* outcome */)
		{
			WorkflowData& data = context().data();
			if (isBlank(data, "card_ref") || isBlank(data, "amount")) throw ValidationError("card_ref and amount are required");

			ConnectorRequest request;
			request.operation = "post_provisional_credit";
			request.params["card_ref"] = data.get("card_ref");
			request.params["amount"] = data.get("amount");

			// demo hook: only the first attempt asks the vendor to fail, so the retry after the failures() count goes through
			if (failures() == 0) request.params["simulate_failures"] = data.getOr("simulate_failures", "0");

			ConnectorResult result = callConnector(context(), "core-banking", request);
			markIrreversibleEffect();
			data.set("ledger_ref", requirePayload(result, "ledger_ref"));
		}

		bool IssueProvisionalCreditTask::performsIrreversibleWork() const
		{
			return true; // a ledger entry exists now
		}

		// FileChargebackTask

		FileChargebackTask::FileChargebackTask(const WorkItemSpec& spec) :
			CloneableTask<FileChargebackTask>(spec) { }

		void FileChargebackTask::performWork(const Participant& /* actor */, Outcome& /* outcome */)
		{
			WorkflowData& data = context().data();
			if (isBlank(data, "transaction_id") || isBlank(data, "reason_code")) throw ValidationError("transaction_id and reason_code are required");

			ConnectorRequest request;
			request.operation = "file_chargeback";
			request.params["transaction_id"] = data.get("transaction_id");
			request.params["reason_code"] = data.get("reason_code");

			if (failures() == 0) request.params["simulate_failures"] = data.getOr("simulate_failures", "0");

			ConnectorResult result = callConnector(context(), "card-scheme", request);
			markIrreversibleEffect();
			data.set("scheme_case", requirePayload(result, "scheme_case"));
		}

		bool FileChargebackTask::performsIrreversibleWork() const
		{
			return true; // the scheme has a case now
		}

		// MerchantResponseTask

		MerchantResponseTask::MerchantResponseTask(const WorkItemSpec& spec) :
			CloneableTask<MerchantResponseTask>(spec) { }

		void MerchantResponseTask::validateOutcome(const Outcome& outcome) const
		{
			if (outcome.code() != "EVIDENCE_SUBMITTED" && outcome.code() != "ACCEPT_LIABILITY")
				throw ValidationError("choose EVIDENCE_SUBMITTED or ACCEPT_LIABILITY");

			if (outcome.code() == "EVIDENCE_SUBMITTED" && outcome.note() == "")
				throw ValidationError("evidence summary required");
		}

		void MerchantResponseTask::performWork(const Participant& /* actor */, Outcome& /* outcome */)
		{
			// nothing to do, the merchant's answer is the work and validateOutcome already checked it
		}

		void MerchantResponseTask::onCompleted(const Outcome& outcome)
		{
			if (outcome.code() == "EVIDENCE_SUBMITTED") context().data().set("merchant_evidence", outcome.note());
		}

		// DecisionTask

		DecisionTask::DecisionTask(const WorkItemSpec& spec) :
			CloneableTask<DecisionTask>(spec) { }

		void DecisionTask::validateOutcome(const Outcome& outcome) const
		{
			if (outcome.code() != "CUSTOMER_WINS" && outcome.code() != "MERCHANT_WINS")
				throw ValidationError("decision must be CUSTOMER_WINS or MERCHANT_WINS");

			if (outcome.note() == "") throw ValidationError("reasoning required");
		}

		void DecisionTask::performWork(const Participant& /* actor */, Outcome& /* outcome */)
		{
			// nothing to do, the decision is the work and validateOutcome already checked it
		}

		void DecisionTask::onCompleted(const Outcome& outcome)
		{
			context().data().set("decision", outcome.code());
			context().data().set("decision_reasoning", outcome.note());
		}

		// SettleDisputeTask

		SettleDisputeTask::SettleDisputeTask(const WorkItemSpec& spec, Winner winner) :
			CloneableTask<SettleDisputeTask>(spec), winner(winner) { }

		void SettleDisputeTask::performWork(const Participant& /* actor */, Outcome& /* outcome */)
		{
			WorkflowData& data = context().data();
			if (isBlank(data, "ledger_ref")) throw ValidationError("no provisional credit on file");

			ConnectorRequest request;
			request.params["ledger_ref"] = data.get("ledger_ref");

			// customer wins: the provisional credit stays and becomes permanent, merchant wins: it is taken back
			if (winner == Winner::Customer) request.operation = "finalise_credit";
			else request.operation = "reverse_credit";

			callConnector(context(), "core-banking", request);
			markIrreversibleEffect();
			
			if (winner == Winner::Customer) data.set("settlement", "CUSTOMER");
			else data.set("settlement", "MERCHANT");
		}

		bool SettleDisputeTask::performsIrreversibleWork() const
		{
			return true; // the ledger has been settled
		}
	} // namespace banking
} // namespace tividy

#endif // TIVIDY_BANKING_BANKINGTASKS_CPP