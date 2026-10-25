/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 25 October 2026
*/

/**
 * @file WorkflowDefinitionValidation.cpp
 * @brief Read-only structural checks for reusable workflow definitions.
 * @details Routing policy keys must name existing source items. Routing targets
 * are checked at runtime by FlowController; unknown targets produce EngineNote events.
 */

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "engine/core/Errors.h"
#include "engine/definition/WorkflowDefinition.h"
#include "engine/items/StageItem.h"
#include "engine/items/WorkItemVisitor.h"

std::vector<std::string> tividy::WorkflowDefinition::validationProblems() const
{
	std::vector<std::string> problems;

	if (!policies_ref.assignment)
	{
		problems.push_back("assignment policy is null");
	}

	if (!template_root)
	{
		problems.push_back("workflow definition has no root item");
		return problems;
	}

	/** Identifies stages through Visitor, including empty or decorated stages. */
	class StageCollector : public tividy::WorkItemVisitor
	{
		public:
			std::set<const tividy::WorkItemSpec*> stages;

			void visitTask(const tividy::TaskItem&) override {}

			void visitStage(const tividy::StageItem& stage) override
			{
				stages.insert(&stage.spec());
			}
	};

	StageCollector collector;
	template_root->accept(collector);

	std::map<tividy::Id, const tividy::WorkItemSpec*> by_id;
	std::set<tividy::Id> duplicate_ids;
	std::vector<const tividy::WorkItemSpec*> specs;
	std::vector<tividy::WorkItem*> pending(1, template_root.get());

	// Collect every id before checking dependencies, so forward references work.
	while (!pending.empty())
	{
		tividy::WorkItem* item = pending.back();
		pending.pop_back();
		const tividy::WorkItemSpec& spec = item->spec();
		const std::vector<tividy::WorkItem*> children = item->children();
		specs.push_back(&spec);

		if (!by_id.insert(std::make_pair(spec.id(), &spec)).second &&
			duplicate_ids.insert(spec.id()).second)
		{
			problems.push_back("duplicate item id: " + spec.id());
		}

		// Decorators forward spec(), so their stage identity is preserved.
		if (collector.stages.count(&spec) != 0 && children.empty())
		{
			problems.push_back("stage '" + spec.id() + "' has no children");
		}

		if (spec.requiresReview() && spec.reviewerRole().empty())
		{
			problems.push_back("item '" + spec.id() + "' requires review but has no reviewer role");
		}

		if (spec.isAutomatic() && spec.requiresReview())
		{
			problems.push_back("automatic item '" + spec.id() + "' must not require review");
		}

		if (!spec.timeoutCode().empty() && spec.dueAfterSeconds() <= 0)
		{
			problems.push_back("item '" + spec.id() + "' has a timeout code without a positive deadline");
		}

		if (spec.isAutomatic() && spec.dueAfterSeconds() != 0)
		{
			problems.push_back("automatic item '" + spec.id() + "' must not have a deadline");
		}

		pending.insert(pending.end(), children.rbegin(), children.rend());
	}

	std::map<tividy::Id, std::set<tividy::Id> > dependencies;
	for (const tividy::WorkItemSpec* spec : specs)
	{
		std::set<tividy::Id> checked;
		for (const tividy::Id& predecessor : spec->dependencies())
		{
			// Repeated dependencies are allowed and need only one diagnostic.
			if (!checked.insert(predecessor).second)
			{
				continue;
			}

			if (predecessor == spec->id())
			{
				problems.push_back("item '" + spec->id() + "' depends on itself");
			}
			else if (by_id.count(predecessor) == 0)
			{
				problems.push_back("item '" + spec->id() + "' depends on unknown item '" + predecessor + "'");
			}
			else
			{
				dependencies[spec->id()].insert(predecessor);
			}
		}
	}

	// Routing targets are checked at runtime; validate only configured source ids.
	for (const auto& policy : policies_ref.routing)
	{
		if (by_id.count(policy.first) == 0)
		{
			problems.push_back("routing source '" + policy.first + "' does not exist");
		}
	}

	/** Three-colour DFS reports cycles without treating shared prerequisites as cycles. */
	enum class VisitState { Unvisited, Visiting, Visited };
	std::map<tividy::Id, VisitState> states;
	std::vector<tividy::Id> path;
	std::function<void(const tividy::Id&)> visit;

	visit = [&](const tividy::Id& id)
	{
		states[id] = VisitState::Visiting;
		path.push_back(id);

		for (const tividy::Id& predecessor : dependencies[id])
		{
			if (states[predecessor] == VisitState::Unvisited)
			{
				visit(predecessor);
			}
			else if (states[predecessor] == VisitState::Visiting)
			{
				std::ostringstream cycle;
				cycle << "dependency cycle: ";
				const auto first = std::find(path.begin(), path.end(), predecessor);
				for (auto item = first; item != path.end(); ++item)
				{
					cycle << *item << " -> ";
				}
				cycle << predecessor;
				problems.push_back(cycle.str());
			}
		}

		path.pop_back();
		states[id] = VisitState::Visited;
	};

	for (const auto& item : by_id)
	{
		if (states[item.first] == VisitState::Unvisited)
		{
			visit(item.first);
		}
	}

	return problems;
}

void tividy::WorkflowDefinition::validate() const
{
	const std::vector<std::string> problems = validationProblems();
	if (problems.empty())
	{
		return;
	}

	std::ostringstream message;
	for (std::size_t i = 0; i < problems.size(); ++i)
	{
		if (i != 0)
		{
			message << "; ";
		}
		message << problems[i];
	}

	throw tividy::InvalidDefinitionError(message.str());
}
