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

#ifndef TIVIDY_TESTS_TASK_ITEM_WORK_FIXTURES_H
#define TIVIDY_TESTS_TASK_ITEM_WORK_FIXTURES_H

// Unit-test collaborators for the actual TaskItemWork.cpp implementation.
// The dedicated Makefile target excludes libtividy.a so these fixtures cannot
// collide with TaskItem, WorkContext, memento or concrete-state implementations.
// State operations forward to rejecting defaults: transitions are outside this
// test target. The helper functions expose the documented execution boundary.
// These isolated fixtures are also checked separately against the available
// production WorkContext, WorkItem, iterator and memento implementations.

#include <chrono>
#include <memory>
#include <utility>
#include "engine/core/Errors.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkContext.h"
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
tividy::WorkItemIterator::~WorkItemIterator() {}
tividy::WorkItemVisitor::~WorkItemVisitor() {}

tividy::WorkItemMemento::WorkItemMemento()
	: status(ItemStatus::Created), assignee(), assigned_at(), rejections(0), failures(0),
	  escalations(0), last_outcome_kind(OutcomeKind::Accepted) {}
tividy::WorkItemMemento::~WorkItemMemento() {}

tividy::WorkContext::WorkContext(const Id& id, WorkflowData& data, EventPublisher& events,
	const Clock& clock, ConnectorRegistry& connectors, ParticipantDirectory& directory)
	: instance_id(id), data_ref(data), events_ref(events), clock_ref(clock),
	  connectors_ref(connectors), directory_ref(directory), irreversible(0) {}
const tividy::Id& tividy::WorkContext::instanceId() const { return instance_id; }
tividy::WorkflowData& tividy::WorkContext::data() { return data_ref; }
tividy::EventPublisher& tividy::WorkContext::events() { return events_ref; }
const tividy::Clock& tividy::WorkContext::clock() const { return clock_ref; }
tividy::ConnectorRegistry& tividy::WorkContext::connectors() { return connectors_ref; }
tividy::ParticipantDirectory& tividy::WorkContext::directory() { return directory_ref; }
void tividy::WorkContext::markIrreversible() { ++irreversible; }
unsigned tividy::WorkContext::irreversibleCount() const { return irreversible; }

tividy::TaskItem::TaskItem(const WorkItemSpec& spec)
	: spec_ref(spec), state(&CreatedState::instance()), context_ref(nullptr),
	  rejections_ref(0), failures_ref(0), escalations_ref(0), irreversible_performed(false) {}
tividy::TaskItem::TaskItem(const TaskItem& prototype) : TaskItem(prototype.spec_ref) {}
tividy::TaskItem::~TaskItem() {}
const tividy::WorkItemSpec& tividy::TaskItem::spec() const { return spec_ref; }
tividy::ItemStatus tividy::TaskItem::status() const { return state->status(); }
const tividy::Id& tividy::TaskItem::assignee() const { return assignee_ref; }
tividy::TimePoint tividy::TaskItem::assignedAt() const { return assigned_at; }
tividy::TimePoint tividy::TaskItem::dueAt() const
{
	if (assigned_at == TimePoint() || spec_ref.dueAfterSeconds() == 0) return TimePoint();
	return assigned_at + std::chrono::seconds(spec_ref.dueAfterSeconds());
}
tividy::WorkItem* tividy::TaskItem::find(const Id& id) { return id == spec_ref.id() ? this : nullptr; }
void tividy::TaskItem::accept(WorkItemVisitor& visitor) const { visitor.visitTask(*this); }
std::unique_ptr<tividy::WorkItemIterator> tividy::TaskItem::createIterator() { return {}; }
void tividy::TaskItem::bind(WorkContext& context)
{
	if (context_ref && context_ref != &context) throw InvalidStateError("already bound");
	context_ref = &context;
}
tividy::WorkContext& tividy::TaskItem::context() const
{
	if (!context_ref) throw InvalidStateError("task is unbound");
	return *context_ref;
}
const tividy::Outcome& tividy::TaskItem::lastOutcome() const { return last_outcome; }
const tividy::Id& tividy::TaskItem::completedBy() const { return completed_by; }
unsigned tividy::TaskItem::rejections() const { return rejections_ref; }
unsigned tividy::TaskItem::failures() const { return failures_ref; }
unsigned tividy::TaskItem::escalations() const { return escalations_ref; }

void tividy::TaskItem::publish() { state->publish(*this); }
void tividy::TaskItem::assign(const Id& id) { state->assign(*this, id); }
void tividy::TaskItem::start(const Participant& actor) { state->start(*this, actor); }
void tividy::TaskItem::submit(const Participant& actor, const Outcome& outcome) { state->submit(*this, actor, outcome); }
void tividy::TaskItem::complete(const Participant& actor, const Outcome& outcome) { state->complete(*this, actor, outcome); }
void tividy::TaskItem::approve(const Participant& actor, const Outcome& outcome) { state->approve(*this, actor, outcome); }
void tividy::TaskItem::reject(const Participant& actor, const std::string& reason) { state->reject(*this, actor, reason); }
void tividy::TaskItem::escalate(const std::string& reason) { state->escalate(*this, reason); }
void tividy::TaskItem::cancel(const std::string& reason) { state->cancel(*this, reason); }
void tividy::TaskItem::skip(const std::string& reason) { state->skip(*this, reason); }
void tividy::TaskItem::expire(const std::string& code, const std::string& reason) { state->expire(*this, code, reason); }

