#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "engine/commands/ActionHistory.h"
#include "engine/core/Types.h"
#include "engine/runtime/WorkflowInstance.h"

/*
ActionHistory only ever asks its instance for status(). WorkflowInstance has no implementation yet (it is assembled from
classes that are still being written), so these tests replace status() at link time and never build a real instance.
When WorkflowInstance.cpp exists, move these tests onto a real, started instance.
*/

namespace tividy
{
    namespace
    {
        InstanceStatus fake_status = InstanceStatus::Running;

        struct Probe
        {
            Probe() : executed(0), undone(0), destroyed(0) { }
            int executed;
            int undone;
            int destroyed;
            std::vector<std::string> undo_order;
        };

        class FakeCommand : public Command
        {
            public:
                FakeCommand(Probe& p, const std::string& n, bool ok = true, bool rev = true, bool undo_ok = true)
                    : probe(p), name(n), execute_ok(ok), reversible(rev), undo_succeeds(undo_ok) { }
                ~FakeCommand() { ++probe.destroyed; }
                CommandResult execute()
                {
                    ++probe.executed;
                    return execute_ok ? CommandResult::ok("ran " + name) : CommandResult::failure("failed " + name);
                }
                CommandResult undo()
                {
                    ++probe.undone;
                    probe.undo_order.push_back(name);
                    return undo_succeeds ? CommandResult::ok("undid " + name) : CommandResult::failure("stuck " + name);
                }
                bool isReversible() const { return reversible; }
                std::string describe() const { return name; }
            private:
                Probe& probe;
                std::string name;
                bool execute_ok;
                bool reversible;
                bool undo_succeeds;
        };

        std::unique_ptr<Command> make(Probe& p, const std::string& n, bool ok = true, bool rev = true, bool undo_ok = true)
        {
            return std::unique_ptr<Command>(new FakeCommand(p, n, ok, rev, undo_ok));
        }

        alignas(WorkflowInstance) unsigned char instance_storage[sizeof(WorkflowInstance)];
    }

    InstanceStatus WorkflowInstance::status() const { return fake_status; }

    class ActionHistoryTest : public ::testing::Test
    {
        protected:
            ActionHistoryTest() : history(*reinterpret_cast<WorkflowInstance*>(instance_storage)) { }
            void SetUp() { fake_status = InstanceStatus::Running; }
            Probe probe;
            ActionHistory history;
    };

    TEST_F(ActionHistoryTest, StartsEmpty)
    {
        EXPECT_EQ(history.size(), 0u);
        EXPECT_TRUE(history.describeAll().empty());
    }

    TEST_F(ActionHistoryTest, RejectsANullCommand)
    {
        CommandResult r = history.execute(std::unique_ptr<Command>());
        EXPECT_FALSE(r.success);
        EXPECT_EQ(r.message, "null command");
        EXPECT_EQ(history.size(), 0u);
    }

    TEST_F(ActionHistoryTest, OnlyRunsWhileTheInstanceIsRunning)
    {
        const InstanceStatus blocked[] = { InstanceStatus::Created, InstanceStatus::Completed, InstanceStatus::Failed, InstanceStatus::Cancelled };
        for (std::size_t i = 0; i < 4; ++i)
        {
            fake_status = blocked[i];
            CommandResult r = history.execute(make(probe, "A"));
            EXPECT_FALSE(r.success);
            EXPECT_EQ(r.message, "instance is " + toString(blocked[i]) + ", not Running");
        }
        EXPECT_EQ(probe.executed, 0);
        EXPECT_EQ(history.size(), 0u);
    }

    TEST_F(ActionHistoryTest, ASuccessfulCommandIsRunAndStacked)
    {
        CommandResult r = history.execute(make(probe, "A"));
        EXPECT_TRUE(r.success);
        EXPECT_EQ(r.message, "ran A");
        EXPECT_EQ(probe.executed, 1);
        EXPECT_EQ(history.size(), 1u);
    }

