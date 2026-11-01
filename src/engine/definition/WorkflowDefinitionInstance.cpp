/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 31 October 2026
*/

/**
 * @file WorkflowDefinitionInstance.cpp
 * @brief Prototype pattern: creating independent instances from a template definition.
 */

#include "engine/definition/WorkflowDefinition.h"
#include "engine/runtime/WorkflowInstance.h"
#include "engine/core/Errors.h"

namespace tividy
{
	std::unique_ptr<WorkflowInstance> WorkflowDefinition::createInstance(
		const Id& instance_id, 
		const WorkflowData& initial_data, 
		Services& services) const
	{
		// 1. Ensure the definition is valid before we try to clone it.
		// (Throws InvalidDefinitionError if there are structural problems).
		validate();

		// 2. Prototype pattern: Deep-clone the template tree.
		std::unique_ptr<WorkItem> cloned_root = template_root->clone();

		// 3. Construct the WorkflowInstance.
	    return std::unique_ptr<WorkflowInstance>(new WorkflowInstance(
			instance_id,
			name_ref,
			version_ref,
			initial_data,
			std::move(cloned_root),
			policies_ref,
			services
		));
	}
} // namespace tividy