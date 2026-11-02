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
 * @file CommandDecorators.cpp
 * @brief Implements CommandDecorator and RetryingCommand.
 * @details CommandDecorator forwards every Command operation to the command
 * it wraps. RetryingCommand adds retries: it re-runs the wrapped command up
 * to a fixed number of attempts before reporting failure.
 */

#ifndef TIVIDY_COMMANDS_COMMANDDECORATORS_CPP
#define TIVIDY_COMMANDS_COMMANDDECORATORS_CPP

#include "engine/commands/MacroCommand.h"
#include "engine/commands/Command.h"
#include "engine/core/Errors.h"
#include <memory>
#include <utility>
#include <string>

namespace tividy
{

    //Start of CommandDecorator
    CommandDecorator::CommandDecorator(std::unique_ptr<Command> inner) : inner_ref(std::move(inner))
    {
        if (!inner_ref)
        {
            throw ValidationError("CommandDecorator: inner cannot be NULL");
        }
    }

    CommandDecorator::~CommandDecorator() = default;

    CommandResult CommandDecorator::execute()
    {
        return inner_ref->execute();
    }

    CommandResult CommandDecorator::undo()
    {
        return inner_ref->undo();
    }

    bool CommandDecorator::isReversible() const
    {
        return inner_ref->isReversible();
    }

    bool CommandDecorator::supportsRetry() const
    {
        return inner_ref->supportsRetry();
    }

    std::string CommandDecorator::describe() const
    {
        return inner_ref->describe();
    }

    Command& CommandDecorator::inner()
    {
        return *inner_ref;  
    }
    //End of CommandDecorator


    //Start of RetryingCommand
    RetryingCommand::RetryingCommand(std::unique_ptr<Command> inner, unsigned max_attempts) : CommandDecorator(std::move(inner)), max_attempts(max_attempts)
    {
        if (max_attempts < 1)
        {
            throw ValidationError("RetryingCommand: max_attempts must be >= 1");
        }
    }

    CommandResult RetryingCommand::execute()
    {
        CommandResult tryExecute = inner().execute(); //the first attempt
        unsigned attempt = 1;

        while (!tryExecute.success && attempt < max_attempts && inner().supportsRetry())
        {
            tryExecute = inner().execute();
            ++attempt;
        }


        if (tryExecute.success)
        {
            return tryExecute;
        }

        std::string msg =  tryExecute.message + " (after " + std::to_string(attempt) + " attempts)"; 
        return CommandResult::failure(msg);
    }

    std::string RetryingCommand::describe() const
    {
        return CommandDecorator::describe() + " (retry up to " + std::to_string(max_attempts) + ")";
    }

    //End of RetryingCommand

}

#endif //TIVIDY_COMMANDS_COMMANDDECORATORS_CPP
