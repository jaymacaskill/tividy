/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)
 
COS 214 (Software Modelling) TIVIDY
Last Modified: 02 November 2026
 
WorkItemValidation.cpp
*/

/**
 * @file WorkItemValidation.cpp
 * @brief Implements ValidationRule, RequiredDataRule and ValidatedWorkItem.
 * @details ValidatedWorkItem checks its rules before a task is submitted or
 * completed and refuses the action if a rule fails. RequiredDataRule
 * fails when named workflow-data keys are missing.
 */

#ifndef TIVIDY_ITEMS_WORKITEMVALIDATION_CPP
#define TIVIDY_ITEMS_WORKITEMVALIDATION_CPP

#include "engine/items/WorkItemDecorator.h"
#include "engine/items/WorkContext.h"
#include "engine/core/Errors.h"
#include <string>
#include <memory>

namespace tividy
{
    //Start of ValidationRule
    ValidationRule::~ValidationRule() = default;

    //End of ValidationRule


    //Start of RequiredDataRule
    RequiredDataRule::RequiredDataRule(const std::vector<std::string>& keys) : keys(keys)
    {
    }

    std::string RequiredDataRule::check(const WorkItem& /*item*/, const WorkflowData& data, const Outcome& /*outcome*/) const
    {
        std::string msg;

        for (std::string key : keys)
        {
            if (!data.has(key) || data.get(key).empty())
            {
                if (msg.empty())
                {
                    msg = "missing required data: ";
                }
                else
                {
                    msg += ", ";
                }

                msg += key;
            }
        }

        return msg;
    }
    //End of RequiredDataRule


    //Start of ValidatedWorkItem
    ValidatedWorkItem::ValidatedWorkItem(std::unique_ptr<WorkItem> component, const std::vector<std::shared_ptr<const ValidationRule> >& rules)
        : WorkItemDecorator(std::move(component)), rules(rules)
    {
    }

    void ValidatedWorkItem::submit(const Participant& actor, const Outcome& outcome)
    {
        const WorkflowData& data = context().data();

        for (auto& rule : rules)
        {
            std::string msg = rule->check(*this, data, outcome);

            if (!msg.empty())
            {
                throw ValidationError(msg);
            }
        }

        component().submit(actor, outcome);
    }

    void ValidatedWorkItem::complete(const Participant& actor, const Outcome& outcome)
    {
        WorkflowData& data = context().data();

        for (auto& rule : rules)
        {
            std::string msg = rule->check(*this, data, outcome);

            if (!msg.empty())
            {
                throw ValidationError(msg);
            }
        }

        component().complete(actor, outcome);
    }

    std::unique_ptr<WorkItem> ValidatedWorkItem::clone() const
    {
        return std::unique_ptr<WorkItem>(new ValidatedWorkItem(cloneComponent(), rules));
    }
    //End of ValidatedWorkItem
}


#endif //TIVIDY_ITEMS_WORKITEMVALIDATION_CPP