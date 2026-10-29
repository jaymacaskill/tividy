/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 29 October 2026
*/

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "engine/definition/WorkflowBuilder.h"
#include "engine/items/WorkItemMemento.h"
#include "support/WorkflowBuilderFixtures.h"

static_assert(std::has_virtual_destructor<tividy::WorkflowBuilder>::value,
	"Builders must support destruction through their interface");

struct BuilderLeafRecord
{
	unsigned destroyed = 0;
	unsigned cloned = 0;
	unsigned bound = 0;
	unsigned lifecycle = 0;
};

class BuilderLeaf : public tividy::WorkItem
{
	public:
		explicit BuilderLeaf(const tividy::WorkItemSpec& spec,
			std::shared_ptr<BuilderLeafRecord> record = std::make_shared<BuilderLeafRecord>())
			: spec_ref(spec), record(record) {}
		~BuilderLeaf() override { ++record->destroyed; }
		const tividy::WorkItemSpec& spec() const override { return spec_ref; }
		tividy::ItemStatus status() const override { return tividy::ItemStatus::Created; }
		const tividy::Id& assignee() const override { return empty; }
		WorkItem* find(const tividy::Id& id) override { return id == spec().id() ? this : nullptr; }
		void accept(tividy::WorkItemVisitor&) const override {}
		std::unique_ptr<tividy::WorkItemIterator> createIterator() override
		{
			return std::unique_ptr<tividy::WorkItemIterator>(new tividy::DepthFirstIterator(*this));
		}
		std::unique_ptr<WorkItem> clone() const override
		{
			++record->cloned;
			throw tividy::EngineError("builder must not clone its input");
		}
		void bind(tividy::WorkContext&) override
		{
			++record->bound;
			throw tividy::EngineError("builder must not bind its input");
		}
		void publish() override { unexpectedLifecycle(); }
		void assign(const tividy::Id&) override { unexpectedLifecycle(); }
		void start(const tividy::Participant&) override { unexpectedLifecycle(); }
		void submit(const tividy::Participant&, const tividy::Outcome&) override { unexpectedLifecycle(); }
		void complete(const tividy::Participant&, const tividy::Outcome&) override { unexpectedLifecycle(); }
		void approve(const tividy::Participant&, const tividy::Outcome&) override { unexpectedLifecycle(); }
		void reject(const tividy::Participant&, const std::string&) override { unexpectedLifecycle(); }
		void escalate(const std::string&) override { unexpectedLifecycle(); }
		void cancel(const std::string&) override { unexpectedLifecycle(); }
		void skip(const std::string&) override { unexpectedLifecycle(); }
		void expire(const std::string&, const std::string&) override { unexpectedLifecycle(); }
	private:
		void unexpectedLifecycle()
		{
			++record->lifecycle;
			throw tividy::EngineError("builder must not execute its input");
		}
		tividy::WorkItemSpec spec_ref;
		std::shared_ptr<BuilderLeafRecord> record;
		tividy::Id empty;
};

class BuilderWrapper : public BuilderLeaf
{
	public:
		explicit BuilderWrapper(std::unique_ptr<tividy::WorkItem> item)
			: BuilderLeaf(item->spec()), inner(std::move(item)) {}
		const tividy::WorkItemSpec& spec() const override { return inner->spec(); }
		void accept(tividy::WorkItemVisitor& visitor) const override { inner->accept(visitor); }
	private:
		std::unique_ptr<tividy::WorkItem> inner;
};

class BuilderRouting : public tividy::RoutingStrategy
{
	public:
		tividy::RoutingDecision decide(const tividy::WorkItem&, const tividy::Outcome&,
			const tividy::WorkflowData&) const override
		{
			tividy::RoutingDecision result;
			result.activate.push_back("unknown-runtime-target");
			return result;
		}
};

static std::unique_ptr<tividy::WorkItem> builderLeaf(const std::string& id)
{
	return std::unique_ptr<tividy::WorkItem>(new BuilderLeaf(tividy::WorkItemSpec(id, id)));
}

