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
 * @file TaskItem.cpp
 * @brief Implementation of the Composite Leaf, State Context, and Memento Originator.
 */

#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemState.h"
#include "engine/items/WorkContext.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemVisitor.h"
#include "engine/core/Errors.h"
#include <chrono>

namespace tividy
{
	TaskItem::TaskItem(const WorkItemSpec& spec)
		: spec_ref(spec), 
		  state(&CreatedState::instance()), 
		  context_ref(nullptr), 
		  assigned_at(), 
		  rejections_ref(0), 
		  failures_ref(0), 
		  escalations_ref(0), 
		  irreversible_performed(false),
		  barrier_recorded(false),
		  last_outcome() 
	{
	}

	TaskItem::TaskItem(const TaskItem& prototype)
		: spec_ref(prototype.spec_ref), 
		  state(&CreatedState::instance()), 
		  context_ref(nullptr), 
		  assigned_at(), 
		  rejections_ref(0), 
		  failures_ref(0), 
		  escalations_ref(0), 
		  irreversible_performed(false),
		  barrier_recorded(false),
		  last_outcome() 
	{
	}

	TaskItem::~TaskItem(){};

	const WorkItemSpec& TaskItem::spec() const { return spec_ref; }
	ItemStatus TaskItem::status() const { return state->status(); }
	const Id& TaskItem::assignee() const { return assignee_ref; }
	TimePoint TaskItem::assignedAt() const { return assigned_at; }
	
	TimePoint TaskItem::dueAt() const 
	{
		if (assigned_at == TimePoint() || spec_ref.dueAfterSeconds() == 0) return TimePoint();
		return assigned_at + std::chrono::seconds(spec_ref.dueAfterSeconds());
	}

	WorkItem* TaskItem::find(const Id& id) 
	{
		if (spec_ref.id() == id) return this;
		return nullptr;
	}

	void TaskItem::publish() { state->publish(*this); }
	void TaskItem::assign(const Id& participant_id) { state->assign(*this, participant_id); }
	void TaskItem::start(const Participant& actor) { state->start(*this, actor); }
	void TaskItem::submit(const Participant& actor, const Outcome& outcome) { state->submit(*this, actor, outcome); }
	void TaskItem::complete(const Participant& actor, const Outcome& outcome) { state->complete(*this, actor, outcome); }
	void TaskItem::approve(const Participant& reviewer, const Outcome& outcome) { state->approve(*this, reviewer, outcome); }
	void TaskItem::reject(const Participant& reviewer, const std::string& reason) { state->reject(*this, reviewer, reason); }
	void TaskItem::escalate(const std::string& reason) { state->escalate(*this, reason); }
	void TaskItem::cancel(const std::string& reason) { state->cancel(*this, reason); }
	void TaskItem::skip(const std::string& reason) { state->skip(*this, reason); }
	void TaskItem::expire(const std::string& outcome_code, const std::string& reason) { state->expire(*this, outcome_code, reason); }

	void TaskItem::accept(WorkItemVisitor& visitor) const 
	{ 
		visitor.visitTask(*this); 
	}

	std::unique_ptr<WorkItemIterator> TaskItem::createIterator() 
	{ 
		return std::unique_ptr<WorkItemIterator>(new DepthFirstIterator(*this)); 
	}

	void TaskItem::bind(WorkContext& context) 
	{
		if (context_ref != nullptr && context_ref != &context) 
		{
			throw InvalidStateError("TaskItem is already bound to a different context");
		}
		context_ref = &context;
	}

	WorkContext& TaskItem::context() const 
	{
		if (context_ref == nullptr) 
		{
			throw InvalidStateError("TaskItem is not bound to a context");
		}
		return *context_ref;
	}

	const Outcome& TaskItem::lastOutcome() const { return last_outcome; }
	const Id& TaskItem::completedBy() const { return completed_by; }
	unsigned TaskItem::rejections() const { return rejections_ref; }
	unsigned TaskItem::failures() const { return failures_ref; }
	unsigned TaskItem::escalations() const { return escalations_ref; }

	ManualTask::ManualTask(const WorkItemSpec& spec) : CloneableTask<ManualTask>(spec) {}
	void ManualTask::performWork(const Participant& actor, Outcome& outcome) { (void)actor; (void)outcome; }

} // namespace tividy