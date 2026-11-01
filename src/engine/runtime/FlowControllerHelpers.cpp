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
