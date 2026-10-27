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

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ui/Console.h"

/*
These tests cover the Console abstraction only (src/ui/Console.cpp).
Tui, TuiScreens and ClientProfile get their tests when their implementations land.
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
		EXPECT_FALSE(console.isInputExhausted());

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
} // namespace tividy