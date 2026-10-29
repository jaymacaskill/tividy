/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 23 October 2026
*/

/**
 * @file StageItem.h
 * @brief Composite "Composite": a group of work items (a stage or sub-process).
 */

#ifndef TIVIDY_ITEMS_STAGEITEM_H
#define TIVIDY_ITEMS_STAGEITEM_H

#include <memory>
#include <vector>

#include "engine/items/WorkItem.h"

namespace tividy
{

	/**
	 * @brief Owns an ordered list of children; its own status is DERIVED, never stored.
	 * Children are held by unique_ptr; destroying a stage destroys its subtree. Stages nest to any depth.
	 *
	 * ### status() (evaluated on every call; first matching step wins)
	 *  1. no children                                   -> Completed.
	 *  2. every child is Skipped                        -> Skipped.
	 *  3. every child isSettled()                       -> Completed.
	 *  4. every child isTerminal() (some Cancelled)     -> Cancelled.
	 *  5. any child status != Created                   -> InProgress.
	 *  6. otherwise                                     -> Created.
	 *
	 * ### Lifecycle operations
	 *  - cancel(reason): cancels every non-terminal child in order (cascade).
	 *  - skip(reason):   skips every Created/Available child; throws InvalidStateError if any task below it has started (This line has changed).
	 *  - publish, assign, start, submit, complete, approve, reject, escalate, expire:
	 *    throw UnsupportedOperationError("a stage is driven by its children").
	 *  - assignee() is "". createMemento()/restore()/isReversible() use the WorkItem defaults.
	 */
	class StageItem : public WorkItem
	{
		public:
			/** @param spec Description of the stage (its dependsOn() gates all descendants). */
			explicit StageItem(const WorkItemSpec& spec);
			~StageItem() override;

			const WorkItemSpec& spec() const override;
			ItemStatus status() const override;
			const Id& assignee() const override;
			std::vector<WorkItem*> children() override;
			WorkItem& add(std::unique_ptr<WorkItem> child) override;
			WorkItem* find(const Id& id) override;

			void publish() override;
			void assign(const Id& participant_id) override;
			void start(const Participant& actor) override;
			void submit(const Participant& actor, const Outcome& outcome) override;
			void complete(const Participant& actor, const Outcome& outcome) override;
			void approve(const Participant& reviewer, const Outcome& outcome) override;
			void reject(const Participant& reviewer, const std::string& reason) override;
			void escalate(const std::string& reason) override;
			void cancel(const std::string& reason) override;
			void skip(const std::string& reason) override;
			void expire(const std::string& outcome_code, const std::string& reason) override;

			/** @brief Prototype: new StageItem with the same spec, each child replaced by child->clone(). */
			std::unique_ptr<WorkItem> clone() const override;

			/** @brief visitor.visitStage(*this) once, then accept(visitor) on each child in order. */
			void accept(WorkItemVisitor& visitor) const override;

			std::unique_ptr<WorkItemIterator> createIterator() override;   /**< DepthFirstIterator rooted here. */
			void bind(WorkContext& context) override;                      /**< Binds all children. */
		private:
			WorkItemSpec spec_ref;
			std::vector<std::unique_ptr<WorkItem> > children_ref;
	};
} // namespace tividy

#endif // TIVIDY_ITEMS_STAGEITEM_H