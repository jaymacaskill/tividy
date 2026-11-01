#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "engine/commands/Command.h"

namespace tividy
{
namespace
{
    // Implements the abstract methods so Command can be tested.
    class TestCommand : public Command
    {
    public:
        CommandResult execute() override
        {
            return CommandResult::ok();
        }

        CommandResult undo() override
        {
            return CommandResult::ok();
        }

        bool isReversible() const override
        {
            return true;
        }

        std::string describe() const override
        {
            return "Test command";
        }
    };

    class DestructionTrackingCommand : public TestCommand
    {
    public:
        explicit DestructionTrackingCommand(bool& destroyed)
            : destroyed_(destroyed) {}

        ~DestructionTrackingCommand() override
        {
            destroyed_ = true;
        }

    private:
        bool& destroyed_;
    };
}

TEST(CommandResultTest, OkDefaultsToEmptyMessage)
{
    const CommandResult result = CommandResult::ok();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.message.empty());
}

TEST(CommandResultTest, OkPreservesMessage)
{
    const CommandResult result =
        CommandResult::ok("Command completed");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "Command completed");
}

TEST(CommandResultTest, FailurePreservesMessage)
{
    const CommandResult result =
        CommandResult::failure("Command failed");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.message, "Command failed");
}

TEST(CommandResultTest, FailureAcceptsEmptyMessage)
{
    const CommandResult result = CommandResult::failure("");

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.message.empty());
}

TEST(CommandTest, DoesNotSupportRetryByDefault)
{
    TestCommand concrete;
    const Command& command = concrete;

    EXPECT_FALSE(command.supportsRetry());
}

TEST(CommandTest, DeletingThroughBasePointerCallsDerivedDestructor)
{
    bool destroyed = false;

    {
        std::unique_ptr<Command> command(
            new DestructionTrackingCommand(destroyed));

        EXPECT_FALSE(destroyed);
    }

    EXPECT_TRUE(destroyed);
}

} // namespace tividy