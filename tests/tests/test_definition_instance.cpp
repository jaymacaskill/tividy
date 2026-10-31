/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 31 October 2026
*/

#include <gtest/gtest.h>
#include "engine/definition/WorkflowDefinition.h"
#include "engine/items/TaskItem.h"
#include "engine/core/Errors.h"
// #include "engine/runtime/WorkflowInstance.h" // Uncomment when Ruan merges

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

TEST(WorkflowDefinitionInstanceTest, ValidDefinitionPassesValidation) {
    DefinitionPolicies good_policies;
    // Assuming RoleBasedAssignment has a default constructor
    good_policies.assignment = std::make_shared<RoleBasedAssignment>(); 
    
    WorkItemSpec spec("task1", "Task 1");
    auto root = std::unique_ptr<WorkItem>(new DummyTask(spec));
    
    WorkflowDefinition def("TestWorkflow", 1, "Desc", std::move(root), good_policies);
    
    EXPECT_NO_THROW(def.validate());
}

TEST(WorkflowDefinitionInstanceTest, CreateInstanceClonesTemplate) {
    DefinitionPolicies good_policies;
    good_policies.assignment = std::make_shared<RoleBasedAssignment>();
    
    WorkItemSpec spec("task1", "Task 1");
    auto root = std::unique_ptr<WorkItem>(new DummyTask(spec));
    WorkItem* original_root_ptr = root.get();
    
    WorkflowDefinition def("TestWorkflow", 1, "Desc", std::move(root), good_policies);
    
    // --- THIS PART IS COMMENTED OUT UNTIL RUAN MERGES WorkflowInstance.cpp ---
    /*
    Services dummy_services{...}; // Needs Clock, Directory, Connectors
    WorkflowData data;
    
    auto instance = def.createInstance("inst1", data, dummy_services);
    
    EXPECT_NE(instance, nullptr);
    EXPECT_EQ(instance->status(), InstanceStatus::Created);
    
    // Check Prototype: the instance root should be a DIFFERENT memory address than the definition root
    EXPECT_NE(&instance->root(), original_root_ptr);
    
    // Check Prototype: mutating the clone shouldn't affect the original
    instance->root().cancel("test");
    EXPECT_EQ(original_root_ptr->status(), ItemStatus::Created);
    */
    
    // For now, just assert the definition itself survived the move
    EXPECT_EQ(def.name(), "TestWorkflow");
}