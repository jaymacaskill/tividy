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
 * @file DemoPack.h
 * @brief The "show a prospective client" pack: generic engine classes only, no vendor integrations.
 */

#ifndef TIVIDY_DEMO_PACK_H
#define TIVIDY_DEMO_PACK_H

#include "engine/facade/IndustryPack.h"

namespace tividy
{
	namespace demo
	{

		/**
		 * @brief Director for workflow "document-approval" version 1. Required data keys: "title", "author".
		 * Uses ONLY engine classes (ManualTask, OutcomeTableRouting, RoleBasedAssignment, RoleEscalationHandler), which proves the engine is generic.
		 *
		 * | Stage / item id | Type (decorators)                              | Role      | Dep                          |
		 * |-----------------|------------------------------------------------|-----------|------------------------------|
		 * | **drafting**    | stage                                          |           |                              |
		 * | write_draft     | ManualTask (Secure)                            | author    |                              |
		 * | **review**      | stage, dep: write_draft                        |           |                              |
		 * | legal_review    | ManualTask (Secure)                            | legal     |                              |
		 * | finance_review  | ManualTask (Secure)                            | finance   |                              |
		 * | consolidate     | ManualTask (Secure)                            | manager   | legal_review, finance_review |
		 * | rework_draft    | ManualTask (Secure)                            | author    | consolidate                  |
		 * | approve         | ManualTask (reviewRequired, reviewer director) | manager   | consolidate, rework_draft    |
		 * | publish         | ManualTask                                     | publisher | approve                      |
		 *
		 * Routing: consolidate uses OutcomeTableRouting; default: skip [rework_draft]; "CHANGES_REQUIRED": activate [rework_draft].
		 * Policies: assignment = pack.createAssignmentStrategy(); escalation = pack.createEscalationChain().
		 */
		class DocumentApprovalDirector : public WorkflowDirector
		{
			public:
				explicit DocumentApprovalDirector(const IndustryPack& pack);

				std::string workflowName() const override;      /**< "document-approval". */
				unsigned version() const override;              /**< 1. */
				std::string description() const override;
				void construct(WorkflowBuilder& builder) const override;
			private:
				const IndustryPack& pack;
		};

		/**
		 * @brief ConcreteFactory for demonstrations. Overrides:
		 *  - key() "demo"; displayName() "Demo: Document Approval"; createDirector(): DocumentApprovalDirector.
		 *  - createParticipants(): demo-author-1 (author), demo-legal-1 (legal), demo-finance-1 (finance), demo-manager-1 (manager, director),
		 *    demo-publisher-1 (publisher).
		 *  - createEscalationChain(): RoleEscalationHandler("manager", 1, RoleEscalationHandler("director", 2)).
		 *  Everything else (connectors, assignment, profile) keeps the engine default.
		 */
		class DemoPack : public IndustryPack
		{
			public:
				std::string key() const override;
				std::string displayName() const override;
				std::unique_ptr<WorkflowDirector> createDirector() const override;
				std::vector<Participant> createParticipants() const override;
				std::shared_ptr<const EscalationHandler> createEscalationChain() const override;
		};
	} // namespace demo
} // namespace tividy

#endif // TIVIDY_DEMO_PACK_H