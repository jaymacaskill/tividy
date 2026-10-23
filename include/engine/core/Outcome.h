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
 * @file Outcome.h
 * @brief The business result a worker or reviewer reports when finishing work.
 */

#ifndef TIVIDY_CORE_OUTCOME_H
#define TIVIDY_CORE_OUTCOME_H

#include <string>

#include "engine/core/Types.h"

namespace tividy
{

	/**
	 * @brief Immutable value describing what happened when work finished.
	 *
	 * An Outcome is NOT a lifecycle state. A "Verify documents" item that finds invalid
	 * documents still *completes*; its Outcome is Rejected with code "DOCS_INVALID", and a
	 * RoutingStrategy uses that to pick the next branch. By contrast WorkItem::reject() is a
	 * reviewer sending work back (a lifecycle transition).
	 *
	 * The engine only ever inspects kind(). Industries put their own vocabulary in code()
	 * (e.g. "CREDIT_REFER") and their RoutingStrategy classes interpret it.
	 */
	class Outcome
	{
		public:
			/** Creates Accepted with empty code and note. */
			Outcome();

			/**
			 * @param kind  Result category.
			 * @param code  Industry-specific label, may be empty. Upper snake case by convention.
			 * @param note  Free-text explanation shown in history; may be empty.
			 */
			explicit Outcome(OutcomeKind kind, const std::string& code = "", const std::string& note = "");

			/** Convenience factories. Each is equivalent to the explicit constructor with that kind. */
			static Outcome accepted(const std::string& code = "", const std::string& note = "");
			static Outcome rejected(const std::string& code = "", const std::string& note = "");
			static Outcome escalated(const std::string& code = "", const std::string& note = "");
			static Outcome failed(const std::string& code = "", const std::string& note = "");

			OutcomeKind kind() const;
			const std::string& code() const;
			const std::string& note() const;

			/**
			 * @return The routing key: code() if non-empty, otherwise toString(kind()).
			 * OutcomeTableRouting looks rules up by this key.
			 */
			std::string routingKey() const;

		private:
			OutcomeKind kind_ref;
			std::string code_ref;
			std::string note_ref;
	};
} // namespace tividy

#endif // TIVIDY_CORE_OUTCOME_H