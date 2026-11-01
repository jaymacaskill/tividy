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
 * @file MacroCommand.h
 * @brief Composite and Decorator applied to commands: macros that can be stacked and "dressed up" at runtime.
 */

#ifndef TIVIDY_COMMANDS_MACROCOMMAND_H
#define TIVIDY_COMMANDS_MACROCOMMAND_H

#include <memory>
#include <string>
#include <vector>

#include "engine/commands/Command.h"

namespace tividy
{

	/**
	 * @brief Composite (GoF "Composite"): Component = Command, Leaf = InstanceCommand family, Composite = MacroCommand.
	 * A macro is itself a Command, so macros nest, can be decorated, and run through ActionHistory like any command.
	 *
	 * ### execute(): all-or-nothing
	 *  1. For each child in insertion order: r = child->execute().
	 *  2. On the first failure: undo every already-executed child in REVERSE order, report rollback failures,
	 *     and allow another execution only if rollback succeeded and every attempted child supports retry.
	 *  3. If all succeed return ok("<name>: n step(s) done"). An empty macro succeeds with "0 step(s)".
	 * ### undo(): undoes children in reverse order. Requires isReversible().
	 * Successful child undos are retained; a retry resumes at the failed child.
	 * The macro remains executed until all child undos succeed.
	 * ### isReversible(): true iff every child is reversible.
	 * ### supportsRetry(): true iff every child supports retry after failure or successful undo; false during partial undo.
	 * ### describe(): "<name> [n step(s)]" then one indented line per child.
	 */
	class MacroCommand : public Command
	{
		public:
			explicit MacroCommand(const std::string& name);
			~MacroCommand() override;

			/** Appends @p child and takes ownership. @pre child != nullptr and the macro has never started (else InvalidStateError). */
			void add(std::unique_ptr<Command> child);

			std::size_t size() const;
			CommandResult execute() override;
			CommandResult undo() override;
			bool isReversible() const override;
			bool supportsRetry() const override;
			std::string describe() const override;
		private:
			std::string name;
			std::vector<std::unique_ptr<Command> > children;
			std::size_t executed_count;
			std::size_t undone_count; /**< Successfully undone suffix of a successful execution. */
			bool executed;
			bool started;
			bool retry_allowed;
	};

	/** Decorator (GoF "Decorator"): a Command that wraps a Command; by default forwards everything. Decorators stack. */
	class CommandDecorator : public Command
	{
		public:
			/** @pre inner != nullptr. Takes ownership. */
			explicit CommandDecorator(std::unique_ptr<Command> inner);
			~CommandDecorator() override;

			CommandResult execute() override;
			CommandResult undo() override;
			bool isReversible() const override;
			std::string describe() const override;
		protected:
			Command& inner();
		private:
			std::unique_ptr<Command> inner_ref;
	};

	/**
	 * @brief Re-runs the wrapped command when it fails.
	 * execute(): try up to max_attempts times (>= 1); return the first success, else the last failure with " (after k attempts)".
	 * Retrying is safe only when the wrapped command reports supportsRetry(). describe(): inner + " (retry up to n)".
	 */
	class RetryingCommand : public CommandDecorator
	{
		public:
			RetryingCommand(std::unique_ptr<Command> inner, unsigned max_attempts);

			CommandResult execute() override;
			std::string describe() const override;
		private:
			unsigned max_attempts;
	};
} // namespace tividy

#endif // TIVIDY_COMMANDS_MACROCOMMAND_H
