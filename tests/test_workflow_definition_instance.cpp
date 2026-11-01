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

/**
 * @file test_workflow_definition_instance.cpp
 * @brief Regression tests for WorkflowDefinition::createInstance(): independent instances, copied data, rejected definitions.
 */

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Clock.h"
#include "engine/core/Errors.h"
#include "engine/core/Participant.h"
#include "engine/core/WorkflowData.h"
#include "engine/definition/WorkflowDefinition.h"
#include "engine/items/StageItem.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/policies/AssignmentStrategy.h"
#include "engine/runtime/WorkflowInstance.h"

namespace tividy
{
	namespace
	{
		class WorkflowDefinitionInstanceTest : public ::testing::Test
		{
			protected:
				WorkflowDefinitionInstanceTest() : clock(TimePoint()), services{clock, directory, connectors}
				{
					policies.assignment = std::make_shared<RoleBasedAssignment>();

					definition.reset(new WorkflowDefinition("TestWorkflow", 1, "Desc",
						std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("task1", "Task 1"))), policies));
				}

				ManualClock clock;
				ParticipantDirectory directory;
				ConnectorRegistry connectors;
				Services services;
				DefinitionPolicies policies;
				std::unique_ptr<WorkflowDefinition> definition;
		};
	}

	TEST_F(WorkflowDefinitionInstanceTest, EachInstanceGetsItsOwnCopyOfTheItemTree)
	{
		std::unique_ptr<WorkflowInstance> first = definition->createInstance("inst-1", WorkflowData(), services);
		std::unique_ptr<WorkflowInstance> second = definition->createInstance("inst-2", WorkflowData(), services);

		ASSERT_NE(first->findItem("task1"), nullptr);
		ASSERT_NE(second->findItem("task1"), nullptr);
		EXPECT_NE(first->findItem("task1"), second->findItem("task1"));
		EXPECT_EQ(first->findItem("task1")->spec().id(), second->findItem("task1")->spec().id());
	}

	TEST_F(WorkflowDefinitionInstanceTest, ChangingOneInstancesItemLeavesTheOtherAndTheTemplateAlone)
	{
		std::unique_ptr<WorkflowInstance> first = definition->createInstance("inst-1", WorkflowData(), services);
		std::unique_ptr<WorkflowInstance> second = definition->createInstance("inst-2", WorkflowData(), services);

		first->findItem("task1")->publish();

		EXPECT_EQ(first->findItem("task1")->status(), ItemStatus::Available);
		EXPECT_EQ(second->findItem("task1")->status(), ItemStatus::Created);

		// an instance made afterwards starts from the untouched template, not from the changed copy
		std::unique_ptr<WorkflowInstance> third = definition->createInstance("inst-3", WorkflowData(), services);
		EXPECT_EQ(third->findItem("task1")->status(), ItemStatus::Created);
	}

	TEST_F(WorkflowDefinitionInstanceTest, ChangingOneInstancesDataLeavesTheOtherAlone)
	{
		WorkflowData data;
		data.set("customer", "Alice");

		std::unique_ptr<WorkflowInstance> first = definition->createInstance("inst-1", data, services);
		std::unique_ptr<WorkflowInstance> second = definition->createInstance("inst-2", data, services);

		first->data().set("customer", "Bob");

		EXPECT_EQ(first->data().get("customer"), "Bob");
		EXPECT_EQ(second->data().get("customer"), "Alice");
	}

	TEST_F(WorkflowDefinitionInstanceTest, TheInitialDataIsCopiedNotShared)
	{
		WorkflowData data;
		data.set("customer", "Alice");

		std::unique_ptr<WorkflowInstance> instance = definition->createInstance("inst-1", data, services);
		EXPECT_EQ(instance->data().get("customer"), "Alice");

		data.set("customer", "Bob");
		EXPECT_EQ(instance->data().get("customer"), "Alice");

		instance->data().set("extra", "1");
		EXPECT_FALSE(data.has("extra"));
	}

	TEST_F(WorkflowDefinitionInstanceTest, TheInstanceRecordsWhichDefinitionItCameFrom)
	{
		std::unique_ptr<WorkflowInstance> instance = definition->createInstance("inst-7", WorkflowData(), services);

		EXPECT_EQ(instance->id(), "inst-7");
		EXPECT_EQ(instance->definitionName(), "TestWorkflow");
		EXPECT_EQ(instance->definitionVersion(), 1u);
		EXPECT_EQ(instance->status(), InstanceStatus::Created);
	}

	TEST_F(WorkflowDefinitionInstanceTest, AnInstanceStillWorksAfterItsDefinitionIsGone)
	{
		std::unique_ptr<WorkflowInstance> instance = definition->createInstance("inst-1", WorkflowData(), services);

		definition.reset();

		ASSERT_NE(instance->findItem("task1"), nullptr);
		EXPECT_EQ(instance->definitionName(), "TestWorkflow");
		EXPECT_NO_THROW(instance->start());
		EXPECT_EQ(instance->status(), InstanceStatus::Running);
	}

	TEST_F(WorkflowDefinitionInstanceTest, ADefinitionWithoutAnAssignmentPolicyIsRejected)
	{
		DefinitionPolicies no_policy;
		no_policy.assignment = nullptr;
		WorkflowDefinition bad("Bad", 1, "Desc",
			std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("task1", "Task 1"))), no_policy);

		EXPECT_THROW(bad.createInstance("inst-1", WorkflowData(), services), InvalidDefinitionError);
	}

	TEST_F(WorkflowDefinitionInstanceTest, ADefinitionWithAnUnknownDependencyIsRejectedWithAReason)
	{
		std::unique_ptr<StageItem> root(new StageItem(WorkItemSpec("root", "Root")));
		root->add(std::unique_ptr<WorkItem>(new ManualTask(WorkItemSpec("task1", "Task 1").dependsOn("ghost"))));
		WorkflowDefinition bad("Bad", 1, "Desc", std::unique_ptr<WorkItem>(root.release()), policies);

		try
		{
			bad.createInstance("inst-1", WorkflowData(), services);
			FAIL() << "createInstance() accepted a definition that depends on an unknown item";
		}
		catch (const InvalidDefinitionError& error)
		{
			EXPECT_NE(std::string(error.what()).find("ghost"), std::string::npos);
		}
	}
} // namespace tividy