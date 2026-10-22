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
 * @file OnboardingPack.h
 * @brief ConcretePack for the "Employee Services" industry: employee onboarding.
 */

#ifndef TIVIDY_ONBOARDING_PACK_H
#define TIVIDY_ONBOARDING_PACK_H

#include "engine/facade/IndustryPack.h"

namespace tividy
{
	namespace onboarding
	{

		/**
		 * @brief Director for workflow "employee-onboarding" version 1.
		 *
		 * Required data keys at start: "full_name", "national_id", "department". Optional: "mentor_id". "dep:" = dependsOn.
		 *
		 * | Stage / item id        | Type (decorators)                                   | Role           | Dep                                |
		 * |------------------------|-----------------------------------------------------|----------------|------------------------------------|
		 * | **preboarding**        | stage                                               |                |                                    |
		 * | sign_contract          | ManualTask                                          | hr-coordinator |                                    |
		 * | verify_docs            | VerifyDocumentsTask (Secure, Validated[RequiredDataRule national_id, full_name]) | hr-coordinator | sign_contract |
		 * | legal_review           | ManualTask (Secure)                                 | legal-advisor  | verify_docs                        |
		 * | background_check       | BackgroundCheckTask (automatic)                     | system         | verify_docs, legal_review          |
		 * | close_rejected_hire    | ManualTask                                          | hr-coordinator | legal_review, background_check     |
		 * | **provisioning**       | stage, dep: background_check, close_rejected_hire   |                |                                    |
		 * | prepare_equipment      | ManualTask                                          | it-support     |                                    |
		 * | create_accounts        | ManualTask                                          | it-support     |                                    |
		 * | sign_off_provisions    | ManualTask (reviewRequired, reviewer hr-head)       | hr-coordinator | prepare_equipment, create_accounts |
		 * | **probation**          | stage, dep: sign_off_provisions                     |                |                                    |
		 * | welcome_and_training   | ManualTask                                          | mentor         |                                    |
		 * | probation_review       | ManualTask (Secure, reviewRequired, reviewer hr-head) | line-manager | welcome_and_training               |
		 * | confirm_employment     | ManualTask                                          | hr-coordinator | probation_review                   |
		 * | terminate_employment   | ManualTask                                          | hr-coordinator | probation_review                   |
		 *
		 * Routing (setRouting, source id -> OutcomeTableRouting; "later" = every item of provisioning and probation):
		 *  - verify_docs: default skip [legal_review]; "DOCS_INVALID": activate [legal_review].
		 *  - legal_review: default: nothing; "HIRE_REJECTED": activate [close_rejected_hire], skip [background_check] + later.
		 *  - background_check: default skip [close_rejected_hire]; "ISSUES_FOUND": activate [close_rejected_hire], skip later.
		 *  - probation_review: "SATISFACTORY": activate [confirm_employment], skip [terminate_employment];
		 *                      "TERMINATE": activate [terminate_employment], skip [confirm_employment].
		 * Normal path never touches close_rejected_hire (skipped), so the instance Completes after confirm_employment.
		 * Policies: assignment = pack.createAssignmentStrategy(); escalation = pack.createEscalationChain(); cancel signal "candidate-withdrew".
		 * Held by reference to the pack; must not outlive it.
		 */
		class OnboardingDirector : public WorkflowDirector
		{
			public:
				explicit OnboardingDirector(const IndustryPack& pack);

				std::string workflowName() const override;      /**< "employee-onboarding". */
				unsigned version() const override;              /**< 1. */
				std::string description() const override;
				void construct(WorkflowBuilder& builder) const override;
			private:
				const IndustryPack& pack;
		};

		/**
		 * @brief ConcreteFactory of the onboarding family. Overrides (everything else keeps the engine default):
		 *  - key() "onboarding"; displayName() "Employee Services: Onboarding"; createDirector(): OnboardingDirector.
		 *  - createParticipants(), ids prefixed "onboarding-": hr-1 (hr-coordinator), legal-1 (legal-advisor), hrhead-1 (hr-head),
		 *    it-1 (it-support), mgr-1 (line-manager), mentor-1 (mentor), sys-1 (system).
		 *  - createConnectors(): BackgroundCheckAdapter(BackgroundCheckVendor) plus the default LoggingNotifier "notifier".
		 *  - createAssignmentStrategy(): DataDrivenAssignment("mentor_id", RoleBasedAssignment).
		 *  - createEscalationChain(): RoleEscalationHandler("line-manager", 1, RoleEscalationHandler("hr-head", 2)).
		 *  - createProfile(): productName "TIVIDY People"; vocabulary {"instance" -> "Onboarding", "item" -> "Task", "participant" -> "Colleague"}; green accent.
		 */
		class OnboardingPack : public IndustryPack
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
	} // namespace onboarding
} // namespace tividy

#endif // TIVIDY_ONBOARDING_PACK_H