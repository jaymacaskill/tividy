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
 * @file WorkItem.h
 * @brief Composite "Component": the uniform interface of tasks, stages and decorators.
 */

#ifndef TIVIDY_ITEMS_WORKITEM_H
#define TIVIDY_ITEMS_WORKITEM_H

#include <memory>
#include <string>
#include <vector>

#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"
#include "engine/core/Types.h"
#include "engine/items/WorkItemSpec.h"

namespace tividy
{
	class WorkContext;
	class WorkItemIterator;
	class WorkItemMemento;
	class WorkItemVisitor;

	/**
	 * @brief Component of Composite, Prototype of Prototype, Component of Decorator, Element of Visitor.
	 * Holds NO transition rules and NO progression logic: those live in WorkItemState and FlowController.
	 *
	 * Contract common to every lifecycle operation below:
	 *  @pre  The item is bound (bind() was called), else InvalidStateError.
	 *  @throws InvalidStateError if the operation is illegal in the current status (table in WorkItemState).
	 *  @throws PermissionDeniedError if a SecureWorkItem decorator rejects the actor.
	 *  On success the status changes and exactly one event describing the change is published.
	 *  On any exception the status is unchanged.
	 */
	class WorkItem
	{
		public:
			virtual ~WorkItem();

			/// @name Description and observable status
			/// @{
			virtual const WorkItemSpec& spec() const = 0;

			/** @return current status. For a StageItem it is DERIVED from its children. */
			virtual ItemStatus status() const = 0;

			/** @return participant id responsible, or "" if none. */
			virtual const Id& assignee() const = 0;
			/// @}

			/// @name Composite structure (transparent form: declared here, supported by StageItem only)
			/// @{
			/** @return direct children, non-owning, in order. Default: empty. */
			virtual std::vector<WorkItem*> children();

			/** @brief Appends @p child and takes ownership. @return the added child.
			 *  @throws UnsupportedOperationError by default (TaskItem). @throws InvalidDefinitionError if a descendant has the same id. */
			virtual WorkItem& add(std::unique_ptr<WorkItem> child);

			/** @return this item or the descendant with spec().id() == @p id (outermost decorator), else nullptr. Depth-first. */
			virtual WorkItem* find(const Id& id) = 0;
			/// @}

			/// @name Lifecycle (delegated to WorkItemState inside TaskItem)
			/// @{
			virtual void publish() = 0;                                                   /**< Created -> Available. */
			virtual void assign(const Id& participant_id) = 0;                            /**< Available -> Assigned (sets the approver while Escalated). */
			virtual void start(const Participant& actor) = 0;                             /**< Assigned -> InProgress. */
			virtual void submit(const Participant& actor, const Outcome& outcome) = 0;    /**< InProgress -> UnderReview. @pre requiresReview. */
			virtual void complete(const Participant& actor, const Outcome& outcome) = 0;  /**< InProgress -> Completed. @pre !requiresReview. */
			virtual void approve(const Participant& reviewer, const Outcome& outcome) = 0; /**< UnderReview or Escalated -> Completed. */
			virtual void reject(const Participant& reviewer, const std::string& reason) = 0; /**< UnderReview -> InProgress, or Cancelled when out of retries. */
			virtual void escalate(const std::string& reason) = 0;                         /**< Assigned/InProgress/UnderReview -> Escalated. */
			virtual void cancel(const std::string& reason) = 0;                           /**< Any non-terminal -> Cancelled. */
			virtual void skip(const std::string& reason) = 0;                             /**< Created/Available -> Skipped. */
			/// @}

			/** @brief Prototype: deep copy of the TEMPLATE state. @return a new unbound item: status Created, no assignee, counters zero.
			 *  A StageItem clones every child; a decorator clones the wrapped item and re-wraps it. @post shares no mutable state. */
			virtual std::unique_ptr<WorkItem> clone() const = 0;

			/** @brief Visitor entry. Task -> visitor.visitTask(*this). Stage -> visitStage(*this), then accept() on each child. Decorator -> forwards. */
			virtual void accept(WorkItemVisitor& visitor) const = 0;

			/** @brief Iterator factory. @return a DepthFirstIterator over this subtree; caller owns it. */
			virtual std::unique_ptr<WorkItemIterator> createIterator() = 0;

			/// @name Memento (originator side). Defaults suit stages.
			/// @{
			/** @return a snapshot of runtime state, or an empty pointer if this item has none (default). */
			virtual std::unique_ptr<WorkItemMemento> createMemento() const;

			/** Restores a snapshot made by createMemento() of the SAME item. Publishes nothing. Default: does nothing. */
			virtual void restore(const WorkItemMemento& memento);

			/** @return false once irreversible work was performed. Default: true. */
			virtual bool isReversible() const;
			/// @}

			/** @brief Attaches this item and all descendants to their instance's collaborators.
			 *  @post lifecycle operations are allowed. Binding twice with a different context throws InvalidStateError. */
			virtual void bind(WorkContext& context) = 0;
	};
} // namespace tividy

#endif // TIVIDY_ITEMS_WORKITEM_H