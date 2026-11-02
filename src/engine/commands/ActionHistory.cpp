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
 * @file ActionHistory.cpp
 * @brief Implements ActionHistory, the Command pattern invoker.
 * @details Runs a command only while the instance is Running, keeps the
 * undo stack, refuses to undo irreversible commands, and lists the
 * executed commands for display.
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
      for (std::size_t i = 0; i < executed.size(); ++i) {
          const std::string prefix = std::to_string(i + 1) + ". ";
          const std::string text = executed[i]->describe();
  
          std::string lined;
          for (std::size_t c = 0; c < text.size(); ++c) {
              lined += text[c];
              if (text[c] == '\n') lined += std::string(prefix.size(), ' ');
          }
          descriptions.push_back(prefix + lined);
      }
      return descriptions;
  }

} // namespace tividy 

#endif // TIVIDY_COMMANDS_ACTIONHISTORY_CPP
