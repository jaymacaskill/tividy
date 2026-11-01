/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 01 November 2026

WorkItemDecorator.cpp
*/

#ifndef TIVIDY_ITEMS_WORKITEMDECORATOR_CPP
#define TIVIDY_ITEMS_WORKITEMDECORATOR_CPP

#include "engine/items/WorkItemDecorator.h"

#include <string>
#include <utility>

#include "engine/core/Errors.h"
#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemMemento.h"
#include "engine/items/WorkItemSpec.h"

namespace tividy
{
	WorkItemDecorator::WorkItemDecorator(std::unique_ptr<WorkItem> wrapped) : component_ref(std::move(wrapped)), context_ref(nullptr)
	{
		if (!component_ref) throw InvalidDefinitionError("a decorator needs a work item to wrap");
	}

	WorkItemDecorator::~WorkItemDecorator() { }

	const WorkItemSpec& WorkItemDecorator::spec() const
	{
		return component().spec();
	}

	ItemStatus WorkItemDecorator::status() const
	{
		return component().status();
	}

	const Id& WorkItemDecorator::assignee() const
	{
		return component().assignee();
	}

	TimePoint WorkItemDecorator::assignedAt() const
	{
		return component().assignedAt();
	}

	TimePoint WorkItemDecorator::dueAt() const
	{
		return component().dueAt();
	}

	std::vector<WorkItem*> WorkItemDecorator::children()
	{
		return component().children();
	}

	WorkItem& WorkItemDecorator::add(std::unique_ptr<WorkItem> child)
	{
		return component().add(std::move(child));
	}

	WorkItem* WorkItemDecorator::find(const Id& id)
	{
		WorkItem* hit = component().find(id);
		if (hit == &component()) return this;

		return hit;
	}

	void WorkItemDecorator::publish()
	{
		component().publish();
	}

	void WorkItemDecorator::assign(const Id& participant_id)
	{
		component().assign(participant_id);
	}

	void WorkItemDecorator::start(const Participant& actor)
	{
		component().start(actor);
	}

	void WorkItemDecorator::submit(const Participant& actor, const Outcome& outcome)
	{
		component().submit(actor, outcome);
	}

	void WorkItemDecorator::complete(const Participant& actor, const Outcome& outcome)
	{
		component().complete(actor, outcome);
	}

	void WorkItemDecorator::approve(const Participant& reviewer, const Outcome& outcome)
	{
		component().approve(reviewer, outcome);
	}

	void WorkItemDecorator::reject(const Participant& reviewer, const std::string& reason)
	{
		component().reject(reviewer, reason);
	}

	void WorkItemDecorator::escalate(const std::string& reason)
	{
		component().escalate(reason);
	}

	void WorkItemDecorator::cancel(const std::string& reason)
	{
		component().cancel(reason);
	}

	void WorkItemDecorator::skip(const std::string& reason)
	{
		component().skip(reason);
	}

	void WorkItemDecorator::expire(const std::string& outcome_code, const std::string& reason)
	{
		component().expire(outcome_code, reason);
	}

	void WorkItemDecorator::accept(WorkItemVisitor& visitor) const
	{
		component().accept(visitor);
	}

	std::unique_ptr<WorkItemIterator> WorkItemDecorator::createIterator()
	{
		return component().createIterator();
	}

	std::unique_ptr<WorkItemMemento> WorkItemDecorator::createMemento() const
	{
		return component().createMemento();
	}

	void WorkItemDecorator::restore(const WorkItemMemento& memento)
	{
		component().restore(memento);
	}

	bool WorkItemDecorator::isReversible() const
	{
		return component().isReversible();
	}

	void WorkItemDecorator::bind(WorkContext& bound)
	{
		// Remember the context only once the component has accepted it, so a refused bind leaves both on the earlier one.
		component().bind(bound);
		context_ref = &bound;
	}

	WorkItem& WorkItemDecorator::component()
	{
		return *component_ref;
	}

	const WorkItem& WorkItemDecorator::component() const
	{
		return *component_ref;
	}

	std::unique_ptr<WorkItem> WorkItemDecorator::cloneComponent() const
	{
		return component_ref->clone();
	}

	WorkContext& WorkItemDecorator::context() const
	{
		if (context_ref == 0) throw InvalidStateError("work item " + spec().id() + " is not bound to a work context");

		return *context_ref;
	}

	namespace
	{
		void requireRole(const Participant& actor, const std::string& role, const std::string& operation, const WorkItemSpec& spec)
		{
			if (role.empty() || actor.hasRole(role)) return;

			throw PermissionDeniedError(actor.id() + " may not " + operation + " '" + spec.name() + "' (requires role " + role + ")");
		}

		void requireReviewer(const WorkItem& item, const Participant& reviewer, const std::string& operation)
		{
			if (item.status() == ItemStatus::Escalated)
			{
				if (reviewer.id() != item.assignee())
					throw PermissionDeniedError(reviewer.id() + " may not " + operation + " '" + item.spec().name() +
												"' (only the assigned approver " + item.assignee() + " may decide an escalated item)");
				return;
			}

			requireRole(reviewer, item.spec().reviewerRole(), operation, item.spec());
		}
	}

	SecureWorkItem::SecureWorkItem(std::unique_ptr<WorkItem> wrapped) : WorkItemDecorator(std::move(wrapped)) { }

	void SecureWorkItem::start(const Participant& actor)
	{
		requireRole(actor, spec().requiredRole(), "start", spec());
		WorkItemDecorator::start(actor);
	}

	void SecureWorkItem::submit(const Participant& actor, const Outcome& outcome)
	{
		requireRole(actor, spec().requiredRole(), "submit", spec());
		WorkItemDecorator::submit(actor, outcome);
	}

	void SecureWorkItem::complete(const Participant& actor, const Outcome& outcome)
	{
		requireRole(actor, spec().requiredRole(), "complete", spec());
		WorkItemDecorator::complete(actor, outcome);
	}

	void SecureWorkItem::approve(const Participant& reviewer, const Outcome& outcome)
	{
		requireReviewer(*this, reviewer, "approve");
		WorkItemDecorator::approve(reviewer, outcome);
	}

	void SecureWorkItem::reject(const Participant& reviewer, const std::string& reason)
	{
		requireReviewer(*this, reviewer, "reject");
		WorkItemDecorator::reject(reviewer, reason);
	}

	std::unique_ptr<WorkItem> SecureWorkItem::clone() const
	{
		return std::unique_ptr<WorkItem>(new SecureWorkItem(cloneComponent()));
	}
} // namespace tividy

#endif // TIVIDY_ITEMS_WORKITEMDECORATOR_CPP