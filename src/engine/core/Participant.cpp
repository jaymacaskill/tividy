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
 * @file Participant.cpp
 * @brief Implements Participant (a person or system with roles) and ParticipantDirectory (look up participants by id or role).
 */


#ifndef TIVIDY_CORE_PARTICIPANT_CPP
#define TIVIDY_CORE_PARTICIPANT_CPP

#include "engine/core/Participant.h"
#include "engine/core/Errors.h"

namespace tividy
{
	Participant::Participant(const Id& id, const std::string& display_name, const std::set<std::string>& roles) :
		id_ref(id), display_name(display_name), roles(roles) { }

	const Id& Participant::id() const
	{
		return id_ref;
	}

	const std::string& Participant::displayName() const
	{
		return display_name;
	}

	bool Participant::hasRole(const std::string& role) const
	{
		return roles.find(role) != roles.end();
	}

	std::string Participant::attribute(const std::string& key, const std::string& fallback) const
	{
		std::map<std::string, std::string>::const_iterator it = attributes.find(key);
		if (it == attributes.end()) return fallback;
		return it->second;
	}

	Participant& Participant::setAttribute(const std::string& key, const std::string& value)
	{
		attributes[key] = value;
		return *this;
	}

	const Participant* ParticipantDirectory::find(const Id& id) const
	{
		std::map<Id, Participant>::const_iterator it = participants.find(id);
		if (it == participants.end()) return nullptr; // using this function requires a null check
		return &it->second;
	}

	const Participant& ParticipantDirectory::require(const Id& id) const
	{
		std::map<Id, Participant>::const_iterator it = participants.find(id);
		if (it == participants.end()) throw NotFoundError("Participant " + id + " not found");
		return it->second;
	}

	std::vector<const Participant*> ParticipantDirectory::withRole(const std::string& role) const
	{
		std::vector<const Participant*> o;

		std::map<Id, Participant>::const_iterator it = participants.begin();
		while (it != participants.end())
		{
			if (it->second.hasRole(role)) o.push_back(&it->second);
			it ++;
		}

		return o;
	}

	void ParticipantDirectory::add(const Participant& participant)
	{
		Id id = participant.id();

		std::map<Id, Participant>::const_iterator it = participants.find(id);
		if (it != participants.end()) throw EngineError("Participant with ID " + id + " is already in the directory");

		participants.emplace(id, participant);
	}
} // namespace tividy

#endif // TIVIDY_CORE_PARTICIPANT_CPP