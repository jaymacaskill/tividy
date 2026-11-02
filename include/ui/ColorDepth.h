/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 27 October 2026
*/

/**
 * @file ColorDepth.h
 * @brief How much colour the terminal can show, decided from environment facts so the choice can be tested.
 */

#ifndef TIVIDY_UI_COLORDEPTH_H
#define TIVIDY_UI_COLORDEPTH_H

#include <string>

namespace tividy
{

	/** Colour support of the terminal, from least to most capable. */
	enum class ColorDepth
	{
		None,        /**< No colour: output is piped or redirected, NO_COLOR is set, or the terminal is "dumb". */
		Ansi256,     /**< The xterm 256 colour palette. */
		TrueColor    /**< 24-bit colour. */
	};

	/**
	 * @brief Picks the best colour depth that is safe to use. Pure: main.cpp reads the environment and passes the facts in,
	 * so tests can cover every case without touching real environment variables.
	 *
	 * Rules, first match wins:
	 *  1. @p no_color or !@p is_tty                                  -> None   (https://no-color.org: any non-empty NO_COLOR disables colour)
	 *  2. @p term is "" or "dumb"                                    -> None
	 *  3. @p colorterm contains "truecolor" or "24bit"               -> TrueColor
	 *  4. @p term_program is "vscode", "iTerm.app", "WezTerm", "ghostty" or "WindowsTerminal" -> TrueColor
	 *  5. @p term contains "256"                                     -> Ansi256
	 *  6. @p term_program is "Apple_Terminal"                        -> Ansi256   (Terminal.app has no truecolor)
	 *  7. @p term contains "kitty"                                   -> TrueColor
	 *  8. anything else                                              -> None
	 *
	 * @param colorterm    value of COLORTERM ("" if unset)
	 * @param term         value of TERM ("" if unset)
	 * @param term_program value of TERM_PROGRAM ("" if unset; main.cpp may pass "WindowsTerminal" when WT_SESSION is set)
	 * @param no_color     true if NO_COLOR is set to a non-empty value
	 * @param is_tty       true if standard output is a terminal (isatty(1))
	 */
	ColorDepth chooseColorDepth(const std::string& colorterm, const std::string& term, const std::string& term_program,
								bool no_color, bool is_tty);
} // namespace tividy

#endif // TIVIDY_UI_COLORDEPTH_H
