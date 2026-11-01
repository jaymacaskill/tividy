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
 
DeadlineMonitor.cpp
*/

#ifndef TIVIDY_RUNTIME_DEADLINEMONITOR_CPP
#define TIVIDY_RUNTIME_DEADLINEMONITOR_CPP
 
#include "engine/runtime/DeadlineMonitor.h"
 
#include <vector>
 
#include "engine/events/Observer.h"
#include "engine/events/WorkflowEvent.h"
#include "engine/items/WorkItem.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/runtime/WorkflowInstance.h"

namespace tividy
{
    DeadlineMonitor::DeadlineMonitor(WorkflowInstance& instance, const Clock& clock)
        : instance(instance), clock(clock), reported() {}   

    std::size_t DeadlineMonitor::sweep() {
        
        const TimePoint now = clock.now();
        std::size_t published = 0;
        
        DepthFirstIterator walk(instance.root());
        for (walk.first(); !walk.isDone(); walk.next()) {

            WorkItem& item = walk.currentItem();
            if (item.children().size()) {
                continue; // Skip composite items.
            }

            const Id id = item.spec().id();
            const ItemStatus status = item.status();
            if (status != ItemStatus::Assigned && status != ItemStatus::InProgress && status != ItemStatus::UnderReview) {
				reported.erase(id);
				continue;
			}
            
            const TimePoint due = item.dueAt();
            if (due == TimePoint() || now <= due || reported.count(id) > 0) {
                continue;
            }

            // remember the id before publishing, so a reaction that triggers another sweep cannot report this lapse twice
            reported.insert(id);
			instance.events().notify(WorkflowEvent(EventType::DeadlineMissed, instance.id(), id, now)
							 .byActor("system").withMessage("deadline passed"));
            ++published;

        }

        return published;
    }

} // namespace tividy

#endif // TIVIDY_RUNTIME_DEADLINEMONITOR_CPP
