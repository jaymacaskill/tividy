/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 23 October 2026
*/

/**
 * @file BankingParts.h
 * @brief Banking dispute classes: 5 tasks, 2 simulated vendor systems, 2 adapters.
 */

#ifndef TIVIDY_BANKING_PARTS_H
#define TIVIDY_BANKING_PARTS_H

#include <memory>
#include <string>

#include "engine/connectors/ExternalConnector.h"
#include "engine/items/TaskItem.h"

namespace tividy
{
	namespace banking
	{

		/**
		 * @brief Human-triggered task: the agent posts the provisional credit to the customer's account while the dispute is investigated.
		 * Its deadline is a regulatory window (illustrative: 10 days), so a late posting escalates instead of being ignored.
		 *
		 * performWork: validate that data has "card_ref" and "amount" (else ValidationError("card_ref and amount are required")); request
		 * {"post_provisional_credit", {"card_ref", "amount"}} on connector "core-banking". Demo hook: while failures() == 0 the request also
		 * carries "simulate_failures" = data.getOr("simulate_failures", "0"). A failed result throws ExternalFailure(message, retryable).
		 * On success store data["ledger_ref"] = payload["ledger_ref"]. performsIrreversibleWork() == true (a ledger entry now exists).
		 */
		class IssueProvisionalCreditTask : public CloneableTask<IssueProvisionalCreditTask>
		{
			public:
				explicit IssueProvisionalCreditTask(const WorkItemSpec& spec);
			protected:
				void performWork(const Participant& actor, Outcome& outcome) override;
				bool performsIrreversibleWork() const override;
		};

		/**
		 * @brief Automatic task: files the chargeback with the card scheme through connector "card-scheme".
		 * performWork: request {"file_chargeback", {"transaction_id", "reason_code"}} taken from data (missing key -> ValidationError); demo hook
		 * "simulate_failures" exactly as in IssueProvisionalCreditTask. On success store data["scheme_case"] = payload["scheme_case"].
		 * performsIrreversibleWork() == true (the scheme now has a case).
		 */
		class FileChargebackTask : public CloneableTask<FileChargebackTask>
		{
			public:
				explicit FileChargebackTask(const WorkItemSpec& spec);
			protected:
				void performWork(const Participant& actor, Outcome& outcome) override;
				bool performsIrreversibleWork() const override;
		};

		/**
		 * @brief Human task: the merchant's representative answers the dispute (the adversarial counter-claim).
		 * validateOutcome: outcome.code() must be "EVIDENCE_SUBMITTED" or "ACCEPT_LIABILITY", else ValidationError("choose EVIDENCE_SUBMITTED or
		 * ACCEPT_LIABILITY"); for EVIDENCE_SUBMITTED the note (a summary of the evidence) must be non-empty, else ValidationError("evidence
		 * summary required"). performWork: leaves the outcome unchanged. onCompleted: for EVIDENCE_SUBMITTED store data["merchant_evidence"] = note.
		 * (If the deadline lapses, the item is expired by the system with code "NO_RESPONSE" and none of this runs.)
		 */
		class MerchantResponseTask : public CloneableTask<MerchantResponseTask>
		{
			public:
				explicit MerchantResponseTask(const WorkItemSpec& spec);
			protected:
				void validateOutcome(const Outcome& outcome) const override;
				void performWork(const Participant& actor, Outcome& outcome) override;
				void onCompleted(const Outcome& outcome) override;
		};

		/**
		 * @brief Human task: an analyst (or senior analyst on appeal) renders a decision.
		 * validateOutcome: code must be "CUSTOMER_WINS" or "MERCHANT_WINS", else ValidationError("decision must be CUSTOMER_WINS or MERCHANT_WINS");
		 * a non-empty note (the reasoning) is required, else ValidationError("reasoning required"). performWork: leaves the outcome unchanged.
		 * onCompleted: data["decision"] = outcome.code() and data["decision_reasoning"] = outcome.note().
		 */
		class DecisionTask : public CloneableTask<DecisionTask>
		{
			public:
				explicit DecisionTask(const WorkItemSpec& spec);
			protected:
				void validateOutcome(const Outcome& outcome) const override;
				void performWork(const Participant& actor, Outcome& outcome) override;
				void onCompleted(const Outcome& outcome) override;
		};

