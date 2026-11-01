#include <gtest/gtest.h>

#include <chrono>
#include <memory>

#include "engine/core/Errors.h"
#include "engine/facade/Tividy.h"

namespace tividy
{
    namespace
    {
        class FacadeTest : public ::testing::Test
        {
        protected:
            FacadeTest()
                : engine(std::unique_ptr<ManualClock>(
                      new ManualClock(TimePoint())))
            {
            }

            Tividy engine;
        };
    }

    TEST_F(FacadeTest, StartsWithEmptyRegistries)
    {
        EXPECT_TRUE(engine.packKeys().empty());
        EXPECT_TRUE(engine.workflowNames().empty());
        EXPECT_TRUE(engine.instanceIds().empty());
    }

    TEST_F(FacadeTest, UsesProvidedClock)
    {
        EXPECT_EQ(engine.now(), TimePoint());
    }

    TEST_F(FacadeTest, AdvanceTimeMovesClock)
    {
        // No instances means no missed deadlines.
        EXPECT_EQ(engine.advanceTime(60), 0u);
        EXPECT_EQ(engine.now(),
                  TimePoint() + std::chrono::seconds(60));
    }

    TEST_F(FacadeTest, ZeroAdvanceKeepsTimeUnchanged)
    {
        EXPECT_EQ(engine.advanceTime(0), 0u);
        EXPECT_EQ(engine.now(), TimePoint());
    }

    TEST_F(FacadeTest, NegativeAdvanceThrows)
    {
        EXPECT_THROW(engine.advanceTime(-1), EngineError);
        EXPECT_EQ(engine.now(), TimePoint());
    }

    TEST_F(FacadeTest, UnknownLookupsThrowNotFound)
    {
        EXPECT_THROW(engine.profile("missing"), NotFoundError);
        EXPECT_THROW(engine.packOf("missing"), NotFoundError);
        EXPECT_THROW(engine.instance("missing"), NotFoundError);
        EXPECT_THROW(engine.history("missing"), NotFoundError);
        EXPECT_THROW(engine.progress("missing"), NotFoundError);
        EXPECT_THROW(engine.signal("missing", "cancel"), NotFoundError);

        const Tividy& readOnly = engine;
        EXPECT_THROW(readOnly.instance("missing"), NotFoundError);
    }

    TEST_F(FacadeTest, StartingUnknownWorkflowThrows)
    {
        WorkflowData data;

        EXPECT_THROW(
            engine.startWorkflow("missing", data),
            NotFoundError);

        EXPECT_TRUE(engine.instanceIds().empty());
    }

    TEST_F(FacadeTest, PerformOnUnknownInstanceReturnsFailure)
    {
        CommandResult result =
            engine.perform("complete", "missing", "task", "actor");

        EXPECT_FALSE(result.success);
    }

    TEST_F(FacadeTest, UndoOnUnknownInstanceReturnsFailure)
    {
        EXPECT_FALSE(engine.undo("missing").success);
    }
}