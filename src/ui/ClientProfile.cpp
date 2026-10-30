/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 26 October 2026

ClientProfile.cpp
*/

#ifndef TIVIDY_UI_CLIENTPROFILE_CPP
#define TIVIDY_UI_CLIENTPROFILE_CPP

#include "ui/ClientProfile.h"

namespace tividy
{
// the demo pack will do a tividy generic outline, banking will do its own, etc
ClientProfile::ClientProfile()
{
	product_name = "TIVIDY";
	tagline = ""; // e.g. A generic workflow engine
	banner = ""; // ASCII
	accent = "\033[38;2;216;137;76m"; // TIVIDY default colour
}

std::string ClientProfile::term(const std::string& engine_term) const
{
	std::map<std::string, std::string>::const_iterator it = vocabulary.find(engine_term);

	if (it != vocabulary.end())
		return it->second;

	return engine_term;
}
} // namespace tividy

#endif // TIVIDY_UI_CLIENTPROFILE_CPP