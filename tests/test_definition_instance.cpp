#include <gtest/gtest.h>
#include "engine/definition/WorkflowDefinition.h"
#include "engine/core/Errors.h"
#include "engine/policies/AssignmentStrategy.h"
#include "engine/items/WorkItemIterator.h" 
#include "engine/items/WorkItemVisitor.h"  
#include <memory>

using namespace tividy;

// A completely fake WorkItem that doesn't rely on TaskItem or Visitor
class MockWorkItem : public WorkItem {
public:
    MockWorkItem(const WorkItemSpec& s) : spec_ref(s) {}
    
    // Implement all pure virtuals just so it links
    const WorkItemSpec& spec() const override { return spec_ref; }
    ItemStatus status() const override { return ItemStatus::Created; }
    const Id& assignee() const override { return empty_id; }
    std::vector<WorkItem*> children() override { return {}; }
    WorkItem* find(const Id&) override { return nullptr; }
    void publish() override {}
    void assign(const Id&) override {}
    void start(const Participant&) override {}
    void submit(const Participant&, const Outcome&) override {}
    void complete(const Participant&, const Outcome&) override {}
    void approve(const Participant&, const Outcome&) override {}
    void reject(const Participant&, const std::string&) override {}
    void escalate(const std::string&) override {}
    void cancel(const std::string&) override {}
    void skip(const std::string&) override {}
    void expire(const std::string&, const std::string&) override {}
    std::unique_ptr<WorkItem> clone() const override { return std::unique_ptr<WorkItem>(new MockWorkItem(spec_ref)); }
    void accept(WorkItemVisitor&) const override {}
    std::unique_ptr<WorkItemIterator> createIterator() override { return nullptr; }
    void bind(WorkContext&) override {}

private:
    WorkItemSpec spec_ref;
    Id empty_id;
};

TEST(WorkflowDefinitionInstanceTest, ValidateThrowsOnInvalidDefinition) {
    DefinitionPolicies bad_policies;
    bad_policies.assignment = nullptr; 
    
    WorkItemSpec spec("task1", "Task 1");
    auto root = std::unique_ptr<WorkItem>(new MockWorkItem(spec));
    
    WorkflowDefinition def("TestWorkflow", 1, "Desc", std::move(root), bad_policies);
    
    EXPECT_THROW(def.validate(), InvalidDefinitionError);
}

TEST(WorkflowDefinitionInstanceTest, ValidDefinitionPassesValidation) {
    DefinitionPolicies good_policies;
    good_policies.assignment = std::make_shared<RoleBasedAssignment>(); 
    
    WorkItemSpec spec("task1", "Task 1");
    auto root = std::unique_ptr<WorkItem>(new MockWorkItem(spec));
    
    WorkflowDefinition def("TestWorkflow", 1, "Desc", std::move(root), good_policies);
    
    EXPECT_NO_THROW(def.validate());
}