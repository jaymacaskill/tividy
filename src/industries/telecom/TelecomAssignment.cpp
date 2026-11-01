/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 26 October 2026
*/

/**
 * @file TelecomAssignment.cpp
 * @brief implementation of RegionalTechnicianAssignment from TelecomParts.h
 */

#ifndef TIVIDY_TELECOM_ASSIGNMENT_CPP
#define TIVIDY_TELECOM_ASSIGNMENT_CPP


#include "industries/telecom/TelecomParts.h"

namespace tividy
{
	namespace telecom
	{
		Id RegionalTechnicianAssignment::chooseParticipant(const WorkItem& /* item */, const std::string& role, 
														   const ParticipantDirectory& directory, const WorkflowData& data) const
		{
			if (role.empty()) return "";

			const std::vector<const Participant*> holders = directory.withRole(role);
			if (holders.empty()) return "";

			if (role != "field-technician") return holders[0]->id();

			const std::string region = data.getOr("region", "");
			if (region.empty()) return "";

			for (std::size_t i = 0; i < holders.size(); i ++)
			{
				if (holders[i]->attribute("region") == region)
					return holders[i]->id();
			}
			return "";
		}
	} // namespace telecom
} // namespace tividy

#endif // TIVIDY_TELECOM_ASSIGNMENT_CPP