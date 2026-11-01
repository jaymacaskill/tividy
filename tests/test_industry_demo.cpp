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

#include <memory>

#include <gtest/gtest.h>

#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Clock.h"
#include "engine/core/Participant.h"
#include "engine/core/WorkflowData.h"
#include "engine/definition/WorkflowBuilder.h"
#include "engine/definition/WorkflowDefinition.h"
#include "engine/items/WorkItem.h"
#include "engine/runtime/WorkflowInstance.h"
#include "industries/demo/DemoPack.h"

namespace tividy
{
	namespace demo
	{
		class DemoPackTest : public ::testing::Test
		{
			protected:
				void SetUp() override
				{
					pack = std::unique_ptr<DemoPack>(new DemoPack());
				}

				std::unique_ptr<DemoPack> pack;
		};

		TEST_F(DemoPackTest, MetadataIsCorrect)
		{
			EXPECT_EQ(pack->key(), "demo");
			EXPECT_EQ(pack->displayName(), "Demo: Document Approval");
		}

		TEST_F(DemoPackTest, ParticipantsAreCorrect)
		{
			auto participants = pack->createParticipants();
			
			// Should have 5 participants based on the header docs
			ASSERT_EQ(participants.size(), 5);

			// Verify the manager has both manager and director roles
			bool found_manager = false;
			for (const auto& p : participants)
			{
				if (p.id() == "demo-manager-1")
				{
					found_manager = true;
					EXPECT_EQ(p.displayName(), "Manager User");
					EXPECT_TRUE(p.hasRole("manager"));
					EXPECT_TRUE(p.hasRole("director"));
				}
			}
			EXPECT_TRUE(found_manager);
		}

		TEST_F(DemoPackTest, EscalationChainIsCorrect)
		{
			auto chain = pack->createEscalationChain();
			// It should return a non-null handler (the manager handler)
			EXPECT_NE(chain, nullptr);
		}

		TEST_F(DemoPackTest, WorkflowDefinitionBuildsAndValidates)
		{
			// 1. Get the director from the pack
			auto director = pack->createDirector();
			ASSERT_NE(director, nullptr);
			
			EXPECT_EQ(director->workflowName(), "document-approval");
			EXPECT_EQ(director->version(), 1);

			// 2. Build the definition using the Standard Builder
			StandardWorkflowBuilder builder;
			auto definition = director->build(builder);
			
			ASSERT_NE(definition, nullptr);
			
			// 3. Validate the definition (this checks dependencies, routing keys, etc.)
			// If there are any structural errors in our DemoPack.cpp, this will throw!
			EXPECT_NO_THROW(definition->validate());
		}

		TEST_F(DemoPackTest, WorkflowContainsExpectedItemsAndDependencies)
		{
			auto director = pack->createDirector();
			StandardWorkflowBuilder builder;
			auto definition = director->build(builder);
		
			// the definition keeps its template private, so look at it through an instance (a deep clone of the template)
			ManualClock clock{TimePoint()};
			ParticipantDirectory directory;
			ConnectorRegistry connectors;
			Services services{clock, directory, connectors};
			std::unique_ptr<WorkflowInstance> instance = definition->createInstance("inst-1", WorkflowData(), services);
		
			// the top level holds the "drafting" and "review" stages
			ASSERT_EQ(instance->root().children().size(), 2u);
		
			ASSERT_NE(instance->findItem("review"), nullptr);
		
			WorkItem* consolidate = instance->findItem("consolidate");
			ASSERT_NE(consolidate, nullptr);
			EXPECT_EQ(consolidate->spec().dependencies().size(), 2u);
		
			WorkItem* approve = instance->findItem("approve");
			ASSERT_NE(approve, nullptr);
			EXPECT_TRUE(approve->spec().requiresReview());
			EXPECT_EQ(approve->spec().reviewerRole(), "director");
		}

	} // namespace demo
} // namespace tividy
