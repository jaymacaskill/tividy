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
 * @file Tui.cpp
 * @brief Implements the interactive text user interface.
 * @details Draws the menu and the screens (start a workflow, my work, macro
 * builder, history, progress, undo, advance time), using each client's
 * vocabulary. It talks only to the Tividy facade and a Console.
 */

#ifndef TIVIDY_UI_TUI_CPP
#define TIVIDY_UI_TUI_CPP

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <memory>
#include <sstream>
#include <vector>

#include "ui/Tui.h"
#include "engine/commands/CommandFactory.h"
#include "engine/commands/MacroCommand.h"
#include "engine/core/Errors.h"

namespace tividy
{
	namespace
	{
		const std::string RESET = "\033[0m";
		const long SECONDS_PER_DAY = 24 * 60 * 60;
		const double MAX_DAYS = 36500; // a hundred years, anything bigger is a typo and would overflow the clock
		const int MAX_MACRO_ATTEMPTS = 5;

		std::string trim(const std::string& text)
		{
			const std::string whitespace = " \t\r\n";

			std::string::size_type first = text.find_first_not_of(whitespace);
			if (first == std::string::npos) return "";

			std::string::size_type last = text.find_last_not_of(whitespace);
			return text.substr(first, last - first + 1);
		}

		std::string lowercase(std::string text)
		{
			for (std::size_t i = 0; i < text.size(); ++i)
				if (text[i] >= 'A' && text[i] <= 'Z') text[i] = static_cast<char>(text[i] - 'A' + 'a');

			return text;
		}

		std::string capitalised(std::string text)
		{
			if (!text.empty() && text[0] >= 'a' && text[0] <= 'z') text[0] = static_cast<char>(text[0] - 'a' + 'A');
			return text;
		}

		std::string pad(const std::string& text, std::size_t width)
		{
			if (text.size() >= width) return text;
			return text + std::string(width - text.size(), ' ');
		}

		// the profile's word for an engine term. a term the pack did not rename comes back unchanged ("work-list"), so tidy it up for people
		std::string label(const ClientProfile& profile, const std::string& engine_term)
		{
			std::string word = profile.term(engine_term);
			if (word == engine_term)
			{
				for (std::size_t i = 0; i < word.size(); ++i)
					if (word[i] == '-') word[i] = ' ';
			}

			return word;
		}

		// whole numbers only: "3" is fine, "3.5", "-1", "three" and "" are not
		bool parseWholeNumber(const std::string& text, int& value)
		{
			if (text.empty() || text.size() > 9) return false;

			for (std::size_t i = 0; i < text.size(); ++i)
				if (text[i] < '0' || text[i] > '9') return false;

			value = std::atoi(text.c_str());
			return true;
		}

		// decimals are allowed ("0.5" is half a day), negatives and non-numbers are not
		bool parseDays(const std::string& text, double& days)
		{
			if (text.empty()) return false;

			char* end = 0;
			double parsed = std::strtod(text.c_str(), &end);
			if (*end != '\0' || !std::isfinite(parsed) || parsed < 0) return false;

			days = parsed;
			return true;
		}

		// the accent is an ANSI colour code from the pack's profile, "" means plain text
		void heading(Console& console, const ClientProfile& profile, const std::string& text)
		{
			console.println("");
			if (profile.accent == "") console.println(text);
			else console.println(profile.accent + text + RESET);
			console.println(std::string(text.size(), '-'));
		}

		std::string resultLine(const CommandResult& result)
		{
			if (result.success) return "OK: " + result.message;
			return "Not allowed: " + result.message;
		}

		std::string formatTime(TimePoint time)
		{
			std::time_t seconds = std::chrono::system_clock::to_time_t(time);
			std::tm* parts = std::gmtime(&seconds);
			if (!parts) return "unknown time";

			char buffer[32];
			std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M UTC", parts);
			return buffer;
		}

