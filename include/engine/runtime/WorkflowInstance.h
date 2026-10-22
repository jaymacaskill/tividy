/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 22 October 2026
*/

/**
 * @file WorkflowInstance.h
 * @brief One independent running execution of a WorkflowDefinition, and its snapshot (Memento).
 */

#ifndef TIVIDY_RUNTIME_WORKFLOWINSTANCE_H
#define TIVIDY_RUNTIME_WORKFLOWINSTANCE_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/commands/ActionHistory.h"
#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Clock.h"
#include "engine/core/Participant.h"
#include "engine/core/WorkflowData.h"
#include "engine/definition/WorkflowDefinition.h"
#include "engine/events/Observer.h"
#include "engine/items/WorkContext.h"
#include "engine/items/WorkItem.h"
#include "engine/runtime/FlowController.h"
#include "engine/runtime/Observers.h"

namespace tividy
{

	/** Organisation-wide collaborators shared by all instances. Plain references, no ownership; owned by Tividy. */
	struct Services
	{
		Clock& clock;
		ParticipantDirectory& directory;
		ConnectorRegistry& connectors;
	};

	class WorkflowInstance;

	/**
	 * @brief Memento (GoF "Memento"). Originator = WorkflowInstance; Caretaker = InstanceCommand.
	 * Holds one WorkItemMemento per task (keyed by item id), a copy of the instance data and the InstanceStatus.
	 * Opaque: only WorkflowInstance (friend) creates or reads it. Not copyable: hold it through std::unique_ptr.
	 */
	class InstanceMemento
	{
		public:
			~InstanceMemento();
		private:
			friend class WorkflowInstance;

			InstanceMemento();
			InstanceMemento(const InstanceMemento&);              // not defined: non-copyable (C++11 idiom)
			InstanceMemento& operator=(const InstanceMemento&);   // not defined

			std::map<Id, std::unique_ptr<WorkItemMemento> > items;
			WorkflowData data;
			InstanceStatus status;
	};

	/**
	 * @brief Owner of one run's data, item tree, event channel, history and controllers.
	 *
	 * ### Ownership (composition; everything dies with the instance)
	 * data_ref, events_ref (ConcreteSubject), audit_ref + audit_observer, root_ref (the cloned tree), context_ref, flow, actions_ref,
	 * extra_observers. Members are declared in construction order; DO NOT reorder them (context_ref needs data_ref and events_ref;
	 * flow and actions_ref need *this).
	 *
	 * ### Constructor body, in order
	 *  1. context_ref is built from (id, data_ref, events_ref, services.clock, services.connectors, services.directory);
	 *  2. root_ref->bind(context_ref);
	 *  3. attach audit_observer, THEN flow, to events_ref;
	 *  4. status_ref = Created. No event is published yet.
	 * Not copyable, not movable.
	 */
	class WorkflowInstance
	{
		public:
			WorkflowInstance(const Id& id, const std::string& definition_name, unsigned definition_version,
							 const WorkflowData& data, std::unique_ptr<WorkItem> root, const DefinitionPolicies& policies,
							 Services& services);
			~WorkflowInstance();

			const Id& id() const;
			const std::string& definitionName() const;
			unsigned definitionVersion() const;     /**< The version this instance was cloned from; never changes. */

			InstanceStatus status() const;
			WorkflowData& data();
			const WorkflowData& data() const;
			WorkItem& root();
			const WorkItem& root() const;

			/** @return the item with spec().id() == id (outermost decorator), or nullptr. */
			WorkItem* findItem(const Id& id);

			/** @return non-composite items assigned to @p participant_id whose status is Assigned, InProgress, UnderReview or Escalated,
			 *          in depth-first order. ("My work".) */
			std::vector<WorkItem*> itemsFor(const Id& participant_id);

			EventPublisher& events();
			AuditLog& audit();
			const AuditLog& audit() const;
			ActionHistory& actions();
			WorkContext& context();

			/** Takes ownership of @p observer and attaches it after the audit and flow observers. @pre non-null. */
			void addObserver(std::unique_ptr<EventObserver> observer);

			/**
			 * @brief Created -> Running. @pre status() == Created, else InvalidStateError.
			 * Steps: status_ref = Running; publish InstanceStarted("started from <name> v<version>"); flow.releaseEligible()
			 * (assignment and automatic tasks happen inside that last step).
			 */
			void start();

			/**
			 * @brief Running -> Cancelled. @pre status() == Running, else InvalidStateError.
			 * Steps: status_ref = Cancelled; publish InstanceCancelled(reason); root().cancel(reason). FlowController ignores the resulting
			 * ItemCancelled events because the instance is no longer Running.
			 */
			void cancel(const std::string& reason);

			/** @return a snapshot of every task (via createMemento(); empty pointers skipped), a copy of data_ref and status_ref. */
			std::unique_ptr<InstanceMemento> captureSnapshot() const;

			/** Restores items (via restore()), data_ref and status_ref from a snapshot made by THIS instance. Publishes nothing. */
			void restoreSnapshot(const InstanceMemento& memento);

		private:
			friend class FlowController;

			/** Called only by FlowController: sets Completed or Failed and publishes InstanceCompleted / InstanceFailed(message).
			 *  No effect if status_ref is not Running. */
			void finish(InstanceStatus final_status, const std::string& message);

			Id id_ref;
			std::string definition_name;
			unsigned definition_version;
			InstanceStatus status_ref;
			WorkflowData data_ref;
			EventPublisher events_ref;
			AuditLog audit_ref;
			AuditObserver audit_observer;
			std::unique_ptr<WorkItem> root_ref;
			WorkContext context_ref;
			FlowController flow;
			ActionHistory actions_ref;
			std::vector<std::unique_ptr<EventObserver> > extra_observers;
	};
} // namespace tividy

#endif // TIVIDY_RUNTIME_WORKFLOWINSTANCE_H