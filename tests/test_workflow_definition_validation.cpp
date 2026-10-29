/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 26 October 2026
*/

#include <gtest/gtest.h>
#include <algorithm>
#include <sstream>
#include "support/WorkflowDefinitionValidationFixtures.h"

static bool contains(const std::vector<std::string>& problems, const std::string& fragment)
{
    return std::any_of(problems.begin(), problems.end(), [&](const std::string& problem) {
        return problem.find(fragment) != std::string::npos;
    });
}

struct DefinitionCoreItemCalls
{
	unsigned visits = 0;
	unsigned destructions = 0;
};

class DefinitionCoreObservedItem : public ValidationTestWrapper
{
	public:
		DefinitionCoreObservedItem(std::unique_ptr<tividy::WorkItem> item, DefinitionCoreItemCalls& calls)
			: ValidationTestWrapper(std::move(item)), calls(calls) {}

		~DefinitionCoreObservedItem() override { ++calls.destructions; }

		void accept(tividy::WorkItemVisitor& visitor) const override
		{
			++calls.visits;
			ValidationTestWrapper::accept(visitor);
		}

	private:
		DefinitionCoreItemCalls& calls;
};

TEST(WorkflowDefinitionCore, CopiesMetadataIndependentlyOfTheCaller)
{
	std::string name = "Approval process";
	std::string description = "A reusable definition";
	auto root = stage();
	root->add(task(tividy::WorkItemSpec("leaf", "Leaf")));
	tividy::WorkflowDefinition value(name, 7, description, std::move(root), policies());

	name.assign("Changed name");
	description.clear();
	EXPECT_EQ("Approval process", value.name());
	EXPECT_EQ(7u, value.version());
	EXPECT_EQ("A reusable definition", value.description());
	EXPECT_NO_THROW(value.validate());
	EXPECT_EQ("Approval process", value.name());
	EXPECT_EQ(7u, value.version());
	EXPECT_EQ("A reusable definition", value.description());
}

TEST(WorkflowDefinitionCore, CopiesPolicyContainersAndSharesTheirImmutableStrategies)
{
	auto selected = policies();
	selected.routing["leaf"].reset(new ValidationTestRouting);
	selected.escalation.reset(new tividy::RoleEscalationHandler("manager", 1));
	selected.cancel_signals.insert("STOP");
	const auto assignment = selected.assignment;
	const auto routing = selected.routing.at("leaf");
	const auto escalation = selected.escalation;
	auto root = stage();
	root->add(task(tividy::WorkItemSpec("leaf", "Leaf")));
	auto value = definition(std::move(root), selected);

	selected.assignment.reset();
	selected.routing.clear();
	selected.escalation.reset();
	selected.cancel_signals.clear();
	selected.cancel_signals.insert("DIFFERENT");

	const auto& stored = value->policies();
	EXPECT_EQ(assignment, stored.assignment);
	ASSERT_EQ(1u, stored.routing.size());
	EXPECT_EQ(routing, stored.routing.at("leaf"));
	EXPECT_EQ(escalation, stored.escalation);
	EXPECT_EQ(std::set<std::string>({"STOP"}), stored.cancel_signals);
	EXPECT_NO_THROW(value->validate());
}

TEST(WorkflowDefinitionCore, RetainsPolicyObjectsUntilTheDefinitionIsDestroyed)
{
	std::weak_ptr<const tividy::AssignmentStrategy> assignment;
	std::weak_ptr<const tividy::RoutingStrategy> routing;
	std::weak_ptr<const tividy::EscalationHandler> escalation;
	std::unique_ptr<tividy::WorkflowDefinition> value;
	{
		auto selected = policies();
		selected.routing["leaf"].reset(new ValidationTestRouting);
		selected.escalation.reset(new tividy::RoleEscalationHandler("manager", 1));
		assignment = selected.assignment;
		routing = selected.routing.at("leaf");
		escalation = selected.escalation;
		auto root = stage();
		root->add(task(tividy::WorkItemSpec("leaf", "Leaf")));
		value = definition(std::move(root), selected);
	}

	EXPECT_FALSE(assignment.expired());
	EXPECT_FALSE(routing.expired());
	EXPECT_FALSE(escalation.expired());
	EXPECT_NO_THROW(value->validate());
	value.reset();
	EXPECT_TRUE(assignment.expired());
	EXPECT_TRUE(routing.expired());
	EXPECT_TRUE(escalation.expired());
}

