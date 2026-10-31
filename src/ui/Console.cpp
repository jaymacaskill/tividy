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
 * @file Console.cpp
 * @brief Implements the Console interface: StdConsole (real terminal) and ScriptedConsole (canned input for tests).
 */

#ifndef TIVIDY_UI_CONSOLE_CPP
#define TIVIDY_UI_CONSOLE_CPP

#include <iostream>

#include "ui/Console.h"

namespace tividy
{
	namespace
	{
		// readLine promises a trimmed line, so both consoles share this (it also strips the \r that windows line endings leave behind)
		std::string trim(const std::string& text)
		{
			const std::string whitespace = " \t\r\n";

			std::string::size_type first = text.find_first_not_of(whitespace);
			if (first == std::string::npos) return "";

			std::string::size_type last = text.find_last_not_of(whitespace);
			return text.substr(first, last - first + 1);
		}
	} // anonymous namespace

	Console::~Console() { } // empty default is fine, we just have this for polymorphism

	void Console::println(const std::string& text)
	{
		print(text + "\n");
	}

	StdConsole::StdConsole() :
		exhausted(false) { }

	void StdConsole::print(const std::string& text)
	{
		std::cout << text;
	}

	std::string StdConsole::readLine(const std::string& prompt)
	{
		std::cout << prompt << std::flush; // flush, otherwise the prompt can sit in the buffer until after the user has typed

		std::string line;
		if (!std::getline(std::cin, line))
		{
			exhausted = true; // the TUI checks this so it exits instead of looping on empty lines forever
			return "";
		}

		return trim(line);
	}

	bool StdConsole::isInputExhausted() const
	{
		return exhausted;
	}

	ScriptedConsole::ScriptedConsole(const std::vector<std::string>& input_lines) :
		input(input_lines.begin(), input_lines.end()), output_buffer(""), exhausted(false) { }

	void ScriptedConsole::print(const std::string& text)
	{
		output_buffer += text;
	}

	std::string ScriptedConsole::readLine(const std::string& prompt)
	{
		if (input.empty())
		{
			exhausted = true;
			return "";
		}

		std::string line = trim(input.front());
		input.pop_front();

		// echo the prompt and the line so the captured output reads like a real terminal session
		output_buffer += prompt + line + "\n";

		return line;
	}

	bool ScriptedConsole::isInputExhausted() const
	{
		return exhausted;
	}

	const std::string& ScriptedConsole::output() const
	{
		return output_buffer;
	}
} // namespace tividy

#endif // TIVIDY_UI_CONSOLE_CPP