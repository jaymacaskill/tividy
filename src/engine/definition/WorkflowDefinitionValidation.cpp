/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 27 October 2026
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
	std::map<tividy::Id, std::set<tividy::Id> > parents;
	std::set<tividy::Id> duplicate_ids;
	std::vector<const tividy::WorkItemSpec*> specs;
	std::vector<std::pair<tividy::WorkItem*, const tividy::WorkItemSpec*> > pending;
	pending.emplace_back(template_root.get(), nullptr);

	// Collect every id before checking dependencies, so forward references work.
	while (!pending.empty())
	{
		const auto entry = pending.back();
		pending.pop_back();
		tividy::WorkItem* item = entry.first;
		const tividy::WorkItemSpec& spec = item->spec();
		const std::vector<tividy::WorkItem*> children = item->children();
		specs.push_back(&spec);
		if (entry.second)
		{
			parents[spec.id()].insert(entry.second->id());
		}

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

		for (auto child = children.rbegin(); child != children.rend(); ++child)
		{
			pending.emplace_back(*child, &spec);
		}
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

	// Readiness and completion are separate: a parent's dependencies gate its
	// children, while its completion waits for every child to settle.
	enum class DependencyPhase { Ready, Settled };
	typedef std::pair<tividy::Id, DependencyPhase> DependencyNode;
	std::map<DependencyNode, std::set<DependencyNode> > waits_for;
	for (const auto& item : by_id)
	{
		const DependencyNode ready(item.first, DependencyPhase::Ready);
		const DependencyNode settled(item.first, DependencyPhase::Settled);
		waits_for[settled].insert(ready);
		for (const tividy::Id& predecessor : dependencies[item.first])
		{
			waits_for[ready].insert(DependencyNode(predecessor, DependencyPhase::Settled));
		}
		for (const tividy::Id& parent : parents[item.first])
		{
			waits_for[ready].insert(DependencyNode(parent, DependencyPhase::Ready));
			waits_for[DependencyNode(parent, DependencyPhase::Settled)].insert(settled);
		}
	}

	/** Three-colour DFS includes inherited gates and stage completion constraints. */
	enum class VisitState { Unvisited, Visiting, Visited };
	std::map<DependencyNode, VisitState> states;
	std::vector<DependencyNode> path;
	std::set<std::string> reported_cycles;
	std::function<void(const DependencyNode&)> visit;

	visit = [&](const DependencyNode& node)
	{
		states[node] = VisitState::Visiting;
		path.push_back(node);

		for (const DependencyNode& predecessor : waits_for[node])
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
				std::vector<tividy::Id> ids;
				for (auto item = first; item != path.end(); ++item)
				{
					if (ids.empty() || ids.back() != item->first)
					{
						ids.push_back(item->first);
					}
				}
				if (ids.size() == 1 || ids.back() != predecessor.first)
				{
					ids.push_back(predecessor.first);
				}
				for (std::size_t i = 0; i < ids.size(); ++i)
				{
					if (i != 0) cycle << " -> ";
					cycle << ids[i];
				}
				if (reported_cycles.insert(cycle.str()).second)
				{
					problems.push_back(cycle.str());
				}
			}
		}

		path.pop_back();
		states[node] = VisitState::Visited;
	};

	for (const auto& node : waits_for)
	{
		if (states[node.first] == VisitState::Unvisited)
		{
			visit(node.first);
		}
	}

	return problems;
}
