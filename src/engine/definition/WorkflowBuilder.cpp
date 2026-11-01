/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 29 October 2026
*/

/**
 * @file WorkflowBuilder.cpp
 * @brief Builds an owned template tree and validates the finished definition.
 */

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/core/Errors.h"
#include "engine/definition/WorkflowBuilder.h"
#include "engine/items/StageItem.h"

static void requireActiveBuild(const std::unique_ptr<tividy::WorkItem>& root)
{
	if (!root)
	{
		throw tividy::InvalidDefinitionError("call reset() before building a workflow definition");
	}
}

tividy::WorkflowBuilder::~WorkflowBuilder() = default;
tividy::StandardWorkflowBuilder::StandardWorkflowBuilder() : version(0) {}
tividy::StandardWorkflowBuilder::~StandardWorkflowBuilder() = default;

void tividy::StandardWorkflowBuilder::reset(const std::string& workflow_name, unsigned workflow_version,
	const std::string& workflow_description)
{
	std::string next_name(workflow_name);
	std::string next_description(workflow_description);
	std::unique_ptr<tividy::WorkItem> next_root(new tividy::StageItem(tividy::WorkItemSpec("root", workflow_name)));
	std::vector<tividy::WorkItem*> next_stages(1, next_root.get());

	name.swap(next_name);
	description.swap(next_description);
	version = workflow_version;
	root.swap(next_root);
	open_stages.swap(next_stages);
	policies = tividy::DefinitionPolicies();
}

void tividy::StandardWorkflowBuilder::beginStage(const tividy::WorkItemSpec& spec)
{
	requireActiveBuild(root);
	std::unique_ptr<tividy::WorkItem> nested(new tividy::StageItem(spec));
	// Reserve before transferring ownership, so a stack allocation failure cannot
	// leave an inserted stage without its matching stack entry.
	open_stages.reserve(open_stages.size() + 1);
	tividy::WorkItem& added = open_stages.back()->add(std::move(nested));
	open_stages.push_back(&added);
}

void tividy::StandardWorkflowBuilder::endStage()
{
	requireActiveBuild(root);
	if (open_stages.size() == 1)
	{
		throw tividy::InvalidDefinitionError("cannot close the implicit root stage");
	}
	open_stages.pop_back();
}

void tividy::StandardWorkflowBuilder::addTask(std::unique_ptr<tividy::WorkItem> task)
{
	requireActiveBuild(root);
	if (!task)
	{
		throw tividy::InvalidDefinitionError("cannot add a null work item");
	}
	open_stages.back()->add(std::move(task));
}

void tividy::StandardWorkflowBuilder::setAssignment(std::shared_ptr<const tividy::AssignmentStrategy> strategy)
{
	requireActiveBuild(root);
	policies.assignment = std::move(strategy);
}

void tividy::StandardWorkflowBuilder::setRouting(const tividy::Id& source_item_id,
	std::shared_ptr<const tividy::RoutingStrategy> strategy)
{
	requireActiveBuild(root);
	policies.routing[source_item_id] = std::move(strategy);
}

void tividy::StandardWorkflowBuilder::setEscalationChain(std::shared_ptr<const tividy::EscalationHandler> head)
{
	requireActiveBuild(root);
	policies.escalation = std::move(head);
}

void tividy::StandardWorkflowBuilder::addCancelSignal(const std::string& signal_name)
{
	requireActiveBuild(root);
	policies.cancel_signals.insert(signal_name);
}

std::unique_ptr<tividy::WorkflowDefinition> tividy::StandardWorkflowBuilder::getResult()
{
	requireActiveBuild(root);
	if (open_stages.size() != 1)
	{
		throw tividy::InvalidDefinitionError("cannot finish while a nested stage is open");
	}

	tividy::DefinitionPolicies finished_policies(policies);
	if (!finished_policies.assignment)
	{
		finished_policies.assignment = std::make_shared<tividy::RoleBasedAssignment>();
	}

	std::string finished_name;
	std::string finished_description;
	finished_name.swap(name);
	finished_description.swap(description);
	const unsigned finished_version = version;
	version = 0;
	std::unique_ptr<tividy::WorkItem> finished_root = std::move(root);
	open_stages.clear();
	policies = tividy::DefinitionPolicies();

	std::unique_ptr<tividy::WorkflowDefinition> result(new tividy::WorkflowDefinition(
		finished_name, finished_version, finished_description, std::move(finished_root), finished_policies));
	result->validate();
	return result;
}
