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
 * @file BankingPack.h
 * @brief ConcretePack for the "Banking" industry: transaction dispute (chargeback) resolution.
 */

#ifndef TIVIDY_BANKING_PACK_H
#define TIVIDY_BANKING_PACK_H

#include "engine/facade/IndustryPack.h"

namespace tividy
{
	namespace banking
	{

		/**
		 * @brief Director for workflow "transaction-dispute" version 1.
		 *
		 * ### The story (why this proves the engine handles negotiation, not just pipelines)
		 * A cardholder disputes a transaction. The bank checks eligibility, posts a PROVISIONAL credit inside a regulatory window, files a
		 * chargeback with the card scheme, and the MERCHANT answers with counter-evidence or concedes. If the merchant stays silent, a deadline
		 * lapses and the dispute is decided against them by default. Otherwise an analyst proposes a decision, a senior analyst approves it
		 * (four-eyes), and the LOSING party may appeal within a window; an appeal goes to a senior analyst for a final decision. Finally the
		 * ledger is settled. Parties: customer, dispute-agent, merchant-rep, dispute-analyst, senior-analyst, compliance-officer, system.
		 *
		 * Required data keys at start: "transaction_id", "amount", "reason_code", "card_ref", "merchant_name".
		 * Optional demo hook: "simulate_failures" (see BankingParts.h).
		 *
		 * ### Items ("dep:" = dependsOn; durations are ILLUSTRATIVE configuration, not legal advice: confirm the real regulatory and
		 *     card-scheme windows before presenting them as such; they live only in this Director and are easy to change)
		 *
		 * | Stage / item id           | Type (decorators)                                       | Role             | Dep                                  | Deadline -> on lapse |
		 * |---------------------------|---------------------------------------------------------|------------------|--------------------------------------|----------------------|
		 * | **intake**                | stage                                                   |                  |                                      |                      |
		 * | lodge_dispute             | ManualTask (Secure, Validated[RequiredDataRule transaction_id, amount, reason_code]) | customer |                |                      |
		 * | assess_eligibility        | ManualTask (Secure)                                     | dispute-agent    | lodge_dispute                        |                      |
		 * | close_ineligible          | ManualTask (Secure)                                     | dispute-agent    | assess_eligibility                   |                      |
		 * | **provisional**           | stage, dep: assess_eligibility, close_ineligible        |                  |                                      |                      |
		 * | issue_provisional_credit  | IssueProvisionalCreditTask (Secure)                     | dispute-agent    |                                      | 10 days -> escalate (regulatory breach) |
		 * | file_chargeback           | FileChargebackTask (automatic)                          | system           | issue_provisional_credit             |                      |
		 * | **contest**               | stage, dep: file_chargeback                             |                  |                                      |                      |
		 * | merchant_response         | MerchantResponseTask (Secure)                           | merchant-rep     |                                      | 14 days -> expire "NO_RESPONSE" |
		 * | analyst_decision          | DecisionTask (Secure, reviewRequired, reviewer senior-analyst) | dispute-analyst | merchant_response                | 30 days -> escalate |
		 * | merchant_appeal           | ManualTask (Secure)                                     | merchant-rep     | analyst_decision                     | 7 days -> expire "NO_APPEAL" |
		 * | customer_appeal           | ManualTask (Secure)                                     | customer         | analyst_decision                     | 7 days -> expire "NO_APPEAL" |
		 * | appeal_review             | DecisionTask (Secure)                                   | senior-analyst   | merchant_appeal, customer_appeal     |                      |
		 * | **settlement**            | stage                                                   |                  |                                      |                      |
		 * | settle_for_customer       | SettleDisputeTask(Winner::Customer) (automatic)         | system           | merchant_appeal, customer_appeal, appeal_review |           |
		 * | settle_for_merchant       | SettleDisputeTask(Winner::Merchant) (automatic)         | system           | merchant_appeal, customer_appeal, appeal_review |           |
		 *
		 * ### Routing (setRouting, source id -> OutcomeTableRouting). "all later" = every item of provisional, contest and settlement.
		 *  - assess_eligibility: default: skip [close_ineligible]; "INELIGIBLE": activate [close_ineligible], skip all later.
		 *  - merchant_response:  default (EVIDENCE_SUBMITTED): nothing (analyst_decision is released by its dependency);
		 *      "ACCEPT_LIABILITY" and "NO_RESPONSE": activate [settle_for_customer], skip [analyst_decision, merchant_appeal, customer_appeal,
		 *      appeal_review, settle_for_merchant].
		 *  - analyst_decision:   "CUSTOMER_WINS": activate [merchant_appeal], skip [customer_appeal];
		 *                        "MERCHANT_WINS": activate [customer_appeal], skip [merchant_appeal].
		 *  - merchant_appeal:    "APPEAL": activate [appeal_review]; default (accept or "NO_APPEAL"): activate [settle_for_customer],
		 *                        skip [appeal_review, settle_for_merchant].
		 *  - customer_appeal:    "APPEAL": activate [appeal_review]; default: activate [settle_for_merchant], skip [appeal_review, settle_for_customer].
		 *  - appeal_review:      "CUSTOMER_WINS": activate [settle_for_customer], skip [settle_for_merchant];
		 *                        "MERCHANT_WINS": activate [settle_for_merchant], skip [settle_for_customer].
		 * Exactly one settlement runs; unused items are Skipped so the instance Completes. The ineligible path Completes after close_ineligible.
		 *
		 * Policies: assignment = pack.createAssignmentStrategy() (the engine default); escalation = pack.createEscalationChain();
		 * cancel signal "customer-withdrew".
		 * Held by reference to the pack; must not outlive it.
		 */
		class DisputeDirector : public WorkflowDirector
		{
			public:
				explicit DisputeDirector(const IndustryPack& pack);

