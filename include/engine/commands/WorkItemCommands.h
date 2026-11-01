/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 22 October 2026
*/

/**
 * @file WorkItemCommands.h
 * @brief ConcreteCommands for the user-level lifecycle actions, with Memento-based undo.
 */

#ifndef TIVIDY_COMMANDS_WORKITEMCOMMANDS_H
#define TIVIDY_COMMANDS_WORKITEMCOMMANDS_H

#include <memory>

#include "engine/commands/Command.h"
#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"
#include "engine/items/WorkItem.h"
#include "engine/runtime/WorkflowInstance.h"

namespace tividy
{

	/**
	 * @brief Base of all item commands. Template Method + Memento Caretaker. Subclasses supply only doExecute(), verb(), detail().
	 *
	 * ### execute() skeleton
	 *  1. item = instance.findItem(item_id); if null return failure("no such item: <id>").
	 *  2. before = instance.captureSnapshot(); irreversible_before = instance.context().irreversibleCount().
	 *  3. try { doExecute(*item, actor) } catch (EngineError& e) { instance.restoreSnapshot(*before); return failure(e.what()); }
	 *  4. executed = true; return ok(describe()).
	 * Reactions to the action's events (FlowController releasing successors, automatic tasks running) happen inside step 3, so the
	 * snapshot covers them.
	 *
	 * ### undo()
	 *  @pre executed && isReversible(), else failure("cannot undo: <reason>"). Restores the snapshot, publishes ItemReverted for the
	 *  target item, sets executed = false.
	 *
	 * ### isReversible()
	 *  true before execute(); afterwards true iff instance.context().irreversibleCount() == irreversible_before.
	 */
	class InstanceCommand : public Command
	{
		public:
			InstanceCommand(WorkflowInstance& instance, const Id& item_id, const Participant& actor);
			~InstanceCommand() override;

			CommandResult execute() override;
			CommandResult undo() override;
			bool supportsRetry() const override { return true; }
			bool isReversible() const override;
			std::string describe() const override;   /**< "<actor display name> <verb()> '<item name>'" + detail(). */
		protected:
			/** Performs the item operation, e.g. item.complete(actor, outcome). May throw EngineError. */
			virtual void doExecute(WorkItem& item, const Participant& actor) = 0;

			/** @return past-tense verb: "completed", "approved", ... */
			virtual std::string verb() const = 0;

			/** @return optional trailing text for describe(), e.g. " [DOCS_INVALID]". Default "". */
			virtual std::string detail() const;
		private:
			WorkflowInstance& instance;
			Id item_id;
			Participant actor;
			std::unique_ptr<InstanceMemento> before;
			unsigned irreversible_before;
			bool executed;
	};

	/** item.start(actor); verb "started". */
	class StartCommand : public InstanceCommand
	{
		public:
			StartCommand(WorkflowInstance& instance, const Id& item_id, const Participant& actor);
		protected:
			void doExecute(WorkItem& item, const Participant& actor) override;
			std::string verb() const override;
	};

	/** item.submit(actor, outcome); verb "submitted"; detail " [<outcome routing key>]". */
	class SubmitCommand : public InstanceCommand
	{
		public:
			SubmitCommand(WorkflowInstance& instance, const Id& item_id, const Participant& actor,
						  const Outcome& outcome);
		protected:
			void doExecute(WorkItem& item, const Participant& actor) override;
			std::string verb() const override;
			std::string detail() const override;
		private:
			Outcome outcome;
	};

	/** item.complete(actor, outcome); verb "completed"; detail " [<outcome routing key>]". */
	class CompleteCommand : public InstanceCommand
	{
		public:
			CompleteCommand(WorkflowInstance& instance, const Id& item_id, const Participant& actor,
							const Outcome& outcome);
		protected:
			void doExecute(WorkItem& item, const Participant& actor) override;
			std::string verb() const override;
			std::string detail() const override;
		private:
			Outcome outcome;
	};

	/** item.approve(actor, outcome); verb "approved"; detail " [<outcome routing key>]". */
	class ApproveCommand : public InstanceCommand
	{
		public:
			ApproveCommand(WorkflowInstance& instance, const Id& item_id, const Participant& actor,
						   const Outcome& outcome);
		protected:
			void doExecute(WorkItem& item, const Participant& actor) override;
			std::string verb() const override;
			std::string detail() const override;
		private:
			Outcome outcome;
	};

	/** item.reject(actor, reason); verb "rejected"; detail ": <reason>". */
	class RejectCommand : public InstanceCommand
	{
		public:
			RejectCommand(WorkflowInstance& instance, const Id& item_id, const Participant& actor,
						  const std::string& reason);
		protected:
			void doExecute(WorkItem& item, const Participant& actor) override;
			std::string verb() const override;
			std::string detail() const override;
		private:
			std::string reason;
	};

	/** Manual escalation: item.escalate(reason); verb "escalated"; detail ": <reason>". */
	class EscalateCommand : public InstanceCommand
	{
		public:
			EscalateCommand(WorkflowInstance& instance, const Id& item_id, const Participant& actor,
							const std::string& reason);
		protected:
			void doExecute(WorkItem& item, const Participant& actor) override;
			std::string verb() const override;
			std::string detail() const override;
		private:
			std::string reason;
	};

	/** item.cancel(reason); verb "cancelled"; detail ": <reason>". */
	class CancelCommand : public InstanceCommand
	{
		public:
			CancelCommand(WorkflowInstance& instance, const Id& item_id, const Participant& actor,
						  const std::string& reason);
		protected:
			void doExecute(WorkItem& item, const Participant& actor) override;
			std::string verb() const override;
			std::string detail() const override;
		private:
			std::string reason;
	};
} // namespace tividy

#endif // TIVIDY_COMMANDS_WORKITEMCOMMANDS_H
