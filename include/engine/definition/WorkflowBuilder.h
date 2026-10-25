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
 * @file WorkflowBuilder.h
 * @brief Builder pattern: Builder, ConcreteBuilder and Director base for assembling a WorkflowDefinition.
 */

#ifndef TIVIDY_DEFINITION_WORKFLOWBUILDER_H
#define TIVIDY_DEFINITION_WORKFLOWBUILDER_H

#include <memory>
#include <string>
#include <vector>

#include "engine/definition/WorkflowDefinition.h"

namespace tividy
{

	/**
	 * @brief Builder (GoF "Builder"). Protocol: reset() -> any mix of beginStage()/addTask()/endStage()/set*() -> getResult().
	 * The builder starts with an implicit top-level stage (id "root"); beginStage() nests in the current stage, addTask() adds to it.
	 */
	class WorkflowBuilder
	{
		public:
			virtual ~WorkflowBuilder();

			/** Discards partial work and starts a new definition. */
			virtual void reset(const std::string& name, unsigned version, const std::string& description) = 0;

			/** Opens a nested StageItem described by @p spec. */
			virtual void beginStage(const WorkItemSpec& spec) = 0;

			/** Closes the current stage. @throws InvalidDefinitionError if only the root is open. */
			virtual void endStage() = 0;

			/** Adds a ready-made (possibly decorated) item to the current stage; takes ownership. @pre task != nullptr. */
			virtual void addTask(std::unique_ptr<WorkItem> task) = 0;

			virtual void setAssignment(std::shared_ptr<const AssignmentStrategy> strategy) = 0;

			/** Registers @p strategy for the item with id @p source_item_id (replaces an earlier registration). */
			virtual void setRouting(const Id& source_item_id, std::shared_ptr<const RoutingStrategy> strategy) = 0;

			virtual void setEscalationChain(std::shared_ptr<const EscalationHandler> head) = 0;

			/** An ExternalSignal with this message cancels the instance. */
			virtual void addCancelSignal(const std::string& signal_name) = 0;

			/**
			 * @brief Finishes and returns the product.
			 * @throws InvalidDefinitionError if a stage is still open or WorkflowDefinition::validate() fails.
			 * @post the builder is empty; call reset() before reuse.
			 */
			virtual std::unique_ptr<WorkflowDefinition> getResult() = 0;
	};

	/** ConcreteBuilder: keeps a stack of open stages and applies the documented policy defaults. */
	class StandardWorkflowBuilder : public WorkflowBuilder
	{
		public:
			StandardWorkflowBuilder();
			~StandardWorkflowBuilder() override;

			void reset(const std::string& name, unsigned version, const std::string& description) override;
			void beginStage(const WorkItemSpec& spec) override;
			void endStage() override;
			void addTask(std::unique_ptr<WorkItem> task) override;
			void setAssignment(std::shared_ptr<const AssignmentStrategy> strategy) override;
			void setRouting(const Id& source_item_id, std::shared_ptr<const RoutingStrategy> strategy) override;
			void setEscalationChain(std::shared_ptr<const EscalationHandler> head) override;
			void addCancelSignal(const std::string& signal_name) override;
			std::unique_ptr<WorkflowDefinition> getResult() override;
		private:
			std::string name;
			unsigned version;
			std::string description;
			std::unique_ptr<WorkItem> root;
			std::vector<WorkItem*> open_stages;   // non-owning stack; back() is the current stage
			DefinitionPolicies policies;
	};

	/** Director (GoF "Director"): knows the ORDER and CONTENT of one concrete workflow. Industry packs supply one. */
	class WorkflowDirector
	{
		public:
			virtual ~WorkflowDirector();

			virtual std::string workflowName() const = 0;     /**< Registry name, e.g. "document-approval". */
			virtual unsigned version() const = 0;
			virtual std::string description() const = 0;

			/** Issues the builder calls describing the workflow. Must NOT call reset() or getResult(). */
			virtual void construct(WorkflowBuilder& builder) const = 0;

			/** @brief Template method: builder.reset(name, version, description); construct(builder); return builder.getResult(). */
			std::unique_ptr<WorkflowDefinition> build(WorkflowBuilder& builder) const;
	};
} // namespace tividy

#endif // TIVIDY_DEFINITION_WORKFLOWBUILDER_H