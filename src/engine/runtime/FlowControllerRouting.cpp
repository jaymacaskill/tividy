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
 * @file FlowControllerRouting.cpp
 * @brief Implements the routing half of FlowController: applying a routing decision and releasing every item whose dependencies are met.
 */

#ifndef TIVIDY_RUNTIME_FLOWCONTROLLER_ROUTING_CPP
#define TIVIDY_RUNTIME_FLOWCONTROLLER_ROUTING_CPP

#include <stdexcept>
#include <functional>

#include "engine/runtime/FlowController.h"
#include "engine/runtime/WorkflowInstance.h"

namespace tividy
{
	/**
	 * @brief Applies a RoutingStrategy decision to the instance's work-item tree.
	 * @param decision The activation and skip targets returned by a routing policy.
	 * @details Activation targets are processed first. An activation target is published only when it resolves
	 *          to an item in Created status; a skip target is skipped only when it resolves to an item in Created
	 *          or Available status. Missing targets and targets in other statuses produce an EngineNote event
	 *          stating that routing ignored the target. This method applies the supplied decision; it does not
	 *          select a routing policy or handle events.
	 */

	void FlowController::applyDecision(const RoutingDecision &decision)
	{
		// Report invalid routing targets
		auto reportIgnored = [this](const Id &id)
		{
			WorkflowEvent event(
				EventType::EngineNote,
				instance.id(),
				id,
				instance.context().clock().now());

			event.withMessage("routing ignored " + id);
			instance.events().notify(event);
		};

		// Activate selected tasks
		for (const Id &id : decision.activate)
		{
			WorkItem *item = instance.root().find(id);

			if (!item || item->status() != ItemStatus::Created)
			{
				reportIgnored(id);
				continue;
			}

			item->publish();
		}

		// Skip unselected tasks
		for (const Id &id : decision.skip)
		{
			WorkItem *item = instance.root().find(id);

			if (!item ||
				(item->status() != ItemStatus::Created &&
				 item->status() != ItemStatus::Available))
			{
				reportIgnored(id);
				continue;
			}

			item->skip("branch not taken");
		}
	}

	/**
	 * @brief Publishes every currently eligible leaf work item as Available.
	 * @details Does nothing unless the instance is Running. Otherwise, visits the root tree in pre-order.
	 *          An item is traversed only if all of its dependencies resolve to items in a settled status; this
	 *          also means an unmet dependency on a composite stage blocks traversal of its descendants. A
	 *          composite item is not published. A leaf item is published when its status is Created.
	 *          Publishing makes the item Available and emits the corresponding item event.
	 */

	void FlowController::releaseEligible()
	{
		if (instance.status() != InstanceStatus::Running)
			return;

		std::function<void(WorkItem &, bool)> visit;

		visit = [&](WorkItem &item, bool ancestorsReady)
		{
			if (!ancestorsReady)
				return;

			// Check this item's dependencies
			for (const Id &dependency : item.spec().dependencies())
			{
				WorkItem *required = instance.root().find(dependency);

				if (!required || !isSettled(required->status()))
					return;
			}

			// Recursively visit children of composite stages
			std::vector<WorkItem *> children = item.children();

			if (!children.empty())
			{
				for (WorkItem *child : children)
					visit(*child, true);

				return;
			}

			// Only release eligible tasks
			if (item.status() == ItemStatus::Created)
				item.publish();
		};

		visit(instance.root(), true);
	}

} // namespace tividy

#endif // TIVIDY_RUNTIME_FLOWCONTROLLER_ROUTING_CPP
