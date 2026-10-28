
/**
 * @file test_macro_command.cpp
 * @brief Unit tests for MacroCommand behavior.
 * @details Covers child execution order, rollback after failure, undo,
 * reversibility, descriptions, nested and empty macros, and invalid lifecycle
 * operations.
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include "engine/core/Errors.h"

#include "engine/commands/MacroCommand.h"

namespace tividy
{
    /**
     * @brief Test double that records command calls and returns configured results.
     * @details Used to verify MacroCommand behavior without depending on other
     * command implementations.
     */
    class FakeCommand : public Command
    {
    public:
        FakeCommand(const std::string &name,
                    std::vector<std::string> &log,
                    bool succeeds = true,
                    bool reversible = true)
            : name(name), log(log),
              succeeds(succeeds), reversible(reversible) {}

        CommandResult execute() override
        {
            log.push_back(name + ".execute");
            return succeeds
                       ? CommandResult::ok()
                       : CommandResult::failure("failed");
        }

        CommandResult undo() override
        {
            log.push_back(name + ".undo");
            return CommandResult::ok();
        }

        bool isReversible() const override
        {
            return reversible;
        }

        std::string describe() const override
        {
            return name;
        }

    private:
        std::string name;
        std::vector<std::string> &log;
        bool succeeds;
        bool reversible;
    };

    TEST(MacroCommandTest, ExecutesInOrder)
    {
        std::vector<std::string> log;
        MacroCommand macro("Routine");

        macro.add(std::unique_ptr<Command>(
            new FakeCommand("A", log)));
        macro.add(std::unique_ptr<Command>(
            new FakeCommand("B", log)));

        EXPECT_TRUE(macro.execute().success);
        EXPECT_EQ(log,
                  (std::vector<std::string>{"A.execute", "B.execute"}));
    }

    TEST(MacroCommandTest, RollsBackOnFailure)
    {
        std::vector<std::string> log;
        MacroCommand macro("Routine");

        macro.add(std::unique_ptr<Command>(
            new FakeCommand("A", log)));
        macro.add(std::unique_ptr<Command>(
            new FakeCommand("B", log)));
        macro.add(std::unique_ptr<Command>(
            new FakeCommand("C", log, false)));

        EXPECT_FALSE(macro.execute().success);

        EXPECT_EQ(log, (std::vector<std::string>{
                           "A.execute", "B.execute", "C.execute",
                           "B.undo", "A.undo"}));
    }

    TEST(MacroCommandTest, UndoRunsInReverseOrder)
    {
        std::vector<std::string> log;
        MacroCommand macro("Routine");

        macro.add(std::unique_ptr<Command>(
            new FakeCommand("A", log)));
        macro.add(std::unique_ptr<Command>(
            new FakeCommand("B", log)));

        ASSERT_TRUE(macro.execute().success);
        EXPECT_TRUE(macro.undo().success);

        EXPECT_EQ(log, (std::vector<std::string>{
                           "A.execute", "B.execute", "B.undo", "A.undo"}));
    }

    TEST(MacroCommandTest, RejectsIrreversibleUndo)
    {
        std::vector<std::string> log;
        MacroCommand macro("Routine");

        macro.add(std::unique_ptr<Command>(
            new FakeCommand("A", log, true, false)));

        EXPECT_FALSE(macro.isReversible());
        ASSERT_TRUE(macro.execute().success);
        EXPECT_FALSE(macro.undo().success);
    }

    TEST(MacroCommandTest, DescribesItsChildren)
    {
        std::vector<std::string> log;
        MacroCommand macro("Routine");

        macro.add(std::unique_ptr<Command>(
            new FakeCommand("A", log)));

        EXPECT_EQ(macro.describe(), "Routine [1 step(s)]\n  A");
    }

    TEST(MacroCommandTest, SupportsNestedMacros)
    {
        std::vector<std::string> log;

        std::unique_ptr<MacroCommand> inner(
            new MacroCommand("Inner"));

        inner->add(std::unique_ptr<Command>(
            new FakeCommand("B", log)));

        MacroCommand outer("Outer");
        outer.add(std::unique_ptr<Command>(
            new FakeCommand("A", log)));
        outer.add(std::move(inner));

        EXPECT_TRUE(outer.execute().success);
        EXPECT_EQ(log,
                  (std::vector<std::string>{"A.execute", "B.execute"}));
    }

    TEST(MacroCommandTest, EmptyMacroSucceeds)
    {
        MacroCommand macro("Empty");

        EXPECT_EQ(macro.size(), 0u);
        EXPECT_TRUE(macro.execute().success);
        EXPECT_TRUE(macro.isReversible());
    }

    TEST(MacroCommandTest, CannotExecuteTwice)
    {
        MacroCommand macro("Routine");

        EXPECT_TRUE(macro.execute().success);
        EXPECT_FALSE(macro.execute().success);
    }

    TEST(MacroCommandTest, CannotUndoBeforeExecution)
    {
        MacroCommand macro("Routine");

        EXPECT_FALSE(macro.undo().success);
    }

    TEST(MacroCommandTest, CannotAddAfterExecution)
    {
        std::vector<std::string> log;
        MacroCommand macro("Routine");

        ASSERT_TRUE(macro.execute().success);

        EXPECT_THROW(
            macro.add(std::unique_ptr<Command>(
                new FakeCommand("A", log))),
            InvalidStateError);
    }
}
