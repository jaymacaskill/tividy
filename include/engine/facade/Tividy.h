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
 * @file Tividy.h
 * @brief Facade: the single, simple entry point to the whole engine (used by the TUI and by tests).
 */

#ifndef TIVIDY_FACADE_TIVIDY_H
#define TIVIDY_FACADE_TIVIDY_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/commands/Command.h"
#include "engine/commands/CommandFactory.h"
#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Clock.h"
#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"
#include "engine/core/WorkflowData.h"
#include "engine/definition/WorkflowLibrary.h"
#include "engine/facade/IndustryPack.h"
#include "engine/runtime/WorkflowInstance.h"

namespace tividy
{

	/** One row of a "my work" list. */
	struct WorkItemView
	{
		Id instance_id;
		Id item_id;
		std::string name;
		ItemStatus status;
		TimePoint due_at;        /**< The item's deadline; TimePoint() when none. */
	};

	/** Progress of one instance, computed with a ProgressVisitor. */
	struct ProgressSummary
	{
		std::size_t total;
		std::map<ItemStatus, std::size_t> by_status;
		double percent_settled;
	};

	/**
	 * @brief Facade (GoF "Facade"). Hides packs, definitions, instances, commands and connectors behind a few calls.
	 *
	 * ### Time
	 * Tividy runs on SIMULATED time: its ManualClock only moves when advanceTime() is called, which also sweeps every Running instance for
	 * lapsed deadlines. This makes deadline-driven behaviour demonstrable in seconds; a production deployment would call advanceTime()
	 * from a timer.
	 *
	 * ### Ownership
	 * Owns: clock, ParticipantDirectory, ConnectorRegistry, installed packs, WorkflowLibrary, all instances (in creation order),
	 * CommandFactory and one ClientProfile per pack. Not copyable.
	 *
	 * ### Error policy
	 * Starting/inspecting methods throw NotFoundError for unknown names or ids. ACTION methods (perform, execute, undo) never throw for
	 * user mistakes; they return CommandResult.
	 */
	class Tividy
	{
		public:
			/** @param clock Simulated clock; if null a ManualClock starting at the current real time is created. */
			explicit Tividy(std::unique_ptr<ManualClock> clock = std::unique_ptr<ManualClock>());
			~Tividy();

			/**
			 * @brief Installs a pack. Steps, in order:
			 *  1. reject a null pack or an already-installed key (EngineError);
			 *  2. build its workflow definition with StandardWorkflowBuilder;
			 *  3. add participants and connectors to the shared services;
			 *  4. register the definition and client profile, then retain the pack.
			 * A build failure registers nothing. Later failures are not transactional and may leave
			 * collaborators or a definition registered; this is not a full rollback guarantee.
			 */
			void installPack(std::unique_ptr<IndustryPack> pack);

			std::vector<std::string> packKeys() const;

			/** @throws NotFoundError. */
			const ClientProfile& profile(const std::string& pack_key) const;

			/** @return key of the pack that owns workflow @p workflow_name. @throws NotFoundError. */
			std::string packOf(const std::string& workflow_name) const;

			std::vector<std::string> workflowNames() const;

			/**
			 * @brief Creates and starts an independent instance of @p workflow_name.
			 * Steps: def = library.get(name); id = "inst-<n>" (n counts from 1); inst = def->createInstance(id, data, services);
			 * inst->addObserver(NotificationObserver(connectors_ref)); store inst; inst->start(); return id.
			 * @throws NotFoundError for an unknown workflow.
			 */
			Id startWorkflow(const std::string& workflow_name, const WorkflowData& data);

			/** @return all instance ids in creation order. */
			std::vector<Id> instanceIds() const;

			/** @throws NotFoundError. */
			WorkflowInstance& instance(const Id& instance_id);
			const WorkflowInstance& instance(const Id& instance_id) const;

			/**
			 * @brief Performs a user action by verb through the instance's ActionHistory.
			 * @param verb        One of commandFactory().verbs().
			 * @param instance_id The instance to act on.
			 * @param item_id     The item to act on.
			 * @param actor_id    The acting participant (must exist in the directory).
			 * @param outcome     Result reported (complete/submit/approve); ignored by other verbs.
			 * @param text        Reason (reject/escalate/cancel); ignored otherwise.
			 * @return the command result; an unknown verb, instance, item or actor yields failure(...) (never throws).
			 */
			CommandResult perform(const std::string& verb, const Id& instance_id, const Id& item_id, const Id& actor_id,
								  const Outcome& outcome = Outcome(), const std::string& text = "");

			/** @brief Runs a prebuilt command (e.g. a MacroCommand) through the instance's ActionHistory. Never throws for user mistakes. */
			CommandResult execute(const Id& instance_id, std::unique_ptr<Command> command);

			/** @brief Undoes the instance's most recent reversible command. @see ActionHistory::undoLast(). */
			CommandResult undo(const Id& instance_id);

			const CommandFactory& commandFactory() const;

			/** @return items awaiting @p participant_id across ALL Running instances (instance creation order, then item order). */
			std::vector<WorkItemView> workFor(const Id& participant_id) const;

			/** @return every audit line of the instance via AuditLog::format(), oldest first. @throws NotFoundError. */
			std::vector<std::string> history(const Id& instance_id) const;

			ProgressSummary progress(const Id& instance_id) const;

			/** @brief Publishes an ExternalSignal(message = @p signal_name) on the instance (see DefinitionPolicies::cancel_signals). @throws NotFoundError. */
			void signal(const Id& instance_id, const std::string& signal_name);

			/**
			 * @brief Moves simulated time forward by @p seconds, then sweeps every Running instance for lapsed deadlines.
			 * @param seconds >= 0 (else EngineError).
			 * @return the total number of DeadlineMissed events published; the FlowController of each instance has already reacted
			 *         (expired or escalated the items) when this returns.
			 */
			std::size_t advanceTime(long seconds);

			/** @return the current simulated time. */
			TimePoint now() const;

			ParticipantDirectory& participants();

			ConnectorRegistry& connectors();
		private:
			Tividy(const Tividy&);              // not defined: non-copyable (C++11 idiom)
			Tividy& operator=(const Tividy&);   // not defined

			std::unique_ptr<ManualClock> clock;
			ParticipantDirectory directory;
			ConnectorRegistry connectors_ref;
			Services services;
			std::map<std::string, std::unique_ptr<IndustryPack> > packs;
			std::map<Id, ClientProfile> profiles;
			WorkflowLibrary library;
			CommandFactory command_factory;
			std::vector<Id> order;
			std::map<Id, std::unique_ptr<WorkflowInstance> > instances;
			unsigned counter;
	};
} // namespace tividy

#endif // TIVIDY_FACADE_TIVIDY_H