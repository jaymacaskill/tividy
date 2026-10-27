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

test_ui.cpp
*/

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "engine/core/Participant.h"
#include "engine/facade/Tividy.h"
#include "ui/ClientProfile.h"
#include "ui/ColorDepth.h"
#include "ui/Console.h"
#include "ui/LogoArt.h"
#include "ui/Tui.h"

/*
These tests cover Console.cpp, ClientProfile.cpp, ColorDepth.cpp, the generated LogoArt.cpp and Tui.cpp.
The Tui tests drive the real screens with a ScriptedConsole (the lines "typed" by the user) and read back what was printed.
They use a Tividy with no packs installed, so they cover navigation, input checking and the empty states.
The screens that need a running workflow (start, act on an item, macro steps, history, progress, undo) need a pack and are tested with one
once the packs are merged.
*/

namespace tividy
{
	namespace
	{
		std::vector<std::string> lines(const std::string& a = "", const std::string& b = "", const std::string& c = "")
		{
			std::vector<std::string> v;
			v.push_back(a);
			if (b != "") v.push_back(b);
			if (c != "") v.push_back(c);
			return v;
		}

		// a named empty script, writing ScriptedConsole console(std::vector<std::string>()) is read by C++ as a function declaration
		std::vector<std::string> emptyScript()
		{
			return std::vector<std::string>();
		}

		// StdConsole talks to the real std::cin/std::cout, so swap their buffers for string streams for the length of one test
		class StreamSwap
		{
			public:
				explicit StreamSwap(const std::string& input) :
					in(input), old_in(std::cin.rdbuf(in.rdbuf())), old_out(std::cout.rdbuf(out.rdbuf())) { }

				~StreamSwap()
				{
					std::cin.rdbuf(old_in);
					std::cout.rdbuf(old_out);
					std::cin.clear();
				}

				std::string written() const { return out.str(); }

			private:
				std::istringstream in;
				std::ostringstream out;
				std::streambuf* old_in;
				std::streambuf* old_out;
		};
	} // anonymous namespace

	// ScriptedConsole

	TEST(ScriptedConsoleTest, StartsWithEmptyOutputAndInputNotExhausted)
	{
		ScriptedConsole console(lines("a"));
		EXPECT_EQ("", console.output());
		EXPECT_FALSE(console.isInputExhausted());
	}

	TEST(ScriptedConsoleTest, ReadLineReturnsScriptedLinesInOrder)
	{
		ScriptedConsole console(lines("first", "second", "third"));
		EXPECT_EQ("first", console.readLine("> "));
		EXPECT_EQ("second", console.readLine("> "));
		EXPECT_EQ("third", console.readLine("> "));
	}

	TEST(ScriptedConsoleTest, ReadLineTrimsSpacesTabsAndCarriageReturns)
	{
		ScriptedConsole console(lines("   padded   ", "\ttabbed\t", "windows\r"));
		EXPECT_EQ("padded", console.readLine(""));
		EXPECT_EQ("tabbed", console.readLine(""));
		EXPECT_EQ("windows", console.readLine(""));
	}

	TEST(ScriptedConsoleTest, ReadLineKeepsSpacesInsideALine)
	{
		ScriptedConsole console(lines("  two  words  "));
		EXPECT_EQ("two  words", console.readLine(""));
	}

	TEST(ScriptedConsoleTest, ReadLineEchoesPromptAndTrimmedLine)
	{
		ScriptedConsole console(lines("  hello  "));
		console.readLine("name: ");
		EXPECT_EQ("name: hello\n", console.output());
	}

	TEST(ScriptedConsoleTest, AWhitespaceOnlyLineBecomesEmptyButIsNotTheEndOfInput)
	{
		std::vector<std::string> script;
		script.push_back("   ");
		script.push_back("after");
		ScriptedConsole console(script);

		EXPECT_EQ("", console.readLine("> "));
		EXPECT_FALSE(console.isInputExhausted());
		EXPECT_EQ("after", console.readLine("> "));
	}

