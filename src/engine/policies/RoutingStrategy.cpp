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

RoutingStrategy.cpp
*/

#ifndef TIVIDY_POLICIES_ROUTINGSTRATEGY_CPP
#define TIVIDY_POLICIES_ROUTINGSTRATEGY_CPP

#include "engine/policies/RoutingStrategy.h"

namespace tividy
{
	RoutingStrategy::~RoutingStrategy() { }

	RoutingDecision DependencyRouting::decide(const WorkItem& /* finished */, const Outcome& /* outcome */, const WorkflowData& /* data */) const
	{
		return RoutingDecision();
	}

	OutcomeTableRouting& OutcomeTableRouting::addRule(const std::string& routing_key, const std::vector<Id>& activate, const std::vector<Id>& skip)
	{
		RoutingDecision d;
		d.activate = activate;
		d.skip = skip;
		rules[routing_key] = d;
		return *this;
	}

	OutcomeTableRouting& OutcomeTableRouting::setDefault(const std::vector<Id>& activate, const std::vector<Id>& skip)
	{
		default_rule.activate = activate;
		default_rule.skip = skip;
		has_default = true;
		return *this;
	}

	RoutingDecision OutcomeTableRouting::decide(const WorkItem& /* finished */, const Outcome& outcome, const WorkflowData& /* data */) const
	{
		std::map<std::string, RoutingDecision>::const_iterator it = rules.find(outcome.routingKey());
		if (it != rules.end()) return it->second;
		if (has_default) return default_rule;
		return RoutingDecision();
	}

	DataValueRouting::DataValueRouting(const std::string& data_key) :
		key(data_key), has_default(false) { }

	DataValueRouting& DataValueRouting::addRule(const std::string& value, const std::vector<Id>& activate, const std::vector<Id>& skip)
	{
		RoutingDecision d;
		d.activate = activate;
		d.skip = skip;
		rules[value] = d;
		return *this;
	}

	DataValueRouting& DataValueRouting::setDefault(const std::vector<Id>& activate, const std::vector<Id>& skip)
	{
		default_rule.activate = activate;
		default_rule.skip = skip;
		has_default = true;
		return *this;
	}

	RoutingDecision DataValueRouting::decide(const WorkItem& /* finished */, const Outcome& /* outcome */, const WorkflowData& data) const
	{
		std::map<std::string, RoutingDecision>::const_iterator it = rules.find(data.getOr(key, ""));
		if (it != rules.end()) return it->second;
		if (has_default) return default_rule;
		return RoutingDecision();
	}
} // namespace tividy

#endif // TIVIDY_POLICIES_ROUTINGSTRATEGY_CPP