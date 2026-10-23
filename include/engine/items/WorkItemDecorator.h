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
 * @file WorkItemDecorator.h
 * @brief Decorator pattern: add behaviour to a work item at definition time without subclassing it.
 */

#ifndef TIVIDY_ITEMS_WORKITEMDECORATOR_H
#define TIVIDY_ITEMS_WORKITEMDECORATOR_H

#include <memory>
#include <string>
#include <vector>

#include "engine/core/WorkflowData.h"
#include "engine/items/WorkItem.h"

namespace tividy
{
	class WorkContext;

	/**
	 * @brief Decorator (GoF "Decorator"): is-a WorkItem and has-a WorkItem.
	 *
	 * EVERY operation forwards to the wrapped component by default, so a concrete decorator overrides only what it
	 * enhances. It owns its component: `new SecureWorkItem(std::unique_ptr<WorkItem>(new MyTask(spec)))`. Decorators stack.
	 *  - clone(): concrete decorators implement `new Same(cloneComponent(), ...config)`.
	 *  - find(id): returns the OUTERMOST wrapper when the wrapped item matches (so permission checks cannot be bypassed by lookup).
	 *  - bind(): stores the context pointer (for decorators that need data) and then forwards.
	 */
	class WorkItemDecorator : public WorkItem
	{
		public:
			/** @pre component != nullptr. Takes ownership. */
			explicit WorkItemDecorator(std::unique_ptr<WorkItem> component);
			~WorkItemDecorator() override;

			const WorkItemSpec& spec() const override;
			ItemStatus status() const override;
			const Id& assignee() const override;
			TimePoint assignedAt() const override;
			TimePoint dueAt() const override;
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

			void accept(WorkItemVisitor& visitor) const override;
			std::unique_ptr<WorkItemIterator> createIterator() override;
			std::unique_ptr<WorkItemMemento> createMemento() const override;
			void restore(const WorkItemMemento& memento) override;
			bool isReversible() const override;
			void bind(WorkContext& context) override;

		protected:
			WorkItem& component();
			const WorkItem& component() const;

			/** @return component().clone(), for use inside a subclass's clone(). */
			std::unique_ptr<WorkItem> cloneComponent() const;

			/** @return the bound context. @throws InvalidStateError if not yet bound. */
			WorkContext& context() const;

		private:
			std::unique_ptr<WorkItem> component_ref;
			WorkContext* context_ref;
	};

	/**
	 * @brief Concrete Decorator: enforces roles BEFORE the action reaches the item.
	 *  - start, submit, complete: if spec().requiredRole() is non-empty, actor.hasRole(requiredRole) must hold.
	 *  - approve, reject: if status() == Escalated, actor.id() must equal assignee() (the approver chosen by the escalation
	 *    chain); otherwise if spec().reviewerRole() is non-empty, actor.hasRole(reviewerRole) must hold.
	 * A failed check throws PermissionDeniedError("<actor id> may not <operation> '<item name>' (requires role <role>)") and changes nothing.
	 * All other operations are system-initiated and are forwarded unchecked.
	 */
	class SecureWorkItem : public WorkItemDecorator
	{
		public:
			explicit SecureWorkItem(std::unique_ptr<WorkItem> component);

			void start(const Participant& actor) override;
			void submit(const Participant& actor, const Outcome& outcome) override;
			void complete(const Participant& actor, const Outcome& outcome) override;
			void approve(const Participant& reviewer, const Outcome& outcome) override;
			void reject(const Participant& reviewer, const std::string& reason) override;

			std::unique_ptr<WorkItem> clone() const override;
	};

	/** One data-validation rule for ValidatedWorkItem. Stateless; shareable between clones. */
	class ValidationRule
	{
		public:
			virtual ~ValidationRule();

			/** @return "" if the rule passes, otherwise a user-readable explanation of what is wrong. */
			virtual std::string check(const WorkItem& item, const WorkflowData& data, const Outcome& outcome) const = 0;
	};

	/** Generic rule: every listed data key must be present and non-empty. Message: "missing required data: a, b". */
	class RequiredDataRule : public ValidationRule
	{
		public:
			explicit RequiredDataRule(const std::vector<std::string>& keys);

			std::string check(const WorkItem& item, const WorkflowData& data, const Outcome& outcome) const override;
		private:
			std::vector<std::string> keys;
	};

	/**
	 * @brief Concrete Decorator: runs the rules, in order, before submit() and complete() reach the item.
	 * The first failing rule throws ValidationError(message); nothing changes.
	 */
	class ValidatedWorkItem : public WorkItemDecorator
	{
		public:
			ValidatedWorkItem(std::unique_ptr<WorkItem> component,
							  const std::vector<std::shared_ptr<const ValidationRule> >& rules);

			void submit(const Participant& actor, const Outcome& outcome) override;
			void complete(const Participant& actor, const Outcome& outcome) override;

			std::unique_ptr<WorkItem> clone() const override;
		private:
			std::vector<std::shared_ptr<const ValidationRule> > rules;
	};
} // namespace tividy

#endif // TIVIDY_ITEMS_WORKITEMDECORATOR_H