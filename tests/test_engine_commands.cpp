/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 30 October 2026
*/

#include <gtest/gtest.h>
#include "engine/commands/WorkItemCommands.h"
#include "engine/runtime/WorkflowInstance.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkContext.h"
#include "engine/core/Errors.h"

namespace tividy
{

	// --- Mock Command for testing InstanceCommand ---
	class MockCommand : public InstanceCommand
	{
		public:
			MockCommand(WorkflowInstance& instance, const Id& item_id, const Participant& actor, bool should_fail = false)
				: InstanceCommand(instance, item_id, actor), fail(should_fail) {}

		protected:
			void doExecute(WorkItem& item, const Participant& actor) override 
			{
				(void)actor; // Prevents unused parameter warning
				if (fail)
				{
					throw InvalidStateError("Mock failure");
				}
				
				// Simulate a successful change of state
				item.publish(); 
			}

			std::string verb() const override { return "mocked"; }
			
		private:
			bool fail;
	};

	class InstanceCommandTest : public ::testing::Test
	{
		protected:
			void SetUp() override
			{
				// 1. Setup a mock participant (requires id, name, and a set of roles)
				std::set<std::string> roles = {"tester"};
				actor = std::unique_ptr<Participant>(new Participant("u1", "User One", roles));

				// 2. Setup a mock instance environment
				WorkflowData data;
				WorkItemSpec spec("task-1", "Test Task");
				
				// Use ManualTask instead of TaskItem (TaskItem is abstract)
				std::unique_ptr<WorkItem> root(new ManualTask(spec));
				
				DefinitionPolicies policies;
				policies.assignment = std::make_shared<RoleBasedAssignment>(); 

				// 3. Create the instance
				instance = std::unique_ptr<WorkflowInstance>(new WorkflowInstance(
					"inst-1", "test_def", 1, data, std::move(root), policies, services
				));
			}

			// ManualClock requires a starting TimePoint
			ManualClock clock{TimePoint()};
			ParticipantDirectory directory;
			ConnectorRegistry connectors;
			Services services{clock, directory, connectors};
			
			std::unique_ptr<Participant> actor;
			std::unique_ptr<WorkflowInstance> instance;
	};

	TEST_F(InstanceCommandTest, ExecuteSuccess)
	{
		// Verify initial state
		EXPECT_EQ(instance->findItem("task-1")->status(), ItemStatus::Created);

		// Execute command
		MockCommand cmd(*instance, "task-1", *actor, false);
		CommandResult result = cmd.execute();

		// Verify success and state change
		EXPECT_TRUE(result.success);
		EXPECT_EQ(instance->findItem("task-1")->status(), ItemStatus::Available);
		
		// Verify command is reversible and can be undone
		EXPECT_TRUE(cmd.isReversible());
		CommandResult undoResult = cmd.undo();
		EXPECT_TRUE(undoResult.success);
		
		// Verify state restored
		EXPECT_EQ(instance->findItem("task-1")->status(), ItemStatus::Created);
	}

	TEST_F(InstanceCommandTest, ExecuteFailure_RestoresSnapshot)
	{
		// Verify initial state
		EXPECT_EQ(instance->findItem("task-1")->status(), ItemStatus::Created);

		// Execute command that throws an error
		MockCommand cmd(*instance, "task-1", *actor, true);
		CommandResult result = cmd.execute();

		// Verify failure result
		EXPECT_FALSE(result.success);
		EXPECT_EQ(result.message, "Mock failure");

		// Verify state was NOT changed (Snapshot restored)
		EXPECT_EQ(instance->findItem("task-1")->status(), ItemStatus::Created);
	}

	TEST_F(InstanceCommandTest, ExecuteMissingItem)
	{
		// Execute command on non-existent item
		MockCommand cmd(*instance, "missing-id", *actor, false);
		CommandResult result = cmd.execute();

		// Verify failure result and no crash
		EXPECT_FALSE(result.success);
		EXPECT_EQ(result.message, "no such item: missing-id");
	}

} // namespace tividy