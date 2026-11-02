/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 2 November 2026

test_demo.cpp
*/

/**
 * @file test_demo.cpp
 * @brief Smoke test of the Demo pack: install it in a real Tividy and run "document-approval" through the facade with the real engine.
 *
 * Covers the default route (rework_draft skipped), the CHANGES_REQUIRED route (rework_draft activated), role enforcement, "my work"
 * lists, undo and progress/history. Static structure of the pack is covered by test_industry_demo.cpp.
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "engine/facade/Tividy.h"
#include "industries/demo/DemoPack.h"

namespace tividy
{
	namespace
	{
		class DemoSmokeTest : public ::testing::Test
		{
			protected:
				void SetUp() override
				{
					tividy.installPack(std::unique_ptr<IndustryPack>(new demo::DemoPack()));
					WorkflowData data;
					data.set("title", "Policy");
					data.set("author", "demo-author-1");
					id = tividy.startWorkflow("document-approval", data);
				}

				ItemStatus status(const Id& item) { return tividy.instance(id).findItem(item)->status(); }

				void finish(const Id& item, const Id& actor, const Outcome& outcome = Outcome::accepted())
				{
					ASSERT_TRUE(tividy.perform("start", id, item, actor).success) << item;
					ASSERT_TRUE(tividy.perform("complete", id, item, actor, outcome).success) << item;
				}

				void reviewAndApprove()
				{
					ASSERT_TRUE(tividy.perform("start", id, "approve", "demo-manager-1").success);
					ASSERT_TRUE(tividy.perform("submit", id, "approve", "demo-manager-1").success);
					ASSERT_TRUE(tividy.perform("approve", id, "approve", "demo-manager-1").success);
				}

				void reachConsolidate()
				{
					finish("write_draft", "demo-author-1");
					finish("legal_review", "demo-legal-1");
					finish("finance_review", "demo-finance-1");
				}

				Tividy tividy;
				Id id;
		};
	}

	TEST_F(DemoSmokeTest, TheWorkflowStartsAndHandsTheDraftToTheAuthor)
	{
		EXPECT_EQ(tividy.instance(id).status(), InstanceStatus::Running);
		ASSERT_EQ(tividy.workFor("demo-author-1").size(), 1u);
		EXPECT_EQ(tividy.workFor("demo-author-1")[0].item_id, "write_draft");
		EXPECT_TRUE(tividy.workFor("demo-legal-1").empty());
	}

	TEST_F(DemoSmokeTest, TheDefaultRouteSkipsReworkAndRunsToCompletion)
	{
		finish("write_draft", "demo-author-1");
		finish("legal_review", "demo-legal-1");
		EXPECT_EQ(status("consolidate"), ItemStatus::Created);
		finish("finance_review", "demo-finance-1");
		finish("consolidate", "demo-manager-1");
		EXPECT_EQ(status("rework_draft"), ItemStatus::Skipped);
		EXPECT_EQ(status("publish"), ItemStatus::Created);
		reviewAndApprove();
		finish("publish", "demo-publisher-1");

		EXPECT_EQ(tividy.instance(id).status(), InstanceStatus::Completed);
		EXPECT_EQ(tividy.progress(id).total, 7u);
		EXPECT_DOUBLE_EQ(tividy.progress(id).percent_settled, 100.0);
		EXPECT_FALSE(tividy.history(id).empty());
	}

	TEST_F(DemoSmokeTest, ChangesRequiredSendsTheDraftBackToTheAuthor)
	{
		reachConsolidate();
		finish("consolidate", "demo-manager-1", Outcome::rejected("CHANGES_REQUIRED"));
		EXPECT_EQ(status("rework_draft"), ItemStatus::Assigned);
		EXPECT_EQ(status("approve"), ItemStatus::Created);
		ASSERT_EQ(tividy.workFor("demo-author-1").size(), 1u);
		EXPECT_EQ(tividy.workFor("demo-author-1")[0].item_id, "rework_draft");

		finish("rework_draft", "demo-author-1");
		reviewAndApprove();
		finish("publish", "demo-publisher-1");
		EXPECT_EQ(tividy.instance(id).status(), InstanceStatus::Completed);
	}

	TEST_F(DemoSmokeTest, OnlyTheRightRoleMayActAndARefusalChangesNothing)
	{
		const CommandResult wrongRole = tividy.perform("start", id, "write_draft", "demo-legal-1");
		EXPECT_FALSE(wrongRole.success);
		EXPECT_NE(wrongRole.message.find("author"), std::string::npos);
		EXPECT_FALSE(tividy.perform("start", id, "write_draft", "nobody").success);
		EXPECT_FALSE(tividy.perform("start", id, "publish", "demo-publisher-1").success);
		EXPECT_EQ(status("write_draft"), ItemStatus::Assigned);
		EXPECT_EQ(status("publish"), ItemStatus::Created);
	}

	TEST_F(DemoSmokeTest, UndoTakesBackTheLastActionBeforeAnythingDownstreamIsReleased)
	{
		ASSERT_TRUE(tividy.perform("start", id, "write_draft", "demo-author-1").success);
		ASSERT_TRUE(tividy.perform("complete", id, "write_draft", "demo-author-1").success);
		ASSERT_TRUE(tividy.undo(id).success);
		EXPECT_EQ(status("write_draft"), ItemStatus::InProgress);
		EXPECT_EQ(status("legal_review"), ItemStatus::Created);
	}
}