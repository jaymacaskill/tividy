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
*/

/** @file test_pattern_builder.cpp
 * @brief Builder/Director/product integration with the real Composite and validation.
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/core/Errors.h"
#include "engine/definition/WorkflowBuilder.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemMemento.h"
#include "engine/items/WorkItemVisitor.h"

struct BuilderInputRecord
{
	unsigned destroyed = 0;
	unsigned cloned = 0;
	unsigned bound = 0;
	unsigned executed = 0;
	std::vector<std::string> visited;
};

// Ready-made input leaves observe ownership and ordering; no pattern participant
// implementation is replaced. Task lifecycle behavior is outside these tests.
class BuilderInput : public tividy::WorkItem
{
	public:
		BuilderInput(const tividy::WorkItemSpec& spec, std::shared_ptr<BuilderInputRecord> record)
			: spec_ref(spec), record(std::move(record)) {}
		~BuilderInput() override { ++record->destroyed; }
		const tividy::WorkItemSpec& spec() const override { return spec_ref; }
		tividy::ItemStatus status() const override { return tividy::ItemStatus::Created; }
		const tividy::Id& assignee() const override { return nobody; }
		WorkItem* find(const tividy::Id& id) override { return id == spec_ref.id() ? this : nullptr; }
		void accept(tividy::WorkItemVisitor&) const override { record->visited.push_back(spec_ref.id()); }
		std::unique_ptr<tividy::WorkItemIterator> createIterator() override
		{
			return std::unique_ptr<tividy::WorkItemIterator>(new tividy::DepthFirstIterator(*this));
		}
		std::unique_ptr<WorkItem> clone() const override
		{
			++record->cloned;
			throw tividy::EngineError("a builder must retain its ready-made inputs");
		}
		void bind(tividy::WorkContext&) override
		{
			++record->bound;
			throw tividy::EngineError("a definition template must remain unbound");
		}
		void publish() override { unexpectedExecution(); }
		void assign(const tividy::Id&) override { unexpectedExecution(); }
		void start(const tividy::Participant&) override { unexpectedExecution(); }
		void submit(const tividy::Participant&, const tividy::Outcome&) override { unexpectedExecution(); }
		void complete(const tividy::Participant&, const tividy::Outcome&) override { unexpectedExecution(); }
		void approve(const tividy::Participant&, const tividy::Outcome&) override { unexpectedExecution(); }
		void reject(const tividy::Participant&, const std::string&) override { unexpectedExecution(); }
		void escalate(const std::string&) override { unexpectedExecution(); }
		void cancel(const std::string&) override { unexpectedExecution(); }
		void skip(const std::string&) override { unexpectedExecution(); }
		void expire(const std::string&, const std::string&) override { unexpectedExecution(); }
	private:
		void unexpectedExecution()
		{
			++record->executed;
			throw tividy::EngineError("building a template must not execute it");
		}
		tividy::WorkItemSpec spec_ref;
		std::shared_ptr<BuilderInputRecord> record;
		tividy::Id nobody;
};

static std::unique_ptr<tividy::WorkItem> builderInput(const std::string& id,
	std::shared_ptr<BuilderInputRecord> record)
{
	return std::unique_ptr<tividy::WorkItem>(new BuilderInput(tividy::WorkItemSpec(id, id), std::move(record)));
}

class ReviewDirector : public tividy::WorkflowDirector
{
	public:
		explicit ReviewDirector(std::shared_ptr<BuilderInputRecord> record) : record(std::move(record)) {}
		std::string workflowName() const override { return "Application review"; }
		unsigned version() const override { return 3; }
		std::string description() const override { return "Request followed by nested checks"; }
		void construct(tividy::WorkflowBuilder& builder) const override
		{
			builder.addTask(builderInput("request", record));
			builder.beginStage(tividy::WorkItemSpec("checks", "Checks").dependsOn("request"));
			builder.addTask(builderInput("review", record));
			builder.endStage();
		}
	private:
		std::shared_ptr<BuilderInputRecord> record;
};

TEST(BuilderPattern, DirectorBuildsANestedValidatedProductWithDefaults)
{
	auto record = std::make_shared<BuilderInputRecord>();
	ReviewDirector director(record);
	tividy::StandardWorkflowBuilder builder;
	auto product = director.build(builder);
	ASSERT_TRUE(product);
	EXPECT_EQ("Application review", product->name());
	EXPECT_EQ(3u, product->version());
	EXPECT_EQ("Request followed by nested checks", product->description());
	EXPECT_EQ((std::vector<std::string>{"request", "review"}), record->visited);
	EXPECT_TRUE(product->policies().assignment);
	EXPECT_FALSE(product->policies().escalation);
	EXPECT_TRUE(product->policies().routing.empty());
	EXPECT_TRUE(product->policies().cancel_signals.empty());
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
	EXPECT_EQ(0u, record->cloned);
	EXPECT_EQ(0u, record->bound);
	EXPECT_EQ(0u, record->executed);
}

TEST(BuilderPattern, ProductsRemainIndependentAfterBuilderReuseAndDestruction)
{
	auto first_record = std::make_shared<BuilderInputRecord>();
	auto second_record = std::make_shared<BuilderInputRecord>();
	std::unique_ptr<tividy::WorkflowDefinition> first, second;
	{
		std::unique_ptr<tividy::WorkflowBuilder> builder(new tividy::StandardWorkflowBuilder());
		first = ReviewDirector(first_record).build(*builder);
		second = ReviewDirector(second_record).build(*builder);
		EXPECT_NE(first.get(), second.get());
	}
	EXPECT_EQ(0u, first_record->destroyed);
	EXPECT_EQ(0u, second_record->destroyed);
	EXPECT_NO_THROW(first->validate());
	EXPECT_NO_THROW(second->validate());
	first.reset();
	EXPECT_EQ(2u, first_record->destroyed);
	EXPECT_EQ(0u, second_record->destroyed);
	second.reset();
	EXPECT_EQ(2u, second_record->destroyed);
}

TEST(BuilderPattern, ProductRetainsConfiguredPoliciesWhileTheNextBuildUsesDefaults)
{
	tividy::StandardWorkflowBuilder builder;
	auto record = std::make_shared<BuilderInputRecord>();
	auto assignment = std::make_shared<tividy::RoleBasedAssignment>();
	auto routing = std::make_shared<tividy::DependencyRouting>();
	builder.reset("Configured", 1, "");
	builder.addTask(builderInput("source", record));
	builder.setAssignment(assignment);
	builder.setRouting("source", routing);
	builder.addCancelSignal("withdrawn");
	builder.addCancelSignal("withdrawn");
	auto configured = builder.getResult();
	auto next = ReviewDirector(record).build(builder);
	EXPECT_EQ(assignment, configured->policies().assignment);
	ASSERT_EQ(1u, configured->policies().routing.size());
	EXPECT_EQ(routing, configured->policies().routing.at("source"));
	EXPECT_EQ(1u, configured->policies().cancel_signals.count("withdrawn"));
	EXPECT_NE(assignment, next->policies().assignment);
	EXPECT_TRUE(next->policies().routing.empty());
	EXPECT_TRUE(next->policies().cancel_signals.empty());
}

TEST(BuilderPattern, DirectorResetDiscardsUnfinishedWorkAndItsPolicies)
{
	tividy::StandardWorkflowBuilder builder;
	auto abandoned = std::make_shared<BuilderInputRecord>();
	builder.reset("Abandoned", 9, "");
	builder.beginStage(tividy::WorkItemSpec("unfinished", "Unfinished"));
	builder.addTask(builderInput("discarded", abandoned));
	builder.setRouting("missing", std::make_shared<tividy::DependencyRouting>());
	builder.addCancelSignal("old signal");
	auto fresh = std::make_shared<BuilderInputRecord>();
	auto product = ReviewDirector(fresh).build(builder);
	EXPECT_EQ(1u, abandoned->destroyed);
	EXPECT_EQ("Application review", product->name());
	EXPECT_TRUE(product->policies().routing.empty());
	EXPECT_TRUE(product->policies().cancel_signals.empty());
	EXPECT_TRUE(product->validationProblems().empty());
}

TEST(BuilderPattern, AnOpenStageCanBeClosedAfterPrematureResultRejection)
{
	tividy::StandardWorkflowBuilder builder;
	auto record = std::make_shared<BuilderInputRecord>();
	builder.reset("Nested", 1, "");
	builder.beginStage(tividy::WorkItemSpec("group", "Group"));
	builder.addTask(builderInput("leaf", record));
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
	EXPECT_EQ(0u, record->destroyed);
	builder.endStage();
	auto product = builder.getResult();
	EXPECT_TRUE(product->validationProblems().empty());
	EXPECT_EQ(0u, record->destroyed);
	product.reset();
	EXPECT_EQ(1u, record->destroyed);
}

TEST(BuilderPattern, DuplicateIdsInDifferentStagesAreRejectedAndOwnedInputsAreReleased)
{
	tividy::StandardWorkflowBuilder builder;
	auto record = std::make_shared<BuilderInputRecord>();
	builder.reset("Duplicate", 1, "");
	for (const std::string id : {"left", "right"})
	{
		builder.beginStage(tividy::WorkItemSpec(id, id));
		builder.addTask(builderInput("duplicate", record));
		builder.endStage();
	}
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
	EXPECT_EQ(2u, record->destroyed);
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
	EXPECT_NO_THROW(ReviewDirector(record).build(builder));
}

TEST(BuilderPattern, RealVisitorIdentifiesAndRejectsAnEmptyNestedStage)
{
	tividy::StandardWorkflowBuilder builder;
	auto record = std::make_shared<BuilderInputRecord>();
	builder.reset("Empty stage", 1, "");
	builder.addTask(builderInput("leaf", record));
	builder.beginStage(tividy::WorkItemSpec("empty", "Empty"));
	builder.endStage();
	try
	{
		builder.getResult();
		FAIL() << "An empty real StageItem must invalidate the product";
	}
	catch (const tividy::InvalidDefinitionError& error)
	{
		EXPECT_NE(std::string::npos, std::string(error.what()).find("stage 'empty' has no children"));
	}
	EXPECT_EQ(1u, record->destroyed);
}

TEST(BuilderPattern, ActualStageContainmentParticipatesInDependencyValidation)
{
	tividy::StandardWorkflowBuilder builder;
	auto record = std::make_shared<BuilderInputRecord>();
	builder.reset("Deadlocked", 1, "");
	builder.beginStage(tividy::WorkItemSpec("parent", "Parent").dependsOn("child"));
	builder.addTask(builderInput("child", record));
	builder.endStage();
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
	EXPECT_EQ(1u, record->destroyed);
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
}
