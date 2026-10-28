/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)
 
COS 214 (Software Modelling) TIVIDY
Last Modified: 28 October 2026
 
ActionHistory.cpp
*/
 
#ifndef TIVIDY_COMMANDS_ACTIONHISTORY_CPP
#define TIVIDY_COMMANDS_ACTIONHISTORY_CPP
 
#include "engine/commands/ActionHistory.h"
 
#include <string>
#include <utility>
 
#include "engine/core/Types.h"
#include "engine/runtime/WorkflowInstance.h"

namespace tividy 
{
    ActionHistory::ActionHistory(WorkflowInstance& owner) : instance(owner), executed() {}

    ActionHistory::~ActionHistory() { }

    CommandResult ActionHistory::execute(std::unique_ptr<Command> command)
    {
        if (!command) {
            return CommandResult::failure("null command");
        }

        InstanceStatus curr = instance.status();
        if (curr != InstanceStatus::Running) {
            return CommandResult::failure("instance is " +  toString(curr) + ", not Running");
        }

        CommandResult result = command->execute();
        if (result.success) {
            executed.push_back(std::move(command));
        }

        return result;
    }   

    CommandResult ActionHistory::undoLast()
    {
        if (executed.empty()) {
            return CommandResult::failure("nothing to undo");
        }

        Command& last = *executed.back();
        if (!last.isReversible()) {
            return CommandResult::failure("cannot undo: " + last.describe() + " is irreversible");
        }

        CommandResult result = last.undo();
        if (result.success) {
            executed.pop_back();
        }

        return result;
    }

    std::size_t ActionHistory::size() const
    {
        return executed.size();
    }

    std::vector<std::string> ActionHistory::describeAll() const
    {
        std::vector<std::string> descriptions;
        // descriptions.reserve(executed.size());
        for (std::size_t i = 0; i < executed.size(); ++i) {
            descriptions.push_back(std::to_string(i + 1) + ". " + executed[i]->describe());
        }

        return descriptions;
    }

} // namespace tividy 

#endif // TIVIDY_COMMANDS_ACTIONHISTORY_CPP