/** @file WorkContext.cpp
 * @brief Role C parameter-object support: non-owning collaborators and per-context bookkeeping.
 * @details Ownership follows the local team implementation plan's Phase 1 Role C allocation.
 * Referenced collaborators must outlive this context; no lifecycle/routing behavior is performed here.
 */
#include "engine/items/WorkContext.h"

namespace tividy
{
	/** @brief Copies the instance id and retains the supplied collaborator references. */
	WorkContext::WorkContext(const Id& instance_id, WorkflowData& data, EventPublisher& events,
		const Clock& clock, ConnectorRegistry& connectors, ParticipantDirectory& directory) :
		instance_id(instance_id), data_ref(data), events_ref(events), clock_ref(clock),
		connectors_ref(connectors), directory_ref(directory), irreversible(0) {}

	const Id& WorkContext::instanceId() const { return instance_id; }
	WorkflowData& WorkContext::data() { return data_ref; }
	EventPublisher& WorkContext::events() { return events_ref; }
	const Clock& WorkContext::clock() const { return clock_ref; }
	ConnectorRegistry& WorkContext::connectors() { return connectors_ref; }
	ParticipantDirectory& WorkContext::directory() { return directory_ref; }

	/** @brief Counts a caller-reported irreversible effect; emits no event and performs no work itself. */
	void WorkContext::markIrreversible() { ++irreversible; }
	unsigned WorkContext::irreversibleCount() const { return irreversible; }
} // namespace tividy
