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

BankingPack.cpp
*/

#ifndef TIVIDY_BANKING_PACK_CPP
#define TIVIDY_BANKING_PACK_CPP

#include "industries/banking/BankingPack.h"
#include "industries/banking/BankingParts.h"
#include "engine/items/WorkItemDecorator.h"
#include "engine/policies/RoutingStrategy.h"

namespace tividy
{
    namespace banking
    {
        //Start of DisputeDirector
        DisputeDirector::DisputeDirector(const IndustryPack& pack) : pack(pack)
        {
        }

        std::string DisputeDirector::workflowName() const
        { 
            return "transaction-dsipute";
        }

        unsigned DisputeDirector::version() const
        {
            return 1;
        }

        std::string DisputeDirector::description() const
        {
            return "Transaction dispute resolution including dispute lodging from customer, merchant response, decision from analyst, appeals and settlement";
        }

        void DisputeDirector::construct(WorkflowBuilder& builder) const
        {
            
        }
    }
}

#endif //TIVIDY_BANKING_PACK_CPP