	TEST(ScriptedConsoleTest, ReadingPastTheEndReturnsEmptyAndSetsExhausted)
	{
		ScriptedConsole console(lines("only"));
		EXPECT_EQ("only", console.readLine("> "));
		EXPECT_FALSE(console.isInputExhausted()); // exhausted only once a read actually finds nothing

		EXPECT_EQ("", console.readLine("> "));
		EXPECT_TRUE(console.isInputExhausted());
	}

	TEST(ScriptedConsoleTest, ExhaustionIsStickyAndEchoesNothing)
	{
		ScriptedConsole console(emptyScript());
		EXPECT_EQ("", console.readLine("> "));
		EXPECT_EQ("", console.readLine("> "));
		EXPECT_TRUE(console.isInputExhausted());
		EXPECT_EQ("", console.output());
	}

	TEST(ScriptedConsoleTest, PrintAppendsWithoutANewline)
	{
		ScriptedConsole console(emptyScript());
		console.print("a");
		console.print("b");
		EXPECT_EQ("ab", console.output());
	}

	TEST(ScriptedConsoleTest, PrintlnAppendsTheTextThenANewline)
	{
		ScriptedConsole console(emptyScript());
		console.println("line one");
		console.println("");
		EXPECT_EQ("line one\n\n", console.output());
	}

	TEST(ScriptedConsoleTest, OutputKeepsPrintsAndEchoedReadsInTheOrderTheyHappened)
	{
		ScriptedConsole console(lines("42"));
		console.println("Welcome");
		console.readLine("number? ");
		console.print("done");
		EXPECT_EQ("Welcome\nnumber? 42\ndone", console.output());
	}

	TEST(ScriptedConsoleTest, WorksThroughTheConsoleBaseClass)
	{
		ScriptedConsole scripted(lines("via base"));
		Console& console = scripted;
		console.println("hi");
		EXPECT_EQ("via base", console.readLine("> "));
		EXPECT_EQ("hi\n> via base\n", scripted.output());
	}

	// StdConsole

	TEST(StdConsoleTest, StartsWithInputNotExhausted)
	{
		StdConsole console;
		EXPECT_FALSE(console.isInputExhausted());
	}

	TEST(StdConsoleTest, PrintAndPrintlnWriteToStandardOutput)
	{
		StreamSwap streams("");
		StdConsole console;
		console.print("a");
		console.println("b");
		EXPECT_EQ("ab\n", streams.written());
	}

	TEST(StdConsoleTest, ReadLineShowsThePromptAndReturnsTheTrimmedLine)
	{
		StreamSwap streams("  typed  \r\nnext\n");
		StdConsole console;
		EXPECT_EQ("typed", console.readLine("? "));
		EXPECT_EQ("next", console.readLine("? "));
		EXPECT_EQ("? ? ", streams.written());
		EXPECT_FALSE(console.isInputExhausted());
	}

	TEST(StdConsoleTest, ReadLineAtEndOfInputReturnsEmptyAndSetsExhausted)
	{
		StreamSwap streams("last\n");
		StdConsole console;
		EXPECT_EQ("last", console.readLine("? "));
		EXPECT_FALSE(console.isInputExhausted());

		EXPECT_EQ("", console.readLine("? "));
		EXPECT_TRUE(console.isInputExhausted());
	}

	TEST(StdConsoleTest, ALastLineWithoutANewlineIsStillRead)
	{
		StreamSwap streams("no newline");
		StdConsole console;
		EXPECT_EQ("no newline", console.readLine("? "));
		EXPECT_EQ("", console.readLine("? "));
		EXPECT_TRUE(console.isInputExhausted());
	}
	// chooseColorDepth

	TEST(ColorDepthTest, OutputThatIsNotATerminalNeverGetsColour)
	{
		EXPECT_EQ(ColorDepth::None, chooseColorDepth("truecolor", "xterm-256color", "vscode", false, false));
	}