TEST(WorkflowDefinitionCore, LeavesValidationExplicitAndPreservesSuppliedPolicies)
{
	DefinitionCoreItemCalls calls;
	std::unique_ptr<tividy::WorkItem> root(new DefinitionCoreObservedItem(stage(), calls));
	tividy::DefinitionPolicies missing_assignment;
	std::unique_ptr<tividy::WorkflowDefinition> value;
	EXPECT_NO_THROW(value.reset(new tividy::WorkflowDefinition(
		"Invalid until checked", 1, "An empty stage", std::move(root), missing_assignment)));
	ASSERT_TRUE(value);
	EXPECT_EQ(0u, calls.visits);
	EXPECT_FALSE(value->policies().assignment);
	EXPECT_THROW(value->validate(), tividy::InvalidDefinitionError);
	EXPECT_EQ(1u, calls.visits);
	EXPECT_EQ("Invalid until checked", value->name());
	EXPECT_EQ("An empty stage", value->description());
}

TEST(WorkflowDefinitionCore, OwnsAndDestroysTheEntireTemplateWithoutCloningOrExecutingIt)
{
	DefinitionCoreItemCalls root_calls;
	DefinitionCoreItemCalls child_calls;
	std::unique_ptr<tividy::WorkItem> child(new DefinitionCoreObservedItem(
		task(tividy::WorkItemSpec("leaf", "Leaf")), child_calls));
	tividy::WorkItem* child_address = child.get();
	auto tree = stage();
	tree->add(std::move(child));
	std::unique_ptr<tividy::WorkItem> root(new DefinitionCoreObservedItem(std::move(tree), root_calls));
	tividy::WorkItem* root_address = root.get();
	auto value = definition(std::move(root));

	EXPECT_FALSE(root);
	EXPECT_EQ(root_address, root_address->find("root"));
	EXPECT_EQ(child_address, root_address->find("leaf"));
	EXPECT_EQ(0u, root_calls.visits);
	EXPECT_EQ(0u, child_calls.visits);
	EXPECT_EQ(0u, root_calls.destructions);
	EXPECT_EQ(0u, child_calls.destructions);
	value.reset();
	EXPECT_EQ(1u, root_calls.destructions);
	EXPECT_EQ(1u, child_calls.destructions);
}

TEST(WorkflowDefinitionValidation, ValidNestedDefinitionsSupportForwardAndRepeatedDependencies) {
	auto root = stage();
	root->add(task(tividy::WorkItemSpec("later", "Later").dependsOn("first").dependsOn("first")));
	auto nested = stage("nested");
	nested->add(task(tividy::WorkItemSpec("inside", "Inside").dependsOn("first")));
	root->add(std::move(nested));
	root->add(task(tividy::WorkItemSpec("first", "First")));
	auto value = definition(std::move(root));
	EXPECT_TRUE(value->validationProblems().empty());
	EXPECT_NO_THROW(value->validate());
}

TEST(WorkflowDefinitionValidation, DuplicateIdsAcrossStagesAreReportedOncePerOffendingId) {
	auto root = stage();
	for (const std::string name : {"one", "two", "three"}) {
		auto nested = stage(name); nested->add(task(tividy::WorkItemSpec("same", "Same"))); root->add(std::move(nested));
	}
	auto problems = definition(std::move(root))->validationProblems();
	ASSERT_TRUE(problems.size() == 1);
	EXPECT_TRUE(contains(problems, "duplicate item id: same"));
}

TEST(WorkflowDefinitionValidation, EmptyStagesAreDistinguishedFromLeafTasksEvenWhenIdsCollide) {
	auto root = stage();
	root->add(task(tividy::WorkItemSpec("same", "Leaf")));
	root->add(stage("same"));
	auto problems = definition(std::move(root))->validationProblems();
	EXPECT_TRUE(problems.size() == 2);
	EXPECT_TRUE(contains(problems, "stage 'same' has no children"));
}

TEST(WorkflowDefinitionValidation, EmptyRootAndNestedStagesAreInvalid) {
	{ SCOPED_TRACE("root"); EXPECT_TRUE(contains(definition(stage())->validationProblems(), "stage 'root' has no children")); }
	{ SCOPED_TRACE("nested");
		auto root = stage(); root->add(stage("empty"));
		EXPECT_TRUE(contains(definition(std::move(root))->validationProblems(), "stage 'empty' has no children"));
	}
}