		// "due in 2d 4h", "due in 3h 15m", "due in 12m" or "OVERDUE". TimePoint() means the item has no deadline
		std::string formatDue(TimePoint due, TimePoint now)
		{
			if (due == TimePoint()) return "no deadline";

			long seconds = static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(due - now).count());
			if (seconds <= 0) return "OVERDUE";

			long days = seconds / SECONDS_PER_DAY;
			long hours = (seconds % SECONDS_PER_DAY) / 3600;
			long minutes = (seconds % 3600) / 60;

			std::ostringstream out;
			out << "due in ";
			if (days > 0) out << days << "d " << hours << "h";
			else if (hours > 0) out << hours << "h " << minutes << "m";
			else if (minutes > 0) out << minutes << "m";
			else out << "<1m";

			return out.str();
		}

		// "[#####-----] 50%": ten slots, one # per full ten percent
		std::string progressBar(double percent)
		{
			if (percent < 0) percent = 0;
			if (percent > 100) percent = 100;

			int filled = static_cast<int>(percent / 10);
			std::ostringstream out;
			out << "[" << std::string(static_cast<std::size_t>(filled), '#') << std::string(static_cast<std::size_t>(10 - filled), '-')
				<< "] " << static_cast<int>(percent + 0.5) << "%";

			return out.str();
		}

		// asks for a number in [1, max]. returns it, or 0 for back / end of input, or -1 if the user typed @p extra_key instead.
		// extra_key is the single key to match ("a"), extra_hint is how it is described in the prompt ("a = act as someone else")
		int chooseOrKey(Console& console, const std::string& prompt, int max, const std::string& extra_key, const std::string& extra_hint = "")
		{
			if (max < 1) return 0;

			std::ostringstream hint;
			hint << prompt << " [1-" << max;
			if (extra_key != "") hint << ", " << (extra_hint != "" ? extra_hint : extra_key);
			hint << ", b = back]: ";

			while (true)
			{
				std::string line = console.readLine(hint.str());
				if (console.isInputExhausted()) return 0;
				if (line == "b" || line == "B") return 0;
				if (extra_key != "" && lowercase(line) == extra_key) return -1;

				int number = 0;
				if (parseWholeNumber(line, number) && number >= 1 && number <= max) return number;

				std::ostringstream complaint;
				complaint << "Please enter a number from 1 to " << max << ", or b to go back.";
				console.println(complaint.str());
			}
		}

		// reads who the user is acting as. blank or "b" goes back. returns false when there is nobody to act as
		bool askActor(Tividy& tividy, Console& console, Id& acting_as)
		{
			if (acting_as != "") return true;

			while (true)
			{
				std::string line = console.readLine("Act as which participant? (type their id, b = back): ");
				if (console.isInputExhausted() || line == "" || line == "b" || line == "B") return false;

				if (tividy.participants().find(line) == 0)
				{
					console.println("There is no participant with the id '" + line + "'. Check the id and try again.");
					continue;
				}

				acting_as = line;
				console.println("Now acting as " + tividy.participants().require(line).displayName() + ".");
				return true;
			}
		}

		// asks the follow-up questions a verb needs. returns false if the input ended
		bool askVerbDetails(Console& console, const std::string& verb, Outcome& outcome, std::string& text)
		{
			outcome = Outcome();
			text = "";

			if (verb == "complete" || verb == "submit" || verb == "approve")
			{
				std::string code = console.readLine("Outcome code (blank = none): ");
				if (console.isInputExhausted()) return false;

				std::string note = console.readLine("Note (blank = none): ");
				if (console.isInputExhausted()) return false;

				// the code decides where the workflow goes next, the kind is what the history and lastOutcome() show
				OutcomeKind kind = OutcomeKind::Accepted;
				while (true)
				{
					std::string kind_line = lowercase(console.readLine("Result (a = accepted, r = rejected, e = escalated, f = failed, blank = accepted): "));
					if (console.isInputExhausted()) return false;

					if (kind_line == "" || kind_line == "a") kind = OutcomeKind::Accepted;
					else if (kind_line == "r") kind = OutcomeKind::Rejected;
					else if (kind_line == "e") kind = OutcomeKind::Escalated;
					else if (kind_line == "f") kind = OutcomeKind::Failed;
					else
					{
						console.println("Please type a, r, e or f, or press Enter for accepted.");
						continue;
					}

					break;
				}

				outcome = Outcome(kind, code, note);
			}
			else if (verb == "reject" || verb == "escalate" || verb == "cancel")
			{
				text = console.readLine("Reason: ");
				if (console.isInputExhausted()) return false;
			}

			return true;
		}