	TEST(ColorDepthTest, NoColorAlwaysWins)
	{
		EXPECT_EQ(ColorDepth::None, chooseColorDepth("truecolor", "xterm-256color", "iTerm.app", true, true));
	}

	TEST(ColorDepthTest, ADumbOrMissingTermMeansNoColour)
	{
		EXPECT_EQ(ColorDepth::None, chooseColorDepth("truecolor", "dumb", "", false, true));
		EXPECT_EQ(ColorDepth::None, chooseColorDepth("truecolor", "", "", false, true));
	}

	TEST(ColorDepthTest, ColortermTruecolorOr24bitMeansTruecolor)
	{
		EXPECT_EQ(ColorDepth::TrueColor, chooseColorDepth("truecolor", "xterm-256color", "", false, true));
		EXPECT_EQ(ColorDepth::TrueColor, chooseColorDepth("24bit", "xterm", "", false, true));
	}

	TEST(ColorDepthTest, KnownTruecolorTerminalsAreRecognisedWithoutColorterm)
	{
		const char* programs[] = {"vscode", "iTerm.app", "WezTerm", "ghostty", "WindowsTerminal"};
		for (unsigned i = 0; i < 5; ++i)
			EXPECT_EQ(ColorDepth::TrueColor, chooseColorDepth("", "xterm-256color", programs[i], false, true)) << programs[i];
	}

	TEST(ColorDepthTest, A256ColourTermMeans256Colours)
	{
		EXPECT_EQ(ColorDepth::Ansi256, chooseColorDepth("", "xterm-256color", "", false, true));
		EXPECT_EQ(ColorDepth::Ansi256, chooseColorDepth("", "screen-256color", "", false, true));
	}

	TEST(ColorDepthTest, MacTerminalAppGets256ColoursEvenThoughItClaimsToBeXterm)
	{
		EXPECT_EQ(ColorDepth::Ansi256, chooseColorDepth("", "xterm", "Apple_Terminal", false, true));
	}

	TEST(ColorDepthTest, KittyIsTruecolor)
	{
		EXPECT_EQ(ColorDepth::TrueColor, chooseColorDepth("", "xterm-kitty", "", false, true));
	}

	TEST(ColorDepthTest, AnUnknownTerminalFallsBackToNoColour)
	{
		EXPECT_EQ(ColorDepth::None, chooseColorDepth("", "linux", "", false, true));
		EXPECT_EQ(ColorDepth::None, chooseColorDepth("", "vt100", "", false, true));
	}

	// LogoArt

	namespace
	{
		std::vector<std::string> splitLines(const std::string& text)
		{
			std::vector<std::string> out;
			std::istringstream stream(text);
			std::string line;
			while (std::getline(stream, line)) out.push_back(line);
			return out;
		}

		// how many character cells a line takes on screen: escape sequences take none, and every block character (3 bytes of UTF-8) takes one
		std::size_t visibleWidth(const std::string& line)
		{
			std::size_t cells = 0;
			for (std::size_t i = 0; i < line.size(); ++i)
			{
				if (line[i] == '\x1b')
				{
					while (i < line.size() && line[i] != 'm') ++i; // skip to the end of the escape sequence
				}
				else if ((static_cast<unsigned char>(line[i]) & 0xC0) != 0x80) ++cells; // count only the first byte of each character
			}
			return cells;
		}

		const ColorDepth DEPTHS[] = {ColorDepth::None, ColorDepth::Ansi256, ColorDepth::TrueColor};
	} // anonymous namespace

	TEST(LogoArtTest, EveryVariantOfEveryLogoHasContent)
	{
		for (unsigned i = 0; i < 3; ++i)
		{
			EXPECT_FALSE(hudsonLogo(DEPTHS[i]).empty());
			EXPECT_FALSE(warpedLogo(DEPTHS[i]).empty());
			EXPECT_FALSE(tividyLogo(DEPTHS[i]).empty());
		}
	}

