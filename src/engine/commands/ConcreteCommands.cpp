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

/** @file ConcreteCommands.cpp
 * @brief Lifecycle receivers for the seven user commands.
 * Snapshot, rollback and audit behaviour belong to InstanceCommand.
 */
#include "engine/commands/WorkItemCommands.h"

namespace tividy
{

	StartCommand::StartCommand(WorkflowInstance& instance, const Id& item_id,
		const Participant& actor) :
		InstanceCommand(instance, item_id, actor) {}

	void StartCommand::doExecute(WorkItem& item, const Participant& actor)
	{
		item.start(actor);
	}

	std::string StartCommand::verb() const { return "started"; }

	SubmitCommand::SubmitCommand(WorkflowInstance& instance, const Id& item_id,
		const Participant& actor, const Outcome& outcome) :
		InstanceCommand(instance, item_id, actor), outcome(outcome) {}

	void SubmitCommand::doExecute(WorkItem& item, const Participant& actor)
	{
		item.submit(actor, outcome);
	}

	std::string SubmitCommand::verb() const { return "submitted"; }

	std::string SubmitCommand::detail() const
	{
		return " [" + outcome.routingKey() + "]";
	}

	CompleteCommand::CompleteCommand(WorkflowInstance& instance, const Id& item_id,
		const Participant& actor, const Outcome& outcome) :
		InstanceCommand(instance, item_id, actor), outcome(outcome) {}

	void CompleteCommand::doExecute(WorkItem& item, const Participant& actor)
	{
		item.complete(actor, outcome);
	}

	std::string CompleteCommand::verb() const { return "completed"; }

	std::string CompleteCommand::detail() const
	{
		return " [" + outcome.routingKey() + "]";
	}

	ApproveCommand::ApproveCommand(WorkflowInstance& instance, const Id& item_id,
		const Participant& actor, const Outcome& outcome) :
		InstanceCommand(instance, item_id, actor), outcome(outcome) {}

	void ApproveCommand::doExecute(WorkItem& item, const Participant& actor)
	{
		item.approve(actor, outcome);
	}

	std::string ApproveCommand::verb() const { return "approved"; }

	std::string ApproveCommand::detail() const
	{
		return " [" + outcome.routingKey() + "]";
	}

	RejectCommand::RejectCommand(WorkflowInstance& instance, const Id& item_id,
		const Participant& actor, const std::string& reason) :
		InstanceCommand(instance, item_id, actor), reason(reason) {}

	void RejectCommand::doExecute(WorkItem& item, const Participant& actor)
	{
		item.reject(actor, reason);
	}

	std::string RejectCommand::verb() const { return "rejected"; }

	std::string RejectCommand::detail() const
	{
		return ": " + reason;
	}

	EscalateCommand::EscalateCommand(WorkflowInstance& instance, const Id& item_id,
		const Participant& actor, const std::string& reason) :
		InstanceCommand(instance, item_id, actor), reason(reason) {}

	void EscalateCommand::doExecute(WorkItem& item, const Participant&)
	{
		item.escalate(reason);
	}

	std::string EscalateCommand::verb() const { return "escalated"; }

	std::string EscalateCommand::detail() const
	{
		return ": " + reason;
	}

	CancelCommand::CancelCommand(WorkflowInstance& instance, const Id& item_id,
		const Participant& actor, const std::string& reason) :
		InstanceCommand(instance, item_id, actor), reason(reason) {}

	void CancelCommand::doExecute(WorkItem& item, const Participant&)
	{
		item.cancel(reason);
	}

	std::string CancelCommand::verb() const { return "cancelled"; }

	std::string CancelCommand::detail() const
	{
		return ": " + reason;
	}
} // namespace tividy
