#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "engine/core/Errors.h"
#include "engine/facade/Tividy.h"
#include "engine/items/StageItem.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemVisitor.h"
#include "support/FacadeActionsFixtures.h"

// Tests for src/engine/facade/TividyActions.cpp. Like test_facade_startup, this binary links explicit objects and
// substitutes only the runtime classes that are not in the tree yet (see support/FacadeActionsFixtures.h).
// The real facade, builder, StageItem, ActionHistory, AuditLog and ManualClock are exercised.

namespace tividy
{
    namespace
    {
        // A leaf whose lifecycle the tests drive directly. Clones start Created/unassigned like a prototype.
        class Leaf : public WorkItem
        {
            public:
                Leaf(const std::string& id, const std::string& name, long due_after = 0)
                    : spec_ref(id, name), state(ItemStatus::Created), who(), due_after(due_after), context(0) { }

                const WorkItemSpec& spec() const override { return spec_ref; }
                ItemStatus status() const override { return state; }
                const Id& assignee() const override { return who; }
                TimePoint assignedAt() const override { return assigned; }
                TimePoint dueAt() const override { return due_after > 0 ? assigned + std::chrono::seconds(due_after) : TimePoint(); }
                WorkItem* find(const Id& id) override { return id == spec_ref.id() ? this : 0; }
                void publish() override { state = ItemStatus::Available; }
                void assign(const Id& participant_id) override
                {
                    who = participant_id;
                    state = ItemStatus::Assigned;
                    assigned = context->clock().now();
                }
                void start(const Participant&) override { state = ItemStatus::InProgress; }
                void submit(const Participant&, const Outcome&) override { state = ItemStatus::UnderReview; }
                void complete(const Participant&, const Outcome&) override { state = ItemStatus::Completed; }
                void approve(const Participant&, const Outcome&) override { state = ItemStatus::Completed; }
                void reject(const Participant&, const std::string&) override { state = ItemStatus::InProgress; }
                void escalate(const std::string&) override { state = ItemStatus::Escalated; }
                void cancel(const std::string&) override { state = ItemStatus::Cancelled; }
                void skip(const std::string&) override { state = ItemStatus::Skipped; }
                void expire(const std::string&, const std::string&) override { state = ItemStatus::Cancelled; }
                void bind(WorkContext& c) override { context = &c; }
                void accept(WorkItemVisitor&) const override { }
                std::unique_ptr<WorkItem> clone() const override { return std::unique_ptr<WorkItem>(new Leaf(spec_ref.id(), spec_ref.name(), due_after)); }
                std::unique_ptr<WorkItemIterator> createIterator() override { return std::unique_ptr<WorkItemIterator>(new DepthFirstIterator(*this)); }
                void restoreTo(ItemStatus s) { state = s; }
            private:
                WorkItemSpec spec_ref;
                ItemStatus state;
                Id who;
                TimePoint assigned;
                long due_after;
                WorkContext* context;
        };

        // Reversible command over one leaf: "finish" completes it, undo puts the previous status back.
        class FinishCommand : public Command
        {
            public:
                FinishCommand(Leaf& leaf, const Participant& actor, bool reversible = true)
                    : leaf(leaf), actor(actor), before(leaf.status()), reversible(reversible) { }
                CommandResult execute() override
                {
                    before = leaf.status();
                    leaf.complete(actor, Outcome());
                    return CommandResult::ok("finished " + leaf.spec().id());
                }
                CommandResult undo() override { leaf.restoreTo(before); return CommandResult::ok("reopened"); }
                bool isReversible() const override { return reversible; }
                std::string describe() const override { return actor.id() + " finished " + leaf.spec().id(); }
            private:
                Leaf& leaf;
                const Participant& actor;
                ItemStatus before;
                bool reversible;
        };

        class FailingCommand : public Command
        {
            public:
                CommandResult execute() override { return CommandResult::failure("nope"); }
                CommandResult undo() override { return CommandResult::failure("nope"); }
                bool isReversible() const override { return true; }
                std::string describe() const override { return "failing"; }
        };

        class ActionsDirector : public WorkflowDirector
        {
            public:
                explicit ActionsDirector(const std::string& name, long due_after = 0) : name(name), due_after(due_after) { }
                std::string workflowName() const override { return name; }
                unsigned version() const override { return 1; }
                std::string description() const override { return "facade actions"; }
                void construct(WorkflowBuilder& builder) const override
                {
                    builder.addTask(std::unique_ptr<WorkItem>(new Leaf("t1", "First task", due_after)));
                    builder.addTask(std::unique_ptr<WorkItem>(new Leaf("t2", "Second task")));
                    builder.addCancelSignal("abort");
                }
            private:
                std::string name;
                long due_after;
        };