	TEST(LogoArtTest, EveryVariantOfALogoHasTheSameNumberOfRows)
	{
		EXPECT_EQ(splitLines(hudsonLogo(ColorDepth::TrueColor)).size(), splitLines(hudsonLogo(ColorDepth::Ansi256)).size());
		EXPECT_EQ(splitLines(hudsonLogo(ColorDepth::TrueColor)).size(), splitLines(hudsonLogo(ColorDepth::None)).size());
		EXPECT_EQ(splitLines(warpedLogo(ColorDepth::TrueColor)).size(), splitLines(warpedLogo(ColorDepth::Ansi256)).size());
		EXPECT_EQ(splitLines(warpedLogo(ColorDepth::TrueColor)).size(), splitLines(warpedLogo(ColorDepth::None)).size());
		EXPECT_EQ(splitLines(tividyLogo(ColorDepth::TrueColor)).size(), splitLines(tividyLogo(ColorDepth::Ansi256)).size());
		EXPECT_EQ(splitLines(tividyLogo(ColorDepth::TrueColor)).size(), splitLines(tividyLogo(ColorDepth::None)).size());
	}

	TEST(LogoArtTest, ThePlainVariantContainsNoEscapeCodes)
	{
		EXPECT_EQ(std::string::npos, hudsonLogo(ColorDepth::None).find('\x1b'));
		EXPECT_EQ(std::string::npos, warpedLogo(ColorDepth::None).find('\x1b'));
		EXPECT_EQ(std::string::npos, tividyLogo(ColorDepth::None).find('\x1b'));
	}

	TEST(LogoArtTest, TheColourVariantsUseTheirOwnEscapeCodes)
	{
		EXPECT_NE(std::string::npos, hudsonLogo(ColorDepth::TrueColor).find("\x1b[38;2;"));
		EXPECT_NE(std::string::npos, warpedLogo(ColorDepth::TrueColor).find("\x1b[48;2;"));
		EXPECT_EQ(std::string::npos, hudsonLogo(ColorDepth::TrueColor).find("\x1b[38;5;"));

		EXPECT_NE(std::string::npos, hudsonLogo(ColorDepth::Ansi256).find("\x1b[38;5;"));
		EXPECT_NE(std::string::npos, warpedLogo(ColorDepth::Ansi256).find("\x1b[48;5;"));
		EXPECT_EQ(std::string::npos, hudsonLogo(ColorDepth::Ansi256).find("\x1b[38;2;"));
	}

	TEST(LogoArtTest, EveryColouredRowEndsWithAResetSoColourCannotLeak)
	{
		const std::string reset = "\x1b[0m";
		for (unsigned i = 1; i < 3; ++i)
		{
			std::vector<std::string> rows = splitLines(hudsonLogo(DEPTHS[i]));
			const std::vector<std::string> warped = splitLines(warpedLogo(DEPTHS[i]));
			rows.insert(rows.end(), warped.begin(), warped.end());
			for (std::size_t r = 0; r < rows.size(); ++r)
				EXPECT_EQ(reset, rows[r].substr(rows[r].size() - reset.size())) << "row " << r;
		}
	}

	TEST(LogoArtTest, NoRowIsWiderThanAClassicEightyColumnTerminal)
	{
		for (unsigned i = 0; i < 3; ++i)
		{
			std::vector<std::string> hudson = splitLines(hudsonLogo(DEPTHS[i]));
			std::vector<std::string> warped = splitLines(warpedLogo(DEPTHS[i]));
			for (std::size_t r = 0; r < hudson.size(); ++r) EXPECT_LE(visibleWidth(hudson[r]), 80u);
			for (std::size_t r = 0; r < warped.size(); ++r) EXPECT_LE(visibleWidth(warped[r]), 80u);
			std::vector<std::string> tividy = splitLines(tividyLogo(DEPTHS[i]));
			for (std::size_t r = 0; r < tividy.size(); ++r) EXPECT_LE(visibleWidth(tividy[r]), 80u);
		}
	}

