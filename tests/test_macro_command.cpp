
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
    namespace
    {
        /**
         * @brief Test double that records command calls and returns configured results.
         * @details Used to verify MacroCommand behavior without depending on other
         * command implementations.
         */
        class FakeCommand : public Command
        {
        private:
            bool undoSucceeds = true;

        public:
            FakeCommand(const std::string &name,
                        std::vector<std::string> &log,
                        bool succeeds = true,
                        bool reversible = true,
                        bool retryable = false)
                : name(name), log(log),
                  succeeds(succeeds), reversible(reversible),
                  retryable(retryable) {}

            void setSucceeds(bool value)
            {
                succeeds = value;
            }

            void setUndoSucceeds(bool value)
            {
                undoSucceeds = value;
            }

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
                return undoSucceeds
                           ? CommandResult::ok()
                           : CommandResult::failure("undo failed");
            }

            bool isReversible() const override
            {
                return reversible;
            }

            bool supportsRetry() const override
            {
                return retryable;
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
            bool retryable;
        };
    }

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

    TEST(MacroCommandTest, CanRetryAfterSuccessfulRollback)
    {
        std::vector<std::string> log;
        MacroCommand macro("Routine");

        macro.add(std::unique_ptr<Command>(
            new FakeCommand("A", log, true, true, true)));

        FakeCommand *second = new FakeCommand("B", log, false, true, true);
        macro.add(std::unique_ptr<Command>(second));

        // First attempt fails and rolls back A.
        EXPECT_FALSE(macro.execute().success);

        // The failed command can now succeed.
        second->setSucceeds(true);

        // The macro should permit another attempt.
        EXPECT_TRUE(macro.execute().success);

        EXPECT_EQ(log, (std::vector<std::string>{
                           "A.execute",
                           "B.execute",
                           "A.undo",
                           "A.execute",
                           "B.execute"}));
    }

    TEST(MacroCommandTest, DoesNotRetryChildrenWithoutRetrySupport)
    {
        std::vector<std::string> log;
        MacroCommand macro("Routine");

        macro.add(std::unique_ptr<Command>(
            new FakeCommand("A", log)));
        macro.add(std::unique_ptr<Command>(
            new FakeCommand("B", log, false, true, true)));

        CommandResult result = macro.execute();

        EXPECT_EQ(result.message,
                  "Routine stopped at step 2: failed; rolled back 1 of 1 step(s); retry unavailable");
        EXPECT_FALSE(macro.execute().success);
        EXPECT_EQ(log, (std::vector<std::string>{
                           "A.execute", "B.execute", "A.undo"}));
    }

    TEST(MacroCommandTest, RetryDoesNotRequireRetrySupportFromUntouchedChildren)
    {
        std::vector<std::string> log;
        MacroCommand macro("Routine");

        FakeCommand *first = new FakeCommand("A", log, false, true, true);
        macro.add(std::unique_ptr<Command>(first));
        macro.add(std::unique_ptr<Command>(
            new FakeCommand("B", log)));

        EXPECT_FALSE(macro.execute().success);
        first->setSucceeds(true);
        EXPECT_TRUE(macro.execute().success);
        EXPECT_EQ(log, (std::vector<std::string>{
                           "A.execute", "A.execute", "B.execute"}));
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

    TEST(MacroCommandTest, RollsBackNestedMacroAndCanRetry)
    {
        std::vector<std::string> log;
        FakeCommand *last = new FakeCommand("C", log, false, true, true);

        std::unique_ptr<MacroCommand> inner(new MacroCommand("Inner"));
        inner->add(std::unique_ptr<Command>(
            new FakeCommand("B", log, true, true, true)));
        inner->add(std::unique_ptr<Command>(last));

        MacroCommand outer("Outer");
        outer.add(std::unique_ptr<Command>(
            new FakeCommand("A", log, true, true, true)));
        outer.add(std::move(inner));

        CommandResult first = outer.execute();
        EXPECT_FALSE(first.success);
        EXPECT_NE(first.message.find("rolled back 1 of 1 step(s)"),
                  std::string::npos);

        last->setSucceeds(true);
        EXPECT_TRUE(outer.execute().success);
        EXPECT_EQ(log, (std::vector<std::string>{
                           "A.execute", "B.execute", "C.execute", "B.undo",
                           "A.undo", "A.execute", "B.execute", "C.execute"}));
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

    TEST(MacroCommandTest, RejectsNullChild)
    {
        MacroCommand macro("Routine");

        EXPECT_THROW(
            macro.add(std::unique_ptr<Command>()),
            InvalidStateError);
    }

    TEST(MacroCommandTest, ReportsIncompleteRollback)
    {
        std::vector<std::string> log;
        MacroCommand macro("Routine");

        FakeCommand *first = new FakeCommand("A", log);
        first->setUndoSucceeds(false);

        macro.add(std::unique_ptr<Command>(first));
        macro.add(std::unique_ptr<Command>(
            new FakeCommand("B", log, false)));

        CommandResult result = macro.execute();

        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.message,
                  "Routine stopped at step 2: failed; rolled back 0 of 1 step(s); rollback incomplete");

        // Unsafe to retry because A was not undone.
        EXPECT_FALSE(macro.execute().success);
    } // End of ReportsIncompleteRollback

    TEST(MacroCommandTest, DoesNotRetryAfterNestedRollbackFailure)
    {
        std::vector<std::string> log;

        std::unique_ptr<MacroCommand> inner(new MacroCommand("Inner"));

        FakeCommand *b = new FakeCommand("B", log, true, true, true);
        b->setUndoSucceeds(false);

        inner->add(std::unique_ptr<Command>(b));
        inner->add(std::unique_ptr<Command>(
            new FakeCommand("C", log, false, true, true)));

        MacroCommand outer("Outer");
        outer.add(std::unique_ptr<Command>(
            new FakeCommand("A", log, true, true, true)));
        outer.add(std::move(inner));

        CommandResult result = outer.execute();

        EXPECT_FALSE(result.success);
        EXPECT_NE(result.message.find("rollback incomplete"),
                  std::string::npos);
        EXPECT_FALSE(outer.supportsRetry());

        EXPECT_EQ(outer.execute().message,
                  "Macro cannot be retried safely");

        EXPECT_EQ(log, (std::vector<std::string>{
                           "A.execute", "B.execute", "C.execute",
                           "B.undo", "A.undo"}));
    }
}
