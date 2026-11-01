/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 1 November 2026

TividyActions.cpp
*/

/**
 * @file TividyActions.cpp
 * @brief Implements the facade's run-time operations: actions, undo, "my work", history, progress, signals and simulated time.
 */
#include "engine/facade/Tividy.h"

#include <exception>
#include <utility>

#include "engine/core/Errors.h"
#include "engine/events/WorkflowEvent.h"
#include "engine/items/WorkItemVisitor.h"

namespace tividy
{
	/** @brief Builds the command for @p verb and runs it through the instance's ActionHistory.
	 * @details Every user mistake (unknown instance, item, actor or verb, or a creator that refuses the request) is
	 * reported as a failed CommandResult; nothing is thrown. */
	CommandResult Tividy::perform(const std::string &verb, const Id &instance_id, const Id &item_id, const Id &actor_id,
								  const Outcome &outcome, const std::string &text)
	{
		const auto found = instances.find(instance_id);
		if (found == instances.end())
			return CommandResult::failure("unknown instance: " + instance_id);

		WorkflowInstance &target = *found->second;

		if (target.findItem(item_id) == nullptr)
			return CommandResult::failure("unknown item: " + item_id);

		const Participant *actor = directory.find(actor_id);
		if (actor == nullptr)
			return CommandResult::failure("unknown participant: " + actor_id);

		std::unique_ptr<Command> command;
		try
		{
			const CommandRequest request = {target, item_id, *actor, outcome, text};
			command = command_factory.create(verb, request);
		}
		catch (const EngineError &error)
		{
			return CommandResult::failure(error.what());
		}

		return target.actions().execute(std::move(command));
	}

	/** @brief Runs a prebuilt command through the instance's ActionHistory; an unknown instance is a failure, not a throw. */
	CommandResult Tividy::execute(const Id &instance_id, std::unique_ptr<Command> command)
	{
		const auto found = instances.find(instance_id);
		if (found == instances.end())
			return CommandResult::failure("unknown instance: " + instance_id);

		return found->second->actions().execute(std::move(command));
	}

	/** @brief Undoes the instance's most recent reversible command; an unknown instance is a failure, not a throw. */
	CommandResult Tividy::undo(const Id &instance_id)
	{
		const auto found = instances.find(instance_id);
		if (found == instances.end())
			return CommandResult::failure("unknown instance: " + instance_id);

		return found->second->actions().undoLast();
	}

	/** @brief Exposes the verb registry so a UI can list verbs or register new ones before the facade is used. */
	const CommandFactory &Tividy::commandFactory() const { return command_factory; }

	/** @brief Collects the items awaiting @p participant_id in every Running instance.
	 * @details Instances are visited in creation order and items in the order WorkflowInstance::itemsFor() returns them. */
	std::vector<WorkItemView> Tividy::workFor(const Id &participant_id) const
	{
		std::vector<WorkItemView> views;
		for (const Id &id : order)
		{
			WorkflowInstance &candidate = *instances.at(id);
			if (candidate.status() != InstanceStatus::Running)
				continue;

			for (const WorkItem *item : candidate.itemsFor(participant_id))
			{
				WorkItemView view;
				view.instance_id = id;
				view.item_id = item->spec().id();
				view.name = item->spec().name();
				view.status = item->status();
				view.due_at = item->dueAt();
				views.push_back(view);
			}
		}
		return views;
	}

	/** @brief Formats every audit entry of the instance, oldest first. @throws NotFoundError for an unknown id. */
	std::vector<std::string> Tividy::history(const Id &instance_id) const
	{
		std::vector<std::string> lines;
		for (const AuditEntry &entry : instance(instance_id).audit().entries())
			lines.push_back(AuditLog::format(entry));
		return lines;
	}

	/** @brief Counts the instance's tasks by status with a ProgressVisitor.
	 * @details by_status holds only statuses that occur. @throws NotFoundError for an unknown id. */
	ProgressSummary Tividy::progress(const Id &instance_id) const
	{
		ProgressVisitor visitor;
		instance(instance_id).root().accept(visitor);

		ProgressSummary summary;
		summary.total = visitor.total();
		summary.percent_settled = visitor.percentSettled();

		const ItemStatus all[] = {ItemStatus::Created,	  ItemStatus::Available, ItemStatus::Assigned,
								  ItemStatus::InProgress, ItemStatus::UnderReview, ItemStatus::Escalated,
								  ItemStatus::Completed,  ItemStatus::Cancelled, ItemStatus::Skipped};
		for (const ItemStatus status : all)
			if (visitor.count(status) > 0)
				summary.by_status[status] = visitor.count(status);

		return summary;
	}

	/** @brief Publishes an ExternalSignal carrying @p signal_name, stamped with the simulated time and actor "system".
	 * @throws NotFoundError for an unknown id. */
	void Tividy::signal(const Id &instance_id, const std::string &signal_name)
	{
		WorkflowInstance &target = instance(instance_id);
		target.events().notify(WorkflowEvent(EventType::ExternalSignal, instance_id, "", clock->now())
								   .byActor("system")
								   .withMessage(signal_name));
	}

	/** @brief Advances the simulated clock, then sweeps each Running instance once.
	 * @return the total number of DeadlineMissed events published.
	 * @throws EngineError if @p seconds is negative; the clock is then left untouched. */
	std::size_t Tividy::advanceTime(long seconds)
	{
		if (seconds < 0)
			throw EngineError("cannot move time backwards");

		clock->advance(seconds);

		std::size_t missed = 0;
		for (const Id &id : order)
		{
			WorkflowInstance &candidate = *instances.at(id);
			if (candidate.status() == InstanceStatus::Running)
				missed += candidate.sweepDeadlines();
		}
		return missed;
	}

	/** @brief Returns the current simulated time. */
	TimePoint Tividy::now() const { return clock->now(); }

	/** @brief Gives access to the participant directory shared by all instances. */
	ParticipantDirectory &Tividy::participants() { return directory; }

	/** @brief Gives access to the connector registry shared by all instances. */
	ConnectorRegistry &Tividy::connectors() { return connectors_ref; }

} // namespace tividy