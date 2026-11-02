/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 2 November 2026

FlowControllerHelpers.cpp
*/

/**
 * @file FlowControllerHelpers.cpp
 * @brief Implements FlowController's assignment and instance-evaluation helpers.
 * @details assignIfPossible() asks the assignment strategy for a participant
 * when an item becomes Available. evaluateInstance() decides whether the
 * workflow instance has finished.
 */

#include "engine/runtime/FlowController.h"
#include "engine/runtime/WorkflowInstance.h"

namespace tividy
{
    void FlowController::assignIfPossible(WorkItem &item)
    {
        if (item.status() != ItemStatus::Available ||
            !policies.assignment)
            return;

        const Id participant = policies.assignment->chooseParticipant(
            item,
            item.spec().requiredRole(),
            instance.context().directory(),
            instance.data());

        if (!participant.empty())
            item.assign(participant);
    }

    void FlowController::evaluateInstance()
    {
        const ItemStatus status = instance.root().status();

        if (status == ItemStatus::Completed)
            instance.finish(InstanceStatus::Completed,
                            "All workflow items settled");
        else if (status == ItemStatus::Cancelled)
            instance.finish(InstanceStatus::Failed,
                            "Workflow cancelled");
    }
}
