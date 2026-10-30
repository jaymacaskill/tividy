#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/commands/ActionHistory.h"
#include "engine/commands/Command.h"
#include "engine/commands/CommandFactory.h"
#include "engine/commands/WorkItemCommands.h"
#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Clock.h"
#include "engine/core/Errors.h"
#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"
#include "engine/core/WorkflowData.h"
#include "engine/definition/WorkflowDefinition.h"
#include "engine/items/StageItem.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/policies/AssignmentStrategy.h"
#include "engine/runtime/Observers.h"
#include "engine/runtime/WorkflowInstance.h"

/*
	Command:          Command (execute, undo, isReversible, describe).
	ConcreteCommands: StartCommand, CompleteCommand, CancelCommand, ... (InstanceCommand family, Memento-based undo).
	Invoker:          ActionHistory (one per WorkflowInstance; keeps the undo stack).
	Receiver:         WorkItem.
	Client:           CommandFactory (creates a command from a verb word, no switch statement).
*/

namespace tividy
{
	namespace
	{
		// Test double: a task whose work cannot be rolled back, so the irreversible paths can be checked without an industry pack.
		class IrreversibleTask : public CloneableTask<IrreversibleTask>
		{
			public:
				explicit IrreversibleTask(const WorkItemSpec& spec) : CloneableTask<IrreversibleTask>(spec) { }
			protected:
				void performWork(const Participant&, Outcome&) override { }
				bool performsIrreversibleWork() const override { return true; }
		};

		bool hasEntry(const AuditLog& log, EventType type)
		{
			for (const AuditEntry& entry : log.entries())
			{
				if (entry.type == type)
				{
					return true;
				}
			}
			return false;
		}

		// A clerk "Una" (id u1) and RoleBasedAssignment, so every clerk item is assigned to u1 as soon as it is released.
		class CommandPatternTest : public ::testing::Test
		{
			protected:
				CommandPatternTest() : clock(TimePoint()), services{clock, directory, connectors}
				{
					directory.add(Participant("u1", "Una", {"clerk"}));
					policies.assignment = std::make_shared<RoleBasedAssignment>();
				}

