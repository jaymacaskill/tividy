/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)
 
COS 214 (Software Modelling) TIVIDY
Last Modified: 31 October 2026
*/

/**
 * @file Command.cpp
 * @brief Implements the Command base class defaults (no retry support) and the CommandResult success and failure helpers.
 */

#ifndef TIVIDY_COMMANDS_COMMAND_CPP
#define TIVIDY_COMMANDS_COMMAND_CPP
 
#include "engine/commands/Command.h"

namespace tividy
{
	Command::~Command() { }

	bool Command::supportsRetry() const { return false; }

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
