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
 * @file Participant.h
 * @brief People and systems that perform work, and the directory that holds them.
 */

#ifndef TIVIDY_CORE_PARTICIPANT_H
#define TIVIDY_CORE_PARTICIPANT_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "engine/core/Types.h"

namespace tividy
{

	/**
	 * @brief A person or automated system. Roles are plain strings ("hr-coordinator", "system"). Industry facts
	 * (region, department) live in attributes so the class never changes. Owned by the ParticipantDirectory.
	 */
	class Participant
	{
		public:
			Participant(const Id& id, const std::string& display_name, const std::set<std::string>& roles);

			const Id& id() const;
			const std::string& displayName() const;
			bool hasRole(const std::string& role) const;

			/** Sets an attribute and returns *this for chaining. */
			Participant& setAttribute(const std::string& key, const std::string& value);

			/** @return the attribute or @p fallback. */
			std::string attribute(const std::string& key, const std::string& fallback = "") const;
		private:
			Id id_ref;
			std::string display_name;
			std::set<std::string> roles;
			std::map<std::string, std::string> attributes;
	};

	/** Owns all participants of one Tividy. */
	class ParticipantDirectory
	{
		public:
			/** Copies @p participant in. @throws EngineError if the id already exists. */
			void add(const Participant& participant);

			/** @return the participant or nullptr. */
			const Participant* find(const Id& id) const;

			/** @return the participant. @throws NotFoundError. */
			const Participant& require(const Id& id) const;

			/** @return all holders of @p role, ordered by id (deterministic). */
			std::vector<const Participant*> withRole(const std::string& role) const;
		private:
			std::map<Id, Participant> participants;
	};
} // namespace tividy

#endif // TIVIDY_CORE_PARTICIPANT_H