    TEST_F(ActionHistoryTest, AFailedCommandIsReportedAndDiscarded)
    {
        CommandResult r = history.execute(make(probe, "A", false));
        EXPECT_FALSE(r.success);
        EXPECT_EQ(r.message, "failed A");
        EXPECT_EQ(history.size(), 0u);
        EXPECT_EQ(probe.destroyed, 1);
    }

    TEST_F(ActionHistoryTest, AnIrreversibleCommandIsStillRecorded)
    {
        EXPECT_TRUE(history.execute(make(probe, "A", true, false)).success);
        EXPECT_EQ(history.size(), 1u);
        EXPECT_EQ(history.describeAll()[0], "1. A");
    }

    TEST_F(ActionHistoryTest, UndoWithNothingStackedFails)
    {
        CommandResult r = history.undoLast();
        EXPECT_FALSE(r.success);
        EXPECT_EQ(r.message, "nothing to undo");
    }

    TEST_F(ActionHistoryTest, UndoRefusesAnIrreversibleCommandAndLeavesTheStackAlone)
    {
        history.execute(make(probe, "A"));
        history.execute(make(probe, "B", true, false));
        CommandResult r = history.undoLast();
        EXPECT_FALSE(r.success);
        EXPECT_EQ(r.message, "cannot undo: B is irreversible");
        EXPECT_EQ(probe.undone, 0);
        EXPECT_EQ(history.size(), 2u);
    }

    TEST_F(ActionHistoryTest, UndoRunsNewestFirstAndPopsOnSuccess)
    {
        history.execute(make(probe, "A"));
        history.execute(make(probe, "B"));
        history.execute(make(probe, "C"));

        CommandResult r = history.undoLast();
        EXPECT_TRUE(r.success);
        EXPECT_EQ(r.message, "undid C");
        EXPECT_EQ(history.size(), 2u);

        history.undoLast();
        history.undoLast();
        EXPECT_EQ(history.size(), 0u);
        ASSERT_EQ(probe.undo_order.size(), 3u);
        EXPECT_EQ(probe.undo_order[0], "C");
        EXPECT_EQ(probe.undo_order[1], "B");
        EXPECT_EQ(probe.undo_order[2], "A");
    }

    TEST_F(ActionHistoryTest, AFailedUndoKeepsTheCommandOnTheStack)
    {
        history.execute(make(probe, "A", true, true, false));
        CommandResult r = history.undoLast();
        EXPECT_FALSE(r.success);
        EXPECT_EQ(r.message, "stuck A");
        EXPECT_EQ(history.size(), 1u);
    }

    TEST_F(ActionHistoryTest, DescribeAllIsNumberedOldestFirst)
    {
        history.execute(make(probe, "A"));
        history.execute(make(probe, "B"));
        std::vector<std::string> lines = history.describeAll();
        ASSERT_EQ(lines.size(), 2u);
        EXPECT_EQ(lines[0], "1. A");
        EXPECT_EQ(lines[1], "2. B");
    }

    TEST_F(ActionHistoryTest, UndoneCommandsDisappearFromDescribeAll)
    {
        history.execute(make(probe, "A"));
        history.execute(make(probe, "B"));
        history.undoLast();
        ASSERT_EQ(history.describeAll().size(), 1u);
        EXPECT_EQ(history.describeAll()[0], "1. A");
    }

    TEST(ActionHistoryLifetimeTest, DestroyingTheHistoryDestroysItsCommands)
    {
        fake_status = InstanceStatus::Running;
        Probe probe;
        {
            ActionHistory history(*reinterpret_cast<WorkflowInstance*>(instance_storage));
            history.execute(make(probe, "A"));
            history.execute(make(probe, "B"));
            EXPECT_EQ(probe.destroyed, 0);
        }
        EXPECT_EQ(probe.destroyed, 2);
    }
}