/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 30 October 2026

TelecomDirector.cpp
*/

#ifndef TIVIDY_TELECOM_TELECOMDIRECTOR_CPP
#define TIVIDY_TELECOM_TELECOMDIRECTOR_CPP

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "industries/telecom/TelecomPack.h"
#include "industries/telecom/TelecomParts.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemDecorator.h"
#include "engine/policies/RoutingStrategy.h"

namespace tividy
{
	namespace telecom
	{
		namespace
		{
			// the same few wrappers on most items, named so construct() stays readable
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
		} // anonymous namespace

		ServiceActivationDirector::ServiceActivationDirector(const IndustryPack& pack) :
			pack(pack) { }

		std::string ServiceActivationDirector::workflowName() const
		{
			return "service-activation";
		}

		unsigned ServiceActivationDirector::version() const
		{
			return 1;
		}

		std::string ServiceActivationDirector::description() const
		{
			return "New mobile or fibre service order: identity capture, an automatic credit check with a referral branch, "
				   "a self-install versus technician fork, provisioning and customer confirmation.";
		}

		void ServiceActivationDirector::construct(WorkflowBuilder& builder) const
		{
			// policies first, they do not depend on the items. going through the pack means an overridden policy applies here too
			builder.setAssignment(pack.createAssignmentStrategy());
			builder.setEscalationChain(pack.createEscalationChain());
			builder.addCancelSignal("customer-cancelled");

			// intake
			builder.beginStage(WorkItemSpec("intake", "Intake"));

			builder.addTask(secureManual(WorkItemSpec("capture_order", "Capture order").requireRole("sales-agent")));

			// identity is where the SIM-registration (RICA) evidence is captured. secure on the outside so the role is checked
			// before the data rule runs
			std::vector<std::shared_ptr<const ValidationRule> > identity_rules;
			identity_rules.push_back(std::shared_ptr<const ValidationRule>(
				new RequiredDataRule(std::vector<std::string>{"customer_id_number"})));

			builder.addTask(secure(std::unique_ptr<WorkItem>(new ValidatedWorkItem(
				manual(WorkItemSpec("verify_identity", "Verify identity")
					.requireRole("sales-agent").dependsOn("capture_order")), identity_rules))));

			builder.addTask(std::unique_ptr<WorkItem>(new CreditCheckTask(
				WorkItemSpec("credit_check", "Credit check")
					.requireRole("system").automatic(true).dependsOn("verify_identity"))));

			builder.addTask(secureManual(WorkItemSpec("manual_credit_review", "Manual credit review")
				.requireRole("credit-analyst").dependsOn("credit_check")));

			builder.addTask(manual(WorkItemSpec("notify_declined", "Notify declined")
				.requireRole("sales-agent").dependsOn("credit_check").dependsOn("manual_credit_review")));

			builder.endStage();

			// fulfilment: the stage's own dependencies gate everything inside it
			builder.beginStage(WorkItemSpec("fulfilment", "Fulfilment")
				.dependsOn("credit_check").dependsOn("manual_credit_review"));

			// an automatic gate: it does no work, its routing picks the install branch
			builder.addTask(manual(WorkItemSpec("choose_install", "Choose install")
				.requireRole("system").automatic(true)));

			builder.addTask(manual(WorkItemSpec("ship_device", "Ship device")
				.requireRole("logistics").dependsOn("choose_install")));

			builder.addTask(manual(WorkItemSpec("schedule_technician", "Schedule technician")
				.requireRole("customer-care").dependsOn("choose_install")));

			builder.addTask(secureManual(WorkItemSpec("install_cpe", "Install CPE")
				.requireRole("field-technician").dependsOn("schedule_technician")));

			builder.endStage();

			// activation
			builder.beginStage(WorkItemSpec("activation", "Activation")
				.dependsOn("ship_device").dependsOn("install_cpe"));

			builder.addTask(std::unique_ptr<WorkItem>(new ProvisionServiceTask(
				WorkItemSpec("provision_service", "Provision service")
					.requireRole("system").automatic(true))));

			builder.addTask(manual(WorkItemSpec("customer_confirmation", "Customer confirmation")
				.requireRole("sales-agent").reviewRequired(true).reviewedBy("customer-care").dependsOn("provision_service")));

			builder.endStage();

			// routing. on a decline everything after the credit decision is skipped so the instance completes after notify_declined
			const std::vector<Id> none;

			std::vector<Id> rest;
			rest.push_back("choose_install");
			rest.push_back("ship_device");
			rest.push_back("schedule_technician");
			rest.push_back("install_cpe");
			rest.push_back("provision_service");
			rest.push_back("customer_confirmation");

			std::vector<Id> declined_skip;
			declined_skip.push_back("manual_credit_review");
			declined_skip.insert(declined_skip.end(), rest.begin(), rest.end());

			// approved needs no rule, the default skips the review and decline items
			std::shared_ptr<OutcomeTableRouting> credit(new OutcomeTableRouting);
			credit->setDefault(none, std::vector<Id>{"manual_credit_review", "notify_declined"});
			credit->addRule("CREDIT_REFER", std::vector<Id>{"manual_credit_review"}, std::vector<Id>{"notify_declined"});
			credit->addRule("CREDIT_DECLINED", std::vector<Id>{"notify_declined"}, declined_skip);
			builder.setRouting("credit_check", credit);

			// the analyst's decision on a referred score. the default is CREDIT_OK
			std::shared_ptr<OutcomeTableRouting> review(new OutcomeTableRouting);
			review->setDefault(none, std::vector<Id>{"notify_declined"});
			review->addRule("CREDIT_DECLINED", std::vector<Id>{"notify_declined"}, rest);
			builder.setRouting("manual_credit_review", review);

			// a technician visit is the default when install_type is missing or unknown
			std::shared_ptr<DataValueRouting> install(new DataValueRouting("install_type"));
			install->addRule("SELF_INSTALL", std::vector<Id>{"ship_device"}, std::vector<Id>{"schedule_technician", "install_cpe"});
			install->addRule("TECHNICIAN", std::vector<Id>{"schedule_technician"}, std::vector<Id>{"ship_device"});
			install->setDefault(std::vector<Id>{"schedule_technician"}, std::vector<Id>{"ship_device"});
			builder.setRouting("choose_install", install);
		}
	} // namespace telecom
} // namespace tividy

#endif // TIVIDY_TELECOM_TELECOMDIRECTOR_CPP