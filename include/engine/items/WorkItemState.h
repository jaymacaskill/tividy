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
 * @file WorkItemState.h
 * @brief State pattern: the legal transitions of a TaskItem, one class per status.
 */

#ifndef TIVIDY_ITEMS_WORKITEMSTATE_H
#define TIVIDY_ITEMS_WORKITEMSTATE_H

#include <string>

#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"
#include "engine/core/Types.h"

namespace tividy
{
	class TaskItem;

	/**
	 * @brief State (GoF "State" participant). Context = TaskItem.
	 *
	 * Every operation has a DEFAULT body that throws InvalidStateError("<op> is not allowed while <status>").
	 * A concrete state overrides only the operations that are legal for it, so there is no switch on status
	 * anywhere in the engine. States are stateless and shared (one static instance each), so TaskItem stores
	 * only a pointer to the current state. Never delete a state.
	 *
	 * ### Legal transitions (everything else throws InvalidStateError)
	 * | From        | Operation                | To                                              |
	 * |-------------|--------------------------|-------------------------------------------------|
	 * | Created     | publish                  | Available                                       |
	 * | Created     | cancel / skip            | Cancelled / Skipped                             |
	 * | Available   | assign                   | Assigned                                        |
	 * | Available   | cancel / skip            | Cancelled / Skipped                             |
	 * | Assigned    | start                    | InProgress                                      |
	 * | Assigned    | escalate / cancel        | Escalated / Cancelled                           |
	 * | Assigned    | expire                   | Completed (deadline lapsed, outcome from spec.timeoutCode) |
	 * | InProgress  | complete [!requiresReview]| Completed (only if the work succeeded)         |
	 * | InProgress  | submit [requiresReview]  | UnderReview                                     |
	 * | InProgress  | escalate / cancel        | Escalated / Cancelled                           |
	 * | InProgress  | expire                   | Completed (as above)                            |
	 * | UnderReview | approve                  | Completed                                       |
	 * | UnderReview | reject                   | InProgress if rejections < spec.maxRetries, else Cancelled |
	 * | UnderReview | escalate / cancel        | Escalated / Cancelled                           |
	 * | UnderReview | expire                   | Completed (as above)                            |
	 * | Escalated   | assign                   | Escalated (sets the approver; no status change) |
	 * | Escalated   | approve                  | Completed (override approval)                   |
	 * | Escalated   | cancel                   | Cancelled (terminate)                           |
	 * | Completed, Cancelled, Skipped | any    | (none: terminal)                                |
	 *
	 * Each concrete state's operation performs, in this order: (1) guard checks, (2) side effects through
	 * the protected helpers below, (3) transition via moveTo(), (4) event via emit(). On a guard failure it
	 * throws BEFORE changing anything.
	 */
	class WorkItemState
	{
		public:
			virtual ~WorkItemState();

			/** @return the ItemStatus this state represents. */
			virtual ItemStatus status() const = 0;

			virtual void publish(TaskItem& item) const;
			virtual void assign(TaskItem& item, const Id& participant_id) const;
			virtual void start(TaskItem& item, const Participant& actor) const;
			virtual void submit(TaskItem& item, const Participant& actor, const Outcome& outcome) const;
			virtual void complete(TaskItem& item, const Participant& actor, const Outcome& outcome) const;
			virtual void approve(TaskItem& item, const Participant& reviewer, const Outcome& outcome) const;
			virtual void reject(TaskItem& item, const Participant& reviewer, const std::string& reason) const;
			virtual void escalate(TaskItem& item, const std::string& reason) const;
			virtual void cancel(TaskItem& item, const std::string& reason) const;
			virtual void skip(TaskItem& item, const std::string& reason) const;

			/** Default: throws InvalidStateError. Overridden by Assigned, InProgress and UnderReview: recordResult(system, Outcome::rejected(outcome_code, reason)),
			 *  moveTo(Completed), emit ItemCompleted. */
			virtual void expire(TaskItem& item, const std::string& outcome_code, const std::string& reason) const;

		protected:
			/// Helpers that reach TaskItem's private data (WorkItemState is its friend; derived states are not).
			/// @{
			/** Switches @p item to @p next. Replaces the item's current-state pointer. */
			static void moveTo(TaskItem& item, const WorkItemState& next);

			/** Publishes an event for @p item on its context's publisher, stamped with the context clock. */
			static void emit(TaskItem& item, EventType type, const Id& actor_id, const Outcome& outcome,
							 const std::string& message, unsigned count = 0);

			/** Sets the item's assignee and, when the item is entering Assigned, assigned_at = clock.now(). */
			static void setAssignee(TaskItem& item, const Id& participant_id);

