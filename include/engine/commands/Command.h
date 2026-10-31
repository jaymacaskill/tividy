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
 * @file Command.h
 * @brief Command pattern: the abstract Command and the result it reports.
 */

#ifndef TIVIDY_COMMANDS_COMMAND_H
#define TIVIDY_COMMANDS_COMMAND_H

#include <string>

namespace tividy
{

	/** Result of running or undoing a command. Ordinary user mistakes are reported here, not thrown. */
	struct CommandResult
	{
		static CommandResult ok(const std::string& message = "");
		static CommandResult failure(const std::string& message);

		bool success;
		std::string message;   /**< Text for the TUI: what happened, or why it did not. */
	};

	/**
	 * @brief Command (GoF "Command"): a request as an object so it can be logged, undone, combined (MacroCommand) and decorated.
	 * Invoker = ActionHistory. Receivers = WorkItem. Commands execute once by default; implementations that return true from
	 * supportsRetry() may execute again after a failed attempt or a successful undo().
	 */
	class Command
	{
		public:
			virtual ~Command();

			/** @brief Runs the request. @return success or failure with a reason. Never throws EngineError. */
			virtual CommandResult execute() = 0;

			/** @brief Reverses a successful execute(). @return failure if not possible. @pre execute() succeeded and isReversible(). */
			virtual CommandResult undo() = 0;

			/** @return true if this command supports execute() again after failure or successful undo(). */
			virtual bool supportsRetry() const;

			/** @return true if undo() can restore the state before execute(). Before execute() returns true. */
			virtual bool isReversible() const = 0;

			/** @return one line for history lists, e.g. "Thandi completed 'Verify documents' [DOCS_INVALID]". */
			virtual std::string describe() const = 0;
	};
} // namespace tividy

#endif // TIVIDY_COMMANDS_COMMAND_H
