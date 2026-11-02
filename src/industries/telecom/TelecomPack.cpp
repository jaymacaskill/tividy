/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 1 November 2026
*/

/**
* @file TelecomPack.cpp
* @brief Implements TelecomPack (Abstract Factory): the telecom people, connectors, regional assignment, escalation chain and look that plug into the engine.
*/

#ifndef TIVIDY_TELECOM_PACK_CPP
#define TIVIDY_TELECOM_PACK_CPP

#include "industries/telecom/TelecomPack.h"
#include "industries/telecom/TelecomParts.h"
#include "engine/policies/EscalationHandler.h"
#include "engine/policies/AssignmentStrategy.h"
#include "engine/connectors/ExternalConnector.h"

namespace tividy
{
    namespace telecom
    {
        std::string TelecomPack::key() const
        {
            return "telecom";
        }

        std::string TelecomPack::displayName() const
        {
            return "Telecommunications: Service Provisioning & Activation";
        }

        std::unique_ptr<WorkflowDirector> TelecomPack::createDirector() const
        {
            return std::unique_ptr<WorkflowDirector>(new ServiceActivationDirector(*this));
        }

        std::vector<Participant> TelecomPack::createParticipants() const
        {
            return {
                Participant("telecom-sales-1", "Sales Agent", {"sales-agent"}),
                Participant("telecom-care-1", "Customer Care", {"customer-care"}),
                Participant("telecom-credit-1", "Credit Analyst", {"credit-analyst"}),
                Participant("telecom-logistics-1", "Logistics", {"logistics"}),
                Participant("telecom-tech-gp-1", "Field Technician", {"field-technician"}).setAttribute("region", "gauteng"),
                Participant("telecom-tech-wc-1", "Field Technician", {"field-technician"}).setAttribute("region", "western-cape"),
                Participant("telecom-neteng-1", "Network Engineer", {"network-engineer"}),
                Participant("telecom-noc-1", "Manager", {"noc-manager"}),
                Participant("telecom-sys-1", "System", {"system"}),
            };
        }

        std::vector<std::unique_ptr<ExternalConnector> > TelecomPack::createConnectors() const
        {
            std::vector<std::unique_ptr<ExternalConnector> > connectors;

            connectors.push_back(std::unique_ptr<ExternalConnector>(new CreditBureauAdapter(
                    std::unique_ptr<CreditBureauVendor>(new CreditBureauVendor()))));
        
            connectors.push_back(std::unique_ptr<ExternalConnector>(new NetworkProvisioningAdapter(
                    std::unique_ptr<NetworkVendor>(new NetworkVendor()))));
        
            connectors.push_back(std::unique_ptr<ExternalConnector>(new LoggingNotifier()));
        
            return connectors;
        }

        std::shared_ptr<const EscalationHandler> TelecomPack::createEscalationChain() const
        {
            auto noc = std::make_shared<RoleEscalationHandler>("noc-manager", 2);            
            return std::make_shared<RoleEscalationHandler>("network-engineer", 1, noc);
        }

        ClientProfile TelecomPack::createProfile() const
        {
            ClientProfile profile;
            
            profile.product_name = "TIVIDY Connect";
            profile.accent = "\033[34m";
            profile.vocabulary["instance"] = "Order";
            profile.vocabulary["item"] = "Step";
            profile.vocabulary["workflow"] = "Service journey";
            profile.vocabulary["participant"] = "Agent";
            profile.vocabulary["escalate"] = "Raise to NOC";

            return profile;
        }

        std::shared_ptr<const AssignmentStrategy> TelecomPack::createAssignmentStrategy() const
        {
            return std::make_shared<RegionalTechnicianAssignment>();
        }
    }
}

#endif //TIVIDY_TELECOM_PACK_CPP
