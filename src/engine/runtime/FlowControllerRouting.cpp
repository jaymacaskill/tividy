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

FlowControllerRouting.cpp
*/

#ifndef TIVIDY_RUNTIME_FLOWCONTROLLER_ROUTING_CPP
#define TIVIDY_RUNTIME_FLOWCONTROLLER_ROUTING_CPP

#include <stdexcept>

#include "engine/runtime/FlowController.h"

namespace tividy
{
	/**
	 * @brief Applies a RoutingStrategy decision to the instance's work-item tree.
	 * @param decision The ordered activation and skip targets produced by a routing policy.
	 * @pre Called by FlowController::react() after a completed item has selected its routing policy.
	 * @details The agreed contract is to process activation targets before skip targets. Unknown targets or
	 *          targets in an incompatible status are reported as EngineNote events; valid targets use the
	 *          WorkItem lifecycle operations. RoutingStrategy is the Strategy participant; FlowController
	 *          is its context and owns applying the returned decision.
	 * @note Integration boundary with Ruan: react() selects the per-source policy from DefinitionPolicies,
	 *       calls decide(finished, outcome, data), and passes that value here before calling
	 *       releaseEligible() and evaluateInstance(). This method does not select a policy or handle events.
	 * @todo Implement the decision traversal under the approved routing tests. This scaffold intentionally
	 *       fails loudly instead of reporting a false success.
	 * @throws std::logic_error Until this scaffold is replaced by the implementation.
	 */
	void FlowController::applyDecision(const RoutingDecision& decision)
	{
		(void)decision;
		throw std::logic_error("FlowController::applyDecision is scaffold-only; implement through TDD");
	}

	/**
	 * @brief Publishes every currently eligible leaf work item as Available.
	 * @pre The instance and its root item tree are alive; a non-Running instance is a no-op per the class contract.
	 * @details The approved rule is a pre-order traversal. A non-composite item is eligible when it is Created
	 *          and every dependency on that item and each ancestor StageItem resolves to a settled item.
	 *          WorkItem::publish() emits ItemAvailable, which Ruan's event handler queues and drains without
	 *          recursive event processing. Composite stage nodes themselves are not published.
	 *          The agreed behavior is a no-op when the instance is not Running.
	 * @note Integration boundary with Ruan: onEvent()/react() may call this after ItemCompleted or ItemSkipped;
	 *       WorkflowInstance::start() also calls it directly. Errors raised while releasing work therefore need
	 *       an agreed policy for both event-driven and direct-start callers.
	 * @todo Implement the traversal and dependency checks under the approved routing tests. This scaffold
	 *       intentionally fails loudly instead of silently leaving work unavailable.
	 * @throws std::logic_error Until this scaffold is replaced by the implementation.
	 */
	void FlowController::releaseEligible()
	{
		throw std::logic_error("FlowController::releaseEligible is scaffold-only; implement through TDD");
	}
} // namespace tividy

#endif // TIVIDY_RUNTIME_FLOWCONTROLLER_ROUTING_CPP