		/** Who a settlement favours. */
		enum class Winner { Customer, Merchant };

		/**
		 * @brief Automatic task: applies the final outcome to the ledger through connector "core-banking".
		 * performWork: requires data["ledger_ref"], else ValidationError("no provisional credit on file"). Winner::Customer -> request
		 * {"finalise_credit", {"ledger_ref"}} (the provisional credit becomes permanent); Winner::Merchant -> {"reverse_credit", {"ledger_ref"}}.
		 * A failed result throws ExternalFailure(message, retryable). On success data["settlement"] = "CUSTOMER" or "MERCHANT".
		 * performsIrreversibleWork() == true. The winner is a member copied by the Prototype copy.
		 */
		class SettleDisputeTask : public CloneableTask<SettleDisputeTask>
		{
			public:
				SettleDisputeTask(const WorkItemSpec& spec, Winner winner);
			protected:
				void performWork(const Participant& actor, Outcome& outcome) override;
				bool performsIrreversibleWork() const override;
			private:
				Winner winner;
		};

		/**
		 * @brief Adaptee: the bank's simulated core ledger. postProvisionalCredit() returns "LEDGER-<n>" (n counts from 1) and throws
		 * std::invalid_argument if the amount is not a positive number. reverse() and finalise() throw std::invalid_argument for an unknown
		 * ledger ref. Every method throws std::runtime_error("core banking unavailable") while failNextCalls(n) is active (the next n calls).
		 */
		class CoreBankingVendor
		{
			public:
				CoreBankingVendor();
				void failNextCalls(unsigned n);
				std::string postProvisionalCredit(const std::string& card_ref, const std::string& amount);
				void reverse(const std::string& ledger_ref);
				void finalise(const std::string& ledger_ref);
			private:
				unsigned failures_left;
				unsigned counter;
		};

		/** Adaptee: simulated card-scheme dispute system. fileChargeback() returns "CASE-" + transactionId; throws std::runtime_error("scheme unavailable") while failNextCalls(n) is active. */
		class CardSchemeVendor
		{
			public:
				CardSchemeVendor();
				void failNextCalls(unsigned n);
				std::string fileChargeback(const std::string& transaction_id, const std::string& reason_code);
			private:
				unsigned failures_left;
		};

		/**
		 * @brief Object Adapter: connector "core-banking". Operations: "post_provisional_credit" {card_ref, amount} -> payload {ledger_ref};
		 * "reverse_credit" {ledger_ref}; "finalise_credit" {ledger_ref}. Optional param "simulate_failures" (integer text > 0) on
		 * post_provisional_credit: the adapter calls vendor.failNextCalls(n) first. std::runtime_error -> failure(what(), true);
		 * std::invalid_argument -> failure(what(), false). Missing params -> failure("<name> is required", false).
		 */
		class CoreBankingAdapter : public ExternalConnector
		{
			public:
				explicit CoreBankingAdapter(std::unique_ptr<CoreBankingVendor> vendor);
				std::string name() const override;
				ConnectorResult invoke(const ConnectorRequest& request) override;
			private:
				std::unique_ptr<CoreBankingVendor> vendor;
		};

		/**
		 * @brief Object Adapter: connector "card-scheme". Operation "file_chargeback" {transaction_id, reason_code} -> payload {scheme_case}.
		 * Optional param "simulate_failures" as above. std::runtime_error -> failure(what(), true). Missing params -> failure(..., false).
		 */
		class CardSchemeAdapter : public ExternalConnector
		{
			public:
				explicit CardSchemeAdapter(std::unique_ptr<CardSchemeVendor> vendor);
				std::string name() const override;
				ConnectorResult invoke(const ConnectorRequest& request) override;
			private:
				std::unique_ptr<CardSchemeVendor> vendor;
		};
	} // namespace banking
} // namespace tividy

#endif // TIVIDY_BANKING_PARTS_H