TEST(WorkflowDefinitionValidation, DecoratedStageIdentitySurvivesWrapperForwarding) {
	{ SCOPED_TRACE("empty");
		std::unique_ptr<tividy::WorkItem> wrapped(new ValidationTestWrapper(stage("wrapped")));
		EXPECT_TRUE(contains(definition(std::move(wrapped))->validationProblems(), "stage 'wrapped' has no children"));
	}
	{ SCOPED_TRACE("nonempty");
		auto root = stage("wrapped"); root->add(task(tividy::WorkItemSpec("leaf", "Leaf")));
		std::unique_ptr<tividy::WorkItem> wrapped(new ValidationTestWrapper(std::move(root)));
		EXPECT_TRUE(definition(std::move(wrapped))->validationProblems().empty());
	}
}

TEST(WorkflowDefinitionValidation, MissingDependenciesAndSelfDependenciesAreDiagnosedWithoutDuplicates) {
	for (const std::string predecessor : {"absent", "leaf"}) {
		auto root = stage(); root->add(task(tividy::WorkItemSpec("leaf", "Leaf").dependsOn(predecessor).dependsOn(predecessor)));
		auto problems = definition(std::move(root))->validationProblems();
		ASSERT_TRUE(problems.size() == 1);
		EXPECT_TRUE(contains(problems, predecessor == "leaf" ? "depends on itself" : "unknown item 'absent'"));
	}
}

TEST(WorkflowDefinitionValidation, AMultiItemDependencyCycleNamesItsPath) {
	auto root = stage();
	root->add(task(tividy::WorkItemSpec("a", "A").dependsOn("b")));
	root->add(task(tividy::WorkItemSpec("b", "B").dependsOn("c")));
	root->add(task(tividy::WorkItemSpec("c", "C").dependsOn("a")));
	EXPECT_TRUE(contains(definition(std::move(root))->validationProblems(), "dependency cycle: a -> b -> c -> a"));
}

TEST(WorkflowDefinitionValidation, SharedPrerequisitesAreValidAndDoNotCreateFalseCycles) {
	auto root = stage();
	root->add(task(tividy::WorkItemSpec("a", "A").dependsOn("b").dependsOn("c")));
	root->add(task(tividy::WorkItemSpec("b", "B").dependsOn("d")));
	root->add(task(tividy::WorkItemSpec("c", "C").dependsOn("d")));
	root->add(task(tividy::WorkItemSpec("d", "D")));
	EXPECT_TRUE(definition(std::move(root))->validationProblems().empty());
}

TEST(WorkflowDefinitionValidation, ReviewAndAutomaticWorkRulesAreEnforcedIndependently) {
	{ SCOPED_TRACE("reviewer required");
		auto root = stage(); root->add(task(tividy::WorkItemSpec("leaf", "Leaf").reviewRequired(true)));
		EXPECT_TRUE(contains(definition(std::move(root))->validationProblems(), "no reviewer role"));
	}
	{ SCOPED_TRACE("reviewer supplied");
		auto root = stage(); root->add(task(tividy::WorkItemSpec("leaf", "Leaf").reviewRequired(true).reviewedBy("manager")));
		EXPECT_TRUE(definition(std::move(root))->validationProblems().empty());
	}
	{ SCOPED_TRACE("automatic review forbidden");
		auto root = stage(); root->add(task(tividy::WorkItemSpec("leaf", "Leaf").automatic(true).reviewRequired(true).reviewedBy("manager")));
		EXPECT_TRUE(contains(definition(std::move(root))->validationProblems(), "must not require review"));
	}
}

TEST(WorkflowDefinitionValidation, AMissingAssignmentPolicyIsInvalid) {
	auto root = stage(); root->add(task(tividy::WorkItemSpec("leaf", "Leaf")));
	auto value = policies(); value.assignment.reset();
	EXPECT_TRUE(contains(definition(std::move(root), value)->validationProblems(), "assignment policy is null"));
}

TEST(WorkflowDefinitionValidation, RoutingSourceReferencesMustNameExistingItems) {
	for (const std::string id : {"leaf", "unknown"}) {
		auto root = stage(); root->add(task(tividy::WorkItemSpec("leaf", "Leaf")));
		auto value = policies(); value.routing[id].reset(new ValidationTestRouting);
		auto problems = definition(std::move(root), value)->validationProblems();
		EXPECT_TRUE(problems.empty() == (id == "leaf"));
		if (id == "unknown") EXPECT_TRUE(contains(problems, "routing source 'unknown' does not exist"));
	}
}