static void expectInactive(tividy::StandardWorkflowBuilder& builder)
{
	EXPECT_THROW(builder.beginStage(tividy::WorkItemSpec("stage", "Stage")), tividy::InvalidDefinitionError);
	EXPECT_THROW(builder.endStage(), tividy::InvalidDefinitionError);
	EXPECT_THROW(builder.addTask(builderLeaf("inactive")), tividy::InvalidDefinitionError);
	EXPECT_THROW(builder.setAssignment({}), tividy::InvalidDefinitionError);
	EXPECT_THROW(builder.setRouting("inactive", {}), tividy::InvalidDefinitionError);
	EXPECT_THROW(builder.setEscalationChain({}), tividy::InvalidDefinitionError);
	EXPECT_THROW(builder.addCancelSignal("cancel"), tividy::InvalidDefinitionError);
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
}

class WorkflowBuilderTest : public ::testing::Test
{
	protected:
		void SetUp() override { ASSERT_TRUE(BuilderStageProbe::live.empty()); }
		void TearDown() override
		{
			EXPECT_TRUE(BuilderStageProbe::live.empty());
			BuilderStageProbe::failNextAdd = false;
		}
};

TEST_F(WorkflowBuilderTest, FreshBuilderRequiresResetBeforeEveryBuilderOperation)
{
	tividy::StandardWorkflowBuilder builder;
	expectInactive(builder);
}

TEST_F(WorkflowBuilderTest, ResetCreatesTheImplicitRootAndCopiesProductMetadata)
{
	tividy::StandardWorkflowBuilder builder;
	std::string name = "Document approval", description = "Review evidence";
	builder.reset(name, 7, description);
	ASSERT_EQ(1u, BuilderStageProbe::live.size());
	auto* root = BuilderStageProbe::live.front();
	EXPECT_EQ("root", root->spec().id());
	EXPECT_EQ(name, root->spec().name());
	builder.addTask(builderLeaf("first"));
	name = "Changed";
	description = "Changed";
	const auto product = builder.getResult();
	EXPECT_EQ("Document approval", product->name());
	EXPECT_EQ(7u, product->version());
	EXPECT_EQ("Review evidence", product->description());
	EXPECT_TRUE(product->validationProblems().empty());
}

TEST_F(WorkflowBuilderTest, NestedStagesAndTasksKeepTheirInsertionOrder)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Nested", 1, "");
	auto* root = BuilderStageProbe::live.front();
	builder.addTask(builderLeaf("first"));
	builder.beginStage(tividy::WorkItemSpec("outer", "Outer"));
	builder.addTask(builderLeaf("second"));
	builder.beginStage(tividy::WorkItemSpec("inner", "Inner"));
	builder.addTask(builderLeaf("third"));
	builder.endStage();
	builder.addTask(builderLeaf("fourth"));
	builder.endStage();
	builder.addTask(builderLeaf("fifth"));
	const auto product = builder.getResult();
	const auto children = root->children();
	ASSERT_EQ(3u, children.size());
	EXPECT_EQ("first", children[0]->spec().id());
	EXPECT_EQ("outer", children[1]->spec().id());
	EXPECT_EQ("fifth", children[2]->spec().id());
	const auto outer = children[1]->children();
	ASSERT_EQ(3u, outer.size());
	EXPECT_EQ("second", outer[0]->spec().id());
	EXPECT_EQ("inner", outer[1]->spec().id());
	EXPECT_EQ("fourth", outer[2]->spec().id());
	ASSERT_EQ(1u, outer[1]->children().size());
	EXPECT_EQ("third", outer[1]->children()[0]->spec().id());
	auto iterator = root->createIterator();
	std::vector<std::string> ids;
	for (iterator->first(); !iterator->isDone(); iterator->next())
		ids.push_back(iterator->currentItem().spec().id());
	EXPECT_EQ((std::vector<std::string>{"root", "first", "outer", "second", "inner", "third", "fourth", "fifth"}), ids);
}

