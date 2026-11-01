#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>

#include "engine/commands/MacroCommand.h"
#include "engine/core/Errors.h"

namespace tividy
{
    namespace
    {
        class FakeCommand : public Command
        {
        public:
            explicit FakeCommand(unsigned failures = 0)
                : failuresBeforeSuccess(failures) {}

            unsigned executeCalls = 0;
            unsigned undoCalls = 0;
            bool reversible = true;
            bool retryable = true;
            unsigned retryCutoff = 0;

            bool supportsRetry() const override
            {
                return retryable && (retryCutoff == 0 || executeCalls < retryCutoff);
            }

            CommandResult execute() override
            {
                ++executeCalls;

                if (executeCalls <= failuresBeforeSuccess)
                {
                    return CommandResult::failure(
                        "failure " + std::to_string(executeCalls));
                }

                return CommandResult::ok("done");
            }

            CommandResult undo() override
            {
                ++undoCalls;
                return CommandResult::ok("undone");
            }

            bool isReversible() const override { return reversible; }
            std::string describe() const override { return "fake"; }

        private:
            unsigned failuresBeforeSuccess;
        };
    }

    TEST(CommandDecoratorTest, RejectsNullInner)
    {
        EXPECT_THROW(CommandDecorator{nullptr}, ValidationError);
    }

    TEST(CommandDecoratorTest, ForwardsOperationsAndMetadata)
    {
        std::unique_ptr<FakeCommand> inner(new FakeCommand);
        FakeCommand* observed = inner.get();
        CommandDecorator command(std::move(inner));

        EXPECT_TRUE(command.isReversible());
        EXPECT_EQ(command.describe(), "fake");

        const CommandResult executed = command.execute();
        EXPECT_TRUE(executed.success);
        EXPECT_EQ(executed.message, "done");
        EXPECT_EQ(observed->executeCalls, 1u);

        observed->reversible = false;
        EXPECT_FALSE(command.isReversible());
        observed->reversible = true;

        const CommandResult undone = command.undo();
        EXPECT_TRUE(undone.success);
        EXPECT_EQ(undone.message, "undone");
        EXPECT_EQ(observed->undoCalls, 1u);
    }

    TEST(CommandDecoratorTest, ForwardsFailureWithoutRetrying)
    {
        std::unique_ptr<FakeCommand> inner(new FakeCommand(1));
        FakeCommand* observed = inner.get();
        CommandDecorator command(std::move(inner));

        const CommandResult result = command.execute();

        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.message, "failure 1");
        EXPECT_EQ(observed->executeCalls, 1u);
    }

    TEST(RetryingCommandTest, RejectsInvalidArguments)
    {
        EXPECT_THROW(RetryingCommand(nullptr, 3), ValidationError);
        EXPECT_THROW(
            RetryingCommand(
                std::unique_ptr<Command>(new FakeCommand), 0),
            ValidationError);
    }

    TEST(RetryingCommandTest, StopsOnImmediateSuccess)
    {
        std::unique_ptr<FakeCommand> inner(new FakeCommand);
        FakeCommand* observed = inner.get();
        RetryingCommand command(std::move(inner), 5);

        const CommandResult result = command.execute();

        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.message, "done");
        EXPECT_EQ(observed->executeCalls, 1u);
        EXPECT_EQ(command.describe(), "fake (retry up to 5)");
    }

    TEST(RetryingCommandTest, StopsAfterFirstSuccessfulAttempt)
    {
        std::unique_ptr<FakeCommand> inner(new FakeCommand(2));
        FakeCommand* observed = inner.get();
        RetryingCommand command(std::move(inner), 5);

        const CommandResult result = command.execute();

        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.message, "done");
        EXPECT_EQ(observed->executeCalls, 3u);
    }

    TEST(RetryingCommandTest, CanSucceedOnFinalAllowedAttempt)
    {
        std::unique_ptr<FakeCommand> inner(new FakeCommand(2));
        FakeCommand* observed = inner.get();
        RetryingCommand command(std::move(inner), 3);

        const CommandResult result = command.execute();

        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.message, "done");
        EXPECT_EQ(observed->executeCalls, 3u);
    }

    TEST(RetryingCommandTest, ReturnsLastFailureAtAttemptLimit)
    {
        std::unique_ptr<FakeCommand> inner(new FakeCommand(4));
        FakeCommand* observed = inner.get();
        RetryingCommand command(std::move(inner), 3);

        const CommandResult result = command.execute();

        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.message, "failure 3 (after 3 attempts)");
        EXPECT_EQ(observed->executeCalls, 3u);
    }

    TEST(RetryingCommandTest, OneAttemptDoesNotRetry)
    {
        std::unique_ptr<FakeCommand> inner(new FakeCommand(1));
        FakeCommand* observed = inner.get();
        RetryingCommand command(std::move(inner), 1);

        const CommandResult result = command.execute();

        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.message, "failure 1 (after 1 attempts)");
        EXPECT_EQ(observed->executeCalls, 1u);
    }

    TEST(CommandDecoratorTest, ForwardsRetryCapability)
    {
        std::unique_ptr<FakeCommand> inner(new FakeCommand);
        FakeCommand* observed = inner.get();
        CommandDecorator command(std::move(inner));
        const Command& decorated = command;
    
        EXPECT_TRUE(decorated.supportsRetry());
    
        observed->retryable = false;
        EXPECT_FALSE(decorated.supportsRetry());
    
        observed->retryable = true;
        EXPECT_TRUE(decorated.supportsRetry());
    }
    
    TEST(RetryingCommandTest, StopsWhenChildDisallowsRetry)
    {
        std::unique_ptr<FakeCommand> inner(new FakeCommand(1));
        FakeCommand* observed = inner.get();
        observed->retryable = false;
        RetryingCommand command(std::move(inner), 5);
    
        const CommandResult result = command.execute();
    
        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.message, "failure 1 (after 1 attempts)");
        EXPECT_EQ(observed->executeCalls, 1u);
    }
    
    TEST(RetryingCommandTest, RechecksCapabilityAfterEachFailure)
    {
        std::unique_ptr<FakeCommand> inner(new FakeCommand(3));
        FakeCommand* observed = inner.get();
        observed->retryCutoff = 2;
        RetryingCommand command(std::move(inner), 5);
    
        const CommandResult result = command.execute();
    
        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.message, "failure 2 (after 2 attempts)");
        EXPECT_EQ(observed->executeCalls, 2u);
    }
}