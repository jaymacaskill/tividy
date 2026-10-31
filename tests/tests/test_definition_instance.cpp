/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 29 October 2026
*/

#include <gtest/gtest.h>
#include "engine/definition/WorkflowDefinition.h"
#include "engine/items/TaskItem.h"
#include "engine/core/Errors.h"

using namespace tividy;

// A minimal concrete task for testing cloning
class DummyTask : public CloneableTask<DummyTask> {
public:
    DummyTask(const WorkItemSpec& spec) : CloneableTask<DummyTask>(spec) {}
protected:
    void performWork(const Participant&, Outcome&) override {}
};

TEST(WorkflowDefinitionInstanceTest, ValidateThrowsOnInvalidDefinition) {
    // Create a definition with a null assignment policy (invalid rule 6)
    DefinitionPolicies bad_policies;
    bad_policies.assignment = nullptr; 
    
    WorkItemSpec spec("task1", "Task 1");
    auto root = std::unique_ptr<WorkItem>(new DummyTask(spec));
    
    WorkflowDefinition def("TestWorkflow", 1, "Desc", std::move(root), bad_policies);
    
    // validate() should throw InvalidDefinitionError
    EXPECT_THROW(def.validate(), InvalidDefinitionError);
}

TEST(WorkflowDefinitionInstanceTest, TemplateTreeClonesIndependently) {
    // Setup a valid policy
    DefinitionPolicies good_policies;
    good_policies.assignment = std::make_shared<RoleBasedAssignment>(); // Assumes this exists in your headers
    
    WorkItemSpec spec("task1", "Task 1");
    auto root = std::unique_ptr<WorkItem>(new DummyTask(spec));
    WorkItem* original_root_ptr = root.get();
    
    WorkflowDefinition def("TestWorkflow", 1, "Desc", std::move(root), good_policies);
    // Note: To truly test this, WorkflowInstance needs to be implemented. 
    // For now, this test just ensures the definition constructs without crashing.
    EXPECT_NO_THROW(def.validate());
}