TEST(WorkflowDefinitionValidation, TimeoutOutcomesRequireAPositiveDeadline) {
	for (const long seconds : {-1L, 0L, 1L}) {
		auto root = stage(); root->add(task(tividy::WorkItemSpec("leaf", "Leaf").dueAfter(seconds).onTimeout("TIMED_OUT")));
		auto problems = definition(std::move(root))->validationProblems();
		EXPECT_TRUE(problems.empty() == (seconds > 0));
		if (seconds <= 0) EXPECT_TRUE(contains(problems, "without a positive deadline"));
	}
}

TEST(WorkflowDefinitionValidation, AutomaticWorkHasNoDeadline) {
	for (const long seconds : {-1L, 0L, 1L}) {
		auto root = stage(); root->add(task(tividy::WorkItemSpec("leaf", "Leaf").automatic(true).dueAfter(seconds)));
		auto problems = definition(std::move(root))->validationProblems();
		EXPECT_TRUE(problems.empty() == (seconds == 0));
		if (seconds != 0) EXPECT_TRUE(contains(problems, "must not have a deadline"));
	}
}

TEST(WorkflowDefinitionValidation, AllProblemsAreCollectedAndValidateJoinsThemInAStableOrder) {
	auto root = stage();
	root->add(task(tividy::WorkItemSpec("leaf", "Leaf").automatic(true).reviewRequired(true).onTimeout("EXPIRED").dependsOn("missing")));
	auto value = policies(); value.assignment.reset(); value.routing["unknown"].reset(new ValidationTestRouting);
	auto workflow = definition(std::move(root), value);
	const auto first = workflow->validationProblems();
	EXPECT_TRUE(first.size() == 6);
	EXPECT_TRUE(workflow->validationProblems() == first);
	std::ostringstream expected;
	for (std::size_t i = 0; i < first.size(); ++i) { if (i) expected << "; "; expected << first[i]; }
	EXPECT_THROW(workflow->validate(), tividy::InvalidDefinitionError);
	try { workflow->validate(); } catch (const tividy::InvalidDefinitionError& error) { EXPECT_TRUE(std::string(error.what()) == expected.str()); }
}

TEST(WorkflowDefinitionValidation, NullRootsAreDiagnosedDefensively) {
	EXPECT_TRUE(contains(definition(std::unique_ptr<tividy::WorkItem>())->validationProblems(), "no root item"));
}

TEST(WorkflowDefinitionValidation, ValidationLeavesTemplateDataAndStructureUnchanged) {
	auto root = stage();
	root->add(task(tividy::WorkItemSpec("leaf", "Leaf").requireRole("worker").dependsOn("missing")));
	tividy::WorkItem* observed = root.get();
	auto workflow = definition(std::move(root));
	const auto before = observed->children();
	const auto deps = before.front()->spec().dependencies();
	workflow->validationProblems();
	EXPECT_TRUE(observed->children() == before);
	EXPECT_TRUE(before.front()->spec().dependencies() == deps);
	EXPECT_TRUE(before.front()->spec().requiredRole() == "worker");
	EXPECT_TRUE((before.front()->status() == tividy::ItemStatus::Created));
}

TEST(WorkflowDefinitionValidation, GeneratedGraphsAgreeWithIndependentTopologicalCycleDetection) {
	unsigned random = 739391;
	for (unsigned sample = 0; sample < 200; ++sample) {
		const std::size_t count = 8;
		std::vector<std::vector<std::size_t> > edges(count);
		std::vector<unsigned> indegree(count, 0);
		for (std::size_t from = 0; from < count; ++from) {
			for (std::size_t to = 0; to < count; ++to) {
				random = random * 1664525u + 1013904223u;
				if (from != to && (random >> 24) < 25) { edges[from].push_back(to); ++indegree[to]; }
			}
		}
		std::vector<std::size_t> ready;
		for (std::size_t i = 0; i < count; ++i) if (indegree[i] == 0) ready.push_back(i);
		std::size_t settled = 0;
		while (!ready.empty()) {
			const auto node = ready.back(); ready.pop_back(); ++settled;
			for (auto target : edges[node]) if (--indegree[target] == 0) ready.push_back(target);
		}
		auto root = stage();
		for (std::size_t from = 0; from < count; ++from) {
			tividy::WorkItemSpec spec("n" + std::to_string(from), "Node");
			for (auto to : edges[from]) spec.dependsOn("n" + std::to_string(to));
			root->add(task(spec));
		}
		const auto problems = definition(std::move(root))->validationProblems();
		SCOPED_TRACE(sample);
		EXPECT_TRUE(contains(problems, "dependency cycle:") == (settled != count));
	}
}

