/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 31 October 2026
*/

/**
 * @file Outcome.cpp
 * @brief Implements Outcome: the result of a piece of work (accepted, rejected, escalated or failed) and the routing key derived from it.
 */

#ifndef TIVIDY_CORE_OUTCOME_CPP
#define TIVIDY_CORE_OUTCOME_CPP

#include "engine/core/Outcome.h"

namespace tividy
{
	Outcome::Outcome() :
		kind_ref(OutcomeKind::Accepted), code_ref(""), note_ref("") { }

	Outcome::Outcome(OutcomeKind kind, const std::string& code, const std::string& note) :
		kind_ref(kind), code_ref(code), note_ref(note) { }

	Outcome Outcome::accepted(const std::string& code, const std::string& note)
	{
		return Outcome(OutcomeKind::Accepted, code, note);
	}

	Outcome Outcome::rejected(const std::string& code, const std::string& note)
	{
		return Outcome(OutcomeKind::Rejected, code, note);
	}
	
	Outcome Outcome::escalated(const std::string& code, const std::string& note)
	{
		return Outcome(OutcomeKind::Escalated, code, note);
	}

	Outcome Outcome::failed(const std::string& code, const std::string& note)
	{
		return Outcome(OutcomeKind::Failed, code, note);
	}

	OutcomeKind Outcome::kind() const { return kind_ref; }
	
	const std::string& Outcome::code() const { return code_ref; }

	const std::string& Outcome::note() const { return note_ref; }

	std::string Outcome::routingKey() const
	{
		if (code_ref != "") return code();
		return toString(kind());
	}
} // namespace tividy

#endif // TIVIDY_CORE_OUTCOME_CPP