        class ActionsPack : public IndustryPack
        {
            public:
                explicit ActionsPack(long due_after = 0) : due_after(due_after) { }
                std::string key() const override { return "actions"; }
                std::string displayName() const override { return "Actions pack"; }
                std::unique_ptr<WorkflowDirector> createDirector() const override { return std::unique_ptr<WorkflowDirector>(new ActionsDirector("actions-flow", due_after)); }
                std::vector<Participant> createParticipants() const override
                {
                    std::vector<Participant> people;
                    people.push_back(Participant("thandi", "Thandi", std::set<std::string>()));
                    people.push_back(Participant("sipho", "Sipho", std::set<std::string>()));
                    return people;
                }
                std::vector<std::unique_ptr<ExternalConnector> > createConnectors() const override { return std::vector<std::unique_ptr<ExternalConnector> >(); }
                ClientProfile createProfile() const override { return ClientProfile(); }
            private:
                long due_after;
        };

        class FacadeActionsTest : public ::testing::Test
        {
            protected:
                void SetUp() override { tividy.reset(new Tividy(std::unique_ptr<ManualClock>(new ManualClock(TimePoint())))); }

                void install(long due_after = 0) { tividy->installPack(std::unique_ptr<IndustryPack>(new ActionsPack(due_after))); }

                Id start()
                {
                    const Id id = tividy->startWorkflow("actions-flow", WorkflowData());
                    return id;
                }

                // Releases a task and assigns it, as FlowController would once it exists in the tree.
                Leaf& give(const Id& instance_id, const Id& item_id, const Id& to)
                {
                    Leaf& leaf = static_cast<Leaf&>(*tividy->instance(instance_id).findItem(item_id));
                    leaf.publish();
                    leaf.assign(to);
                    return leaf;
                }

                void registerFinish()
                {
                    // CommandFactory is reachable read-only through the facade; the fixture factory lets us register on it.
                    const_cast<CommandFactory&>(tividy->commandFactory()).registerVerb("finish", [](const CommandRequest& r) {
                        return std::unique_ptr<Command>(new FinishCommand(static_cast<Leaf&>(*r.instance.findItem(r.item_id)), r.actor));
                    });
                    const_cast<CommandFactory&>(tividy->commandFactory()).registerVerb("explode", [](const CommandRequest&) -> std::unique_ptr<Command> {
                        throw ValidationError("refused by creator");
                    });
                }

                std::unique_ptr<Tividy> tividy;
        };
    }

    // ---- perform ----