bool tividy::WorkItemState::runWork(TaskItem& item, const Participant& actor, Outcome& outcome)
{
	return item.executeWork(actor, outcome);
}
void tividy::WorkItemState::moveTo(TaskItem& item, const WorkItemState& next) { item.state = &next; }
void tividy::WorkItemState::setAssignee(TaskItem& item, const Id& id)
{
	item.assignee_ref = id;
	if (item.status() == ItemStatus::Assigned) item.assigned_at = item.context().clock().now();
}
void tividy::WorkItemState::recordResult(TaskItem& item, const Participant& actor, const Outcome& outcome)
{
	item.last_outcome = outcome;
	item.completed_by = actor.id();
}
unsigned tividy::WorkItemState::bumpRejections(TaskItem& item) { return ++item.rejections_ref; }
unsigned tividy::WorkItemState::bumpEscalations(TaskItem& item) { return ++item.escalations_ref; }

#define TASK_WORK_TEST_STATE(type, status_value) \
	const tividy::type& tividy::type::instance() { static const type value; return value; } \
	tividy::ItemStatus tividy::type::status() const { return ItemStatus::status_value; }

TASK_WORK_TEST_STATE(CreatedState, Created)
TASK_WORK_TEST_STATE(AvailableState, Available)
TASK_WORK_TEST_STATE(AssignedState, Assigned)
TASK_WORK_TEST_STATE(InProgressState, InProgress)
TASK_WORK_TEST_STATE(UnderReviewState, UnderReview)
TASK_WORK_TEST_STATE(EscalatedState, Escalated)
TASK_WORK_TEST_STATE(CompletedState, Completed)
TASK_WORK_TEST_STATE(CancelledState, Cancelled)
TASK_WORK_TEST_STATE(SkippedState, Skipped)

#define TASK_WORK_TEST_FORWARD(type, operation, parameters, arguments) \
	void tividy::type::operation parameters const { WorkItemState::operation arguments; }

TASK_WORK_TEST_FORWARD(CreatedState, publish, (TaskItem& item), (item))
TASK_WORK_TEST_FORWARD(CreatedState, cancel, (TaskItem& item, const std::string& reason), (item, reason))
TASK_WORK_TEST_FORWARD(CreatedState, skip, (TaskItem& item, const std::string& reason), (item, reason))
TASK_WORK_TEST_FORWARD(AvailableState, assign, (TaskItem& item, const Id& id), (item, id))
TASK_WORK_TEST_FORWARD(AvailableState, cancel, (TaskItem& item, const std::string& reason), (item, reason))
TASK_WORK_TEST_FORWARD(AvailableState, skip, (TaskItem& item, const std::string& reason), (item, reason))
TASK_WORK_TEST_FORWARD(AssignedState, start, (TaskItem& item, const Participant& actor), (item, actor))
TASK_WORK_TEST_FORWARD(AssignedState, escalate, (TaskItem& item, const std::string& reason), (item, reason))
TASK_WORK_TEST_FORWARD(AssignedState, cancel, (TaskItem& item, const std::string& reason), (item, reason))
TASK_WORK_TEST_FORWARD(AssignedState, expire, (TaskItem& item, const std::string& code, const std::string& reason), (item, code, reason))
TASK_WORK_TEST_FORWARD(InProgressState, complete, (TaskItem& item, const Participant& actor, const Outcome& outcome), (item, actor, outcome))
TASK_WORK_TEST_FORWARD(InProgressState, submit, (TaskItem& item, const Participant& actor, const Outcome& outcome), (item, actor, outcome))
TASK_WORK_TEST_FORWARD(InProgressState, escalate, (TaskItem& item, const std::string& reason), (item, reason))
TASK_WORK_TEST_FORWARD(InProgressState, cancel, (TaskItem& item, const std::string& reason), (item, reason))
TASK_WORK_TEST_FORWARD(InProgressState, expire, (TaskItem& item, const std::string& code, const std::string& reason), (item, code, reason))
TASK_WORK_TEST_FORWARD(UnderReviewState, approve, (TaskItem& item, const Participant& actor, const Outcome& outcome), (item, actor, outcome))
TASK_WORK_TEST_FORWARD(UnderReviewState, reject, (TaskItem& item, const Participant& actor, const std::string& reason), (item, actor, reason))
TASK_WORK_TEST_FORWARD(UnderReviewState, escalate, (TaskItem& item, const std::string& reason), (item, reason))
TASK_WORK_TEST_FORWARD(UnderReviewState, cancel, (TaskItem& item, const std::string& reason), (item, reason))
TASK_WORK_TEST_FORWARD(UnderReviewState, expire, (TaskItem& item, const std::string& code, const std::string& reason), (item, code, reason))
TASK_WORK_TEST_FORWARD(EscalatedState, assign, (TaskItem& item, const Id& id), (item, id))
TASK_WORK_TEST_FORWARD(EscalatedState, approve, (TaskItem& item, const Participant& actor, const Outcome& outcome), (item, actor, outcome))
TASK_WORK_TEST_FORWARD(EscalatedState, cancel, (TaskItem& item, const std::string& reason), (item, reason))

#undef TASK_WORK_TEST_FORWARD
#undef TASK_WORK_TEST_STATE

#endif