TEST_F(WorkflowBuilderTest, CannotCloseTheImplicitRootButCanContinueBuilding)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Test", 1, "");
	EXPECT_THROW(builder.endStage(), tividy::InvalidDefinitionError);
	builder.addTask(builderLeaf("first"));
	EXPECT_NO_THROW(builder.getResult());
}

TEST_F(WorkflowBuilderTest, UnclosedStagesCanBeClosedAfterGetResultRejectsThem)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Test", 1, "");
	builder.beginStage(tividy::WorkItemSpec("nested", "Nested"));
	builder.addTask(builderLeaf("first"));
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
	EXPECT_EQ(2u, BuilderStageProbe::live.size());
	builder.endStage();
	EXPECT_NO_THROW(builder.getResult());
}

TEST_F(WorkflowBuilderTest, ProductOwnsTheExactWrappedInputWithoutCloningBindingOrExecutingIt)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Test", 1, "");
	auto* root = BuilderStageProbe::live.front();
	auto record = std::make_shared<BuilderLeafRecord>();
	std::unique_ptr<tividy::WorkItem> input(new BuilderWrapper(std::unique_ptr<tividy::WorkItem>(
		new BuilderLeaf(tividy::WorkItemSpec("wrapped", "Wrapped"), record))));
	auto* supplied = input.get();
	builder.addTask(std::move(input));
	EXPECT_FALSE(input);
	auto product = builder.getResult();
	ASSERT_EQ(1u, root->children().size());
	EXPECT_EQ(supplied, root->children().front());
	EXPECT_EQ(0u, record->destroyed);
	EXPECT_EQ(0u, record->cloned);
	EXPECT_EQ(0u, record->bound);
	EXPECT_EQ(0u, record->lifecycle);
	product.reset();
	EXPECT_EQ(1u, record->destroyed);
	EXPECT_TRUE(BuilderStageProbe::live.empty());
}

TEST_F(WorkflowBuilderTest, NullTasksAreRejectedWithoutDamagingTheCurrentStage)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Test", 1, "");
	auto* root = BuilderStageProbe::live.front();
	EXPECT_THROW(builder.addTask({}), tividy::InvalidDefinitionError);
	EXPECT_TRUE(root->children().empty());
	builder.addTask(builderLeaf("valid"));
	EXPECT_NO_THROW(builder.getResult());
}

TEST_F(WorkflowBuilderTest, UnsetAssignmentUsesTheRealRoleBasedStrategyAndOtherPoliciesStayEmpty)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Test", 1, "");
	auto* root = BuilderStageProbe::live.front();
	builder.setAssignment(std::make_shared<tividy::RoleBasedAssignment>());
	builder.setAssignment({});
	builder.addTask(builderLeaf("first"));
	const auto product = builder.getResult();
	const auto& policy = product->policies();
	ASSERT_TRUE(policy.assignment);
	EXPECT_FALSE(policy.escalation);
	EXPECT_TRUE(policy.routing.empty());
	EXPECT_TRUE(policy.cancel_signals.empty());
	tividy::ParticipantDirectory directory;
	directory.add(tividy::Participant("z-worker", "Z", {"worker"}));
	directory.add(tividy::Participant("a-worker", "A", {"worker"}));
	tividy::WorkflowData data;
	EXPECT_EQ("a-worker", policy.assignment->chooseParticipant(*root->children()[0], "worker", directory, data));
	EXPECT_EQ("", policy.assignment->chooseParticipant(*root->children()[0], "missing", directory, data));
}

