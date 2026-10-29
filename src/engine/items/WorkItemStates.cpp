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

#ifndef TIVIDY_ITEMS_WORKITEMSTATE_CPP
#define TIVIDY_ITEMS_WORKITEMSTATE_CPP

#include "engine/items/WorkItemState.h"
#include "engine/items/TaskItem.h"
#include "engine/core/Errors.h"
#include "engine/core/Types.h"
#include <string>

namespace tividy
{
    namespace 
    {
        const Participant& system_actor()
        {
            static const Participant p("system", "System", std::set<std::string>());
            return p;
        }
    } // anonymous namespace
    

    void WorkItemState::publish(TaskItem& /*item*/) const
    {
        std::string msg = "publish() is not allowed while " + toString(status());
        throw InvalidStateError(msg);
    }

    void WorkItemState::assign(TaskItem& /*item*/, const Id& /*participant_id*/) const
    {
        std::string msg = "assign() is not allowed while " + toString(status());
        throw InvalidStateError(msg);       
    }

    void WorkItemState::start(TaskItem& /*item*/, const Participant& /*actor*/) const
    {
        std::string msg = "start() is not allowed while " + toString(status());
        throw InvalidStateError(msg); 
    }

    void WorkItemState::submit(TaskItem& /*item*/, const Participant& /*actor*/, const Outcome& /*outcome*/) const
    {
        std::string msg = "submit() is not allowed while " + toString(status());
        throw InvalidStateError(msg);        
    }

    void WorkItemState::complete(TaskItem& /*item*/, const Participant& /*actor*/, const Outcome& /*outcome*/) const
    {
        std::string msg = "complete() is not allowed while " + toString(status());
        throw InvalidStateError(msg);        
    }

    void WorkItemState::approve(TaskItem& /*item*/, const Participant& /*reviewer*/, const Outcome& /*outcome*/) const
    {
        std::string msg = "approve() is not allowed while " + toString(status());
        throw InvalidStateError(msg);         
    }

    void WorkItemState::reject(TaskItem& /*item*/, const Participant& /*reviewer*/, const std::string& /*reason*/) const
    {
        std::string msg = "reject() is not allowed while " + toString(status());
        throw InvalidStateError(msg);    
    }

    void WorkItemState::escalate(TaskItem& /*item*/, const std::string& /*reason*/) const
    {
        std::string msg = "escalate() is not allowed while " + toString(status());
        throw InvalidStateError(msg);   
    }

    void WorkItemState::cancel(TaskItem& /*item*/, const std::string& /*reason*/) const
    {
        std::string msg = "cancel() is not allowed while " + toString(status());
        throw InvalidStateError(msg);   
    }

    void WorkItemState::skip(TaskItem& /*item*/, const std::string& /*reason*/) const
    {
        std::string msg = "skip() is not allowed while " + toString(status());
        throw InvalidStateError(msg);   
    }

    void WorkItemState::expire(TaskItem& /*item*/, const std::string& /*outcome_code*/, const std::string& /*reason*/) const
    {
        std::string msg = "expire() is not allowed while " + toString(status());
        throw InvalidStateError(msg);   
    }
    
    WorkItemState::~WorkItemState() = default;


    //CreatedState implementation
    const CreatedState& CreatedState::instance()
    {
        static CreatedState instance;
        return instance;
    }

    ItemStatus CreatedState::status() const
    {
        return ItemStatus::Created;
    }

    void CreatedState::publish(TaskItem& item) const
    {
        moveTo(item, AvailableState::instance());
        emit(item, EventType::ItemAvailable, "", Outcome(), "Item published and now available.");
    }

    void CreatedState::cancel(TaskItem& item, const std::string& reason) const
    {
        moveTo(item, CancelledState::instance());
        emit(item, EventType::ItemCancelled, "", Outcome::rejected("CANCELLED", reason), "Item cancelled.");
    }

    void CreatedState::skip(TaskItem& item, const std::string& reason) const
    {
        moveTo(item, SkippedState::instance());
        emit(item, EventType::ItemSkipped, "", Outcome::rejected("SKIPPED", reason), "Item skipped.");
    }

    //End of CreatedState implementation


    //AvailableState implementation
    const AvailableState& AvailableState::instance()
    {
        static AvailableState instance;
        return instance;
    }

    ItemStatus AvailableState::status() const
    {
        return ItemStatus::Available;
    }   

