#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "engine/commands/CommandFactory.h"
#include "engine/commands/WorkItemCommands.h"
#include "engine/core/Clock.h"
#include "engine/core/Errors.h"
#include "engine/items/TaskItem.h"
#include "engine/policies/AssignmentStrategy.h"

namespace tividy
{
namespace
{
    class CustomCommand : public Command
    {
    public:
        explicit CustomCommand(const std::string& label)
            : label(label) {}

        CommandResult execute() override { return CommandResult::ok(); }
        CommandResult undo() override { return CommandResult::ok(); }
        bool isReversible() const override { return true; }
        std::string describe() const override { return label; }

    private:
        std::string label;
    };

    class CommandFactoryTest : public ::testing::Test
    {
    protected:
        ManualClock clock{TimePoint()};
        ParticipantDirectory directory;
        ConnectorRegistry connectors;
        Services services{clock, directory, connectors};
        Participant actor{"actor-1", "Tester", {"worker"}};
        std::unique_ptr<WorkflowInstance> instance;
        CommandFactory factory;

        void SetUp() override
        {
            DefinitionPolicies policies;
            policies.assignment = std::make_shared<RoleBasedAssignment>();

            instance.reset(new WorkflowInstance(
                "instance-1", "Factory test", 1, WorkflowData(),
                std::unique_ptr<WorkItem>(
                    new ManualTask(WorkItemSpec("task-1", "Task"))),
                policies, services));
        }

        CommandRequest request()
        {
            return CommandRequest{
                *instance, "task-1", actor,
                Outcome::accepted("DONE"), "Test reason"
            };
        }

        template <class ExpectedCommand>
        void expectCreated(const std::string& verb,
                           const std::string& description)
        {
            SCOPED_TRACE(verb);

            auto command = factory.create(verb, request());

            ASSERT_NE(command.get(), nullptr);
            EXPECT_NE(dynamic_cast<ExpectedCommand*>(command.get()), nullptr);
            EXPECT_EQ(command->describe(), description);
        }
    };

    TEST(CommandFactoryTest, CreatesAllSevenCommands)
    {
        expectCreated<StartCommand>("start", "Tester started 'Task'");
        expectCreated<SubmitCommand>("submit", "Tester submitted 'Task' [DONE]");
        expectCreated<CompleteCommand>("complete", "Tester completed 'Task' [DONE]");
        expectCreated<ApproveCommand>("approve", "Tester approved 'Task' [DONE]");
        expectCreated<RejectCommand>("reject", "Tester rejected 'Task': Test reason");
        expectCreated<EscalateCommand>("escalate", "Tester escalated 'Task': Test reason");
        expectCreated<CancelCommand>("cancel", "Tester cancelled 'Task': Test reason");
    }

    TEST(CommandFactoryTest, UnknownVerbsThrowNotFoundError)
    {
        const CommandRequest req = request();

        for (const char* verb : {"unknown", "", "START", " start "})
        {
            SCOPED_TRACE(verb);
            EXPECT_THROW(factory.create(verb, req), NotFoundError);
        }
    }

    TEST(CommandFactoryTest, UnknownVerbHasExpectedErrorMessage)
    {
        try
        {
            factory.create("unknown", request());
            FAIL() << "Expected NotFoundError";
        }
        catch (const NotFoundError& error)
        {
            EXPECT_STREQ(error.what(), "unknown action");
        }
    }

    TEST(CommandFactoryTest, RegistersCustomVerbAndForwardsRequest)
    {
        const CommandRequest req = request();
        const CommandRequest* received = nullptr;

        factory.registerVerb("custom", [&received](const CommandRequest& r) -> std::unique_ptr<Command> 
            {
                received = &r;
                return std::unique_ptr<Command>(new CustomCommand(r.text));
            });

        // Registration stores the creator; creation invokes it.
        EXPECT_EQ(received, nullptr);

        auto command = factory.create("custom", req);

        EXPECT_EQ(received, &req);
        ASSERT_NE(command.get(), nullptr);
        EXPECT_NE(dynamic_cast<CustomCommand*>(command.get()), nullptr);
        EXPECT_EQ(command->describe(), req.text);
    }

    TEST(CommandFactoryTest, RegistrationReplacesExistingVerb)
    {
        // Replace a built-in creator.
        factory.registerVerb("start", [](const CommandRequest&) -> std::unique_ptr<Command>
            {
                return std::unique_ptr<Command>(new CustomCommand("replacement"));
            });

        auto command = factory.create("start", request());

        ASSERT_NE(command.get(), nullptr);
        EXPECT_NE(dynamic_cast<CustomCommand*>(command.get()), nullptr);
        EXPECT_EQ(command->describe(), "replacement");

        // Replacing a verb must not add another entry.
        EXPECT_EQ(factory.verbs().size(), 7u);
    }

    TEST(CommandFactoryTest, ListsExactlySevenSortedBuiltInVerbs)
    {
        const std::vector<std::string> expected
        {
            "approve", 
            "cancel", 
            "complete", 
            "escalate",
            "reject", 
            "start", 
            "submit"
        };

        EXPECT_EQ(factory.verbs(), expected);
    }

    TEST(CommandFactoryTest, IncludesCustomVerbsInSortedOrder)
    {
        const CommandFactory::Creator creator = [](const CommandRequest&) -> std::unique_ptr<Command>
            {
                return std::unique_ptr<Command>(new CustomCommand("custom"));
            };

        // Register in reverse alphabetical order.
        factory.registerVerb("zzz", creator);
        factory.registerVerb("aaa", creator);

        const std::vector<std::string> expected
        {
            "aaa", 
            "approve", 
            "cancel", 
            "complete", 
            "escalate",
            "reject", 
            "start", 
            "submit", 
            "zzz"
        };

        EXPECT_EQ(factory.verbs(), expected);
    }
}
} // namespace tividy