		// lists the verbs, asks for one. returns "" for back / end of input
		std::string askVerb(Tividy& tividy, Console& console, const ClientProfile& profile)
		{
			std::vector<std::string> verbs = tividy.commandFactory().verbs();
			for (std::size_t i = 0; i < verbs.size(); ++i)
			{
				std::ostringstream row;
				row << "  " << (i + 1) << ". " << capitalised(label(profile, verbs[i]));
				console.println(row.str());
			}

			int choice = chooseOrKey(console, "Which action?", static_cast<int>(verbs.size()), "");
			if (choice < 1) return "";

			return verbs[static_cast<std::size_t>(choice - 1)];
		}

		// lists every instance and asks which one. returns "" for back, end of input, or when nothing has been started
		Id pickInstance(Tividy& tividy, Console& console, const ClientProfile& profile, const Id& focus, const std::string& prompt)
		{
			std::vector<Id> ids = tividy.instanceIds();
			if (ids.empty())
			{
				console.println("Nothing has been started yet. Start a " + lowercase(label(profile, "workflow")) + " first.");
				return "";
			}

			for (std::size_t i = 0; i < ids.size(); ++i)
			{
				const WorkflowInstance& instance = tividy.instance(ids[i]);

				std::ostringstream row;
				row << "  " << (i + 1) << ". " << pad(ids[i], 10) << pad(instance.definitionName(), 24) << toString(instance.status());
				if (ids[i] == focus) row << "   <- in focus";
				console.println(row.str());
			}

			int choice = chooseOrKey(console, prompt, static_cast<int>(ids.size()), "");
			if (choice < 1) return "";

			return ids[static_cast<std::size_t>(choice - 1)];
		}
	} // anonymous namespace

	Tui::Tui(Tividy& tividy, Console& console) :
		tividy_ref(tividy), console(console), acting_as(""), focus_instance("") { }

	int Tui::run()
	{
		const ClientProfile& profile = profileFor("");

		if (profile.banner != "")
		{
			console.print(profile.banner);
			if (profile.banner[profile.banner.size() - 1] != '\n') console.println("");
		}

		if (profile.tagline != "") console.println(profile.tagline);

		mainMenu();

		console.println("Goodbye.");
		return 0;
	}

	const ClientProfile& Tui::profileFor(const Id& instance_id) const
	{
		static const ClientProfile neutral;

		try
		{
			// the main menu passes "" and gets the first installed pack, a screen about an instance gets that instance's own pack
			if (instance_id != "")
			{
				const WorkflowInstance& instance = tividy_ref.instance(instance_id);
				return tividy_ref.profile(tividy_ref.packOf(instance.definitionName()));
			}

			std::vector<std::string> keys = tividy_ref.packKeys();
			if (!keys.empty()) return tividy_ref.profile(keys[0]);
		}
		catch (const NotFoundError&)
		{
			// fall through to the neutral profile, a missing pack should never stop the screens from working
		}

		return neutral;
	}

	int Tui::askChoice(const std::string& prompt, int max)
	{
		return chooseOrKey(console, prompt, max, "");
	}

	void Tui::mainMenu()
	{
		while (!console.isInputExhausted())
		{
			const ClientProfile& profile = profileFor("");

			heading(console, profile, profile.product_name + " - main menu");

			const std::string options[] = {
				"Start a " + lowercase(label(profile, "workflow")),
				"My " + lowercase(label(profile, "work-list")),
				capitalised(label(profile, "history")),
				"Progress",
				"Macro builder (several actions in one go)",
				"Undo the last action",
				"Advance time (simulated)",
				"Quit"
			};

			const int option_count = static_cast<int>(sizeof(options) / sizeof(options[0]));
			for (int i = 0; i < option_count; ++i)
			{
				std::ostringstream row;
				row << "  " << (i + 1) << ". " << options[i];
				console.println(row.str());
			}

			int choice = askChoice("Choose", option_count);
			if (choice == 0 || choice == option_count) return; // "b", end of input and "Quit" all leave

			// the screens already turn ordinary mistakes into messages, this only stops anything unexpected from ending the program
			try
			{
				// menu dispatch, not state dispatch: each case is the entry point of one screen and nothing varies by type,
				// so this is a lookup from a number to a screen, not a switch standing in for polymorphism
				switch (choice)
				{
					case 1: startWorkflowScreen(); break;
					case 2: myWorkScreen(); break;
					case 3: historyScreen(); break;
					case 4: progressScreen(); break;
					case 5: macroBuilderScreen(); break;
					case 6: undoScreen(); break;
					case 7: advanceTimeScreen(); break;
				}
			}
			catch (const std::exception& error)
			{
				console.println(std::string("Something went wrong: ") + error.what());
			}
		}
	}

	void Tui::startWorkflowScreen()
	{
		std::vector<std::string> names = tividy_ref.workflowNames();
		if (names.empty())
		{
			console.println("No workflows are installed.");
			return;
		}

		while (!console.isInputExhausted())
		{
			heading(console, profileFor(""), "Start a " + lowercase(label(profileFor(""), "workflow")));

			for (std::size_t i = 0; i < names.size(); ++i)
			{
				std::ostringstream row;
				row << "  " << (i + 1) << ". " << pad(names[i], 28) << "(" << tividy_ref.packOf(names[i]) << ")";
				console.println(row.str());
			}

			int choice = askChoice("Which one?", static_cast<int>(names.size()));
			if (choice == 0) return;

			const std::string name = names[static_cast<std::size_t>(choice - 1)];
			const ClientProfile& profile = tividy_ref.profile(tividy_ref.packOf(name));

			console.println("Enter the starting data as key=value, one per line. A blank line finishes.");

			WorkflowData data;
			while (true)
			{
				std::string line = console.readLine("  data> ");
				if (console.isInputExhausted()) return; // the input ended halfway, so nothing is started
				if (line == "") break;

				std::string::size_type equals = line.find('=');
				std::string key = (equals == std::string::npos) ? "" : trim(line.substr(0, equals));
				if (key == "")
				{
					console.println("Use key=value, for example amount=250");
					continue;
				}

				data.set(key, trim(line.substr(equals + 1)));
			}

			try
			{
				Id id = tividy_ref.startWorkflow(name, data);
				focus_instance = id;
				console.println("Started " + lowercase(label(profile, "instance")) + " " + id + " (" + name + ").");
				return;
			}
			catch (const std::exception& error)
			{
				console.println(std::string("Could not start: ") + error.what());
			}
		}
	}

	void Tui::myWorkScreen()
	{
		while (!console.isInputExhausted())
		{
			if (!askActor(tividy_ref, console, acting_as)) return;

			const ClientProfile& profile = profileFor(focus_instance);
			const std::string who = tividy_ref.participants().require(acting_as).displayName();

			heading(console, profile, "My " + lowercase(label(profile, "work-list")) + " - " + who);

			std::vector<WorkItemView> work = tividy_ref.workFor(acting_as);
			if (work.empty())
			{
				console.println("Nothing is waiting for " + who + ".");
				std::string line = console.readLine("Press Enter to switch participant, b = back: ");
				if (console.isInputExhausted() || line == "b" || line == "B") return;

				acting_as = "";
				continue;
			}

			const TimePoint now = tividy_ref.now();
			console.println("   #  " + pad(capitalised(label(profile, "item")), 30) + pad("Status", 13) + pad("Due", 18) + "Where");
			for (std::size_t i = 0; i < work.size(); ++i)
			{
				std::ostringstream row;
				row << "  " << pad(std::to_string(i + 1), 3) << " " << pad(work[i].name, 30) << pad(toString(work[i].status), 13)
					<< pad(formatDue(work[i].due_at, now), 18) << work[i].instance_id;
				console.println(row.str());
			}

			int choice = chooseOrKey(console, "Pick one", static_cast<int>(work.size()), "a", "a = act as someone else");
			if (choice == 0) return;
			if (choice == -1)
			{
				acting_as = "";
				continue;
			}

			const WorkItemView& picked = work[static_cast<std::size_t>(choice - 1)];
			focus_instance = picked.instance_id;

			console.println("What do you want to do with '" + picked.name + "'?");
			std::string verb = askVerb(tividy_ref, console, profileFor(focus_instance));
			if (verb == "") continue;

			Outcome outcome;
			std::string text;
			if (!askVerbDetails(console, verb, outcome, text)) return;

			CommandResult result = tividy_ref.perform(verb, picked.instance_id, picked.item_id, acting_as, outcome, text);
			console.println(resultLine(result));
		}
	}

	void Tui::macroBuilderScreen()
	{
		const ClientProfile& chosen_profile = profileFor(focus_instance);
		heading(console, chosen_profile, "Macro builder");
		console.println("A macro runs several actions as one: if a step fails, the steps before it are undone.");

		if (!askActor(tividy_ref, console, acting_as)) return;

		Id instance_id = pickInstance(tividy_ref, console, chosen_profile, focus_instance, "Build it for which " + lowercase(label(chosen_profile, "instance")) + "?");
		if (instance_id == "") return;
		focus_instance = instance_id;

		const ClientProfile& profile = profileFor(instance_id);
		WorkflowInstance& instance = tividy_ref.instance(instance_id);
		const Participant& actor = tividy_ref.participants().require(acting_as);

		std::string name = console.readLine("Name for this macro (blank = \"macro\"): ");
		if (console.isInputExhausted()) return;
		if (name == "") name = "macro";

		std::unique_ptr<MacroCommand> macro(new MacroCommand(name));

		console.println("Add steps one at a time. Type the " + lowercase(label(profile, "item")) + " id, or a blank line when you are done.");
		while (true)
		{
			std::ostringstream prompt;
			prompt << "Step " << (macro->size() + 1) << " - " << lowercase(label(profile, "item")) << " id: ";

			std::string item_id = console.readLine(prompt.str());
			if (console.isInputExhausted()) return; // the input ended halfway, so nothing is run
			if (item_id == "") break;

			WorkItem* item = instance.findItem(item_id);
			if (item == 0)
			{
				console.println("There is no " + lowercase(label(profile, "item")) + " with the id '" + item_id + "' in " + instance_id + ".");
				continue;
			}

			console.println("What should " + actor.displayName() + " do with '" + item->spec().name() + "'?");
			std::string verb = askVerb(tividy_ref, console, profile);
			if (verb == "") continue;

			Outcome outcome;
			std::string text;
			if (!askVerbDetails(console, verb, outcome, text)) return;

			try
			{
				CommandRequest request = {instance, item_id, actor, outcome, text};
				macro->add(tividy_ref.commandFactory().create(verb, request));
				console.println("Added: " + verb + " " + item_id + ".");
			}
			catch (const EngineError& error)
			{
				console.println(std::string("Could not add that step: ") + error.what());
			}
		}

		if (macro->size() == 0)
		{
			console.println("No steps were added, so there is nothing to run.");
			return;
		}

		console.println(macro->describe());

		int attempts = 1;
		while (true)
		{
			std::ostringstream prompt;
			prompt << "Try again if it fails? Attempts 1-" << MAX_MACRO_ATTEMPTS << " (blank = 1, no retry): ";

			std::string line = console.readLine(prompt.str());
			if (console.isInputExhausted()) return;
			if (line == "") break;

			int number = 0;
			if (parseWholeNumber(line, number) && number >= 1 && number <= MAX_MACRO_ATTEMPTS)
			{
				attempts = number;
				break;
			}

			console.println("Please enter a number from 1 to 5.");
		}

		std::unique_ptr<Command> command(macro.release());
		if (attempts > 1)
		{
			std::unique_ptr<Command> retrying(new RetryingCommand(std::move(command), static_cast<unsigned>(attempts)));
			command = std::move(retrying);
		}

		console.println(resultLine(tividy_ref.execute(instance_id, std::move(command))));
	}

	void Tui::historyScreen()
	{
		while (!console.isInputExhausted())
		{
			const ClientProfile& profile = profileFor(focus_instance);
			heading(console, profile, capitalised(label(profile, "history")));

			Id instance_id = pickInstance(tividy_ref, console, profile, focus_instance, "Show the " + lowercase(label(profile, "history")) + " of");
			if (instance_id == "") return;
			focus_instance = instance_id;

			std::vector<std::string> lines = tividy_ref.history(instance_id);
			if (lines.empty()) console.println("Nothing has happened yet.");

			for (std::size_t i = 0; i < lines.size(); ++i) console.println("  " + lines[i]);
		}
	}

	void Tui::progressScreen()
	{
		while (!console.isInputExhausted())
		{
			const ClientProfile& profile = profileFor(focus_instance);
			heading(console, profile, "Progress");

			Id instance_id = pickInstance(tividy_ref, console, profile, focus_instance, "Show the progress of");
			if (instance_id == "") return;
			focus_instance = instance_id;

			ProgressSummary summary = tividy_ref.progress(instance_id);

			std::ostringstream total;
			total << summary.total << " " << lowercase(label(profile, "item")) << (summary.total == 1 ? "" : "s");
			console.println("  " + progressBar(summary.percent_settled) + "  settled, out of " + total.str());

			// only the statuses that have items, so the list stays short
			for (std::map<ItemStatus, std::size_t>::const_iterator it = summary.by_status.begin(); it != summary.by_status.end(); ++it)
			{
				if (it->second == 0) continue;

				std::ostringstream row;
				row << "    " << pad(toString(it->first), 13) << it->second;
				console.println(row.str());
			}
		}
	}

	void Tui::undoScreen()
	{
		while (!console.isInputExhausted())
		{
			const ClientProfile& profile = profileFor(focus_instance);
			heading(console, profile, "Undo the last action");

			Id instance_id = pickInstance(tividy_ref, console, profile, focus_instance, "Undo the last action in");
			if (instance_id == "") return;
			focus_instance = instance_id;

			console.println(resultLine(tividy_ref.undo(instance_id)));
		}
	}

	void Tui::advanceTimeScreen()
	{
		while (!console.isInputExhausted())
		{
			const ClientProfile& profile = profileFor(focus_instance);
			heading(console, profile, "Advance time");
			console.println("Time here is simulated: it only moves when you move it, so deadlines can be shown in seconds.");
			console.println("Now: " + formatTime(tividy_ref.now()));

			std::string line = console.readLine("How many days? (decimals allowed, b = back): ");
			if (console.isInputExhausted() || line == "b" || line == "B") return;

			double days = 0;
			if (!parseDays(line, days) || days > MAX_DAYS)
			{
				console.println("Please enter a number of days from 0 to 36500, for example 2 or 0.5.");
				continue;
			}

			long seconds = static_cast<long>(days * SECONDS_PER_DAY + 0.5);
			std::size_t lapsed = tividy_ref.advanceTime(seconds);

			std::ostringstream report;
			if (lapsed == 0) report << "No deadlines lapsed.";
			else report << lapsed << " deadline" << (lapsed == 1 ? "" : "s") << " lapsed.";

			console.println(report.str());
			console.println("Time is now " + formatTime(tividy_ref.now()) + ".");
		}
	}
} // namespace tividy

#endif // TIVIDY_UI_TUI_CPP