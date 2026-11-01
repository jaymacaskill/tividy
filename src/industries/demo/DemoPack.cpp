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
 * @file DemoPack.cpp
 * @brief A minimal industry pack demonstrating a Document Approval workflow.
 */

#include "industries/demo/DemoPack.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemDecorator.h" // For SecureWorkItem
#include "engine/policies/RoutingStrategy.h" // For OutcomeTableRouting
#include "engine/policies/EscalationHandler.h" // For RoleEscalationHandler

namespace tividy
{
	namespace demo
	{
		DocumentApprovalDirector::DocumentApprovalDirector(const IndustryPack& pack) : pack(pack) {}

		std::string DocumentApprovalDirector::workflowName() const { return "document-approval"; }
		unsigned DocumentApprovalDirector::version() const { return 1; }
		std::string DocumentApprovalDirector::description() const { return "A demo workflow for document approval."; }

		void DocumentApprovalDirector::construct(WorkflowBuilder& builder) const
		{
			// 1. Apply policies from the pack
			builder.setAssignment(pack.createAssignmentStrategy());
			builder.setEscalationChain(pack.createEscalationChain());

			// 2. Stage: drafting
			builder.beginStage(WorkItemSpec("drafting", "Drafting"));
			{
				WorkItemSpec spec("write_draft", "Write Draft");
				spec.requireRole("author");
				// Wrap in SecureWorkItem to enforce the role
				builder.addTask(std::unique_ptr<WorkItem>(new SecureWorkItem(std::unique_ptr<WorkItem>(new ManualTask(spec)))));
			}
			builder.endStage();

			// 3. Stage: review
			WorkItemSpec review_stage_spec("review", "Review");
			review_stage_spec.dependsOn("write_draft");
			builder.beginStage(review_stage_spec);
			{
				// Legal Review
				WorkItemSpec legal_spec("legal_review", "Legal Review");
				legal_spec.requireRole("legal");
				builder.addTask(std::unique_ptr<WorkItem>(new SecureWorkItem(std::unique_ptr<WorkItem>(new ManualTask(legal_spec)))));

				// Finance Review
				WorkItemSpec finance_spec("finance_review", "Finance Review");
				finance_spec.requireRole("finance");
				builder.addTask(std::unique_ptr<WorkItem>(new SecureWorkItem(std::unique_ptr<WorkItem>(new ManualTask(finance_spec)))));

				// Consolidate
				WorkItemSpec consolidate_spec("consolidate", "Consolidate Feedback");
				consolidate_spec.requireRole("manager");
				consolidate_spec.dependsOn("legal_review");
				consolidate_spec.dependsOn("finance_review");
				builder.addTask(std::unique_ptr<WorkItem>(new SecureWorkItem(std::unique_ptr<WorkItem>(new ManualTask(consolidate_spec)))));

				// Rework Draft
				WorkItemSpec rework_spec("rework_draft", "Rework Draft");
				rework_spec.requireRole("author");
				rework_spec.dependsOn("consolidate");
				builder.addTask(std::unique_ptr<WorkItem>(new SecureWorkItem(std::unique_ptr<WorkItem>(new ManualTask(rework_spec)))));

				// Approve
				WorkItemSpec approve_spec("approve", "Final Approval");
				approve_spec.requireRole("manager");
				approve_spec.reviewedBy("director");
				approve_spec.reviewRequired(true);
				approve_spec.dependsOn("consolidate");
				approve_spec.dependsOn("rework_draft");
				builder.addTask(std::unique_ptr<WorkItem>(new SecureWorkItem(std::unique_ptr<WorkItem>(new ManualTask(approve_spec)))));

				// Publish
				WorkItemSpec publish_spec("publish", "Publish Document");
				publish_spec.requireRole("publisher");
				publish_spec.dependsOn("approve");
				// Note: No SecureWorkItem here based on the header table!
				builder.addTask(std::unique_ptr<WorkItem>(new ManualTask(publish_spec)));
			}
			builder.endStage();

			// 4. Routing
			auto routing = std::make_shared<OutcomeTableRouting>();
			
			// Default: skip rework_draft
			routing->setDefault({}, {"rework_draft"}); 
			
			// If CHANGES_REQUIRED, activate rework_draft
			routing->addRule("CHANGES_REQUIRED", {"rework_draft"}, {});
			
			builder.setRouting("consolidate", routing);
		}

		std::string DemoPack::key() const { return "demo"; }
		std::string DemoPack::displayName() const { return "Demo: Document Approval"; }

		std::unique_ptr<WorkflowDirector> DemoPack::createDirector() const
		{
			return std::unique_ptr<WorkflowDirector>(new DocumentApprovalDirector(*this));
		}

		std::vector<Participant> DemoPack::createParticipants() const
		{
			std::vector<Participant> participants;
			participants.push_back(Participant("demo-author-1", "Author User", {"author"}));
			participants.push_back(Participant("demo-legal-1", "Legal User", {"legal"}));
			participants.push_back(Participant("demo-finance-1", "Finance User", {"finance"}));
			participants.push_back(Participant("demo-manager-1", "Manager User", {"manager", "director"})); // Has both roles
			participants.push_back(Participant("demo-publisher-1", "Publisher User", {"publisher"}));
			return participants;
		}

		std::shared_ptr<const EscalationHandler> DemoPack::createEscalationChain() const
		{
			// RoleEscalationHandler("manager", 1, RoleEscalationHandler("director", 2))
			auto directorHandler = std::make_shared<RoleEscalationHandler>("director", 2);
			auto managerHandler = std::make_shared<RoleEscalationHandler>("manager", 1, directorHandler);
			return managerHandler;
		}

	} // namespace demo
} // namespace tividy