TEST_F(WorkflowBuilderTest, ExplicitAssignmentIsSharedByTheProductAndReleasedByTheEmptyBuilder)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Test", 1, "");
	auto policy = std::make_shared<tividy::RoleBasedAssignment>();
	std::weak_ptr<const tividy::AssignmentStrategy> lifetime = policy;
	builder.setAssignment(policy);
	builder.addTask(builderLeaf("first"));
	auto product = builder.getResult();
	EXPECT_EQ(policy, product->policies().assignment);
	policy.reset();
	EXPECT_FALSE(lifetime.expired());
	product.reset();
	EXPECT_TRUE(lifetime.expired());
}

TEST_F(WorkflowBuilderTest, RoutingRegistrationReplacesTheSameSourceAndAllowsForwardReferences)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Test", 1, "");
	auto replaced = std::make_shared<BuilderRouting>();
	std::weak_ptr<const tividy::RoutingStrategy> old = replaced;
	builder.setRouting("first", replaced);
	replaced.reset();
	auto selected = std::make_shared<BuilderRouting>();
	builder.setRouting("first", selected);
	EXPECT_TRUE(old.expired());
	builder.addTask(builderLeaf("first"));
	const auto product = builder.getResult();
	ASSERT_EQ(1u, product->policies().routing.size());
	EXPECT_EQ(selected, product->policies().routing.at("first"));
	// The strategy's unknown target is a runtime concern, not a builder check.
	EXPECT_TRUE(product->validationProblems().empty());
}

TEST_F(WorkflowBuilderTest, EscalationAndCancellationConfigurationIsPreservedAndDeduplicated)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Test", 1, "");
	auto chain = std::make_shared<tividy::RoleEscalationHandler>("manager", 1);
	builder.setEscalationChain(chain);
	builder.addCancelSignal("withdrawn");
	builder.addCancelSignal("withdrawn");
	builder.addCancelSignal("closed");
	builder.addTask(builderLeaf("first"));
	const auto product = builder.getResult();
	EXPECT_EQ(chain, product->policies().escalation);
	EXPECT_EQ((std::set<std::string>{"closed", "withdrawn"}), product->policies().cancel_signals);
}

TEST_F(WorkflowBuilderTest, EscalationChainCanBeExplicitlyCleared)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Test", 1, "");
	builder.setEscalationChain(std::make_shared<tividy::RoleEscalationHandler>("manager", 1));
	builder.setEscalationChain({});
	builder.addTask(builderLeaf("first"));
	EXPECT_FALSE(builder.getResult()->policies().escalation);
}

TEST_F(WorkflowBuilderTest, ResetDiscardsPartialNestedWorkAndAllOldPolicyConfiguration)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Old", 1, "Old description");
	builder.beginStage(tividy::WorkItemSpec("old-stage", "Old"));
	auto record = std::make_shared<BuilderLeafRecord>();
	builder.addTask(std::unique_ptr<tividy::WorkItem>(new BuilderLeaf(tividy::WorkItemSpec("old", "Old"), record)));
	builder.setRouting("old", std::make_shared<BuilderRouting>());
	builder.setEscalationChain(std::make_shared<tividy::RoleEscalationHandler>("manager", 1));
	builder.addCancelSignal("old");
	builder.reset("New", 2, "New description");
	EXPECT_EQ(1u, record->destroyed);
	ASSERT_EQ(1u, BuilderStageProbe::live.size());
	builder.addTask(builderLeaf("new"));
	const auto product = builder.getResult();
	EXPECT_EQ("New", product->name());
	EXPECT_EQ(2u, product->version());
	EXPECT_EQ("New description", product->description());
	EXPECT_TRUE(product->policies().routing.empty());
	EXPECT_FALSE(product->policies().escalation);
	EXPECT_TRUE(product->policies().cancel_signals.empty());
}

TEST_F(WorkflowBuilderTest, CompletedBuilderRequiresResetBeforeReuse)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("First", 1, "");
	builder.addTask(builderLeaf("first"));
	const auto product = builder.getResult();
	expectInactive(builder);
}

