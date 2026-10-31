/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 30 October 2026
*/

/**
 * @file InstanceCommand.cpp
 * @brief Template Method & Memento: handles the pre/post execution sequence for user actions.
 */

#include "engine/commands/WorkItemCommands.h"
#include "engine/core/Errors.h"
#include "engine/events/WorkflowEvent.h"

namespace tividy
{
	InstanceCommand::InstanceCommand(WorkflowInstance& instance, const Id& item_id, const Participant& actor)
		: instance(instance), 
		  item_id(item_id), 
		  actor(actor), 
		  irreversible_before(0), 
		  executed(false)
	{
	}

	InstanceCommand::~InstanceCommand() = default;

	CommandResult InstanceCommand::execute()
	{
		// 1. Find the item
		WorkItem* item = instance.findItem(item_id);
		if (!item)
		{
			return CommandResult::failure("no such item: " + item_id);
		}

		// 2. Capture the snapshot and irreversible count BEFORE doing anything
		before = instance.captureSnapshot();
		irreversible_before = instance.context().irreversibleCount();

		try
		{
			// 3. Run the specific subclass logic (Template Method primitive)
			doExecute(*item, actor);
		}
		catch (const EngineError& e)
		{
			// 4. Failure! Restore the snapshot immediately and return the error message.
			if (before)
			{
				instance.restoreSnapshot(*before);
				before.reset();
			}
			return CommandResult::failure(e.what());
		}

		// 5. Success!
		executed = true;
		return CommandResult::ok(describe());
	}

	CommandResult InstanceCommand::undo()
	{
		if (!executed)
		{
			return CommandResult::failure("cannot undo: command was not executed");
		}
		
		if (!isReversible())
		{
			return CommandResult::failure("cannot undo: action was irreversible");
		}

		if (before)
		{
			instance.restoreSnapshot(*before);
			before.reset();
			executed = false;
			
			// Publish the ItemReverted event so the AuditLog knows it happened
			WorkflowEvent revertEvent(EventType::ItemReverted, instance.id(), item_id, instance.context().clock().now());
			revertEvent.byActor(actor.id()).withMessage("Action reverted");
			instance.events().notify(revertEvent);
			
			return CommandResult::ok("Undid: " + describe());
		}

		return CommandResult::failure("cannot undo: no snapshot available");
	}

	bool InstanceCommand::isReversible() const
	{
		if (!executed) return true;
		return instance.context().irreversibleCount() == irreversible_before;
	}

	std::string InstanceCommand::describe() const
	{
		std::string desc = actor.displayName() + " " + verb();
		
		WorkItem* item = instance.findItem(item_id);
		if (item)
		{
			desc += " '" + item->spec().name() + "'";
		}
		
		desc += detail();
		return desc;
	}

	std::string InstanceCommand::detail() const
	{
		return "";
	}
} // namespace tividy