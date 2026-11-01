/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 1 November 2026
*/

#include <gtest/gtest.h>
#include "engine/definition/WorkflowDefinition.h"
#include "engine/runtime/WorkflowInstance.h"
#include "engine/items/TaskItem.h" 
#include "engine/core/Errors.h"

namespace tividy
{
	class WorkflowDefinitionInstanceTest : public ::testing::Test
	{
		protected:
			void SetUp() override
			{
				// Set up a valid root item (ManualTask is a concrete TaskItem)
				WorkItemSpec spec("task1", "Task 1");
				std::unique_ptr<WorkItem> root(new ManualTask(spec));

				policies.assignment = std::make_shared<RoleBasedAssignment>();
				
				// Create a valid definition
				definition = std::unique_ptr<WorkflowDefinition>(
					new WorkflowDefinition("TestWorkflow", 1, "Desc", std::move(root), policies)
				);

				// Setup services for WorkflowInstance
				// (Assuming ManualClock defaults are fine, check Clock.h if this errors)
				clock = std::unique_ptr<ManualClock>(new ManualClock(TimePoint()));
				services = std::unique_ptr<Services>(new Services{*clock, directory, connectors});
			}

			DefinitionPolicies policies;
			std::unique_ptr<WorkflowDefinition> definition;
			
			std::unique_ptr<ManualClock> clock;
			ParticipantDirectory directory;
			ConnectorRegistry connectors;
			std::unique_ptr<Services> services;
	};

	// 1. Connor's request: Proves independent instances with separate mutable state
	TEST_F(WorkflowDefinitionInstanceTest, CreateInstance_ProducesIndependentInstances)
	{
		WorkflowData data1;
		data1.set("key", "value1");
		WorkflowData data2;
		data2.set("key", "value2");

		// Create two instances from the SAME definition
		auto instance1 = definition->createInstance("inst1", data1, *services);
		auto instance2 = definition->createInstance("inst2", data2, *services);

		// Verify they are independent objects with their own data
		EXPECT_EQ(instance1->id(), "inst1");
		EXPECT_EQ(instance2->id(), "inst2");
		EXPECT_EQ(instance1->data().get("key"), "value1");
		EXPECT_EQ(instance2->data().get("key"), "value2");

		// Verify the root items are separate memory (Prototype clone worked)
		EXPECT_NE(instance1->root().spec().id(), ""); 
		EXPECT_NE(&instance1->root(), &instance2->root());

		// Mutate instance1's tree, verify instance2 is untouched
		instance1->findItem("task1")->publish();
		EXPECT_EQ(instance1->findItem("task1")->status(), ItemStatus::Available);
		EXPECT_EQ(instance2->findItem("task1")->status(), ItemStatus::Created);
	}

	// 2. Connor's request: Proves initial data is copied correctly
	TEST_F(WorkflowDefinitionInstanceTest, CreateInstance_CopiesInitialData)
	{
		WorkflowData data;
		data.set("customer", "Alice");

		auto instance = definition->createInstance("inst1", data, *services);

		// Verify the data made it into the instance
		EXPECT_EQ(instance->data().get("customer"), "Alice");
		
		// Mutate the original data object, verify instance is unaffected (Deep copy)
		data.set("customer", "Bob");
		EXPECT_EQ(instance->data().get("customer"), "Alice");
	}

	// 3. Keval's request: Proves invalid definitions are rejected
	TEST_F(WorkflowDefinitionInstanceTest, CreateInstance_RejectsInvalidDefinition)
	{
		// Make the definition invalid by removing the assignment policy
		DefinitionPolicies bad_policies;
		bad_policies.assignment = nullptr; // Causes validation failure
		
		WorkItemSpec spec("task1", "Task 1");
		std::unique_ptr<WorkItem> root(new ManualTask(spec));
		
		WorkflowDefinition bad_def("BadWorkflow", 1, "Desc", std::move(root), bad_policies);

		WorkflowData data;
		
		// createInstance calls validate() first, which should throw
		EXPECT_THROW(
			{
				bad_def.createInstance("inst1", data, *services);
			}, 
			InvalidDefinitionError
		);
	}
} // namespace tividy