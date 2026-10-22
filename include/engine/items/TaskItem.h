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
 * @file TaskItem.h
 * @brief Composite "Leaf", State "Context", Template Method host and Memento "Originator".
 */

#ifndef TIVIDY_ITEMS_TASKITEM_H
#define TIVIDY_ITEMS_TASKITEM_H

#include <memory>

#include "engine/items/WorkItem.h"
#include "engine/items/WorkItemMemento.h"
#include "engine/items/WorkItemState.h"

namespace tividy
{
	class WorkContext;

	/**
	 * @brief One indivisible unit of work performed by one participant (or automatically).
	 *
	 * ### Pattern roles
	 * Composite Leaf; State Context (holds `const WorkItemState*`, delegates every lifecycle call); Template Method host
	 * (executeWork); Memento Originator (createMemento/restore); Prototype (protected copy constructor).
	 *
	 * ### Subclassing recipe (industry tasks)
	 * Derive from CloneableTask<YourTask>, pass a WorkItemSpec to the base constructor, override performWork().
	 * Never write clone() and never touch status yourself.
	 *
	 * ### Template Method: executeWork(actor, outcome)  [private, non-virtual]
	 *  1. @pre bound, else InvalidStateError.
	 *  2. validateOutcome(outcome)      -- hook; may throw ValidationError (nothing has changed yet).
	 *  3. performWork(actor, outcome)   -- primitive; the outcome is passed BY REFERENCE so the work may refine it
	 *     (e.g. a background check turns Accepted into Rejected("ISSUES_FOUND")). The refined outcome is what is recorded and routed.
	 *     If it throws ExternalFailure f: ++failures_ref; publish ItemFailed with count = failures_ref and
	 *     Outcome::failed(f.retryable() ? "RETRYABLE" : "PERMANENT", f.what()); return false (status unchanged).
	 *     Any other exception propagates unchanged.
	 *  4. If performsIrreversibleWork(): context().markIrreversible() and remember it (isReversible() becomes false).
	 *  5. onCompleted(outcome)          -- hook; default does nothing.
	 *  6. return true. The calling state then records the result, moves to Completed and emits ItemCompleted.
	 */
	class TaskItem : public WorkItem
	{
		public:
			explicit TaskItem(const WorkItemSpec& spec);
			~TaskItem() override;

			const WorkItemSpec& spec() const override;
			ItemStatus status() const override;          /**< @return state->status(). */
			const Id& assignee() const override;
			WorkItem* find(const Id& id) override;       /**< @return this if spec().id() == id, else nullptr. */

			void publish() override;                     /**< Each lifecycle method simply calls state->op(*this, ...). */
			void assign(const Id& participant_id) override;
			void start(const Participant& actor) override;
			void submit(const Participant& actor, const Outcome& outcome) override;
			void complete(const Participant& actor, const Outcome& outcome) override;
			void approve(const Participant& reviewer, const Outcome& outcome) override;
			void reject(const Participant& reviewer, const std::string& reason) override;
			void escalate(const std::string& reason) override;
			void cancel(const std::string& reason) override;
			void skip(const std::string& reason) override;

			void accept(WorkItemVisitor& visitor) const override;          /**< visitor.visitTask(*this). */
			std::unique_ptr<WorkItemIterator> createIterator() override;   /**< DepthFirstIterator over just this item. */
			std::unique_ptr<WorkItemMemento> createMemento() const override;
			void restore(const WorkItemMemento& memento) override;
			bool isReversible() const override;
			void bind(WorkContext& context) override;

			/** @return outcome recorded when the item last completed or was submitted (Accepted/empty before that). */
			const Outcome& lastOutcome() const;

			/** @return id of the participant who completed or approved it, or "". */
			const Id& completedBy() const;

			unsigned rejections() const;
			unsigned failures() const;
			unsigned escalations() const;

		protected:
			/** @brief Prototype copy: copies ONLY the spec. The new item is Created, unbound, unassigned, counters zero.
			 *  @pre @p prototype is an unbound template item. */
			TaskItem(const TaskItem& prototype);

			/** @brief Primitive operation: do the real work. Called only from executeWork().
			 *  @param actor   The participant doing the work (the "system" participant for automatic items).
			 *  @param outcome In/out: on entry what the actor reported (Accepted for automatic items); may be replaced.
			 *  @throws ExternalFailure for technical failures. May throw ValidationError. */
			virtual void performWork(const Participant& actor, Outcome& outcome) = 0;

			/** @brief Hook: reject impossible outcomes early. Default accepts all. @throws ValidationError with a readable message. */
			virtual void validateOutcome(const Outcome& outcome) const;

			/** @brief Hook: react after successful work (e.g. store a result in context().data()). Default: nothing. */
			virtual void onCompleted(const Outcome& outcome);

			/** @brief Hook: true if performWork() has effects that cannot be rolled back. Default false. */
			virtual bool performsIrreversibleWork() const;

			/** @return the bound context. @throws InvalidStateError if unbound. */
			WorkContext& context() const;

		private:
			friend class WorkItemState;

			bool executeWork(const Participant& actor, Outcome& outcome);   // Template Method

			WorkItemSpec spec_ref;
			const WorkItemState* state;
			WorkContext* context_ref;
			Id assignee_ref;
			unsigned rejections_ref;
			unsigned failures_ref;
			unsigned escalations_ref;
			bool irreversible_performed;
			Outcome last_outcome;
			Id completed_by;
	};

	/**
	 * @brief CRTP helper implementing clone() for any concrete task so authors cannot forget it.
	 * @tparam Derived The concrete task; needs an accessible copy constructor (the implicit one is fine).
	 */
	template <class Derived>
	class CloneableTask : public TaskItem
	{
		public:
			explicit CloneableTask(const WorkItemSpec& spec) : TaskItem(spec) {}

			std::unique_ptr<WorkItem> clone() const override
			{
				return std::unique_ptr<WorkItem>(new Derived(static_cast<const Derived&>(*this)));
			}
		protected:
			CloneableTask(const CloneableTask& other) : TaskItem(other) {}
	};

	/**
	 * @brief Generic task: a person (or a gate) does the work outside the system; performWork() does nothing.
	 * With spec().automatic(true) it acts as an automatic "gate" item, handy for pure routing decisions.
	 */
	class ManualTask : public CloneableTask<ManualTask>
	{
		public:
			explicit ManualTask(const WorkItemSpec& spec);
		protected:
			void performWork(const Participant& actor, Outcome& outcome) override;
	};
} // namespace tividy

#endif // TIVIDY_ITEMS_TASKITEM_H