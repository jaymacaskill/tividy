#ifndef TIVIDY_TESTS_FACADE_ACTIONS_FIXTURES_H
#define TIVIDY_TESTS_FACADE_ACTIONS_FIXTURES_H

// Link-time substitutes ONLY for collaborators absent from main. This executable
// links Tividy.cpp and the real builder, validator, StageItem, events and services
// explicitly, never libtividy.a. These seams test facade orchestration, NOT the
// unpublished library/prototype/runtime implementations or task transitions.
// No raw storage, private-access macros or partially constructed objects.
#include <utility>
#include "engine/facade/Tividy.h"
#include "engine/core/Errors.h"
#include "engine/items/WorkItemVisitor.h"

namespace tividy {
WorkItemVisitor::~WorkItemVisitor() = default;
void WorkflowLibrary::add(const std::string& key, std::unique_ptr<WorkflowDefinition> definition) {
    const std::string name = definition->name();
    if (entries.count(name)) throw InvalidDefinitionError("duplicate workflow");
    Entry entry;
    entry.pack_key = key;
    entry.definition = std::shared_ptr<const WorkflowDefinition>(std::move(definition));
    entries.emplace(name, std::move(entry));
}
std::shared_ptr<const WorkflowDefinition> WorkflowLibrary::get(const std::string& name) const {
    auto it = entries.find(name);
    if (it == entries.end()) throw NotFoundError("unknown workflow");
    return it->second.definition;
}
std::string WorkflowLibrary::packKeyOf(const std::string& name) const {
    get(name);
    return entries.at(name).pack_key;
}
std::vector<std::string> WorkflowLibrary::names() const {
    std::vector<std::string> result;
    for (const auto& entry : entries) result.push_back(entry.first);
    return result;
}
WorkContext::WorkContext(const Id& id, WorkflowData& data, EventPublisher& events,
    const Clock& clock, ConnectorRegistry& connectors, ParticipantDirectory& directory)
    : instance_id(id), data_ref(data), events_ref(events), clock_ref(clock),
      connectors_ref(connectors), directory_ref(directory), irreversible(0) {}
FlowController::FlowController(WorkflowInstance& owner, const DefinitionPolicies& config)
    : instance(owner), policies(config), draining(false) {}
FlowController::~FlowController() = default;
void FlowController::onEvent(const WorkflowEvent&) {}
WorkflowInstance::WorkflowInstance(const Id& id, const std::string& name, unsigned version,
    const WorkflowData& data, std::unique_ptr<WorkItem> root, const DefinitionPolicies& policies, Services& services)
    : id_ref(id), definition_name(name), definition_version(version), status_ref(InstanceStatus::Created),
      data_ref(data), audit_observer(audit_ref), root_ref(std::move(root)),
      context_ref(id, data_ref, events_ref, services.clock, services.connectors, services.directory),
      flow(*this, policies), deadlines(*this, services.clock), actions_ref(*this) {
    root_ref->bind(context_ref);
    events_ref.attach(&audit_observer);   // as the real constructor does: the audit log is fed from events
}
WorkflowInstance::~WorkflowInstance() = default;
const Id& WorkflowInstance::id() const { return id_ref; }
const std::string& WorkflowInstance::definitionName() const { return definition_name; }
unsigned WorkflowInstance::definitionVersion() const { return definition_version; }
InstanceStatus WorkflowInstance::status() const { return status_ref; }
WorkflowData& WorkflowInstance::data() { return data_ref; }
WorkItem& WorkflowInstance::root() { return *root_ref; }
WorkContext& WorkflowInstance::context() { return context_ref; }
EventPublisher& WorkflowInstance::events() { return events_ref; }
ParticipantDirectory& WorkContext::directory() { return directory_ref; }
ConnectorRegistry& WorkContext::connectors() { return connectors_ref; }
void WorkflowInstance::addObserver(std::unique_ptr<EventObserver> observer) {
    events_ref.attach(observer.get());
    extra_observers.push_back(std::move(observer));
}
void WorkflowInstance::start() {
    status_ref = InstanceStatus::Running;
    events_ref.notify(WorkflowEvent(EventType::InstanceStarted, id_ref, "", TimePoint()));
}
std::unique_ptr<WorkflowInstance> WorkflowDefinition::createInstance(const Id& id,
    const WorkflowData& data, Services& services) const {
    return std::unique_ptr<WorkflowInstance>(new WorkflowInstance(id, name_ref, version_ref,
        data, template_root->clone(), policies_ref, services));
}

// ---- Additions for the Tividy run-time operations (TividyActions.cpp). Everything below is a link-time substitute
// for runtime classes that are not in this checkout (WorkflowInstance queries, DeadlineMonitor, CommandFactory,
// the ProgressVisitor counters). The real ActionHistory IS linked, so commands run through the real invoker.
DeadlineMonitor::DeadlineMonitor(WorkflowInstance& owner, const Clock& time) : instance(owner), clock(time) {}
std::size_t DeadlineMonitor::sweep() {
    std::size_t count = 0;
    for (WorkItem* child : instance.root().children())
        if (child->dueAt() != TimePoint() && clock.now() > child->dueAt() && reported.insert(child->spec().id()).second) {
            instance.events().notify(WorkflowEvent(EventType::DeadlineMissed, instance.id(), child->spec().id(), clock.now())
                                         .byActor("system").withMessage("deadline passed"));
            ++count;
        }
    return count;
}
const WorkItem& WorkflowInstance::root() const { return *root_ref; }
AuditLog& WorkflowInstance::audit() { return audit_ref; }
const AuditLog& WorkflowInstance::audit() const { return audit_ref; }
ActionHistory& WorkflowInstance::actions() { return actions_ref; }
std::size_t WorkflowInstance::sweepDeadlines() { return deadlines.sweep(); }
WorkItem* WorkflowInstance::findItem(const Id& id) { return root_ref->find(id); }
std::vector<WorkItem*> WorkflowInstance::itemsFor(const Id& participant_id) {
    std::vector<WorkItem*> result;
    for (WorkItem* child : root_ref->children()) {
        const ItemStatus s = child->status();
        if (child->assignee() == participant_id && (s == ItemStatus::Assigned || s == ItemStatus::InProgress ||
            s == ItemStatus::UnderReview || s == ItemStatus::Escalated)) result.push_back(child);
    }
    return result;
}
CommandFactory::CommandFactory() {}
void CommandFactory::registerVerb(const std::string& verb, Creator creator) { creators[verb] = creator; }
std::unique_ptr<Command> CommandFactory::create(const std::string& verb, const CommandRequest& request) const {
    const auto it = creators.find(verb);
    if (it == creators.end()) throw NotFoundError("unknown action");
    return it->second(request);
}
std::vector<std::string> CommandFactory::verbs() const {
    std::vector<std::string> result;
    for (const auto& entry : creators) result.push_back(entry.first);
    return result;
}
ProgressVisitor::ProgressVisitor() : total_ref(0) {}
void ProgressVisitor::visitTask(const TaskItem&) { ++total_ref; }  // TaskItem is not linked here; fakes never reach this
void ProgressVisitor::visitStage(const StageItem&) {}
std::size_t ProgressVisitor::total() const { return total_ref; }
std::size_t ProgressVisitor::count(ItemStatus s) const { const auto it = counts.find(s); return it == counts.end() ? 0 : it->second; }
double ProgressVisitor::percentSettled() const {
    if (total_ref == 0) return 100.0;
    return 100.0 * (count(ItemStatus::Completed) + count(ItemStatus::Skipped)) / total_ref;
}
const Clock& WorkContext::clock() const { return clock_ref; }
void WorkflowInstance::cancel(const std::string& reason) {
    status_ref = InstanceStatus::Cancelled;
    events_ref.notify(WorkflowEvent(EventType::InstanceCancelled, id_ref, "", TimePoint()).withMessage(reason));
}
} // namespace tividy
#endif