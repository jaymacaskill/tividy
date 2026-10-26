/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 22 October 2026
*/

/**
 * @file CommandFactory.h
 * @brief Registry-based creation of commands from a verb word (no switch statement).
 */

#ifndef TIVIDY_COMMANDS_COMMANDFACTORY_H
#define TIVIDY_COMMANDS_COMMANDFACTORY_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/commands/Command.h"
#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"

namespace tividy
{
	class WorkflowInstance;

	/** Everything a creator needs. @c text carries the reason for reject/escalate/cancel; ignored otherwise. */
	struct CommandRequest
	{
		WorkflowInstance& instance;
		Id item_id;
		const Participant& actor;
		Outcome outcome;
		std::string text;
	};

	/**
	 * @brief Maps a verb to a creator function. Built-in verbs (registered by the constructor): "start", "submit", "complete",
	 * "approve", "reject", "escalate", "cancel".
	 */
	class CommandFactory
	{
		public:
			typedef std::function<std::unique_ptr<Command>(const CommandRequest&)> Creator;

			CommandFactory();

			/** Adds or replaces a verb. */
			void registerVerb(const std::string& verb, Creator creator);

			/** @return the new command. @throws NotFoundError("unknown action"). */
			std::unique_ptr<Command> create(const std::string& verb, const CommandRequest& request) const;

			/** @return all verbs, sorted (the TUI builds its action menu from this). */
			std::vector<std::string> verbs() const;
		private:
			std::map<std::string, Creator> creators;
	};
} // namespace tividy

#endif // TIVIDY_COMMANDS_COMMANDFACTORY_H