TEST_F(WorkflowBuilderTest, ReusingTheBuilderDoesNotChangeAnAlreadyReturnedProduct)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("First", 1, "First description");
	auto* first_root = BuilderStageProbe::live.back();
	builder.addTask(builderLeaf("first"));
	builder.addCancelSignal("first-signal");
	const auto first = builder.getResult();
	builder.reset("Second", 2, "Second description");
	auto* second_root = BuilderStageProbe::live.back();
	builder.addTask(builderLeaf("second"));
	const auto second = builder.getResult();
	EXPECT_NE(first_root, second_root);
	EXPECT_EQ("first", first_root->children()[0]->spec().id());
	EXPECT_EQ("second", second_root->children()[0]->spec().id());
	EXPECT_EQ("First", first->name());
	EXPECT_EQ("Second", second->name());
	EXPECT_EQ(1u, first->policies().cancel_signals.count("first-signal"));
	EXPECT_TRUE(second->policies().cancel_signals.empty());
}

TEST_F(WorkflowBuilderTest, ReturnedProductOutlivesTheBuilder)
{
	auto record = std::make_shared<BuilderLeafRecord>();
	std::unique_ptr<tividy::WorkflowDefinition> product;
	{
		tividy::StandardWorkflowBuilder builder;
		builder.reset("Independent", 3, "");
		builder.addTask(std::unique_ptr<tividy::WorkItem>(new BuilderLeaf(tividy::WorkItemSpec("first", "First"), record)));
		product = builder.getResult();
	}
	EXPECT_EQ(0u, record->destroyed);
	EXPECT_EQ("Independent", product->name());
	EXPECT_NO_THROW(product->validate());
	product.reset();
	EXPECT_EQ(1u, record->destroyed);
}

TEST_F(WorkflowBuilderTest, DestroyingThroughTheBuilderInterfaceReleasesIncompleteWork)
{
	auto record = std::make_shared<BuilderLeafRecord>();
	{
		std::unique_ptr<tividy::WorkflowBuilder> builder(new tividy::StandardWorkflowBuilder);
		builder->reset("Unfinished", 1, "");
		builder->beginStage(tividy::WorkItemSpec("nested", "Nested"));
		builder->addTask(std::unique_ptr<tividy::WorkItem>(new BuilderLeaf(tividy::WorkItemSpec("first", "First"), record)));
	}
	EXPECT_EQ(1u, record->destroyed);
}

TEST_F(WorkflowBuilderTest, EmptyRootFailsValidationAndLeavesTheBuilderEmpty)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Empty", 1, "");
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
	EXPECT_TRUE(BuilderStageProbe::live.empty());
	expectInactive(builder);
	builder.reset("Recovered", 1, "");
	builder.addTask(builderLeaf("valid"));
	EXPECT_NO_THROW(builder.getResult());
}

TEST_F(WorkflowBuilderTest, InvalidDefinitionsUseTheRealValidatorsJoinedDiagnosticsAndReleaseTheirTree)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Invalid", 1, "");
	auto record = std::make_shared<BuilderLeafRecord>();
	builder.addTask(std::unique_ptr<tividy::WorkItem>(new BuilderLeaf(
		tividy::WorkItemSpec("invalid", "Invalid").reviewRequired(true).onTimeout("TIMEOUT"), record)));
	try
	{
		builder.getResult();
		FAIL() << "Invalid product was accepted";
	}
	catch (const tividy::InvalidDefinitionError& error)
	{
		const std::string message = error.what();
		EXPECT_NE(std::string::npos, message.find("reviewer role"));
		EXPECT_NE(std::string::npos, message.find("positive deadline"));
		EXPECT_NE(std::string::npos, message.find("; "));
	}
	EXPECT_EQ(1u, record->destroyed);
	expectInactive(builder);
}

TEST_F(WorkflowBuilderTest, UnknownRoutingSourcesAreRejectedWhenTheProductIsValidated)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Invalid", 1, "");
	builder.setRouting("missing-source", std::make_shared<BuilderRouting>());
	builder.addTask(builderLeaf("valid"));
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
	expectInactive(builder);
}