				std::string workflowName() const override;      /**< "transaction-dispute". */
				unsigned version() const override;              /**< 1. */
				std::string description() const override;
				void construct(WorkflowBuilder& builder) const override;
			private:
				const IndustryPack& pack;
		};

		/**
		 * @brief ConcreteFactory of the banking family. Overrides (the default assignment strategy is kept on purpose):
		 *  - key() "banking"; displayName() "Banking: Transaction Dispute Resolution"; createDirector(): DisputeDirector.
		 *  - createParticipants(), ids prefixed "banking-": cust-1 (customer), agent-1 (dispute-agent), merchant-1 (merchant-rep),
		 *    analyst-1 (dispute-analyst), senior-1 (senior-analyst), compliance-1 (compliance-officer), sys-1 (system).
		 *  - createConnectors(): CoreBankingAdapter(CoreBankingVendor), CardSchemeAdapter(CardSchemeVendor) and the default LoggingNotifier.
		 *  - createEscalationChain(): RoleEscalationHandler("senior-analyst", 1, RoleEscalationHandler("compliance-officer", 2)).
		 *  - createProfile(): productName "TIVIDY Resolve"; vocabulary {"instance" -> "Dispute case", "item" -> "Action", "workflow" -> "Dispute process",
		 *    "participant" -> "Party", "escalate" -> "Refer to supervisor"}; amber accent.
		 */
		class BankingPack : public IndustryPack
		{
			public:
				std::string key() const override;
				std::string displayName() const override;
				std::unique_ptr<WorkflowDirector> createDirector() const override;
				std::vector<Participant> createParticipants() const override;
				std::vector<std::unique_ptr<ExternalConnector> > createConnectors() const override;
				std::shared_ptr<const EscalationHandler> createEscalationChain() const override;
				ClientProfile createProfile() const override;
		};
	} // namespace banking
} // namespace tividy

#endif // TIVIDY_BANKING_PACK_H