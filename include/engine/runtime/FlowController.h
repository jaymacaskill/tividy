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
 * @file FlowController.h
 * @brief The progression engine: reacts to events, routes work, assigns people, handles failures and escalation.
 */

#ifndef TIVIDY_RUNTIME_FLOWCONTROLLER_H
#define TIVIDY_RUNTIME_FLOWCONTROLLER_H

#include <deque>

#include "engine/definition/WorkflowDefinition.h"
#include "engine/events/Observer.h"

namespace tividy
{
	class WorkflowInstance;

	/**
	 * @brief Concrete Observer that drives ONE instance forward; Context for the Strategy and Chain participants.
	 * Attached to the publisher by the instance constructor AFTER the AuditObserver, so history is written before reactions.
	 *
	 * ### Re-entrancy rule
	 * onEvent() appends the event to a FIFO queue and, if not already draining, processes the queue until empty. Reactions never recurse.
	 * Any EngineError thrown by a reaction is caught and republished as EngineNote; it never escapes onEvent().
	 *
	 * ### Reaction table (event -> action). Reactions other than ItemCancelled/ExternalSignal do nothing unless instance.status() == Running.
	 *  - ItemAvailable (X): assignIfPossible(X).
	 *  - ItemAssigned (X, assignee A): if X.spec().isAutomatic() and X.status() == Assigned: X.start(A) then X.complete(A, Outcome::accepted())
	 *      (A looked up in the directory). An EngineError other than ExternalFailure is republished as ItemFailed with
	 *      Outcome::failed("PERMANENT", what()).
	 *  - ItemCompleted (X): routing = policies.routing[X.id] or DependencyRouting; d = routing.decide(X, X.lastOutcome, data);
	 *      applyDecision(d); releaseEligible(); evaluateInstance().
	 *  - ItemSkipped: releaseEligible(); evaluateInstance().
	 *  - ItemFailed (X, n = event.count(), code RETRYABLE or PERMANENT): if code == "RETRYABLE" and n <= X.spec().maxRetries():
	 *      retry (an automatic X is re-run exactly as above; a human X is retried by the user); otherwise X.escalate(message).
	 *  - ItemEscalated (X, level n = event.count()): r = policies.escalation ? policies.escalation->handle({instance id, X.id, message, n, &data})
	 *      : unresolved. If resolved: id = assignment.chooseParticipant(X, r.approver_role, directory, data); if non-empty X.assign(id)
	 *      (status stays Escalated). Otherwise publish EngineNote "escalation unresolved for <X.id>"; X stays Escalated.
	 *  - ItemCancelled (X): if the instance is Running: instance.finish(Failed, "item cancelled: <X.id>").
	 *  - ExternalSignal (message m): if m is in policies.cancel_signals: instance.cancel("signal: " + m).
	 *  - everything else: nothing.
	 *
	 * ### Helper algorithms
	 *  - assignIfPossible(X): id = assignment.chooseParticipant(X, X.spec().requiredRole(), directory, data); if non-empty and X is Available: X.assign(id).
	 *  - applyDecision(d): for each id in d.activate: item = root.find(id); if null or not Created -> EngineNote "routing ignored <id>", else
	 *      item.publish(). Then for each id in d.skip: if Created or Available item.skip("branch not taken") else EngineNote.
	 *  - releaseEligible(): if not Running return. Pre-order walk keeping the ancestor stages. A NON-composite item is eligible iff status() == Created
	 *      and, for the item and each ancestor stage, every id in spec().dependencies() resolves to an item whose status isSettled().
	 *      Eligible items get publish().
	 *  - evaluateInstance(): root.status() == Completed -> instance.finish(Completed); == Cancelled -> instance.finish(Failed).
	 */
	class FlowController : public EventObserver
	{
		public:
			/** @param instance The instance to drive; must outlive this controller.
			 *  @param policies Copied (the shared_ptrs are shared). @pre policies.assignment non-null. */
			FlowController(WorkflowInstance& instance, const DefinitionPolicies& policies);

			~FlowController() override;

			void onEvent(const WorkflowEvent& event) override;

			/** Makes every eligible Created item Available (algorithm above). Called by WorkflowInstance::start(). */
			void releaseEligible();
		private:
			void drain();
			void react(const WorkflowEvent& event);
			void assignIfPossible(WorkItem& item);
			void runAutomatic(WorkItem& item);
			void applyDecision(const RoutingDecision& decision);
			void evaluateInstance();

			WorkflowInstance& instance;
			DefinitionPolicies policies;
			std::deque<WorkflowEvent> pending;
			bool draining;
	};
} // namespace tividy

#endif // TIVIDY_RUNTIME_FLOWCONTROLLER_H