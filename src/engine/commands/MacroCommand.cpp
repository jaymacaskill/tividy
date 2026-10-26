/**
 * @file MacroCommand.cpp
 * @brief Role C MacroCommand support and explicit TDD scaffolds (C++11).
 * @details CommandDecorator and RetryingCommand belong to Aidan and are defined elsewhere.
 * No child dispatch, rollback, undo, reversibility aggregation or description algorithm is implemented here.
 */
#include "engine/commands/MacroCommand.h"
#include "engine/core/Errors.h"
#include <stdexcept>
#include <utility>

namespace tividy
{
	/** @brief Establishes an empty, unexecuted Composite command. @param name Display name used in results. */
	MacroCommand::MacroCommand(const std::string& name) : name(name), executed_count(0), executed(false) {}

	/** @brief Releases the uniquely owned children. Requires the genuine Command destructor at link time. */
	MacroCommand::~MacroCommand() = default;

	/** @brief Returns the number of owned child commands without executing them. */
	std::size_t MacroCommand::size() const { return children.size(); }

	/** @brief Takes ownership of one child without executing it. @pre Non-null child and macro has not executed.
	 * @throws InvalidStateError When either insertion precondition is violated.
	 * @details The ownership vector contains only non-null commands, in insertion order. */
	void MacroCommand::add(std::unique_ptr<Command> child)
	{
		if (!child) throw InvalidStateError("cannot add a null command");
		if (executed) throw InvalidStateError("cannot add a command after macro execution");
		children.push_back(std::move(child));
	}

	/** @brief Will execute in insertion order and reverse prior successful steps on first failure.
	 * @details Invariant: executed_count describes successful steps, never the failed step. See header for result text.
	 * Clarify irreversible children, undo failures and repeated execution before claiming transaction guarantees.
	 * @throws std::logic_error Temporary scaffold; dispatch/rollback is graded work for Connor. */
	CommandResult MacroCommand::execute() { throw std::logic_error("MacroCommand::execute awaits TDD implementation"); }

	/** @brief Will undo a successful reversible macro in reverse child order. @pre Successful execution.
	 * @throws std::logic_error Temporary scaffold; no undo is performed. */
	CommandResult MacroCommand::undo() { throw std::logic_error("MacroCommand::undo awaits TDD implementation"); }

	/** @brief Will aggregate child reversibility as specified by MacroCommand.h.
	 * @throws std::logic_error Temporary scaffold; no invented reversibility result. */
	bool MacroCommand::isReversible() const { throw std::logic_error("MacroCommand::isReversible awaits TDD implementation"); }

	/** @brief Will format the macro name/count and indented child descriptions without executing them.
	 * @throws std::logic_error Temporary scaffold; formatting remains a separate TDD step. */
	std::string MacroCommand::describe() const { throw std::logic_error("MacroCommand::describe awaits TDD implementation"); }
} // namespace tividy
