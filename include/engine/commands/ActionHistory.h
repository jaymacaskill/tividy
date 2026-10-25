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
 * @file ActionHistory.h
 * @brief Command pattern "Invoker": runs commands and keeps the undo stack.
 */

#ifndef TIVIDY_COMMANDS_ACTIONHISTORY_H
#define TIVIDY_COMMANDS_ACTIONHISTORY_H

#include <memory>
#include <string>
#include <vector>

#include "engine/commands/Command.h"

namespace tividy
{
	class WorkflowInstance;

	/**
	 * @brief Invoker (GoF "Invoker"). One per WorkflowInstance. It does NOT write the audit log (AuditObserver does, from events).
	 */
	class ActionHistory
	{
		public:
			/** @param instance The owning instance; must outlive this object. */
			explicit ActionHistory(WorkflowInstance& instance);
			~ActionHistory();

			/**
			 * @brief Runs @p command.
			 * @return failure("null command") for null; failure("instance is <status>, not Running") unless the instance is Running;
			 *         otherwise command->execute()'s result. A successful command is pushed on the undo stack (even if irreversible,
			 *         so describeAll() is complete); a failed one is discarded.
			 */
			CommandResult execute(std::unique_ptr<Command> command);

			/**
			 * @brief Undoes the most recent successful command.
			 * @return failure("nothing to undo") if empty; failure("cannot undo: <describe()> is irreversible") if the top command
			 *         !isReversible() (stack unchanged); otherwise the command's undo() result, popping it only on success.
			 */
			CommandResult undoLast();

			std::size_t size() const;

			/** @return describe() of every stacked command, oldest first, prefixed "1. ", "2. ", ... */
			std::vector<std::string> describeAll() const;
		private:
			WorkflowInstance& instance;
			std::vector<std::unique_ptr<Command> > executed;
	};
} // namespace tividy

#endif // TIVIDY_COMMANDS_ACTIONHISTORY_H