	TEST(LogoArtTest, TheColouredRowsOfALogoAreAllTheSameWidth)
	{
		std::vector<std::string> rows = splitLines(warpedLogo(ColorDepth::TrueColor));
		ASSERT_FALSE(rows.empty());
		for (std::size_t r = 1; r < rows.size(); ++r) EXPECT_EQ(visibleWidth(rows[0]), visibleWidth(rows[r])) << "row " << r;
	}

	TEST(LogoArtTest, TheLogosAreAllDifferent)
	{
		EXPECT_NE(hudsonLogo(ColorDepth::TrueColor), warpedLogo(ColorDepth::TrueColor));
		EXPECT_NE(hudsonLogo(ColorDepth::TrueColor), tividyLogo(ColorDepth::TrueColor));
		EXPECT_NE(warpedLogo(ColorDepth::TrueColor), tividyLogo(ColorDepth::TrueColor));
	}

	namespace
	{
		// everything the tests need to run the Tui once: type these lines, get back what was printed
		struct TuiRun
		{
			TuiRun(Tividy& tividy, const std::vector<std::string>& typed) :
				console(typed), exit_code(-1)
			{
				Tui tui(tividy, console);
				exit_code = tui.run();
			}

			bool printed(const std::string& text) const
			{
				return console.output().find(text) != std::string::npos;
			}

			std::size_t countOf(const std::string& text) const
			{
				std::size_t count = 0;
				std::string::size_type at = 0;
				while ((at = console.output().find(text, at)) != std::string::npos)
				{
					++count;
					at += text.size();
				}
				return count;
			}

			ScriptedConsole console;
			int exit_code;
		};

		std::vector<std::string> typed(const char* first, const char* second = 0, const char* third = 0, const char* fourth = 0,
									   const char* fifth = 0, const char* sixth = 0)
		{
			std::vector<std::string> v;
			const char* all[] = {first, second, third, fourth, fifth, sixth};
			for (int i = 0; i < 6; ++i)
				if (all[i] != 0) v.push_back(all[i]);
			return v;
		}

		void addPat(Tividy& tividy)
		{
			std::set<std::string> roles;
			roles.insert("clerk");
			tividy.participants().add(Participant("p-1", "Pat", roles));
		}

