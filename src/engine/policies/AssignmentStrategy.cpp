/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 25 October 2026

AssignmentStrategy.cpp
*/

#ifndef TIVIDY_POLICIES_ASSIGNMENTSTRATEGY_CPP
#define TIVIDY_POLICIES_ASSIGNMENTSTRATEGY_CPP

#include "engine/policies/AssignmentStrategy.h"

namespace tividy
{
	AssignmentStrategy::~AssignmentStrategy() { }

	Id RoleBasedAssignment::chooseParticipant(const WorkItem& /* item */, const std::string& role,
											  const ParticipantDirectory& directory, const WorkflowData& /* data */) const
	{
		if (role == "") return "";
		std::vector<const Participant*> p = directory.withRole(role);
		if (p.empty()) return "";
		return p[0]->id();
	}

	DataDrivenAssignment::DataDrivenAssignment(const std::string& data_key, std::shared_ptr<const AssignmentStrategy> fallback) :
		data_key(data_key), fallback(fallback) { }

	Id DataDrivenAssignment::chooseParticipant(const WorkItem& item, const std::string& role,
											   const ParticipantDirectory& directory, const WorkflowData& data) const
	{
		const std::string id = data.getOr(data_key, "");
		if (id != "")
		{
			const Participant* p = directory.find(id);
			if (p && (role == "" || p->hasRole(role)))
				return id;
		}

		if (!fallback) return "";
		return fallback->chooseParticipant(item, role, directory, data);
	}
} // namespace tividy

#endif // TIVIDY_POLICIES_ASSIGNMENTSTRATEGY_CPP