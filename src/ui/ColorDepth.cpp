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
*/

/**
 * @file ColorDepth.cpp
 * @brief Implements the choice of terminal colour depth.
 * @details A pure function of environment facts (NO_COLOR, terminal name and
 * colour settings, whether output is a terminal), so the decision can be
 * tested without a real terminal.
 */

#ifndef TIVIDY_UI_COLORDEPTH_CPP
#define TIVIDY_UI_COLORDEPTH_CPP

#include "ui/ColorDepth.h"

namespace tividy
{
	namespace
	{
		bool contains(const std::string& text, const std::string& part)
		{
			return text.find(part) != std::string::npos;
		}
	} // anonymous namespace

	ColorDepth chooseColorDepth(const std::string& colorterm, const std::string& term, const std::string& term_program,
								bool no_color, bool is_tty)
	{
		// colour codes in a file or a pipe are just noise, and NO_COLOR is the user saying "not here"
		if (no_color || !is_tty) return ColorDepth::None;
		if (term == "" || term == "dumb") return ColorDepth::None;

		if (contains(colorterm, "truecolor") || contains(colorterm, "24bit")) return ColorDepth::TrueColor;

		// these terminals all do 24-bit colour but do not always set COLORTERM
		if (term_program == "vscode" || term_program == "iTerm.app" || term_program == "WezTerm"
			|| term_program == "ghostty" || term_program == "WindowsTerminal") return ColorDepth::TrueColor;

		if (contains(term, "256")) return ColorDepth::Ansi256;
		if (term_program == "Apple_Terminal") return ColorDepth::Ansi256; // Terminal.app has 256 colours but no truecolor

		if (contains(term, "kitty")) return ColorDepth::TrueColor;

		return ColorDepth::None;
	}
} // namespace tividy

#endif // TIVIDY_UI_COLORDEPTH_CPP
