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
 * @file Console.h
 * @brief Text input/output abstraction so the TUI can be tested without a real terminal.
 */

#ifndef TIVIDY_UI_CONSOLE_H
#define TIVIDY_UI_CONSOLE_H

#include <deque>
#include <string>
#include <vector>

namespace tividy
{

	/** Abstract text terminal. */
	class Console
	{
		public:
			virtual ~Console();

			/** Writes @p text with no newline added. */
			virtual void print(const std::string& text) = 0;

			/** Writes @p text then "\n". */
			void println(const std::string& text);

			/** @brief Prints @p prompt and reads one line. @return the line trimmed of surrounding spaces. At end of input returns ""
			 *  and isInputExhausted() becomes true (the TUI then exits instead of looping forever). */
			virtual std::string readLine(const std::string& prompt) = 0;
			virtual bool isInputExhausted() const = 0;
	};

	/** Real terminal: std::cout / std::cin. */
	class StdConsole : public Console
	{
		public:
			StdConsole();
			void print(const std::string& text) override;
			std::string readLine(const std::string& prompt) override;
			bool isInputExhausted() const override;
		private:
			bool exhausted;
	};

	/** Test double: feeds scripted input lines and captures output. Used by automated TUI tests and demo replays. */
	class ScriptedConsole : public Console
	{
		public:
			explicit ScriptedConsole(const std::vector<std::string>& input_lines);
			void print(const std::string& text) override;

			/** Pops the next scripted line and echoes prompt + line into output(). */
			std::string readLine(const std::string& prompt) override;
			bool isInputExhausted() const override;
			const std::string& output() const;
		private:
			std::deque<std::string> input;
			std::string output_buffer;
			bool exhausted;
	};
} // namespace tividy

#endif // TIVIDY_UI_CONSOLE_H