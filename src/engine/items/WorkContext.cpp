/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 31 October 2026
*/

/**
 * @file WorkContext.cpp
 * @brief Implements WorkContext: the bundle of collaborators (data, events, clock, connectors, directory) that every work item uses.
 * @details Holds non-owning references, so the referenced objects must outlive the context. It does no routing or lifecycle work itself.
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
