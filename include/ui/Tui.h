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
 * @file Tui.h
 * @brief The interactive text user interface. Talks ONLY to the Tividy facade and a Console.
 */
#ifndef TIVIDY_UI_TUI_H
#define TIVIDY_UI_TUI_H

#include <string>

#include "engine/facade/Tividy.h"
#include "ui/Console.h"

namespace tividy
{

	/**
	 * @brief Interactive controller.
	 *
	 * ### Screens (private methods; each loops until the user types "b" for back). Input errors re-prompt, never throw.
	 *  - main menu: start workflow, my work, history, progress, macro builder, undo, quit.
	 *  - start workflow: list workflowNames(); ask "key=value" data lines (blank line ends); startWorkflow(); print the new id.
	 *  - my work: ask "act as" participant id (remembered); show workFor() as a numbered table; pick one; list the verbs in
	 *    commandFactory().verbs(); ask an outcome code (complete/submit/approve) or a reason (reject/escalate/cancel); call perform();
	 *    print "OK: <message>" or "Not allowed: <message>". Illegal actions just show the reason.
	 *  - macro builder: collect several (verb, item) steps into a MacroCommand; optionally wrap it in RetryingCommand; run via execute().
	 *  - history: show history() lines. progress: show a bar "[#####-----] 50%" and per-status counts. undo: call undo().
	 * Every screen prints headings in the accent colour and labels through the CURRENT pack's ClientProfile::term(), where the current
	 * pack is packOf(workflow of the instance in focus) (the main menu uses the first installed pack's profile).
	 * The loop ends when the user quits or console.isInputExhausted() is true.
	 */
	class Tui
	{
		public:
			Tui(Tividy& tividy, Console& console);

			/** @brief Runs until quit or end of input. @return 0 always. */
			int run();
		private:
			void mainMenu();
			void startWorkflowScreen();
			void myWorkScreen();
			void macroBuilderScreen();
			void historyScreen();
			void progressScreen();
			void undoScreen();

			/** Asks until the user enters an integer in [1, max] or "b"; @return the number, or 0 for back/end of input. */
			int askChoice(const std::string& prompt, int max);

			/** @return the profile for the instance in focus (or the first installed pack when none). */
			const ClientProfile& profileFor(const Id& instance_id) const;

			Tividy& tividy_ref;
			Console& console;
			Id acting_as;
			Id focus_instance;
	};
} // namespace tividy

#endif // TIVIDY_UI_TUI_H