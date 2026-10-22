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
 * @file ClientProfile.h
 * @brief How one client's TIVIDY looks and sounds: vocabulary, colour, branding. No workflow logic.
 */

#ifndef TIVIDY_UI_CLIENTPROFILE_H
#define TIVIDY_UI_CLIENTPROFILE_H

#include <map>
#include <string>

namespace tividy
{

	/**
	 * @brief Pure data produced by IndustryPack::createProfile(). Changing a profile never changes workflow behaviour, which is why
	 * two industries can look very different on one engine.
	 *
	 * Engine terms the TUI looks up through term(): "workflow", "instance", "item", "participant", "start", "submit", "complete",
	 * "approve", "reject", "escalate", "cancel", "history", "work-list". Example: telecom maps "instance" -> "Order".
	 */
	struct ClientProfile
	{
		ClientProfile();

		/** @return vocabulary[engineTerm] if present, else @p engineTerm itself (every term always has a usable label). */
		std::string term(const std::string& engine_term) const;

		std::string product_name;                          /**< Banner title, e.g. "TIVIDY Connect". Default "TIVIDY". */
		std::string tagline;                               /**< Default "". */
		std::string banner;                                /**< Optional multi-line ASCII art. Default "". */
		std::string accent;                                /**< ANSI colour code for headings, e.g. "\033[36m". "" = plain text. */
		std::map<std::string, std::string> vocabulary;     /**< engine term -> display term. */
	};
} // namespace tividy

#endif // TIVIDY_UI_CLIENTPROFILE_H