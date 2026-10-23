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
 * @file WorkContext.h
 * @brief The collaborators a bound work item may use while it executes.
 */

#ifndef TIVIDY_ITEMS_WORKCONTEXT_H
#define TIVIDY_ITEMS_WORKCONTEXT_H

#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Clock.h"
#include "engine/core/Participant.h"
#include "engine/core/WorkflowData.h"
#include "engine/events/Observer.h"

namespace tividy
{

	/**
	 * @brief Parameter object given to every item by WorkItem::bind().
	 *
	 * Template items (inside a WorkflowDefinition) own no runtime references, so they stay clonable. Only after
	 * clone() does the instance bind each item to ITS OWN data, publisher and clock. An unbound item throws
	 * InvalidStateError on any lifecycle call. Owned by WorkflowInstance; items keep a non-owning pointer.
	 */
	class WorkContext
	{
		public:
			WorkContext(const Id& instance_id, WorkflowData& data, EventPublisher& events, const Clock& clock,
						ConnectorRegistry& connectors, ParticipantDirectory& directory);

			const Id& instanceId() const;

			WorkflowData& data();                 /**< The instance's own data (never shared with another instance). */

			EventPublisher& events();

			const Clock& clock() const;

			ConnectorRegistry& connectors();

			ParticipantDirectory& directory();

			/** Called by a task right after an effect that cannot be undone (e.g. activating a service). @post irreversibleCount() + 1. */
			void markIrreversible();

			unsigned irreversibleCount() const;
		private:
			Id instance_id;
			WorkflowData& data_ref;
			EventPublisher& events_ref;
			const Clock& clock_ref;
			ConnectorRegistry& connectors_ref;
			ParticipantDirectory& directory_ref;
			unsigned irreversible;
	};
} // namespace tividy

#endif // TIVIDY_ITEMS_WORKCONTEXT_H