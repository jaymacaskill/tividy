#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/definition/WorkflowLibrary.h"
#include "support/WorkflowDefinitionValidationFixtures.h"

namespace
{
using namespace tividy;

    std::unique_ptr<WorkflowDefinition> makeDefinition(
        const std::string& name,
        unsigned version = 1)
    {
        return std::unique_ptr<WorkflowDefinition>(
            new WorkflowDefinition(
                name,
                version,
                "Library test",
                task(WorkItemSpec("task", "Task")),
                policies()));
    }

    TEST(WorkflowLibrary, StartsEmpty)
    {
        WorkflowLibrary library;

        EXPECT_TRUE(library.names().empty());
    }

    TEST(WorkflowLibrary, StoresDefinitionAndPackKey)
    {
        WorkflowLibrary library;
        auto value = makeDefinition("Approval", 3);
        const auto* original = value.get();

        ASSERT_NO_THROW(library.add("banking", std::move(value)));

        EXPECT_FALSE(value);

        const auto stored = library.get("Approval");
        ASSERT_TRUE(stored);
        EXPECT_EQ(original, stored.get());
        EXPECT_EQ("Approval", stored->name());
        EXPECT_EQ(3u, stored->version());
        EXPECT_EQ("banking", library.packKeyOf("Approval"));
        EXPECT_EQ(stored, library.get("Approval"));
    }

    TEST(WorkflowLibrary, RejectsNullDefinition)
    {
        WorkflowLibrary library;

        EXPECT_THROW(
            library.add("banking", nullptr),
            InvalidDefinitionError);

        EXPECT_TRUE(library.names().empty());
    }

    TEST(WorkflowLibrary, RejectsInvalidDefinition)
    {
        WorkflowLibrary library;

        // No assignment policy: construction succeeds, validation fails.
        auto invalid = std::unique_ptr<WorkflowDefinition>(
            new WorkflowDefinition(
                "Invalid",
                1,
                "Missing assignment policy",
                task(WorkItemSpec("task", "Task")),
                DefinitionPolicies()));

        EXPECT_THROW(
            library.add("banking", std::move(invalid)),
            InvalidDefinitionError);

        EXPECT_TRUE(library.names().empty());
        EXPECT_THROW(library.get("Invalid"), NotFoundError);
    }

    TEST(WorkflowLibrary, DuplicateNamePreservesOriginalEntry)
    {
        WorkflowLibrary library;
        library.add("banking", makeDefinition("Approval", 1));
        const auto original = library.get("Approval");

        // A different version and pack still cannot reuse the name.
        EXPECT_THROW(
            library.add("telecom", makeDefinition("Approval", 2)),
            InvalidDefinitionError);

        EXPECT_EQ(original, library.get("Approval"));
        EXPECT_EQ(1u, library.get("Approval")->version());
        EXPECT_EQ("banking", library.packKeyOf("Approval"));
        EXPECT_EQ(1u, library.names().size());
    }

    TEST(WorkflowLibrary, MissingNamesThrow)
    {
        WorkflowLibrary library;

        EXPECT_THROW(library.get("Missing"), NotFoundError);
        EXPECT_THROW(library.packKeyOf("Missing"), NotFoundError);

        library.add("banking", makeDefinition("Approval"));

        EXPECT_THROW(library.get("Missing"), NotFoundError);
        EXPECT_THROW(library.packKeyOf("Missing"), NotFoundError);
    }

    TEST(WorkflowLibrary, ReturnsNamesInSortedOrder)
    {
        WorkflowLibrary library;
        library.add("pack", makeDefinition("Z"));
        library.add("pack", makeDefinition("A"));
        library.add("pack", makeDefinition("M"));

        const std::vector<std::string> expected{
            "A", "M", "Z"
        };

        EXPECT_EQ(expected, library.names());
    }

    TEST(WorkflowLibrary, RetrievedDefinitionOutlivesLibrary)
    {
        std::shared_ptr<const WorkflowDefinition> stored;

        {
            WorkflowLibrary library;
            library.add("banking", makeDefinition("Approval"));
            stored = library.get("Approval");
        }

        ASSERT_TRUE(stored);
        EXPECT_EQ("Approval", stored->name());
        EXPECT_NO_THROW(stored->validate());
    }
}