/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 2 November 2026
*/

/**
 * @file CommandFactory.cpp
 * @brief Implements CommandFactory, which turns a verb into a Command.
 * @details The constructor registers the seven standard verbs (start,
 * submit, complete, approve, reject, escalate, cancel). Industries can
 * register more with registerVerb(). Unknown verbs are reported, not thrown.
 */

#ifndef TIVIDY_COMMANDS_COMMANDFACTORY_CPP
#define TIVIDY_COMMANDS_COMMANDFACTORY_CPP

#include "engine/commands/CommandFactory.h"
#include "engine/commands/Command.h"
#include "engine/commands/WorkItemCommands.h"
#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"
#include "engine/core/Errors.h"
#include <memory>

namespace tividy
{
    CommandFactory::CommandFactory()
    {
        registerVerb("start", [](const CommandRequest& r)
        {
            return std::unique_ptr<Command>(new StartCommand(r.instance, r.item_id, r.actor));
        });

        registerVerb("submit", [](const CommandRequest& r)
        {
            return std::unique_ptr<Command>(new SubmitCommand(r.instance, r.item_id, r.actor, r.outcome));
        });

        registerVerb("complete", [](const CommandRequest& r)
        {
            return std::unique_ptr<Command>(new CompleteCommand(r.instance, r.item_id, r.actor, r.outcome));
        });

        registerVerb("approve", [](const CommandRequest& r)
        {
            return std::unique_ptr<Command>(new ApproveCommand(r.instance, r.item_id, r.actor, r.outcome));
        });

        registerVerb("reject", [](const CommandRequest& r)
        {
            return std::unique_ptr<Command>(new RejectCommand(r.instance, r.item_id, r.actor, r.text));
        });

        registerVerb("escalate", [](const CommandRequest& r)
        {
            return std::unique_ptr<Command>(new EscalateCommand(r.instance, r.item_id, r.actor, r.text));
        });

        registerVerb("cancel", [](const CommandRequest& r)
        {
            return std::unique_ptr<Command>(new CancelCommand(r.instance, r.item_id, r.actor, r.text));
        });
    }

    void CommandFactory::registerVerb(const std::string& verb, Creator creator)
    {
        creators[verb] = std::move(creator);
    }

    std::unique_ptr<Command> CommandFactory::create(const std::string& verb, const CommandRequest& request) const
    {
        const auto creator = creators.find(verb);
        
        if (creator == creators.end())
        {
            throw NotFoundError("unknown action");
        }

        return creator->second(request);
    }

    std::vector<std::string> CommandFactory::verbs() const
    {
        std::vector<std::string> result;

        for (auto& entry : creators)
        {
            result.push_back(entry.first);
        }

        return result;
    }
}


#endif //TIVIDY_COMMANDS_COMMANDFACTORY_CPP