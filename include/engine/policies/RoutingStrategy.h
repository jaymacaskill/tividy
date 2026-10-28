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
 * @file RoutingStrategy.h
 * @brief Strategy: what happens next when an item finishes?
 */

#ifndef TIVIDY_POLICIES_ROUTINGSTRATEGY_H
#define TIVIDY_POLICIES_ROUTINGSTRATEGY_H

#include <map>
#include <string>
#include <vector>

#include "engine/core/Outcome.h"
#include "engine/core/WorkflowData.h"
#include "engine/items/WorkItem.h"

namespace tividy
{

	/** The branch choice made by a RoutingStrategy. */
	struct RoutingDecision
	{
		std::vector<Id> activate;  /**< Created items to make Available now (branch taken). */
		std::vector<Id> skip;      /**< Created/Available items to Skip (branch not taken). */
	};

	/**
	 * @brief Strategy (GoF "Strategy"). Context = FlowController.
	 * Registered per SOURCE item id in DefinitionPolicies; called once each time that item reaches Completed.
	 * FlowController applies the decision first (activate, then skip), THEN performs "dependency release": every still-Created
	 * item whose dependencies are all settled becomes Available. So a strategy only mentions items it treats specially.
	 */
	class RoutingStrategy
	{
		public:
			virtual ~RoutingStrategy();

			/**
			 * @param finished The item that just completed (its lastOutcome is recorded).
			 * @param outcome  The outcome it completed with.
			 * @param data     The instance's data (read-only).
			 * @return the decision. Unknown ids or illegal statuses are ignored by FlowController with an EngineNote. Pure and deterministic.
			 */
			virtual RoutingDecision decide(const WorkItem& finished, const Outcome& outcome, const WorkflowData& data) const = 0;
	};

	/** Default: no explicit decision; relies entirely on dependency release. */
	class DependencyRouting : public RoutingStrategy
	{
		public:
			RoutingDecision decide(const WorkItem& finished, const Outcome& outcome, const WorkflowData& data) const override;
	};

	/**
	 * @brief Branching by outcome, configured as data so most industries need NO custom routing class.
	 * Lookup: outcome.routingKey() in the rule table; else the default rule if set; else an empty decision.
	 * Configure with addRule()/setDefault() BEFORE sharing; never modify afterwards.
	 */
	class OutcomeTableRouting : public RoutingStrategy
	{
		public:
			OutcomeTableRouting() : has_default(false) { }
			/** Rule for @p routing_key (an Outcome code such as "DOCS_INVALID", or a kind name such as "Rejected"). */
			OutcomeTableRouting& addRule(const std::string& routing_key, const std::vector<Id>& activate,
										 const std::vector<Id>& skip);

			OutcomeTableRouting& setDefault(const std::vector<Id>& activate, const std::vector<Id>& skip);

			RoutingDecision decide(const WorkItem& finished, const Outcome& outcome, const WorkflowData& data) const override;
		private:
			std::map<std::string, RoutingDecision> rules;
			RoutingDecision default_rule;
			bool has_default;
	};

	/**
	 * @brief Branching by a value stored in the instance data (e.g. install_type), configured as data.
	 * Lookup: data.getOr(key, "") in the rule table; else the default rule if set; else empty. Same configure-then-share rule.
	 */
	class DataValueRouting : public RoutingStrategy
	{
		public:
			explicit DataValueRouting(const std::string& data_key);

			DataValueRouting& addRule(const std::string& value, const std::vector<Id>& activate, const std::vector<Id>& skip);

			DataValueRouting& setDefault(const std::vector<Id>& activate, const std::vector<Id>& skip);

			RoutingDecision decide(const WorkItem& finished, const Outcome& outcome, const WorkflowData& data) const override;
		private:
			std::string key;
			std::map<std::string, RoutingDecision> rules;
			RoutingDecision default_rule;
			bool has_default;
	};
} // namespace tividy

#endif // TIVIDY_POLICIES_ROUTINGSTRATEGY_H