TEST(WorkflowDefinitionValidation, RejectsTaskDependingOnItsParentStage)
{
	auto root = stage();
	root->add(task(tividy::WorkItemSpec("child", "Child").dependsOn("root")));
	const auto problems = definition(std::move(root))->validationProblems();
	EXPECT_TRUE(contains(problems, "dependency cycle:"));
	EXPECT_TRUE(contains(problems, "child"));
	EXPECT_TRUE(contains(problems, "root"));
}

TEST(WorkflowDefinitionValidation, RejectsTaskDependingOnADistantAncestor)
{
	auto root = stage();
	auto group = stage("group");
	group->add(task(tividy::WorkItemSpec("child", "Child").dependsOn("root")));
	root->add(std::move(group));
	EXPECT_TRUE(contains(definition(std::move(root))->validationProblems(), "dependency cycle:"));
}

TEST(WorkflowDefinitionValidation, RejectsStageDependingOnItsChild)
{
	std::unique_ptr<tividy::StageItem> root(new tividy::StageItem(
		tividy::WorkItemSpec("root", "Root").dependsOn("child")));
	root->add(task(tividy::WorkItemSpec("child", "Child")));
	EXPECT_TRUE(contains(definition(std::move(root))->validationProblems(), "dependency cycle:"));
}

TEST(WorkflowDefinitionValidation, RejectsStageDependingOnADistantDescendant)
{
	std::unique_ptr<tividy::StageItem> root(new tividy::StageItem(
		tividy::WorkItemSpec("root", "Root").dependsOn("child")));
	auto group = stage("group");
	group->add(task(tividy::WorkItemSpec("child", "Child")));
	root->add(std::move(group));
	EXPECT_TRUE(contains(definition(std::move(root))->validationProblems(), "dependency cycle:"));
}

TEST(WorkflowDefinitionValidation, DetectsIndirectCyclesThroughStageCompletion)
{
	auto root = stage();
	auto group = stage("group");
	group->add(task(tividy::WorkItemSpec("inside", "Inside").dependsOn("outside")));
	root->add(std::move(group));
	root->add(task(tividy::WorkItemSpec("outside", "Outside").dependsOn("group")));
	EXPECT_TRUE(contains(definition(std::move(root))->validationProblems(), "dependency cycle:"));
}

TEST(WorkflowDefinitionValidation, DetectsCyclesThroughInheritedStageDependencies)
{
	auto root = stage();
	std::unique_ptr<tividy::StageItem> left(new tividy::StageItem(
		tividy::WorkItemSpec("left", "Left").dependsOn("right_task")));
	left->add(task(tividy::WorkItemSpec("left_task", "Left task")));
	std::unique_ptr<tividy::StageItem> right(new tividy::StageItem(
		tividy::WorkItemSpec("right", "Right").dependsOn("left_task")));
	right->add(task(tividy::WorkItemSpec("right_task", "Right task")));
	root->add(std::move(left));
	root->add(std::move(right));
	EXPECT_TRUE(contains(definition(std::move(root))->validationProblems(), "dependency cycle:"));
}

TEST(WorkflowDefinitionValidation, AllowsDependenciesOnIndependentSiblingStages)
{
	auto root = stage();
	std::unique_ptr<tividy::StageItem> left(new tividy::StageItem(
		tividy::WorkItemSpec("left", "Left").dependsOn("right")));
	left->add(task(tividy::WorkItemSpec("left_task", "Left task")));
	auto right = stage("right");
	right->add(task(tividy::WorkItemSpec("right_task", "Right task")));
	root->add(std::move(left));
	root->add(std::move(right));
	EXPECT_TRUE(definition(std::move(root))->validationProblems().empty());
}

