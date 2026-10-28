/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)
 
COS 214 (Software Modelling) TIVIDY
Last Modified: 25 October 2026
 
Command.cpp
*/

#ifndef TIVIDY_COMMANDS_COMMAND_CPP
#define TIVIDY_COMMANDS_COMMAND_CPP
 
#include "engine/commands/Command.h"

namespace tividy
{
    Command::~Command() { }

    CommandResult CommandResult::ok(const std::string& message)
    {
        CommandResult obj;
        obj.success = true;
        obj.message = message;

        return obj;
    }

    CommandResult CommandResult::failure(const std::string& message)
    {
        CommandResult obj;
        obj.success = false;
        obj.message = message;

        return obj;
    }

} // namespace tividy

#endif // TIVIDY_COMMANDS_COMMAND_CPP