    TEST_F(FacadeActionsTest, PerformRunsTheVerbThroughTheInstanceHistory)
    {
        install();
        registerFinish();
        const Id id = start();
        Leaf& leaf = give(id, "t1", "thandi");

        const CommandResult result = tividy->perform("finish", id, "t1", "thandi");

        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.message, "finished t1");
        EXPECT_EQ(leaf.status(), ItemStatus::Completed);
        EXPECT_EQ(tividy->instance(id).actions().size(), 1u);
    }

    TEST_F(FacadeActionsTest, PerformReportsUnknownInstanceItemActorAndVerbWithoutThrowing)
    {
        install();
        registerFinish();
        const Id id = start();
        give(id, "t1", "thandi");

        CommandResult r = tividy->perform("finish", "inst-99", "t1", "thandi");
        EXPECT_FALSE(r.success);
        EXPECT_NE(r.message.find("inst-99"), std::string::npos);

        r = tividy->perform("finish", id, "nope", "thandi");
        EXPECT_FALSE(r.success);
        EXPECT_NE(r.message.find("nope"), std::string::npos);

        r = tividy->perform("finish", id, "t1", "ghost");
        EXPECT_FALSE(r.success);
        EXPECT_NE(r.message.find("ghost"), std::string::npos);

        r = tividy->perform("dance", id, "t1", "thandi");
        EXPECT_FALSE(r.success);
        EXPECT_NE(r.message.find("unknown action"), std::string::npos);

        EXPECT_EQ(tividy->instance(id).actions().size(), 0u);   // nothing ran
        EXPECT_EQ(static_cast<Leaf*>(tividy->instance(id).findItem("t1"))->status(), ItemStatus::Assigned);
    }

    TEST_F(FacadeActionsTest, PerformTurnsACreatorRefusalIntoAFailure)
    {
        install();
        registerFinish();
        const Id id = start();
        give(id, "t1", "thandi");

        CommandResult r;
        ASSERT_NO_THROW(r = tividy->perform("explode", id, "t1", "thandi"));
        EXPECT_FALSE(r.success);
        EXPECT_EQ(r.message, "refused by creator");
    }

    TEST_F(FacadeActionsTest, PerformFailsOnAnInstanceThatIsNotRunning)
    {
        install();
        registerFinish();
        const Id id = start();
        give(id, "t1", "thandi");
        tividy->instance(id).cancel("stop");

        const CommandResult r = tividy->perform("finish", id, "t1", "thandi");

        EXPECT_FALSE(r.success);
        EXPECT_NE(r.message.find("not Running"), std::string::npos);
    }

    // ---- execute / undo ----

    TEST_F(FacadeActionsTest, ExecuteRunsAPrebuiltCommandAndUndoReversesIt)
    {
        install();
        const Id id = start();
        Leaf& leaf = give(id, "t1", "thandi");
        const Participant& thandi = tividy->participants().require("thandi");

        const CommandResult ran = tividy->execute(id, std::unique_ptr<Command>(new FinishCommand(leaf, thandi)));
        EXPECT_TRUE(ran.success);
        EXPECT_EQ(leaf.status(), ItemStatus::Completed);

        const CommandResult undone = tividy->undo(id);
        EXPECT_TRUE(undone.success);
        EXPECT_EQ(leaf.status(), ItemStatus::Assigned);
        EXPECT_EQ(tividy->instance(id).actions().size(), 0u);
    }

    TEST_F(FacadeActionsTest, ExecuteAndUndoReportProblemsAsFailures)
    {
        install();
        const Id id = start();
        Leaf& leaf = give(id, "t1", "thandi");
        const Participant& thandi = tividy->participants().require("thandi");

        EXPECT_FALSE(tividy->execute("inst-99", std::unique_ptr<Command>(new FailingCommand)).success);
        EXPECT_FALSE(tividy->undo("inst-99").success);

        const CommandResult nullCommand = tividy->execute(id, std::unique_ptr<Command>());
        EXPECT_FALSE(nullCommand.success);
        EXPECT_EQ(nullCommand.message, "null command");

        const CommandResult failed = tividy->execute(id, std::unique_ptr<Command>(new FailingCommand));
        EXPECT_FALSE(failed.success);
        EXPECT_EQ(tividy->instance(id).actions().size(), 0u);   // failed commands are not stacked

        EXPECT_EQ(tividy->undo(id).message, "nothing to undo");

        tividy->execute(id, std::unique_ptr<Command>(new FinishCommand(leaf, thandi, false)));
        const CommandResult irreversible = tividy->undo(id);
        EXPECT_FALSE(irreversible.success);
        EXPECT_NE(irreversible.message.find("irreversible"), std::string::npos);
        EXPECT_EQ(leaf.status(), ItemStatus::Completed);
    }

    TEST_F(FacadeActionsTest, UndoOnlyAffectsTheNamedInstance)
    {
        install();
        registerFinish();
        const Id a = start();
        const Id b = start();
        give(a, "t1", "thandi");
        give(b, "t1", "thandi");
        tividy->perform("finish", a, "t1", "thandi");
        tividy->perform("finish", b, "t1", "thandi");

        tividy->undo(a);

        EXPECT_EQ(tividy->instance(a).findItem("t1")->status(), ItemStatus::Assigned);
        EXPECT_EQ(tividy->instance(b).findItem("t1")->status(), ItemStatus::Completed);
    }

    TEST_F(FacadeActionsTest, CommandFactoryExposesTheVerbRegistry)
    {
        install();
        registerFinish();
        const std::vector<std::string> verbs = tividy->commandFactory().verbs();
        EXPECT_EQ(verbs, (std::vector<std::string>{"explode", "finish"}));
    }

    // ---- workFor ----

    TEST_F(FacadeActionsTest, WorkForListsAwaitingItemsAcrossRunningInstancesInOrder)
    {
        install(600);
        const Id a = start();
        const Id b = start();
        give(a, "t1", "thandi");
        give(a, "t2", "thandi");
        give(b, "t1", "thandi");
        give(b, "t2", "sipho");

        const std::vector<WorkItemView> mine = tividy->workFor("thandi");

        ASSERT_EQ(mine.size(), 3u);
        EXPECT_EQ(mine[0].instance_id, a);
        EXPECT_EQ(mine[0].item_id, "t1");
        EXPECT_EQ(mine[0].name, "First task");
        EXPECT_EQ(mine[0].status, ItemStatus::Assigned);
        EXPECT_EQ(mine[0].due_at, tividy->now() + std::chrono::seconds(600));
        EXPECT_EQ(mine[1].instance_id, a);
        EXPECT_EQ(mine[1].item_id, "t2");
        EXPECT_EQ(mine[1].due_at, TimePoint());
        EXPECT_EQ(mine[2].instance_id, b);
        EXPECT_EQ(tividy->workFor("sipho").size(), 1u);
    }

    TEST_F(FacadeActionsTest, WorkForSkipsFinishedItemsAndInstancesThatAreNotRunning)
    {
        install();
        registerFinish();
        const Id a = start();
        const Id b = start();
        give(a, "t1", "thandi");
        give(b, "t1", "thandi");
        tividy->perform("finish", a, "t1", "thandi");
        EXPECT_EQ(tividy->workFor("thandi").size(), 1u);   // a's task is Completed

        tividy->instance(b).cancel("stop");
        EXPECT_TRUE(tividy->workFor("thandi").empty());     // b is Cancelled
        EXPECT_TRUE(tividy->workFor("ghost").empty());
    }

    // ---- history ----

    TEST_F(FacadeActionsTest, HistoryReturnsFormattedAuditLinesOldestFirst)
    {
        install();
        const Id id = start();
        tividy->advanceTime(90);
        tividy->signal(id, "ping");

        const std::vector<std::string> lines = tividy->history(id);

        ASSERT_EQ(lines.size(), 2u);
        EXPECT_NE(lines[0].find("InstanceStarted"), std::string::npos);
        EXPECT_NE(lines[1].find("ExternalSignal"), std::string::npos);
        EXPECT_NE(lines[1].find("ping"), std::string::npos);
        EXPECT_NE(lines[1].find("00:01:30"), std::string::npos);   // simulated time, not wall time
        EXPECT_THROW(tividy->history("inst-99"), NotFoundError);
    }

    // ---- progress ----

    TEST_F(FacadeActionsTest, ProgressUsesAVisitorOverTheTree)
    {
        install();
        const Id id = start();

        const ProgressSummary summary = tividy->progress(id);

        EXPECT_EQ(summary.total, 0u);            // fake leaves are not TaskItems; the real counting is covered at integration
        EXPECT_TRUE(summary.by_status.empty());
        EXPECT_DOUBLE_EQ(summary.percent_settled, 100.0);
        EXPECT_THROW(tividy->progress("inst-99"), NotFoundError);
    }

    // ---- signal ----

    TEST_F(FacadeActionsTest, SignalPublishesAnExternalSignalWithTheNameAndSystemActor)
    {
        install();
        const Id id = start();
        tividy->advanceTime(5);

        tividy->signal(id, "abort");

        const std::vector<AuditEntry>& entries = tividy->instance(id).audit().entries();
        ASSERT_EQ(entries.size(), 2u);
        EXPECT_EQ(entries[1].type, EventType::ExternalSignal);
        EXPECT_EQ(entries[1].message, "abort");
        EXPECT_EQ(entries[1].actor_id, "system");
        EXPECT_EQ(entries[1].when, TimePoint() + std::chrono::seconds(5));
        EXPECT_THROW(tividy->signal("inst-99", "abort"), NotFoundError);
    }

    // ---- time ----

    TEST_F(FacadeActionsTest, AdvanceTimeMovesTheClockAndRejectsNegativeSeconds)
    {
        install();
        const TimePoint start_time = tividy->now();

        EXPECT_EQ(tividy->advanceTime(0), 0u);
        EXPECT_EQ(tividy->now(), start_time);
        tividy->advanceTime(120);
        EXPECT_EQ(tividy->now(), start_time + std::chrono::seconds(120));

        EXPECT_THROW(tividy->advanceTime(-1), EngineError);
        EXPECT_EQ(tividy->now(), start_time + std::chrono::seconds(120));   // refused before the clock moved
    }

    TEST_F(FacadeActionsTest, AdvanceTimeSweepsEveryRunningInstanceAndTotalsTheMisses)
    {
        install(100);
        const Id a = start();
        const Id b = start();
        const Id c = start();
        give(a, "t1", "thandi");
        give(b, "t1", "thandi");
        give(c, "t1", "thandi");
        tividy->instance(c).cancel("stop");   // not Running: must not be swept

        EXPECT_EQ(tividy->advanceTime(99), 0u);    // not yet due
        EXPECT_EQ(tividy->advanceTime(2), 2u);     // a and b lapsed, c is skipped
        EXPECT_EQ(tividy->advanceTime(500), 0u);   // each lapse is reported once
    }

    TEST_F(FacadeActionsTest, ConstructorGivesSharedDirectoryAndConnectorAccess)
    {
        install();
        EXPECT_NE(tividy->participants().find("thandi"), static_cast<const Participant*>(0));
        EXPECT_TRUE(tividy->participants().find("ghost") == 0);
        tividy->participants().add(Participant("new", "New", std::set<std::string>()));
        EXPECT_NE(tividy->participants().find("new"), static_cast<const Participant*>(0));
        EXPECT_FALSE(tividy->connectors().has("missing"));
    }
}