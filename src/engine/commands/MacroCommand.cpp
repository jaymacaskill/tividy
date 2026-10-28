/**
 * @file MacroCommand.cpp
 * @brief Implements MacroCommand operations.
 * @details Child commands are owned in insertion order. Execution stops at the
 * first child failure and attempts to undo previously successful children in
 * reverse order. Results from those rollback attempts are ignored.
 */
#include "engine/commands/MacroCommand.h"
#include "engine/core/Errors.h"
#include <stdexcept>
#include <utility>

namespace tividy
{
	/**
	 * @brief Creates an empty, unexecuted macro.
	 * @param name Display name used in result messages and descriptions.
	 */
	MacroCommand::MacroCommand(const std::string &name) : name(name), executed_count(0), executed(false) {}

	/** @brief Destroys the macro and its owned child commands. */
	MacroCommand::~MacroCommand() = default;

	/** @brief Returns the number of child commands currently owned by the macro. */
	std::size_t MacroCommand::size() const { return children.size(); }

	/**
	 * @brief Adds a child command without executing it.
	 * @param child Non-null command whose ownership is transferred to the macro.
	 * @throws InvalidStateError If @p child is null or this macro has already
	 * been executed.
	 * @details Children are retained in insertion order.
	 */
	void MacroCommand::add(std::unique_ptr<Command> child)
	{
		if (!child)
			throw InvalidStateError("cannot add a null command");
		if (executed)
			throw InvalidStateError("cannot add a command after macro execution");
		children.push_back(std::move(child));
	}

	/**
	 * @brief Executes child commands in insertion order.
	 * @details Stops at the first failed child and attempts to undo all previously
	 * successful children in reverse order. Rollback failures are ignored. A
	 * macro can be executed only once; an empty macro succeeds. On failure,
	 * executed_count is reset to zero and the failed child is not included in
	 * rollback.
	 */

	CommandResult MacroCommand::execute()
	{
		if (executed)
			return CommandResult::failure("Macro already executed");

		executed = true;
		executed_count = 0;

		for (std::size_t i = 0; i < children.size(); ++i)
		{
			CommandResult result = children[i]->execute();

			if (!result.success)
			{
				std::size_t completed = executed_count;

				// Roll back successful commands in reverse order
				for (std::size_t j = completed; j > 0; --j)
					children[j - 1]->undo();

				executed_count = 0;

				return CommandResult::failure(
					name + " stopped at step " + std::to_string(i + 1) +
					": " + result.message + "; rolled back " +
					std::to_string(completed) + " step(s)");
			}

			++executed_count;
		}

		return CommandResult::ok(
			name + ": " + std::to_string(executed_count) +
			" step(s) done");
	}

	/**
	 * @brief Undoes a successfully executed reversible macro.
	 * @details Children are undone in reverse order. Stops and returns a failure
	 * if a child undo fails; otherwise resets the successful execution count.
	 */
	CommandResult MacroCommand::undo()
	{
		if (!executed || executed_count != children.size())
			return CommandResult::failure(
				"Macro has not executed successfully");

		if (!isReversible())
			return CommandResult::failure(
				"Macro contains irreversible commands");

		for (std::size_t i = executed_count; i > 0; --i)
		{
			CommandResult result = children[i - 1]->undo();

			if (!result.success)
				return CommandResult::failure(
					name + " undo failed at step " +
					std::to_string(i) + ": " + result.message);
		}

		executed_count = 0;

		return CommandResult::ok(name + ": undo completed");
	}

	/** @brief Returns whether every child command can be undone. */
	bool MacroCommand::isReversible() const
	{
		for (const auto &child : children)
		{
			if (!child->isReversible())
				return false;
		}
		return true;
	}

	/** @brief Returns the macro name, child count, and indented child descriptions. */
	std::string MacroCommand::describe() const
	{
		std::string result = name + " [" +
							 std::to_string(children.size()) + " step(s)]";

		for (const auto &child : children)
			result += "\n  " + child->describe();

		return result;
	}
} // namespace tividy
