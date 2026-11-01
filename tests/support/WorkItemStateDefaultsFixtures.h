/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 1 November 2026
*/

#ifndef TIVIDY_TESTS_WORK_ITEM_STATE_DEFAULTS_FIXTURES_H
#define TIVIDY_TESTS_WORK_ITEM_STATE_DEFAULTS_FIXTURES_H

// Structural task fixtures for the dedicated state-defaults unit-test binary.
// Link only the real WorkItemState implementation and the listed value objects,
// without libtividy.a, while TaskItem's implementation is being integrated.
// Non-default field values make unintended mutations visible. Lifecycle calls
// deliberately fail: a default state must reject an operation before delegating.

#include <chrono>
#include <memory>
#include "engine/core/Errors.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemVisitor.h"

tividy::WorkItem::~WorkItem() {}
tividy::TimePoint tividy::WorkItem::assignedAt() const { return TimePoint(); }
tividy::TimePoint tividy::WorkItem::dueAt() const { return TimePoint(); }
std::vector<tividy::WorkItem*> tividy::WorkItem::children() { return {}; }
tividy::WorkItem& tividy::WorkItem::add(std::unique_ptr<WorkItem>)
{
	throw UnsupportedOperationError("not a stage");
}
std::unique_ptr<tividy::WorkItemMemento> tividy::WorkItem::createMemento() const { return {}; }
void tividy::WorkItem::restore(const WorkItemMemento&) {}
bool tividy::WorkItem::isReversible() const { return true; }
tividy::WorkItemMemento::~WorkItemMemento() {}
tividy::WorkItemIterator::~WorkItemIterator() {}
tividy::WorkItemVisitor::~WorkItemVisitor() {}

tividy::TaskItem::TaskItem(const WorkItemSpec& spec)
	: spec_ref(spec), state(nullptr), context_ref(nullptr), assignee_ref("initial-worker"),
	  assigned_at(TimePoint(std::chrono::seconds(42))), rejections_ref(2), failures_ref(3),
	  escalations_ref(4), irreversible_performed(false), barrier_recorded(false),
	  last_outcome(Outcome::rejected("INITIAL", "Original result")), completed_by("initial-reviewer") {}
tividy::TaskItem::~TaskItem() {}
const tividy::WorkItemSpec& tividy::TaskItem::spec() const { return spec_ref; }
tividy::ItemStatus tividy::TaskItem::status() const { return ItemStatus::Assigned; }
const tividy::Id& tividy::TaskItem::assignee() const { return assignee_ref; }
tividy::TimePoint tividy::TaskItem::assignedAt() const { return assigned_at; }
tividy::TimePoint tividy::TaskItem::dueAt() const
{
	return assigned_at + std::chrono::seconds(spec_ref.dueAfterSeconds());
}
tividy::WorkItem* tividy::TaskItem::find(const Id& id) { return id == spec_ref.id() ? this : nullptr; }
void tividy::TaskItem::accept(WorkItemVisitor& visitor) const { visitor.visitTask(*this); }
std::unique_ptr<tividy::WorkItemIterator> tividy::TaskItem::createIterator() { return {}; }
std::unique_ptr<tividy::WorkItemMemento> tividy::TaskItem::createMemento() const { return {}; }
void tividy::TaskItem::restore(const WorkItemMemento&) { throw EngineError("unexpected restore"); }
bool tividy::TaskItem::isReversible() const { return !irreversible_performed; }
void tividy::TaskItem::bind(WorkContext&) { throw EngineError("unexpected bind"); }
const tividy::Outcome& tividy::TaskItem::lastOutcome() const { return last_outcome; }
const tividy::Id& tividy::TaskItem::completedBy() const { return completed_by; }
unsigned tividy::TaskItem::rejections() const { return rejections_ref; }
unsigned tividy::TaskItem::failures() const { return failures_ref; }
unsigned tividy::TaskItem::escalations() const { return escalations_ref; }
void tividy::TaskItem::validateOutcome(const Outcome&) const { throw EngineError("unexpected validation"); }
void tividy::TaskItem::onCompleted(const Outcome&) { throw EngineError("unexpected completion hook"); }
bool tividy::TaskItem::performsIrreversibleWork() const { return false; }

void tividy::TaskItem::publish() { throw EngineError("unexpected lifecycle delegation"); }
void tividy::TaskItem::assign(const Id&) { throw EngineError("unexpected lifecycle delegation"); }
void tividy::TaskItem::start(const Participant&) { throw EngineError("unexpected lifecycle delegation"); }
void tividy::TaskItem::submit(const Participant&, const Outcome&) { throw EngineError("unexpected lifecycle delegation"); }
void tividy::TaskItem::complete(const Participant&, const Outcome&) { throw EngineError("unexpected lifecycle delegation"); }
void tividy::TaskItem::approve(const Participant&, const Outcome&) { throw EngineError("unexpected lifecycle delegation"); }
void tividy::TaskItem::reject(const Participant&, const std::string&) { throw EngineError("unexpected lifecycle delegation"); }
void tividy::TaskItem::escalate(const std::string&) { throw EngineError("unexpected lifecycle delegation"); }
void tividy::TaskItem::cancel(const std::string&) { throw EngineError("unexpected lifecycle delegation"); }
void tividy::TaskItem::skip(const std::string&) { throw EngineError("unexpected lifecycle delegation"); }
void tividy::TaskItem::expire(const std::string&, const std::string&) { throw EngineError("unexpected lifecycle delegation"); }

#endif
