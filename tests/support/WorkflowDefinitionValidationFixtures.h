/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 27 October 2026
*/

#ifndef TIVIDY_TESTS_WORKFLOW_DEFINITION_VALIDATION_FIXTURES_H
#define TIVIDY_TESTS_WORKFLOW_DEFINITION_VALIDATION_FIXTURES_H

// Structural item fixtures for the definition tests' dedicated unit-test binary.
// The Makefile links the real definition core, validator and supporting value objects
// directly, without libtividy.a, so these item definitions cannot conflict with
// production implementations. Lifecycle calls deliberately fail: the definition
// owns its template without binding or executing it. Full integration tests should
// use real item collaborators when their implementations are available.

#include <memory>
#include <utility>
#include "engine/core/Errors.h"
#include "engine/definition/WorkflowDefinition.h"
#include "engine/items/StageItem.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemVisitor.h"

tividy::WorkItem::~WorkItem() {}
tividy::TimePoint tividy::WorkItem::assignedAt() const { return TimePoint(); }
tividy::TimePoint tividy::WorkItem::dueAt() const { return TimePoint(); }
std::vector<tividy::WorkItem*> tividy::WorkItem::children() { return {}; }
tividy::WorkItem& tividy::WorkItem::add(std::unique_ptr<WorkItem>) { throw UnsupportedOperationError("not a stage"); }
std::unique_ptr<tividy::WorkItemMemento> tividy::WorkItem::createMemento() const { return {}; }
void tividy::WorkItem::restore(const WorkItemMemento&) {}
bool tividy::WorkItem::isReversible() const { return true; }
tividy::WorkItemMemento::~WorkItemMemento() {}
tividy::WorkItemIterator::~WorkItemIterator() {}
tividy::WorkItemVisitor::~WorkItemVisitor() {}
tividy::AssignmentStrategy::~AssignmentStrategy() {}
tividy::RoutingStrategy::~RoutingStrategy() {}

#define NO_LIFECYCLE(type) \
	void tividy::type::publish() { throw EngineError("unexpected lifecycle call"); } \
	void tividy::type::assign(const Id&) { throw EngineError("unexpected lifecycle call"); } \
	void tividy::type::start(const Participant&) { throw EngineError("unexpected lifecycle call"); } \
	void tividy::type::submit(const Participant&, const Outcome&) { throw EngineError("unexpected lifecycle call"); } \
	void tividy::type::complete(const Participant&, const Outcome&) { throw EngineError("unexpected lifecycle call"); } \
	void tividy::type::approve(const Participant&, const Outcome&) { throw EngineError("unexpected lifecycle call"); } \
	void tividy::type::reject(const Participant&, const std::string&) { throw EngineError("unexpected lifecycle call"); } \
	void tividy::type::escalate(const std::string&) { throw EngineError("unexpected lifecycle call"); } \
	void tividy::type::cancel(const std::string&) { throw EngineError("unexpected lifecycle call"); } \
	void tividy::type::skip(const std::string&) { throw EngineError("unexpected lifecycle call"); } \
	void tividy::type::expire(const std::string&, const std::string&) { throw EngineError("unexpected lifecycle call"); } \
	void tividy::type::bind(WorkContext&) { throw EngineError("unexpected bind"); }

tividy::StageItem::StageItem(const WorkItemSpec& spec) : spec_ref(spec) {}
tividy::StageItem::~StageItem() {}
const tividy::WorkItemSpec& tividy::StageItem::spec() const { return spec_ref; }
tividy::ItemStatus tividy::StageItem::status() const { return ItemStatus::Created; }
const tividy::Id& tividy::StageItem::assignee() const { static const Id empty; return empty; }
std::vector<tividy::WorkItem*> tividy::StageItem::children() {
	std::vector<WorkItem*> result; for (auto& child : children_ref) result.push_back(child.get()); return result;
}
tividy::WorkItem& tividy::StageItem::add(std::unique_ptr<WorkItem> child) {
	children_ref.push_back(std::move(child)); return *children_ref.back();
}
tividy::WorkItem* tividy::StageItem::find(const Id& id) {
	if (spec_ref.id() == id) return this;
	for (auto& child : children_ref) if (WorkItem* found = child->find(id)) return found;
	return nullptr;
}
void tividy::StageItem::accept(WorkItemVisitor& visitor) const {
	visitor.visitStage(*this); for (const auto& child : children_ref) child->accept(visitor);
}
std::unique_ptr<tividy::WorkItem> tividy::StageItem::clone() const { throw EngineError("unexpected clone"); }
std::unique_ptr<tividy::WorkItemIterator> tividy::StageItem::createIterator() { return {}; }
NO_LIFECYCLE(StageItem)

tividy::TaskItem::TaskItem(const WorkItemSpec& spec)
	: spec_ref(spec), state(nullptr), context_ref(nullptr), rejections_ref(0),
	  failures_ref(0), escalations_ref(0), irreversible_performed(false) {}