		long secondsBetween(TimePoint from, TimePoint to)
		{
			return static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(to - from).count());
		}
	} // anonymous namespace

	TEST(TuiTest, QuitReturnsZeroAndSaysGoodbye)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("8"));
		EXPECT_EQ(0, run.exit_code);
		EXPECT_TRUE(run.printed("Goodbye."));
	}

	TEST(TuiTest, TheMenuListsAllEightOptions)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("8"));
		EXPECT_TRUE(run.printed("main menu"));
		EXPECT_TRUE(run.printed("1. Start a"));
		EXPECT_TRUE(run.printed("2. My"));
		EXPECT_TRUE(run.printed("3. "));
		EXPECT_TRUE(run.printed("4. Progress"));
		EXPECT_TRUE(run.printed("5. Macro builder"));
		EXPECT_TRUE(run.printed("6. Undo"));
		EXPECT_TRUE(run.printed("7. Advance time"));
		EXPECT_TRUE(run.printed("8. Quit"));
	}

	TEST(TuiTest, BackAtTheMenuLeaves)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("b"));
		EXPECT_EQ(0, run.exit_code);
	}

	TEST(TuiTest, RunningOutOfInputLeavesInsteadOfLoopingForever)
	{
		Tividy tividy;
		TuiRun run(tividy, emptyScript());
		EXPECT_EQ(0, run.exit_code);
	}

	TEST(TuiTest, InputEndingInsideAScreenStillLeaves)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("7"));
		EXPECT_EQ(0, run.exit_code);
	}

	TEST(TuiTest, ANonNumberAsksAgain)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("hello", "8"));
		EXPECT_TRUE(run.printed("Please enter a number from 1 to 8"));
		EXPECT_TRUE(run.printed("Goodbye."));
	}

	TEST(TuiTest, NumbersOutsideTheMenuAreRejected)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("0", "9", "8"));
		EXPECT_EQ(2u, run.countOf("Please enter a number from 1 to 8"));
	}

	TEST(TuiTest, NegativeAndDecimalChoicesAreRejected)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("-1", "2.5", "8"));
		EXPECT_EQ(2u, run.countOf("Please enter a number from 1 to 8"));
	}

	TEST(TuiTest, HeadingsCarryNoColourCodesWhenTheProfileHasNoAccent)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("8"));
		EXPECT_EQ(std::string::npos, run.console.output().find('\033'));
	}

	TEST(TuiTest, StartScreenSaysWhenNoWorkflowsAreInstalled)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("1", "8"));
		EXPECT_TRUE(run.printed("No workflows are installed."));
		EXPECT_TRUE(run.printed("Goodbye."));
	}

	TEST(TuiTest, HistoryWithNothingStartedSaysSo)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("3", "8"));
		EXPECT_TRUE(run.printed("Nothing has been started yet."));
		EXPECT_TRUE(run.printed("Goodbye."));
	}

	TEST(TuiTest, ProgressWithNothingStartedSaysSo)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("4", "8"));
		EXPECT_TRUE(run.printed("Nothing has been started yet."));
	}

	TEST(TuiTest, UndoWithNothingStartedSaysSo)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("6", "8"));
		EXPECT_TRUE(run.printed("Nothing has been started yet."));
	}

	TEST(TuiTest, AdvanceTimeMovesTheClockByWholeDays)
	{
		Tividy tividy;
		TimePoint before = tividy.now();
		TuiRun run(tividy, typed("7", "1", "b", "8"));
		EXPECT_EQ(86400, secondsBetween(before, tividy.now()));
		EXPECT_TRUE(run.printed("No deadlines lapsed."));
		EXPECT_TRUE(run.printed("Time is now"));
	}

	TEST(TuiTest, AdvanceTimeAcceptsDecimalDays)
	{
		Tividy tividy;
		TimePoint before = tividy.now();
		TuiRun run(tividy, typed("7", "0.5", "b", "8"));
		EXPECT_EQ(43200, secondsBetween(before, tividy.now()));
	}

	TEST(TuiTest, AdvanceTimeByZeroIsAllowedAndChangesNothing)
	{
		Tividy tividy;
		TimePoint before = tividy.now();
		TuiRun run(tividy, typed("7", "0", "b", "8"));
		EXPECT_EQ(0, secondsBetween(before, tividy.now()));
		EXPECT_TRUE(run.printed("No deadlines lapsed."));
	}

	TEST(TuiTest, AdvanceTimeRejectsNegativeTextAndHugeValues)
	{
		Tividy tividy;
		TimePoint before = tividy.now();
		TuiRun run(tividy, typed("7", "-1", "abc", "36501", "", "b"));
		EXPECT_EQ(4u, run.countOf("Please enter a number of days"));
		EXPECT_EQ(0, secondsBetween(before, tividy.now()));
	}

	TEST(TuiTest, AdvanceTimeAddsUpOverSeveralEntries)
	{
		Tividy tividy;
		TimePoint before = tividy.now();
		TuiRun run(tividy, typed("7", "1", "2", "b", "8"));
		EXPECT_EQ(3 * 86400, secondsBetween(before, tividy.now()));
	}

	TEST(TuiTest, AdvanceTimeBackReturnsToTheMenu)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("7", "b", "8"));
		EXPECT_EQ(2u, run.countOf("main menu"));
		EXPECT_TRUE(run.printed("Goodbye."));
	}

	TEST(TuiTest, MyWorkRejectsAnUnknownParticipant)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("2", "nobody", "", "8"));
		EXPECT_TRUE(run.printed("There is no participant with the id 'nobody'"));
		EXPECT_TRUE(run.printed("Goodbye."));
	}

	TEST(TuiTest, MyWorkWithAKnownParticipantWithNothingWaiting)
	{
		Tividy tividy;
		addPat(tividy);
		TuiRun run(tividy, typed("2", "p-1", "b", "8"));
		EXPECT_TRUE(run.printed("Now acting as Pat."));
		EXPECT_TRUE(run.printed("Nothing is waiting for Pat."));
	}

	TEST(TuiTest, PressingEnterOnAnEmptyWorkListSwitchesParticipant)
	{
		Tividy tividy;
		addPat(tividy);
		TuiRun run(tividy, typed("2", "p-1", "", "b", "8"));
		EXPECT_EQ(2u, run.countOf("Act as which participant?"));
	}

	TEST(TuiTest, TheActingParticipantIsRememberedBetweenVisits)
	{
		Tividy tividy;
		addPat(tividy);
		TuiRun run(tividy, typed("2", "p-1", "b", "2", "b", "8"));
		EXPECT_EQ(1u, run.countOf("Act as which participant?"));
		EXPECT_EQ(2u, run.countOf("Nothing is waiting for Pat."));
	}

	TEST(TuiTest, MacroBuilderExplainsItselfThenBacksOutWithoutAParticipant)
	{
		Tividy tividy;
		TuiRun run(tividy, typed("5", "", "8"));
		EXPECT_TRUE(run.printed("Macro builder"));
		EXPECT_TRUE(run.printed("if a step fails, the steps before it are undone"));
		EXPECT_TRUE(run.printed("Goodbye."));
	}

	TEST(TuiTest, MacroBuilderWithNothingStartedSaysSo)
	{
		Tividy tividy;
		addPat(tividy);
		TuiRun run(tividy, typed("5", "p-1", "8"));
		EXPECT_TRUE(run.printed("Nothing has been started yet."));
		EXPECT_TRUE(run.printed("Goodbye."));
	}

	TEST(TuiTest, TheTuiNeverThrowsOnRubbishInput)
	{
		Tividy tividy;
		addPat(tividy);
		EXPECT_NO_THROW({
			TuiRun run(tividy, typed("2", "!!", "p-1", "??", "b", "7"));
			EXPECT_EQ(0, run.exit_code);
		});
	}

	TEST(ClientProfileTest, DefaultsAreTheNeutralTividyLook)
	{
		ClientProfile profile;
		EXPECT_EQ("TIVIDY", profile.product_name);
		EXPECT_EQ("", profile.tagline);
		EXPECT_EQ("", profile.banner);
		EXPECT_EQ("", profile.accent);
		EXPECT_TRUE(profile.vocabulary.empty());
	}

	TEST(ClientProfileTest, AnUnmappedTermComesBackUnchanged)
	{
		ClientProfile profile;
		EXPECT_EQ("work-list", profile.term("work-list"));
		EXPECT_EQ("", profile.term(""));
	}

	TEST(ClientProfileTest, AMappedTermComesBackRenamed)
	{
		ClientProfile profile;
		profile.vocabulary["instance"] = "Dispute case";
		EXPECT_EQ("Dispute case", profile.term("instance"));
	}

	TEST(ClientProfileTest, RenamingOneTermLeavesTheOthersAlone)
	{
		ClientProfile profile;
		profile.vocabulary["instance"] = "Order";
		EXPECT_EQ("item", profile.term("item"));
	}

	TEST(ClientProfileTest, TermLookupIsCaseSensitive)
	{
		ClientProfile profile;
		profile.vocabulary["instance"] = "Order";
		EXPECT_EQ("Instance", profile.term("Instance"));
	}

	TEST(ClientProfileTest, ACopyKeepsItsOwnVocabulary)
	{
		ClientProfile original;
		original.vocabulary["item"] = "Step";

		ClientProfile copy = original;
		copy.vocabulary["item"] = "Action";

		EXPECT_EQ("Step", original.term("item"));
		EXPECT_EQ("Action", copy.term("item"));
	}
} // namespace tividy