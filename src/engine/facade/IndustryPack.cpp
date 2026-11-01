/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 29 October 2026
*/

/**
 * @file IndustryPack.cpp
 * @brief Default products for an industry's family of engine collaborators.
 */

#include "engine/facade/IndustryPack.h"

tividy::IndustryPack::~IndustryPack() = default;

std::vector<tividy::Participant> tividy::IndustryPack::createParticipants() const
{
	return std::vector<tividy::Participant>();
}

std::vector<std::unique_ptr<tividy::ExternalConnector> > tividy::IndustryPack::createConnectors() const
{
	std::vector<std::unique_ptr<tividy::ExternalConnector> > connectors;
	connectors.push_back(std::unique_ptr<tividy::ExternalConnector>(new tividy::LoggingNotifier("notifier")));
	return connectors;
}

std::shared_ptr<const tividy::AssignmentStrategy> tividy::IndustryPack::createAssignmentStrategy() const
{
	return std::make_shared<tividy::RoleBasedAssignment>();
}

std::shared_ptr<const tividy::EscalationHandler> tividy::IndustryPack::createEscalationChain() const
{
	return std::shared_ptr<const tividy::EscalationHandler>();
}

tividy::ClientProfile tividy::IndustryPack::createProfile() const
{
	return tividy::ClientProfile();
}
