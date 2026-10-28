/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 27 October 2026
*/

/**
 * @file WorkflowDefinition.cpp
 * @brief Ownership, metadata and explicit validation of a reusable workflow definition.
 */

#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "engine/core/Errors.h"
#include "engine/definition/WorkflowDefinition.h"

tividy::WorkflowDefinition::WorkflowDefinition(const std::string& name, unsigned version,
	const std::string& description, std::unique_ptr<tividy::WorkItem> root,
	const tividy::DefinitionPolicies& policies)
	: name_ref(name), version_ref(version), description_ref(description),
	  template_root(std::move(root)), policies_ref(policies)
{
}

tividy::WorkflowDefinition::~WorkflowDefinition() = default;

const std::string& tividy::WorkflowDefinition::name() const
{
	return name_ref;
}

unsigned tividy::WorkflowDefinition::version() const
{
	return version_ref;
}

const std::string& tividy::WorkflowDefinition::description() const
{
	return description_ref;
}

const tividy::DefinitionPolicies& tividy::WorkflowDefinition::policies() const
{
	return policies_ref;
}

void tividy::WorkflowDefinition::validate() const
{
	const std::vector<std::string> problems = validationProblems();
	if (problems.empty())
	{
		return;
	}

	std::ostringstream message;
	for (std::size_t i = 0; i < problems.size(); ++i)
	{
		if (i != 0)
		{
			message << "; ";
		}
		message << problems[i];
	}

	throw tividy::InvalidDefinitionError(message.str());
}
