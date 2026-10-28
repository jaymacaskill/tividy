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
