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
#include "engine/items/TaskItem.h"          // For ManualTask
#include "engine/items/WorkItemDecorator.h" // For SecureWorkItem, ValidatedWorkItem, RequiredDataRule
#include "engine/items/WorkContext.h"       // For WorkContext
#include "engine/core/Errors.h"

namespace tividy
{
	class DecoratorTest : public ::testing::Test
	{
		protected:
			DecoratorTest() 
				: manager("mgr-1", "Manager", {"manager"}),
				  employee("emp-1", "Employee", {"employee"}),
				  clock{TimePoint()}
			{}

			void SetUp() override
			{
				// Setup dummy context for binding
				context = std::unique_ptr<WorkContext>(new WorkContext(
					"inst-1", data, events, clock, connectors, directory
				));
			}

			Participant manager;
			Participant employee;
			ManualClock clock;
			WorkflowData data;
			EventPublisher events;
			ParticipantDirectory directory;
			ConnectorRegistry connectors;
			std::unique_ptr<WorkContext> context;
	};

	// 1. SecureWorkItem Tests
	TEST_F(DecoratorTest, SecureWorkItem_Transparency)
	{
		WorkItemSpec spec("secure_task", "Secure Task");
		spec.requireRole("manager");

		std::unique_ptr<WorkItem> task(new SecureWorkItem(std::unique_ptr<WorkItem>(new ManualTask(spec))));
		task->bind(*context);

		EXPECT_EQ(task->spec().id(), "secure_task");
		EXPECT_EQ(task->status(), ItemStatus::Created);

		EXPECT_NO_THROW(task->publish());
		EXPECT_EQ(task->status(), ItemStatus::Available);
		
		EXPECT_NO_THROW(task->assign("mgr-1"));
		EXPECT_EQ(task->status(), ItemStatus::Assigned);
	}

	TEST_F(DecoratorTest, SecureWorkItem_EnforcesRoles)
	{
		WorkItemSpec spec("secure_task", "Secure Task");
		spec.requireRole("manager");

		std::unique_ptr<WorkItem> task(new SecureWorkItem(std::unique_ptr<WorkItem>(new ManualTask(spec))));
		task->bind(*context);
		task->publish();
		task->assign("mgr-1");

		EXPECT_THROW(task->start(employee), PermissionDeniedError);

		EXPECT_NO_THROW(task->start(manager));
		EXPECT_EQ(task->status(), ItemStatus::InProgress);
	}

	TEST_F(DecoratorTest, SecureWorkItem_EscalatedApprovalRequiresExactAssignee)
	{
		WorkItemSpec spec("secure_task", "Secure Task");
		spec.requireRole("manager");
		spec.reviewedBy("director");

		std::unique_ptr<WorkItem> task(new SecureWorkItem(std::unique_ptr<WorkItem>(new ManualTask(spec))));
		task->bind(*context);
		task->publish();
		task->assign("mgr-1");
		task->start(manager);
		task->escalate("needs help"); 

		Participant randomManager("mgr-2", "Other Manager", {"manager"});
		Participant director("dir-1", "Director", {"director"});

		task->assign("dir-1");

		EXPECT_THROW(task->approve(randomManager, Outcome::accepted()), PermissionDeniedError);
		
		EXPECT_NO_THROW(task->approve(director, Outcome::accepted()));
		EXPECT_EQ(task->status(), ItemStatus::Completed);
	}

	// 2. ValidatedWorkItem Tests
	TEST_F(DecoratorTest, ValidatedWorkItem_ThrowsOnMissingData)
	{
		WorkItemSpec spec("val_task", "Validated Task");
		
		std::vector<std::shared_ptr<const ValidationRule>> rules;
		std::vector<std::string> required_keys = {"document_id", "author"};
		rules.push_back(std::make_shared<RequiredDataRule>(required_keys));

		std::unique_ptr<WorkItem> task(new ValidatedWorkItem(
			std::unique_ptr<WorkItem>(new ManualTask(spec)), 
			rules
		));
		task->bind(*context);
		task->publish();
		task->assign("emp-1");
		task->start(employee);

		EXPECT_THROW(task->complete(employee, Outcome::accepted()), ValidationError);

		context->data().set("document_id", "DOC-123");
		context->data().set("author", "Alice");

		EXPECT_NO_THROW(task->complete(employee, Outcome::accepted()));
		EXPECT_EQ(task->status(), ItemStatus::Completed);
	}

	// 3. Stacking Tests
	TEST_F(DecoratorTest, DecoratorsStackCorrectly)
	{
		WorkItemSpec spec("stacked_task", "Stacked Task");
		spec.requireRole("manager");

		std::vector<std::shared_ptr<const ValidationRule>> rules;
		rules.push_back(std::make_shared<RequiredDataRule>(std::vector<std::string>{"signature"}));

		std::unique_ptr<WorkItem> stacked_task(new SecureWorkItem(
			std::unique_ptr<WorkItem>(new ValidatedWorkItem(
				std::unique_ptr<WorkItem>(new ManualTask(spec)),
				rules
			))
		));
		
		stacked_task->bind(*context);
		stacked_task->publish();
		stacked_task->assign("mgr-1");

		EXPECT_THROW(stacked_task->start(employee), PermissionDeniedError);

		stacked_task->start(manager);
		
		EXPECT_THROW(stacked_task->complete(manager, Outcome::accepted()), ValidationError);

		context->data().set("signature", "M. User");
		EXPECT_NO_THROW(stacked_task->complete(manager, Outcome::accepted()));
		EXPECT_EQ(stacked_task->status(), ItemStatus::Completed);
	}

} // namespace tividy