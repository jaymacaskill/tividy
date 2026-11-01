/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 28 October 2026
*/

#ifndef TIVIDY_ITEMS_WORKITEMSTATEHELPERS_CPP
#define TIVIDY_ITEMS_WORKITEMSTATEHELPERS_CPP

#include "engine/items/WorkItemState.h"
#include "engine/items/TaskItem.h"
#include "engine/core/Errors.h"
#include "engine/core/Types.h"
#include "engine/core/Outcome.h"
#include "engine/items/WorkContext.h"
#include "engine/core/Participant.h"
#include "engine/items/WorkItemSpec.h"
#include <string>

namespace tividy
{
    void WorkItemState::moveTo(TaskItem& item, const WorkItemState& next)
    {
        item.state = &next;
    }

    void WorkItemState::emit(TaskItem& item, EventType type, const Id& actor_id, const Outcome& outcome, 
        const std::string& message, unsigned count)
    {
        WorkflowEvent event(type, item.context().instanceId(), item.spec().id(), item.context().clock().now());
        event.byActor(actor_id.empty() ? "system" : actor_id).withOutcome(outcome).withMessage(message).withCount(count);
        item.context().events().notify(event);
    }

    void WorkItemState::setAssignee(TaskItem& item, const Id& participant_id)
    {
        item.assignee_ref = participant_id;
        if (item.assignedAt() == TimePoint())
        {
            item.assigned_at = item.context().clock().now();
        }
    }

    bool WorkItemState::runWork(TaskItem& item, const Participant& actor, Outcome& outcome)
    {
        // executeWork() is the Template Method (TaskItemWork.cpp): validate, perform, publish ItemFailed on a technical failure,
        // mark irreversible work, onCompleted. The states only call it, so that logic lives in exactly one place.
        return item.executeWork(actor, outcome);
    }

    void WorkItemState::recordResult(TaskItem& item, const Participant& actor, const Outcome& outcome)
    {
        recordOutcome(item, outcome);
        item.completed_by = actor.id();
    }

    void WorkItemState::recordOutcome(TaskItem& item, const Outcome& outcome)
    {
        item.last_outcome = outcome;
    }

    unsigned WorkItemState::bumpRejections(TaskItem& item)
    {
        ++item.rejections_ref;
        return item.rejections_ref;
    }

    unsigned WorkItemState::bumpEscalations(TaskItem& item)
    {
        ++item.escalations_ref;
        return item.escalations_ref;
    }

    unsigned WorkItemState::rejections(const TaskItem& item)
    {
        return item.rejections_ref;
    }

    void WorkItemState::requireAssignee(const TaskItem& item, const Participant& actor)
    {
        if (actor.id() != item.assignee())
        {
            throw PermissionDeniedError("Operation is only allowed by the assigned participant.");
        }
    }
}

#endif //TIVIDY_ITEMS_WORKITEMSTATEHELPERS_CPP