    void AvailableState::assign(TaskItem& item, const Id& participant_id) const
    {
        setAssignee(item, participant_id);
        moveTo(item, AssignedState::instance());
        emit(item, EventType::ItemAssigned, participant_id, Outcome(), "Item assigned to participant.");
    }

    void AvailableState::cancel(TaskItem& item, const std::string& reason) const
    {
        moveTo(item, CancelledState::instance());
        emit(item, EventType::ItemCancelled, "", Outcome::rejected("CANCELLED", reason), "Item cancelled.");
    }

    void AvailableState::skip(TaskItem& item, const std::string& reason) const
    {
        moveTo(item, SkippedState::instance());
        emit(item, EventType::ItemSkipped, "", Outcome::rejected("SKIPPED", reason), "Item skipped.");
    }

    //End of AvailableState implementation


    //AssignedState implementation

    const AssignedState& AssignedState::instance()
    {
        static AssignedState instance;
        return instance;
    }

    ItemStatus AssignedState::status() const
    {
        return ItemStatus::Assigned;
    }

    void AssignedState::start(TaskItem& item, const Participant& actor) const
    {
        if (actor.id() != item.assignee())
        {
            throw PermissionDeniedError("start() is only allowed by the assigned participant.");
        }
        moveTo(item, InProgressState::instance());
        emit(item, EventType::ItemStarted, actor.id(), Outcome(), "Item started by assigned participant.");
    }

    void AssignedState::escalate(TaskItem& item, const std::string& reason) const
    {
        moveTo(item, EscalatedState::instance());
        emit(item, EventType::ItemEscalated, "", Outcome::escalated("ESCALATED", reason), "Item escalated.");
    }

    void AssignedState::cancel(TaskItem& item, const std::string& reason) const
    {
        moveTo(item, CancelledState::instance());
        emit(item, EventType::ItemCancelled, "", Outcome::rejected("CANCELLED", reason), "Item cancelled.");
    }

    void AssignedState::expire(TaskItem& item, const std::string& outcome_code, const std::string& reason) const
    {
        recordResult(item, system_actor() ,Outcome::rejected(outcome_code, reason));
        moveTo(item, CompletedState::instance());
        emit(item, EventType::ItemCompleted, "", Outcome::rejected(outcome_code, reason), "Item expired and completed.");
    }

    //End of AssignedState implementation


    //InProgressState implementation

    const InProgressState& InProgressState::instance()
    {
        static InProgressState instance;
        return instance;
    }

    ItemStatus InProgressState::status() const
    {
        return ItemStatus::InProgress;
    }

    void InProgressState::complete(TaskItem& item, const Participant& actor, const Outcome& outcome) const
    {
        if (item.spec().requiresReview())
        {
            throw InvalidStateError("use submit(): this item requires review");
        }

        if (actor.id() != item.assignee())
        {
            throw PermissionDeniedError("complete() is only allowed by the assigned participant.");
        }

        Outcome edit = outcome;

        if (!runWork(item, actor, edit))
        {
            return; // left in InProgress
        }

        recordResult(item, actor, edit);
        moveTo(item, CompletedState::instance());
        emit(item, EventType::ItemCompleted, actor.id(), edit, "Item completed by assigned participant.");
    }

    void InProgressState::submit(TaskItem& item, const Participant& actor, const Outcome& outcome) const
    {
        if (!item.spec().requiresReview())
        {
            throw InvalidStateError("use complete(): this item has no review step");
        }

        if (actor.id() != item.assignee())
        {
            throw PermissionDeniedError("submit() is only allowed by the assigned participant.");
        }

        moveTo(item, UnderReviewState::instance());
        emit(item, EventType::ItemSubmitted, actor.id(), outcome, "Item submitted for review by assigned participant.");
    }

    void InProgressState::escalate(TaskItem& item, const std::string& reason) const
    {
        moveTo(item, EscalatedState::instance());
        emit(item, EventType::ItemEscalated, "", Outcome::escalated("ESCALATED", reason), "Item escalated.");
    }

    void InProgressState::cancel(TaskItem& item, const std::string& reason) const
    {
        moveTo(item, CancelledState::instance());
        emit(item, EventType::ItemCancelled, "", Outcome::rejected("CANCELLED", reason), "Item cancelled.");
    }

    void InProgressState::expire(TaskItem& item, const std::string& outcome_code, const std::string& reason) const
    {
        recordResult(item, system_actor() ,Outcome::rejected(outcome_code, reason));
        moveTo(item, CompletedState::instance());
        emit(item, EventType::ItemCompleted, "", Outcome::rejected(outcome_code, reason), "Item expired and completed.");
    }

