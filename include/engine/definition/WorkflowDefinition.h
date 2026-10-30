/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 26 October 2026
*/

/**
 * @file WorkflowDefinition.h
 * @brief The reusable description of a process (never executed itself), plus its configurable policies.
 */

#ifndef TIVIDY_DEFINITION_WORKFLOWDEFINITION_H
#define TIVIDY_DEFINITION_WORKFLOWDEFINITION_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "engine/core/WorkflowData.h"
#include "engine/items/WorkItem.h"
#include "engine/policies/AssignmentStrategy.h"
#include "engine/policies/EscalationHandler.h"
#include "engine/policies/RoutingStrategy.h"

namespace tividy
{
	class WorkflowInstance;
	struct Services;

	/**
	 * @brief Everything configurable about HOW a definition runs. All members are shared_ptr-to-const (stateless), so
	 * one set serves every instance without leaking state between them.
	 * Defaults applied by StandardWorkflowBuilder when unset: assignment = RoleBasedAssignment; escalation = null
	 * (escalated items wait for a human); routing = empty (every item uses DependencyRouting); cancel_signals = empty.
	 */
	struct DefinitionPolicies
	{
		std::shared_ptr<const AssignmentStrategy> assignment;                  /**< Required, non-null. */
		std::map<Id, std::shared_ptr<const RoutingStrategy> > routing;         /**< Keyed by SOURCE item id. */
		std::shared_ptr<const EscalationHandler> escalation;                   /**< Head of the chain; may be null. */
		std::set<std::string> cancel_signals;                                  /**< ExternalSignal messages that cancel the instance. */
	};

	/**
	 * @brief Product of the Builder; Prototype client; owner of the TEMPLATE tree. The template is never bound or executed.
	 * createInstance() deep-clones it, so instances never share mutable state. Immutable after construction.
	 */
	class WorkflowDefinition
	{
		public:
			/** @pre template_root != nullptr. Takes ownership. Does NOT validate (call validate()). */
			WorkflowDefinition(const std::string& name, unsigned version, const std::string& description,
							   std::unique_ptr<WorkItem> template_root, const DefinitionPolicies& policies);
			~WorkflowDefinition();

			const std::string& name() const;
			unsigned version() const;
			const std::string& description() const;
			const DefinitionPolicies& policies() const;

			/**
			 * @brief Lists every structural problem; empty means valid. Rules (one message each, naming the offending id):
			 *  1. item ids are unique across the tree;
			 *  2. every stage has at least one child;
			 *  3. every dependency names an existing item other than itself, and the dependency graph has no cycle;
			 *  4. a reviewRequired item has a non-empty reviewerRole;
			 *  5. an automatic item is not reviewRequired;
			 *  6. policies.assignment is non-null;
			 *  7. checks routing source keys only;
			 *  8. an item with a non-empty timeout_code has dueAfterSeconds() > 0, and an automatic item has no deadline.
			 */
			std::vector<std::string> validationProblems() const;

			/** @throws InvalidDefinitionError whose what() joins all problems with "; " if validationProblems() is non-empty. */
			void validate() const;

			/**
			 * @brief Creates one independent, not-yet-started instance.
			 * @param instance_id  Unique id chosen by the caller.
			 * @param initial_data Copied into the instance.
			 * @param services     Shared collaborators; must outlive the instance.
			 * @return an instance with status Created owning a deep clone of the template, bound to its own data/publisher/clock.
			 *         No event is published yet; call instance->start().
			 * @throws InvalidDefinitionError if validate() fails.
			 */
			std::unique_ptr<WorkflowInstance> createInstance(const Id& instance_id, const WorkflowData& initial_data,
															 Services& services) const;

		private:
			std::string name_ref;
			unsigned version_ref;
			std::string description_ref;
			std::unique_ptr<WorkItem> template_root;
			DefinitionPolicies policies_ref;
	};
} // namespace tividy

#endif // TIVIDY_DEFINITION_WORKFLOWDEFINITION_H