TEST_F(WorkflowBuilderTest, DuplicateIdsInDifferentSiblingStagesAreRejectedByProductValidation)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Invalid", 1, "");
	for (const std::string id : {"left", "right"})
	{
		builder.beginStage(tividy::WorkItemSpec(id, id));
		builder.addTask(builderLeaf("duplicate"));
		builder.endStage();
	}
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
}

TEST_F(WorkflowBuilderTest, AStageDependingOnItsOwnDescendantIsRejected)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Invalid", 1, "");
	builder.beginStage(tividy::WorkItemSpec("parent", "Parent").dependsOn("child"));
	builder.addTask(builderLeaf("child"));
	builder.endStage();
	EXPECT_THROW(builder.getResult(), tividy::InvalidDefinitionError);
}

TEST_F(WorkflowBuilderTest, MetadataHasNoAdditionalUndocumentedNameOrVersionRestrictions)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("", 0, "");
	builder.addTask(builderLeaf("valid"));
	const auto product = builder.getResult();
	EXPECT_TRUE(product->name().empty());
	EXPECT_EQ(0u, product->version());
}

TEST_F(WorkflowBuilderTest, FailedStageInsertionDoesNotLeaveAnUnownedStageOnTheStack)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Recoverable", 1, "");
	BuilderStageProbe::failNextAdd = true;
	EXPECT_THROW(builder.beginStage(tividy::WorkItemSpec("failed", "Failed")), tividy::EngineError);
	ASSERT_EQ(1u, BuilderStageProbe::live.size());
	builder.addTask(builderLeaf("valid"));
	EXPECT_NO_THROW(builder.getResult());
}

TEST_F(WorkflowBuilderTest, FailedTaskInsertionReleasesTheInputAndPreservesTheOpenStage)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Recoverable", 1, "");
	auto record = std::make_shared<BuilderLeafRecord>();
	BuilderStageProbe::failNextAdd = true;
	EXPECT_THROW(builder.addTask(std::unique_ptr<tividy::WorkItem>(
		new BuilderLeaf(tividy::WorkItemSpec("failed", "Failed"), record))), tividy::EngineError);
	EXPECT_EQ(1u, record->destroyed);
	builder.addTask(builderLeaf("valid"));
	EXPECT_NO_THROW(builder.getResult());
}

TEST_F(WorkflowBuilderTest, DeepNestingSurvivesStackGrowth)
{
	tividy::StandardWorkflowBuilder builder;
	builder.reset("Deep", 1, "");
	for (unsigned i = 0; i < 64; ++i)
		builder.beginStage(tividy::WorkItemSpec("stage-" + std::to_string(i), "Stage"));
	builder.addTask(builderLeaf("leaf"));
	for (unsigned i = 0; i < 64; ++i) builder.endStage();
	EXPECT_NO_THROW(builder.getResult());
}

TEST_F(WorkflowBuilderTest, RealDirectorBuildsAndReusesTheConcreteBuilder)
{
	class Recipe : public tividy::WorkflowDirector
	{
		public:
			std::string workflowName() const override { return "Directed"; }
			unsigned version() const override { return 4; }
			std::string description() const override { return "Director recipe"; }
			void construct(tividy::WorkflowBuilder& builder) const override
			{
				builder.beginStage(tividy::WorkItemSpec("nested", "Nested"));
				builder.addTask(builderLeaf("first"));
				builder.endStage();
			}
	};
	const Recipe director;
	tividy::StandardWorkflowBuilder builder;
	const auto first = director.build(builder);
	const auto second = director.build(builder);
	EXPECT_NE(first.get(), second.get());
	EXPECT_EQ("Directed", first->name());
	EXPECT_EQ(4u, first->version());
	EXPECT_EQ("Director recipe", first->description());
	EXPECT_TRUE(first->validationProblems().empty());
	EXPECT_TRUE(second->validationProblems().empty());
}
