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
 * @file MacroCommand.cpp
 * @brief Implements MacroCommand operations.
 * @details Commands execute in order and roll back in reverse order.
 * Retry is allowed only when rollback succeeds and all attempted
 * commands support retry.
 */

#include "engine/commands/MacroCommand.h"
#include "engine/core/Errors.h"
#include <stdexcept>
#include <utility>

namespace tividy
{
	MacroCommand::MacroCommand(const std::string &name)
		: name(name),
		  executed_count(0),
		  executed(false),
		  started(false),
		  retry_allowed(false)
	{
	}

	MacroCommand::~MacroCommand() = default;

	std::size_t MacroCommand::size() const
	{
		return children.size();
	}

	void MacroCommand::add(std::unique_ptr<Command> child)
	{
		if (!child)
			throw InvalidStateError("cannot add a null command");

		if (started)
			throw InvalidStateError(
				"cannot add a command after macro execution");

		children.push_back(std::move(child));
	}

	CommandResult MacroCommand::execute()
	{
		if (executed)
			return CommandResult::failure("Macro already executed");

		if (started && !retry_allowed)
			return CommandResult::failure(
				"Macro cannot be retried safely");

		executed = true;
		started = true;
		executed_count = 0;

		for (std::size_t i = 0; i < children.size(); ++i)
		{
			CommandResult result = children[i]->execute();

			if (!result.success)
			{
				const std::size_t completed = executed_count;
				std::size_t rolled_back = 0;
				bool rollback_complete = true;
				bool retry_safe = true;

				// Check retry capability of every attempted command.
				for (std::size_t k = 0; k <= i; ++k)
				{
					if (!children[k]->supportsRetry())
						retry_safe = false;
				}

				// Roll back successful commands in reverse order.
				for (std::size_t j = completed; j > 0; --j)
				{
					if (!children[j - 1]->isReversible())
					{
						rollback_complete = false;
						continue;
					}

					CommandResult undo_result =
						children[j - 1]->undo();

					if (undo_result.success)
						++rolled_back;
					else
						rollback_complete = false;
				}

				executed_count = 0;

				// Retry requires complete rollback and safe children.
				retry_allowed = rollback_complete && retry_safe;

				// A failed execution is never a successful execution.
				executed = false;

				return CommandResult::failure(
					name + " stopped at step " +
					std::to_string(i + 1) +
					": " + result.message +
					"; rolled back " +
					std::to_string(rolled_back) +
					" of " + std::to_string(completed) +
					" step(s)" +
					(rollback_complete
						 ? ""
						 : "; rollback incomplete") +
					(rollback_complete && !retry_safe
						 ? "; retry unavailable"
						 : ""));
			}

			++executed_count;
		}

		return CommandResult::ok(
			name + ": " +
			std::to_string(executed_count) +
			" step(s) done");
	}

	CommandResult MacroCommand::undo()
	{
		if (!executed || executed_count != children.size())
			return CommandResult::failure(
				"Macro has not executed successfully");

		if (!isReversible())
			return CommandResult::failure(
				"Macro contains irreversible commands");

		// Undo successful commands in reverse order.
		for (std::size_t i = executed_count; i > 0; --i)
		{
			CommandResult result = children[i - 1]->undo();

			if (!result.success)
				return CommandResult::failure(
					name + " undo failed at step " +
					std::to_string(i) +
					": " + result.message);
		}

		executed_count = 0;

		// Evaluate retry capability before clearing executed.
		retry_allowed = supportsRetry();
		executed = false;

		return CommandResult::ok(name + ": undo completed");
	}

	bool MacroCommand::isReversible() const
	{
		for (const auto &child : children)
		{
			if (!child->isReversible())
				return false;
		}

		return true;
	}

	bool MacroCommand::supportsRetry() const
	{
		// Prevent retry after unsafe failure or incomplete rollback.
		// This also protects parent macros containing this macro.
		if (started && !executed && !retry_allowed)
			return false;

		for (const auto &child : children)
		{
			if (!child->supportsRetry())
				return false;
		}

		return true;
	}

	std::string MacroCommand::describe() const
	{
		std::string result =
			name + " [" +
			std::to_string(children.size()) +
			" step(s)]";

		for (const auto &child : children)
		{
			std::string text = child->describe();
			std::string indented;
			for (std::size_t i = 0; i < text.size(); ++i)
			{
				indented += text[i];
				if (text[i] == '\n') indented += "  ";
			}
			result += "\n  " + indented;
		}

		return result;
	}

} // namespace tividy
