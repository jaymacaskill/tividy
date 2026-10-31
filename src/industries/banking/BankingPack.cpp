/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 30 October 2026
*/

/**
* @file BankingPack.cpp
* @brief Implements BankingPack (Abstract Factory): the banking people, connectors, escalation chain and look that plug into the engine.
*/

#ifndef TIVIDY_BANKING_PACK_CPP
#define TIVIDY_BANKING_PACK_CPP

#include "industries/banking/BankingPack.h"
#include "industries/banking/BankingParts.h"
#include "engine/items/WorkItemDecorator.h"
#include "engine/policies/RoutingStrategy.h"
#include "engine/facade/IndustryPack.h"
#include "engine/policies/EscalationHandler.h"
#include "engine/connectors/ExternalConnector.h"
#include "ui/ClientProfile.h"

namespace tividy
{
    namespace banking
    {
        std::string BankingPack::key() const
        {
            return "banking";
        }

        std::string BankingPack::displayName() const
        {
            return "Banking: Transaction Dispute Resolution";
        }

        std::unique_ptr<WorkflowDirector> BankingPack::createDirector() const
        {
            return std::unique_ptr<WorkflowDirector>(new DisputeDirector(*this));
        }

        std::vector<Participant> BankingPack::createParticipants() const
        {
            return {
                Participant("banking-cust-1", "Customer", {"customer"}),
                Participant("banking-agent-1", "Dispute Agent", {"dispute-agent"}),
                Participant("banking-merchant-1", "Merchant Representative", {"merchant-rep"}),
                Participant("banking-analyst-1", "Dispute Analyst", {"dispute-analyst"}),
                Participant("banking-senior-1", "Senior Analyst", {"senior-analyst"}),
                Participant("banking-compliance-1", "Compliance Officer", {"compliance-officer"}),
                Participant("banking-sys-1", "System", {"system"})
            };
        }

        std::vector<std::unique_ptr<ExternalConnector> > BankingPack::createConnectors() const
        {
            std::vector<std::unique_ptr<ExternalConnector> > connectors;

            connectors.push_back(std::unique_ptr<ExternalConnector>(new CoreBankingAdapter(
                    std::unique_ptr<CoreBankingVendor>(new CoreBankingVendor()))));
        
            connectors.push_back(std::unique_ptr<ExternalConnector>(new CardSchemeAdapter(
                    std::unique_ptr<CardSchemeVendor>(new CardSchemeVendor()))));
        
            connectors.push_back(std::unique_ptr<ExternalConnector>(new LoggingNotifier()));
        
            return connectors;
        }

        std::shared_ptr<const EscalationHandler> BankingPack::createEscalationChain() const
        {
            auto compliance = std::make_shared<RoleEscalationHandler>("compliance-officer", 2);
            return std::make_shared<RoleEscalationHandler>("senior-analyst", 1, compliance);
        }

        ClientProfile BankingPack::createProfile() const
        {
            ClientProfile profile;
            
            profile.product_name = "TIVIDY Resolve";
            profile.accent = "\033[38;2;255;191;0m";
            profile.vocabulary["instance"] = "Dispute case";
            profile.vocabulary["item"] = "Action";
            profile.vocabulary["workflow"] = "Dispute process";
            profile.vocabulary["participant"] = "Party";
            profile.vocabulary["escalate"] = "Refer to supervisor";

            return profile;
        }
    }
}

#endif //TIVIDY_BANKING_PACK_CPP