TEST(WorkflowDefinitionValidation, AllowsATaskToDependOnAnIndependentStage)
{
	auto root = stage();
	auto group = stage("group");
	group->add(task(tividy::WorkItemSpec("inside", "Inside")));
	root->add(task(tividy::WorkItemSpec("outside", "Outside").dependsOn("group")));
	root->add(std::move(group));
	EXPECT_TRUE(definition(std::move(root))->validationProblems().empty());
}

TEST(WorkflowDefinitionValidation, DecoratedStagesRetainInheritedDependencyConstraints)
{
	auto root = stage();
	auto group = stage("group");
	group->add(task(tividy::WorkItemSpec("child", "Child").dependsOn("group")));
	root->add(std::unique_ptr<tividy::WorkItem>(new ValidationTestWrapper(std::move(group))));
	EXPECT_TRUE(contains(definition(std::move(root))->validationProblems(), "dependency cycle:"));
}

TEST(WorkflowDefinitionValidation, DoesNotExecuteRoutingStrategiesDuringValidation)
{
	class RuntimeRouting : public tividy::RoutingStrategy
	{
	public:
		tividy::RoutingDecision decide(const tividy::WorkItem&, const tividy::Outcome&,
			const tividy::WorkflowData&) const override
		{
			throw tividy::EngineError("runtime routing must not run during validation");
		}
	};
	auto root = stage();
	root->add(task(tividy::WorkItemSpec("leaf", "Leaf")));
	auto policy = policies();
	policy.routing["leaf"].reset(new RuntimeRouting);
	EXPECT_TRUE(definition(std::move(root), policy)->validationProblems().empty());
}

TEST(WorkflowDefinitionValidation, NestedGraphsAgreeWithIndependentCompletionSimulation)
{
	const std::vector<std::string> ids{"root", "left", "left_a", "left_b", "right", "right_a", "right_b"};
	const std::vector<int> parent{-1, 0, 1, 1, 0, 4, 4};
	const std::vector<std::vector<std::size_t> > children{{1, 4}, {2, 3}, {}, {}, {5, 6}, {}, {}};
	unsigned random = 163841;
	unsigned valid = 0;
	unsigned invalid = 0;
	for (unsigned sample = 0; sample < 300; ++sample)
	{
		SCOPED_TRACE(sample);
		std::vector<std::vector<std::size_t> > dependencies(ids.size());
		std::vector<tividy::WorkItemSpec> specs;
		for (std::size_t from = 0; from < ids.size(); ++from)
		{
			specs.emplace_back(ids[from], ids[from]);
			for (std::size_t to = 0; to < ids.size(); ++to)
			{
				random = random * 1664525u + 1013904223u;
				if (from != to && (random >> 24) < 18)
				{
					dependencies[from].push_back(to);
					specs.back().dependsOn(ids[to]);
				}
			}
		}

		// Independently simulate eventual settlement: tasks need their own and
		// every ancestor's prerequisites; stages settle after their children.
		std::vector<bool> settled(ids.size(), false);
		bool changed = true;
		while (changed)
		{
			changed = false;
			for (std::size_t item = 0; item < ids.size(); ++item)
			{
				if (settled[item]) continue;
				bool ready = true;
				if (!children[item].empty())
				{
					for (std::size_t child : children[item]) ready = ready && settled[child];
				}
				else
				{
					for (int ancestor = static_cast<int>(item); ancestor >= 0; ancestor = parent[ancestor])
					{
						for (std::size_t predecessor : dependencies[ancestor]) ready = ready && settled[predecessor];
					}
				}
				if (ready)
				{
					settled[item] = true;
					changed = true;
				}
			}
		}
		const bool completes = std::all_of(settled.begin(), settled.end(), [](bool value) { return value; });
		if (completes) ++valid; else ++invalid;

		std::unique_ptr<tividy::StageItem> root(new tividy::StageItem(specs[0]));
		std::unique_ptr<tividy::StageItem> left(new tividy::StageItem(specs[1]));
		left->add(task(specs[2]));
		left->add(task(specs[3]));
		std::unique_ptr<tividy::StageItem> right(new tividy::StageItem(specs[4]));
		right->add(task(specs[5]));
		right->add(task(specs[6]));
		root->add(std::move(left));
		root->add(std::move(right));
		const auto problems = definition(std::move(root))->validationProblems();
		EXPECT_EQ(contains(problems, "dependency cycle:"), !completes) << ::testing::PrintToString(problems);
	}
	EXPECT_GT(valid, 0u);
	EXPECT_GT(invalid, 0u);
}
