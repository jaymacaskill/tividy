#ifndef TIVIDY_TESTS_FACADE_STARTUP_FIXTURES_H
#define TIVIDY_TESTS_FACADE_STARTUP_FIXTURES_H

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
CommandFactory::CommandFactory() {}
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
DeadlineMonitor::DeadlineMonitor(WorkflowInstance& owner, const Clock& time) : instance(owner), clock(time) {}
ActionHistory::ActionHistory(WorkflowInstance& owner) : instance(owner) {}
ActionHistory::~ActionHistory() = default;
WorkflowInstance::WorkflowInstance(const Id& id, const std::string& name, unsigned version,
    const WorkflowData& data, std::unique_ptr<WorkItem> root, const DefinitionPolicies& policies, Services& services)
    : id_ref(id), definition_name(name), definition_version(version), status_ref(InstanceStatus::Created),
      data_ref(data), audit_observer(audit_ref), root_ref(std::move(root)),
      context_ref(id, data_ref, events_ref, services.clock, services.connectors, services.directory),
      flow(*this, policies), deadlines(*this, services.clock), actions_ref(*this) {
    root_ref->bind(context_ref);
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
} // namespace tividy
#endif
