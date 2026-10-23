/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 22 October 2026
*/

/**
 * @file TelecomPack.h
 * @brief ConcretePack for the "Telecommunications" industry: service provisioning and activation.
 */

#ifndef TIVIDY_TELECOM_PACK_H
#define TIVIDY_TELECOM_PACK_H

#include "engine/facade/IndustryPack.h"

namespace tividy
{
	namespace telecom
	{

		/**
		 * @brief Director for workflow "service-activation" version 1 (a new mobile or fibre service order).
		 *
		 * Required data keys at start: "customer_ref", "customer_id_number", "region", "product", "install_type" ("SELF_INSTALL" or "TECHNICIAN").
		 * The identity step is where SIM-registration evidence (RICA in South Africa) is captured. "dep:" = dependsOn.
		 *
		 * | Stage / item id         | Type (decorators)                                  | Role             | Dep                              |
		 * |-------------------------|----------------------------------------------------|------------------|----------------------------------|
		 * | **intake**              | stage                                              |                  |                                  |
		 * | capture_order           | ManualTask (Secure)                                | sales-agent      |                                  |
		 * | verify_identity         | ManualTask (Secure, Validated[RequiredDataRule customer_id_number]) | sales-agent | capture_order            |
		 * | credit_check            | CreditCheckTask (automatic)                        | system           | verify_identity                  |
		 * | manual_credit_review    | ManualTask (Secure)                                | credit-analyst   | credit_check                     |
		 * | notify_declined         | ManualTask                                         | sales-agent      | credit_check, manual_credit_review |
		 * | **fulfilment**          | stage, dep: credit_check, manual_credit_review     |                  |                                  |
		 * | choose_install          | ManualTask (automatic: a routing gate)             | system           |                                  |
		 * | ship_device             | ManualTask                                         | logistics        | choose_install                   |
		 * | schedule_technician     | ManualTask                                         | customer-care    | choose_install                   |
		 * | install_cpe             | ManualTask (Secure)                                | field-technician | schedule_technician              |
		 * | **activation**          | stage, dep: ship_device, install_cpe               |                  |                                  |
		 * | provision_service       | ProvisionServiceTask (automatic)                   | system           |                                  |
		 * | customer_confirmation   | ManualTask (reviewRequired, reviewer customer-care)| sales-agent      | provision_service                |
		 *
		 * Routing (setRouting, source id -> strategy; "rest" = choose_install, ship_device, schedule_technician, install_cpe, provision_service,
		 * customer_confirmation):
		 *  - credit_check (OutcomeTableRouting): default skip [manual_credit_review, notify_declined]; "CREDIT_REFER": activate [manual_credit_review],
		 *    skip [notify_declined]; "CREDIT_DECLINED": activate [notify_declined], skip [manual_credit_review] + rest.
		 *  - manual_credit_review (OutcomeTableRouting): default (CREDIT_OK) skip [notify_declined]; "CREDIT_DECLINED": activate [notify_declined], skip rest.
		 *  - choose_install (DataValueRouting "install_type"): "SELF_INSTALL": activate [ship_device], skip [schedule_technician, install_cpe];
		 *    "TECHNICIAN" and default: activate [schedule_technician], skip [ship_device].
		 * Decline path: the instance Completes after notify_declined (everything else is Skipped).
		 * Policies: assignment = pack.createAssignmentStrategy(); escalation = pack.createEscalationChain(); cancel signal "customer-cancelled".
		 */
		class ServiceActivationDirector : public WorkflowDirector
		{
			public:
				explicit ServiceActivationDirector(const IndustryPack& pack);

				std::string workflowName() const override;      /**< "service-activation". */
				unsigned version() const override;              /**< 1. */
				std::string description() const override;
				void construct(WorkflowBuilder& builder) const override;
			private:
				const IndustryPack& pack;
		};

		/**
		 * @brief ConcreteFactory of the telecom family. Overrides:
		 *  - key() "telecom"; displayName() "Telecommunications: Service Provisioning & Activation"; createDirector(): ServiceActivationDirector.
		 *  - createParticipants(), ids prefixed "telecom-": sales-1 (sales-agent), care-1 (customer-care), credit-1 (credit-analyst),
		 *    logistics-1 (logistics), tech-gp-1 (field-technician, region "gauteng"), tech-wc-1 (field-technician, region "western-cape"),
		 *    neteng-1 (network-engineer), noc-1 (noc-manager), sys-1 (system).
		 *  - createConnectors(): CreditBureauAdapter(CreditBureauVendor), NetworkProvisioningAdapter(NetworkVendor) and the default LoggingNotifier.
		 *  - createAssignmentStrategy(): RegionalTechnicianAssignment (for roles other than field-technician it behaves like RoleBasedAssignment).
		 *  - createEscalationChain(): RoleEscalationHandler("network-engineer", 1, RoleEscalationHandler("noc-manager", 2)).
		 *  - createProfile(): productName "TIVIDY Connect"; vocabulary {"instance" -> "Order", "item" -> "Step", "workflow" -> "Service journey",
		 *    "participant" -> "Agent", "escalate" -> "Raise to NOC"}; blue accent.
		 */
		class TelecomPack : public IndustryPack
		{
			public:
				std::string key() const override;
				std::string displayName() const override;
				std::unique_ptr<WorkflowDirector> createDirector() const override;
				std::vector<Participant> createParticipants() const override;
				std::vector<std::unique_ptr<ExternalConnector> > createConnectors() const override;
				std::shared_ptr<const AssignmentStrategy> createAssignmentStrategy() const override;
				std::shared_ptr<const EscalationHandler> createEscalationChain() const override;
				ClientProfile createProfile() const override;
		};
	} // namespace telecom
} // namespace tividy

#endif // TIVIDY_TELECOM_PACK_H