				// root
				//  |- a  "A"  (clerk)
				//  |- b  "B"  (clerk, depends on a)
				static std::unique_ptr<StageItem> twoStepFlow()
				{
					std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
					root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("a", "A").requireRole("clerk"))));
					root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("b", "B").requireRole("clerk").dependsOn("a"))));
					return root;
				}

				// root
				//  |- launch  "Launch"  (clerk, irreversible work)
				static std::unique_ptr<StageItem> irreversibleFlow()
				{
					std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
					root->add(std::unique_ptr<WorkItem>(new IrreversibleTask(WorkItemSpec("launch", "Launch").requireRole("clerk"))));
					return root;
				}

				std::unique_ptr<WorkflowInstance> makeInstance(std::unique_ptr<StageItem> root)
				{
					return std::unique_ptr<WorkflowInstance>(new WorkflowInstance("inst-1", "flow", 1, WorkflowData(),
						std::unique_ptr<WorkItem>(root.release()), policies, services));
				}

				std::unique_ptr<WorkflowInstance> startedInstance(std::unique_ptr<StageItem> root)
				{
					std::unique_ptr<WorkflowInstance> instance = makeInstance(std::move(root));
					instance->start();
					return instance;
				}

				static WorkItem& item(WorkflowInstance& instance, const Id& id)
				{
					WorkItem* found = instance.findItem(id);
					if (found == nullptr)
					{
						throw std::runtime_error("test setup: no item " + id);
					}
					return *found;
				}

				const Participant& clerk() const { return directory.require("u1"); }

				std::unique_ptr<Command> start(WorkflowInstance& instance, const Id& id)
				{
					return std::unique_ptr<Command>(new StartCommand(instance, id, clerk()));
				}

				std::unique_ptr<Command> complete(WorkflowInstance& instance, const Id& id, const Outcome& outcome = Outcome::accepted())
				{
					return std::unique_ptr<Command>(new CompleteCommand(instance, id, clerk(), outcome));
				}

				CommandRequest request(WorkflowInstance& instance, const Id& id, const Outcome& outcome = Outcome::accepted(), const std::string& text = "")
				{
					return CommandRequest{instance, id, clerk(), outcome, text};
				}

				ManualClock clock;
				ParticipantDirectory directory;
				ConnectorRegistry connectors;
				Services services;
				DefinitionPolicies policies;
		};
	}

	// --- ConcreteCommands through the abstract Command interface ---

	TEST_F(CommandPatternTest, ExecuteRunsTheRequestOnTheReceiver)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		std::unique_ptr<Command> command = start(*instance, "a");

		EXPECT_TRUE(command->isReversible());
		CommandResult result = command->execute();

		EXPECT_TRUE(result.success);
		EXPECT_EQ(result.message, command->describe());
		EXPECT_EQ(item(*instance, "a").status(), ItemStatus::InProgress);
	}

	TEST_F(CommandPatternTest, DescribeNamesActorVerbItemAndDetail)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		std::unique_ptr<Command> started = start(*instance, "a");
		started->execute();
		std::unique_ptr<Command> completed = complete(*instance, "a", Outcome::rejected("DOCS_INVALID"));
		completed->execute();
		std::unique_ptr<Command> cancelled(new CancelCommand(*instance, "b", clerk(), "no longer needed"));
		cancelled->execute();

		EXPECT_EQ(started->describe(), "Una started 'A'");
		EXPECT_EQ(completed->describe(), "Una completed 'A' [DOCS_INVALID]");
		EXPECT_EQ(cancelled->describe(), "Una cancelled 'B': no longer needed");
	}

	TEST_F(CommandPatternTest, IllegalRequestFailsWithoutThrowingAndChangesNothing)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		std::unique_ptr<Command> command = complete(*instance, "a");   // a is Assigned, not started

		CommandResult result = CommandResult::ok("not run");
		EXPECT_NO_THROW(result = command->execute());

		EXPECT_FALSE(result.success);
		EXPECT_FALSE(result.message.empty());
		EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Assigned);
		EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Created);
	}

	TEST_F(CommandPatternTest, UnknownItemFails)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		CommandResult result = start(*instance, "ghost")->execute();

		EXPECT_FALSE(result.success);
		EXPECT_EQ(result.message, "no such item: ghost");
	}

	TEST_F(CommandPatternTest, UndoRestoresTheItemAndTheReactionsItCaused)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		start(*instance, "a")->execute();
		std::unique_ptr<Command> command = complete(*instance, "a");
		command->execute();
		ASSERT_EQ(item(*instance, "b").status(), ItemStatus::Assigned);

		CommandResult result = command->undo();

		EXPECT_TRUE(result.success);
		EXPECT_EQ(item(*instance, "a").status(), ItemStatus::InProgress);
		EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Created);
		EXPECT_TRUE(hasEntry(instance->audit(), EventType::ItemReverted));
	}

	TEST_F(CommandPatternTest, IrreversibleCommandCannotBeUndone)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(irreversibleFlow());
		start(*instance, "launch")->execute();
		std::unique_ptr<Command> command = complete(*instance, "launch");

		EXPECT_TRUE(command->isReversible());
		ASSERT_TRUE(command->execute().success);

		EXPECT_FALSE(command->isReversible());
		EXPECT_FALSE(command->undo().success);
		EXPECT_EQ(item(*instance, "launch").status(), ItemStatus::Completed);
	}

	// --- ActionHistory (Invoker) ---

	TEST_F(CommandPatternTest, InvokerStacksSuccessfulCommandsInOrder)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		ActionHistory& history = instance->actions();

		EXPECT_TRUE(history.execute(start(*instance, "a")).success);
		EXPECT_TRUE(history.execute(complete(*instance, "a")).success);

		ASSERT_EQ(history.size(), 2u);
		const std::vector<std::string> expected = {"1. Una started 'A'", "2. Una completed 'A' [Accepted]"};
		EXPECT_EQ(history.describeAll(), expected);
	}

	TEST_F(CommandPatternTest, InvokerDiscardsFailedAndNullCommands)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		ActionHistory& history = instance->actions();

		EXPECT_FALSE(history.execute(complete(*instance, "a")).success);
		CommandResult null_result = history.execute(std::unique_ptr<Command>());

		EXPECT_FALSE(null_result.success);
		EXPECT_EQ(null_result.message, "null command");
		EXPECT_EQ(history.size(), 0u);
	}

	TEST_F(CommandPatternTest, InvokerRefusesCommandsWhenInstanceIsNotRunning)
	{
		std::unique_ptr<WorkflowInstance> instance = makeInstance(twoStepFlow());   // never started
		CommandResult result = instance->actions().execute(start(*instance, "a"));

		EXPECT_FALSE(result.success);
		EXPECT_EQ(result.message, "instance is Created, not Running");
		EXPECT_EQ(instance->actions().size(), 0u);
	}

	TEST_F(CommandPatternTest, InvokerUndoesMostRecentFirst)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		ActionHistory& history = instance->actions();
		history.execute(start(*instance, "a"));
		history.execute(complete(*instance, "a"));

		EXPECT_TRUE(history.undoLast().success);
		EXPECT_EQ(history.size(), 1u);
		EXPECT_EQ(item(*instance, "a").status(), ItemStatus::InProgress);
		EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Created);

		EXPECT_TRUE(history.undoLast().success);
		EXPECT_EQ(history.size(), 0u);
		EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Assigned);

		CommandResult empty = history.undoLast();
		EXPECT_FALSE(empty.success);
		EXPECT_EQ(empty.message, "nothing to undo");
	}

	TEST_F(CommandPatternTest, InvokerRefusesToUndoIrreversibleCommand)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(irreversibleFlow());
		ActionHistory& history = instance->actions();
		history.execute(start(*instance, "launch"));
		history.execute(complete(*instance, "launch"));

		CommandResult result = history.undoLast();

		EXPECT_FALSE(result.success);
		EXPECT_EQ(result.message, "cannot undo: Una completed 'Launch' [Accepted] is irreversible");
		EXPECT_EQ(history.size(), 2u);
		EXPECT_EQ(item(*instance, "launch").status(), ItemStatus::Completed);
	}

	// --- CommandFactory (Client) ---

	TEST_F(CommandPatternTest, FactoryRegistersTheSevenBuiltInVerbsSorted)
	{
		CommandFactory factory;
		const std::vector<std::string> expected = {"approve", "cancel", "complete", "escalate", "reject", "start", "submit"};
		EXPECT_EQ(factory.verbs(), expected);
	}

	TEST_F(CommandPatternTest, FactoryCreatesCommandsFromVerbs)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		CommandFactory factory;

		ASSERT_TRUE(factory.create("start", request(*instance, "a"))->execute().success);
		ASSERT_TRUE(factory.create("complete", request(*instance, "a", Outcome::rejected("DOCS_INVALID")))->execute().success);

		EXPECT_EQ(item(*instance, "a").status(), ItemStatus::Completed);
		EXPECT_EQ(item(*instance, "b").status(), ItemStatus::Assigned);
	}

	TEST_F(CommandPatternTest, FactoryUnknownVerbThrowsNotFound)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		CommandFactory factory;

		EXPECT_THROW(factory.create("teleport", request(*instance, "a")), NotFoundError);
	}

	TEST_F(CommandPatternTest, FactoryAcceptsNewVerbsWithoutChangingExistingCode)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		CommandFactory factory;
		factory.registerVerb("begin", [](const CommandRequest& r) -> std::unique_ptr<Command>
		{
			return std::unique_ptr<Command>(new StartCommand(r.instance, r.item_id, r.actor));
		});

		const std::vector<std::string> verbs = factory.verbs();
		EXPECT_EQ(verbs.size(), 8u);
		EXPECT_EQ(verbs[0], "approve");
		EXPECT_EQ(verbs[1], "begin");

		EXPECT_TRUE(factory.create("begin", request(*instance, "a"))->execute().success);
		EXPECT_EQ(item(*instance, "a").status(), ItemStatus::InProgress);
	}

	TEST_F(CommandPatternTest, FactoryCommandsRunThroughTheInvoker)
	{
		std::unique_ptr<WorkflowInstance> instance = startedInstance(twoStepFlow());
		CommandFactory factory;
		ActionHistory& history = instance->actions();

		EXPECT_TRUE(history.execute(factory.create("start", request(*instance, "a"))).success);
		EXPECT_TRUE(history.execute(factory.create("cancel", request(*instance, "b", Outcome::accepted(), "not needed"))).success);

		const std::vector<std::string> expected = {"1. Una started 'A'", "2. Una cancelled 'B': not needed"};
		EXPECT_EQ(history.describeAll(), expected);
	}
} // namespace tividy