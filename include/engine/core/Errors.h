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
 * @file Errors.h
 * @brief Exception hierarchy. All engine errors derive from EngineError.
 *
 * Policy: an operation that is *illegal* (wrong state, wrong role, bad data) throws.
 * The command layer (CommandHandler/ActionHistory) catches EngineError and converts
 * it to a CommandResult, so the TUI never sees an exception for ordinary user mistakes.
 */

#ifndef TIVIDY_CORE_ERRORS_H
#define TIVIDY_CORE_ERRORS_H

#include <stdexcept>
#include <string>

namespace tividy
{

	/** Base class of every error raised by the engine. */
	class EngineError : public std::runtime_error
	{
		public:
			explicit EngineError(const std::string& message) : std::runtime_error(message) {}
	};

	/** An operation is not legal in the item's/instance's current state (e.g. assign() on Completed). */
	class InvalidStateError : public EngineError
	{
		public:
			explicit InvalidStateError(const std::string& m) : EngineError(m) {}
	};

	/** The acting participant lacks the role required for the operation. */
	class PermissionDeniedError : public EngineError
	{
		public:
			explicit PermissionDeniedError(const std::string& m) : EngineError(m) {}
	};

	/** A WorkflowDefinition violates a structural rule (duplicate id, dependency cycle, ...). */
	class InvalidDefinitionError : public EngineError
	{
		public:
			explicit InvalidDefinitionError(const std::string& m) : EngineError(m) {}
	};

	/** A named thing (item, instance, definition, participant, connector) does not exist. */
	class NotFoundError : public EngineError
	{
		public:
			explicit NotFoundError(const std::string& m) : EngineError(m) {}
	};

	/** The operation is deliberately unsupported by this class (e.g. add() on a TaskItem). */
	class UnsupportedOperationError : public EngineError
	{
		public:
			explicit UnsupportedOperationError(const std::string& m) : EngineError(m) {}
	};

	/** Supplied data failed a ValidationRule or an outcome check. */
	class ValidationError : public EngineError
	{
		public:
			explicit ValidationError(const std::string& m) : EngineError(m) {}
	};

	/** An external system (connector) failed. @c retryable_ref says whether trying again may help. */
	class ExternalFailure : public EngineError
	{
		public:
			ExternalFailure(const std::string& m, bool retryable) : EngineError(m), retryable_ref(retryable) {}

			/** @return true if the failure is transient (timeout, 503); false if permanent (bad data, rejected). */
			bool retryable() const { return retryable_ref; }
		private:
			bool retryable_ref;
	};
} // namespace tividy

#endif // TIVIDY_CORE_ERRORS_H