			/** The Template Method driver: validateOutcome -> performWork -> onCompleted. See TaskItem::executeWork().
			 *  @param item    The task whose work runs.
			 *  @param actor   The participant doing the work.
			 *  @param outcome In/out: may be refined by the work; callers then use the refined value for recordResult()/emit().
			 *  @return true if the work succeeded; false if it failed technically (ItemFailed already published,
			 *          item left InProgress). */
			static bool runWork(TaskItem& item, const Participant& actor, Outcome& outcome);

			/** Remembers the last outcome and who finished the item. */
			static void recordResult(TaskItem& item, const Participant& actor, const Outcome& outcome);

			/** Increments and returns the item's reject / escalation counter. */
			static unsigned bumpRejections(TaskItem& item);
			static unsigned bumpEscalations(TaskItem& item);

			/** @return item.rejections() */
			static unsigned rejections(const TaskItem& item);

			/** @throws PermissionDeniedError unless @p actor is the item's assignee. */
			static void requireAssignee(const TaskItem& item, const Participant& actor);
			/// @}
	};

	/** Concrete states. Each exposes instance() returning the single shared object. */
	class CreatedState : public WorkItemState
	{
		public:
			static const CreatedState& instance();
			ItemStatus status() const override;
			void publish(TaskItem& item) const override;
			void cancel(TaskItem& item, const std::string& reason) const override;
			void skip(TaskItem& item, const std::string& reason) const override;
	};

	class AvailableState : public WorkItemState
	{
		public:
			static const AvailableState& instance();
			ItemStatus status() const override;
			void assign(TaskItem& item, const Id& participant_id) const override;
			void cancel(TaskItem& item, const std::string& reason) const override;
			void skip(TaskItem& item, const std::string& reason) const override;
	};

	class AssignedState : public WorkItemState
	{
		public:
			static const AssignedState& instance();
			ItemStatus status() const override;
			void start(TaskItem& item, const Participant& actor) const override;
			void escalate(TaskItem& item, const std::string& reason) const override;
			void cancel(TaskItem& item, const std::string& reason) const override;
			void expire(TaskItem& item, const std::string& outcome_code, const std::string& reason) const override;
	};

	class InProgressState : public WorkItemState
	{
		public:
			static const InProgressState& instance();
			ItemStatus status() const override;

			/** @throws InvalidStateError("use submit(): this item requires review") if spec().requiresReview(). */
			void complete(TaskItem& item, const Participant& actor, const Outcome& outcome) const override;

			/** @throws InvalidStateError("use complete(): this item has no review step") if !spec().requiresReview(). */
			void submit(TaskItem& item, const Participant& actor, const Outcome& outcome) const override;

			void escalate(TaskItem& item, const std::string& reason) const override;
			void cancel(TaskItem& item, const std::string& reason) const override;
			void expire(TaskItem& item, const std::string& outcome_code, const std::string& reason) const override;
	};

	class UnderReviewState : public WorkItemState
	{
		public:
			static const UnderReviewState& instance();
			ItemStatus status() const override;
			void approve(TaskItem& item, const Participant& reviewer, const Outcome& outcome) const override;
			void reject(TaskItem& item, const Participant& reviewer, const std::string& reason) const override;
			void escalate(TaskItem& item, const std::string& reason) const override;
			void cancel(TaskItem& item, const std::string& reason) const override;
			void expire(TaskItem& item, const std::string& outcome_code, const std::string& reason) const override;
	};

	class EscalatedState : public WorkItemState
	{
		public:
			static const EscalatedState& instance();
			ItemStatus status() const override;

			/** Sets the escalation approver as assignee; status stays Escalated; publishes ItemAssigned. */
			void assign(TaskItem& item, const Id& participant_id) const override;

			/** Override approval: runs the work, then Completed. */
			void approve(TaskItem& item, const Participant& reviewer, const Outcome& outcome) const override;

			/** "Terminate". */
			void cancel(TaskItem& item, const std::string& reason) const override;
	};

	/** Terminal states override nothing: every operation throws the default InvalidStateError. */
	class CompletedState : public WorkItemState
	{
		public:
			static const CompletedState& instance();
			ItemStatus status() const override;
	};

	class CancelledState : public WorkItemState
	{
		public:
			static const CancelledState& instance();
			ItemStatus status() const override;
	};

	class SkippedState : public WorkItemState
	{
		public:
			static const SkippedState& instance();
			ItemStatus status() const override;
	};
} // namespace tividy

#endif // TIVIDY_ITEMS_WORKITEMSTATE_H