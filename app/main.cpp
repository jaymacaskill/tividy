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

main.cpp
*/

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include <unistd.h> // isatty: POSIX only, the Docker image is Linux so this builds there

#include "engine/facade/Tividy.h"
#include "industries/banking/BankingPack.h"
#include "industries/demo/DemoPack.h"
#include "industries/telecom/TelecomPack.h"
#include "ui/ColorDepth.h"
#include "ui/Console.h"
#include "ui/LogoArt.h"
#include "ui/Tui.h"

namespace
{
	// an unset variable and an empty one mean the same thing to us
	std::string env(const char* name)
	{
		const char* value = std::getenv(name);
		return value ? value : "";
	}

	// Decorator: passes everything to the real console but removes colour codes (ESC [ ... m) from what is printed.
	// Used when the terminal has no colour (or NO_COLOR is set), because the profiles' accent colours are always ANSI codes.
	class PlainConsole : public tividy::Console
	{
		public:
			explicit PlainConsole(tividy::Console& inner) :
				inner_ref(inner) { }

			void print(const std::string& text) override
			{
				std::string plain;
				for (std::string::size_type i = 0; i < text.size(); ++i)
				{
					if (text[i] == '\033' && i + 1 < text.size() && text[i + 1] == '[')
					{
						std::string::size_type end = text.find('m', i);
						if (end != std::string::npos)
						{
							i = end;
							continue;
						}
					}

					plain += text[i];
				}

				inner_ref.print(plain);
			}

			std::string readLine(const std::string& prompt) override { return inner_ref.readLine(prompt); }
			bool isInputExhausted() const override { return inner_ref.isInputExhausted(); }
		private:
			tividy::Console& inner_ref;
	};

	void printUsage()
	{
		std::cout << "Usage: tividy [tividy | hudson | warped | --no-logo | --help]\n"
				  << "  tividy      start with the TIVIDY logo (default, the demo pack)\n"
				  << "  hudson      start with the Hudson bank logo\n"
				  << "  warped      start with the Warped telecom logo\n"
				  << "  --no-logo   skip the logo\n"
				  << "Set NO_COLOR=1 to turn colour off.\n";
	}
}

int main(int argc, char* argv[])
{
	using namespace tividy;

	std::string client = "tividy";
	bool no_logo = false;
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg == "-h" || arg == "--help")
		{
			printUsage();
			return 0;
		}

		if (arg == "tividy" || arg == "hudson" || arg == "warped") client = arg;
		else if (arg == "--no-logo") no_logo = true;
		else
		{
			std::cerr << "Unknown option '" << arg << "'.\n";
			printUsage();
			return 1;
		}
	}

	// NO_COLOR counts only when it is not empty (https://no-color.org: "present and not an empty string"), which is what env() gives us.
	// the environment is read here and handed to chooseColorDepth, so that function stays pure and testable
	std::string term_program = env("TERM_PROGRAM");
	if (term_program == "" && env("WT_SESSION") != "") term_program = "WindowsTerminal";

	ColorDepth depth = chooseColorDepth(env("COLORTERM"), env("TERM"), term_program, env("NO_COLOR") != "", isatty(1) != 0);

	if (!no_logo)
	{
		if (client == "hudson") std::cout << hudsonLogo(depth);
		else if (client == "warped") std::cout << warpedLogo(depth);
		else std::cout << tividyLogo(depth);

		std::cout << "powered by TIVIDY\n";
	}

	try
	{
		Tividy engine;
		// the demo pack goes first: the main menu takes its branding from the first installed pack
		engine.installPack(std::unique_ptr<IndustryPack>(new demo::DemoPack()));
		engine.installPack(std::unique_ptr<IndustryPack>(new banking::BankingPack()));
		engine.installPack(std::unique_ptr<IndustryPack>(new telecom::TelecomPack()));

		StdConsole std_console;
		PlainConsole plain_console(std_console);

		// the Tui colours its headings with the pack's accent, so with no colour available the codes are stripped on the way out
		Console& console = (depth == ColorDepth::None) ? static_cast<Console&>(plain_console) : static_cast<Console&>(std_console);
		Tui tui(engine, console);
		return tui.run();
	}
	catch (const std::exception& error)
	{
		std::cerr << "TIVIDY could not start: " << error.what() << "\n";
		return 1;
	}
}