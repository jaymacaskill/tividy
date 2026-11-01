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

/**
 * @file TaskItemWork.cpp
 * @brief Task execution template, runtime snapshots and reversibility tracking.
 */

#include <map>
#include <memory>

#include "engine/core/Errors.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkContext.h"

bool tividy::TaskItem::executeWork(const tividy::Participant& actor, tividy::Outcome& outcome)
{
	tividy::WorkContext& runtime = context();
	validateOutcome(outcome);
	barrier_recorded = false;
	try
	{
		performWork(actor, outcome);
	}
	catch (const tividy::ExternalFailure& error)
	{
		++failures_ref;
		tividy::WorkflowEvent event(tividy::EventType::ItemFailed, runtime.instanceId(),
			spec_ref.id(), runtime.clock().now());
		event.byActor(actor.id())
			.withOutcome(tividy::Outcome::failed(error.retryable() ? "RETRYABLE" : "PERMANENT", error.what()))
			.withMessage(error.what())
			.withCount(failures_ref);
		runtime.events().notify(event);
		return false;
	}

	if (performsIrreversibleWork())
	{
		markIrreversibleEffect();
	}
	onCompleted(outcome);
	return true;
}

void tividy::TaskItem::markIrreversibleEffect()
{
	if (barrier_recorded)
	{
		return;
	}
	context().markIrreversible();
	irreversible_performed = true;
	barrier_recorded = true;
}

std::unique_ptr<tividy::WorkItemMemento> tividy::TaskItem::createMemento() const
{
	std::unique_ptr<tividy::WorkItemMemento> snapshot(new tividy::WorkItemMemento);
	snapshot->status = status();
	snapshot->assignee = assignee_ref;
	snapshot->assigned_at = assigned_at;
	snapshot->rejections = rejections_ref;
	snapshot->failures = failures_ref;
	snapshot->escalations = escalations_ref;
	snapshot->last_outcome_kind = last_outcome.kind();
	snapshot->last_outcome_code = last_outcome.code();
	snapshot->last_outcome_note = last_outcome.note();
	snapshot->completed_by = completed_by;
	return snapshot;
}

void tividy::TaskItem::restore(const tividy::WorkItemMemento& snapshot)
{
	static const std::map<tividy::ItemStatus, const tividy::WorkItemState*> states = {
		{tividy::ItemStatus::Created, &tividy::CreatedState::instance()},
		{tividy::ItemStatus::Available, &tividy::AvailableState::instance()},
		{tividy::ItemStatus::Assigned, &tividy::AssignedState::instance()},
		{tividy::ItemStatus::InProgress, &tividy::InProgressState::instance()},
		{tividy::ItemStatus::UnderReview, &tividy::UnderReviewState::instance()},
		{tividy::ItemStatus::Escalated, &tividy::EscalatedState::instance()},
		{tividy::ItemStatus::Completed, &tividy::CompletedState::instance()},
		{tividy::ItemStatus::Cancelled, &tividy::CancelledState::instance()},
		{tividy::ItemStatus::Skipped, &tividy::SkippedState::instance()}
	};
	const tividy::WorkItemState* restored_state = states.at(snapshot.status);
	const tividy::Outcome restored_outcome(snapshot.last_outcome_kind,
		snapshot.last_outcome_code, snapshot.last_outcome_note);

	state = restored_state;
	assignee_ref = snapshot.assignee;
	assigned_at = snapshot.assigned_at;
	rejections_ref = snapshot.rejections;
	failures_ref = snapshot.failures;
	escalations_ref = snapshot.escalations;
	last_outcome = restored_outcome;
	completed_by = snapshot.completed_by;
	// Restoring runtime fields cannot undo effects already performed externally.
}

bool tividy::TaskItem::isReversible() const
{
	return !irreversible_performed;
}

void tividy::TaskItem::validateOutcome(const tividy::Outcome&) const
{
}

void tividy::TaskItem::onCompleted(const tividy::Outcome&)
{
}

bool tividy::TaskItem::performsIrreversibleWork() const
{
	return false;
}