    //End of InProgressState implementation


    //UnderReviewState implementation

    const UnderReviewState& UnderReviewState::instance()
    {
        static UnderReviewState instance;
        return instance;
    }

    ItemStatus UnderReviewState::status() const
    {
        return ItemStatus::UnderReview;
    }

    void UnderReviewState::approve(TaskItem& item, const Participant& reviewer, const Outcome& outcome) const
    {
        recordResult(item, reviewer, outcome);
        moveTo(item, CompletedState::instance());
        emit(item, EventType::ItemCompleted, reviewer.id(), outcome, "Item approved and completed by reviewer.");
    }

    void UnderReviewState::reject(TaskItem& item, const Participant& reviewer, const std::string& reason) const
    {
        unsigned rejectCount = bumpRejections(item);
        if (rejectCount < item.spec().maxRetries())
        {
            moveTo(item, InProgressState::instance());
            emit(item, EventType::ItemRejected, reviewer.id(), Outcome::rejected("REJECTED", reason), "Item rejected by reviewer. Returning to InProgress.");
        }
        else
        {
            moveTo(item, CancelledState::instance());
            emit(item, EventType::ItemCancelled, reviewer.id(), Outcome::rejected("CANCELLED", reason), "Item rejected by reviewer. Max retries exceeded, item cancelled.");
        }
    }

    void UnderReviewState::escalate(TaskItem& item, const std::string& reason) const
    {
        moveTo(item, EscalatedState::instance());
        emit(item, EventType::ItemEscalated, "", Outcome::escalated("ESCALATED", reason), "Item escalated.");
    }
    
    void UnderReviewState::cancel(TaskItem& item, const std::string& reason) const
    {
        moveTo(item, CancelledState::instance());
        emit(item, EventType::ItemCancelled, "", Outcome::rejected("CANCELLED", reason), "Item cancelled.");
    }

    void UnderReviewState::expire(TaskItem& item, const std::string& outcome_code, const std::string& reason) const
    {
        recordResult(item, system_actor() ,Outcome::rejected(outcome_code, reason));
        moveTo(item, CompletedState::instance());
        emit(item, EventType::ItemCompleted, "", Outcome::rejected(outcome_code, reason), "Item expired and completed.");
    }

    //End of UnderReviewState implementation


    //EscalatedState implementation
    const EscalatedState& EscalatedState::instance()
    {
        static EscalatedState instance;
        return instance;
    }

    ItemStatus EscalatedState::status() const
    {
        return ItemStatus::Escalated;
    }

    void EscalatedState::assign(TaskItem& item, const Id& participant_id) const
    {
        setAssignee(item, participant_id);
        emit(item, EventType::ItemAssigned, participant_id, Outcome(), "Escalation approver assigned to item.");
    }

    void EscalatedState::approve(TaskItem& item, const Participant& reviewer, const Outcome& outcome) const
    {
        Outcome edit = outcome;
        
        if (!runWork(item, reviewer, edit))
        {
            return; // left in Escalated
        }
        recordResult(item, reviewer, edit);
        moveTo(item, CompletedState::instance());
        emit(item, EventType::ItemCompleted, reviewer.id(), edit, "Item approved and completed by escalation approver.");
    }

    void EscalatedState::cancel(TaskItem& item, const std::string& reason) const
    {
        moveTo(item, CancelledState::instance());
        emit(item, EventType::ItemCancelled, "", Outcome::rejected("CANCELLED", reason), "Item cancelled.");
    }

    //End of EscalatedState implementation


    //CompletedState implementation

    const CompletedState& CompletedState::instance()
    {
        static CompletedState instance;
        return instance;
    }

    ItemStatus CompletedState::status() const
    {
        return ItemStatus::Completed;
    }

    //End of CompletedState implementation


    //CancelledState implementation

    const CancelledState& CancelledState::instance()
    {
        static CancelledState instance;
        return instance;
    }

    ItemStatus CancelledState::status() const
    {
        return ItemStatus::Cancelled;
    }

    //End of CancelledState implmentation


    //SkippedState implementation

    const SkippedState& SkippedState::instance()
    {
        static SkippedState instance;
        return instance;
    }

    ItemStatus SkippedState::status() const
    {
        return ItemStatus::Skipped;
    }

    //End of SkippedState implementation

}

#endif // TIVIDY_ITEMS_WORKITEMSTATE_CPP
