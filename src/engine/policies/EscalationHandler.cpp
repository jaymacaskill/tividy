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
 * @file EscalationHandler.cpp
 * @brief Implements the Chain of Responsibility for escalation: each handler resolves a request at its level or passes it to its successor.
 */

#ifndef TIVIDY_POLICIES_ESCALATIONHANDLER_CPP
#define TIVIDY_POLICIES_ESCALATIONHANDLER_CPP

#include "engine/policies/EscalationHandler.h"

namespace tividy
{
	EscalationHandler::EscalationHandler(std::shared_ptr<const EscalationHandler> successor) :
		successor(successor) { }

	EscalationHandler::~EscalationHandler() { }

	EscalationResult EscalationHandler::handle(const EscalationRequest& request) const
	{
		if (canHandle(request))
			return resolve(request);

		if (successor)
			return successor->handle(request);

		EscalationResult unresolved;
		unresolved.resolved = false;
		unresolved.approver_role = "";
		unresolved.handled_by = "";
		return unresolved;
	}

	RoleEscalationHandler::RoleEscalationHandler(const std::string& approver_role, unsigned level,
												std::shared_ptr<const EscalationHandler> successor) :
		EscalationHandler(successor), approver_role(approver_role), level(level) { }

	bool RoleEscalationHandler::canHandle(const EscalationRequest& request) const
	{
		return request.level == level;
	}

	EscalationResult RoleEscalationHandler::resolve(const EscalationRequest& /* request */ ) const // parameter is unused here, but some handler may need to use the request
	{
		EscalationResult resolved;
		resolved.resolved = true;
		resolved.approver_role = approver_role;
		resolved.handled_by = approver_role;
		return resolved;
	}
} // namespace tividy

#endif // TIVIDY_POLICIES_ESCALATIONHANDLER_CPP