tividy::TaskItem::TaskItem(const TaskItem& other) : TaskItem(other.spec_ref) {}
tividy::TaskItem::~TaskItem() {}
const tividy::WorkItemSpec& tividy::TaskItem::spec() const { return spec_ref; }
tividy::ItemStatus tividy::TaskItem::status() const { return ItemStatus::Created; }
const tividy::Id& tividy::TaskItem::assignee() const { return assignee_ref; }
tividy::TimePoint tividy::TaskItem::assignedAt() const { return assigned_at; }
tividy::TimePoint tividy::TaskItem::dueAt() const { return TimePoint(); }
tividy::WorkItem* tividy::TaskItem::find(const Id& id) { return id == spec_ref.id() ? this : nullptr; }
void tividy::TaskItem::accept(WorkItemVisitor& visitor) const { visitor.visitTask(*this); }
std::unique_ptr<tividy::WorkItemIterator> tividy::TaskItem::createIterator() { return {}; }
std::unique_ptr<tividy::WorkItemMemento> tividy::TaskItem::createMemento() const { return {}; }
void tividy::TaskItem::restore(const WorkItemMemento&) {}
bool tividy::TaskItem::isReversible() const { return true; }
void tividy::TaskItem::validateOutcome(const Outcome&) const {}
void tividy::TaskItem::onCompleted(const Outcome&) {}
bool tividy::TaskItem::performsIrreversibleWork() const { return false; }
NO_LIFECYCLE(TaskItem)
tividy::ManualTask::ManualTask(const WorkItemSpec& spec) : CloneableTask<ManualTask>(spec) {}
void tividy::ManualTask::performWork(const Participant&, Outcome&) {}

class ValidationTestAssignment : public tividy::AssignmentStrategy {
public:
	tividy::Id chooseParticipant(const tividy::WorkItem&, const std::string&,
		const tividy::ParticipantDirectory&, const tividy::WorkflowData&) const override { return ""; }
};
class ValidationTestRouting : public tividy::RoutingStrategy {
public:
	tividy::RoutingDecision decide(const tividy::WorkItem&, const tividy::Outcome&,
		const tividy::WorkflowData&) const override { return {}; }
};

// A wrapper preserves the identity and traversal of its wrapped stage or task.
class ValidationTestWrapper : public tividy::WorkItem {
public:
	explicit ValidationTestWrapper(std::unique_ptr<WorkItem> value) : inner(std::move(value)) {}
	const tividy::WorkItemSpec& spec() const override { return inner->spec(); }
	tividy::ItemStatus status() const override { return inner->status(); }
	const tividy::Id& assignee() const override { return inner->assignee(); }
	std::vector<WorkItem*> children() override { return inner->children(); }
	WorkItem* find(const tividy::Id& id) override { return id == spec().id() ? this : inner->find(id); }
	void accept(tividy::WorkItemVisitor& visitor) const override { inner->accept(visitor); }
	std::unique_ptr<WorkItem> clone() const override { throw tividy::EngineError("unexpected clone"); }
	std::unique_ptr<tividy::WorkItemIterator> createIterator() override { return {}; }
	void publish() override { throw tividy::EngineError("unexpected publish"); }
	void assign(const tividy::Id&) override { throw tividy::EngineError("unexpected assign"); }
	void start(const tividy::Participant&) override { throw tividy::EngineError("unexpected start"); }
	void submit(const tividy::Participant&, const tividy::Outcome&) override { throw tividy::EngineError("unexpected submit"); }
	void complete(const tividy::Participant&, const tividy::Outcome&) override { throw tividy::EngineError("unexpected complete"); }
	void approve(const tividy::Participant&, const tividy::Outcome&) override { throw tividy::EngineError("unexpected approve"); }
	void reject(const tividy::Participant&, const std::string&) override { throw tividy::EngineError("unexpected reject"); }
	void escalate(const std::string&) override { throw tividy::EngineError("unexpected escalate"); }
	void cancel(const std::string&) override { throw tividy::EngineError("unexpected cancel"); }
	void skip(const std::string&) override { throw tividy::EngineError("unexpected skip"); }
	void expire(const std::string&, const std::string&) override { throw tividy::EngineError("unexpected expire"); }
	void bind(tividy::WorkContext&) override { throw tividy::EngineError("unexpected bind"); }
private:
	std::unique_ptr<WorkItem> inner;
};

static std::unique_ptr<tividy::WorkItem> task(const tividy::WorkItemSpec& spec) {
	return std::unique_ptr<tividy::WorkItem>(new tividy::ManualTask(spec));
}
static std::unique_ptr<tividy::StageItem> stage(const std::string& id = "root") {
	return std::unique_ptr<tividy::StageItem>(new tividy::StageItem(tividy::WorkItemSpec(id, id)));
}
static tividy::DefinitionPolicies policies() {
	tividy::DefinitionPolicies value; value.assignment.reset(new ValidationTestAssignment); return value;
}
static std::unique_ptr<tividy::WorkflowDefinition> definition(std::unique_ptr<tividy::WorkItem> root,
	const tividy::DefinitionPolicies& value = policies()) {
	return std::unique_ptr<tividy::WorkflowDefinition>(new tividy::WorkflowDefinition("test", 1, "test", std::move(root), value));
}

#undef NO_LIFECYCLE

#endif
