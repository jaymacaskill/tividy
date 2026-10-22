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
 * @file IndustryPack.h
 * @brief Abstract Factory: everything an industry (or client) plugs into the generic engine.
 */

#ifndef TIVIDY_FACADE_INDUSTRYPACK_H
#define TIVIDY_FACADE_INDUSTRYPACK_H

#include <memory>
#include <string>
#include <vector>

#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Participant.h"
#include "engine/definition/WorkflowBuilder.h"
#include "engine/policies/AssignmentStrategy.h"
#include "engine/policies/EscalationHandler.h"
#include "ui/ClientProfile.h"

namespace tividy
{

	/**
	 * @brief AbstractFactory (GoF "AbstractFactory"). ConcreteFactories: one class per industry under src/industries, and any client subclass.
	 *
	 * One pack = one FAMILY of related products (a workflow, its policies, connectors, people, UI look). Tividy::installPack() asks the pack
	 * for each product and registers it, so adding an industry never edits engine code.
	 *
	 * ### Overriding (the "AWS" extension model)
	 * Every method EXCEPT key(), displayName() and createDirector() has a working default, so a pack overrides only what differs. A client
	 * customises further by deriving from a pack and overriding one method. The pack's Director receives the pack by reference and calls
	 * createAssignmentStrategy()/createEscalationChain() inside construct(), so an overridden policy applies to the workflow.
	 * The pack must outlive its Director (Tividy owns the pack).
	 */
	class IndustryPack
	{
		public:
			virtual ~IndustryPack();

			/** @return unique registry key, lower-case, e.g. "demo". Constant. */
			virtual std::string key() const = 0;

			/** @return display name, e.g. "Demo: Document Approval". */
			virtual std::string displayName() const = 0;

			/** @return the Director of the workflow this pack publishes. It holds a reference to *this. */
			virtual std::unique_ptr<WorkflowDirector> createDirector() const = 0;

			/** @return participants to seed. Ids must be globally unique: prefix with key() + "-". Default: empty. */
			virtual std::vector<Participant> createParticipants() const;

			/** @return connectors to register. Default: one LoggingNotifier named "notifier". */
			virtual std::vector<std::unique_ptr<ExternalConnector> > createConnectors() const;

			/** @return default: RoleBasedAssignment. */
			virtual std::shared_ptr<const AssignmentStrategy> createAssignmentStrategy() const;

			/** @return head of the escalation chain. Default: nullptr (escalated items wait for a human). */
			virtual std::shared_ptr<const EscalationHandler> createEscalationChain() const;

			/** @return the look and vocabulary. Default: ClientProfile() (neutral). */
			virtual ClientProfile createProfile() const;
	};
} // namespace tividy

#endif // TIVIDY_FACADE_INDUSTRYPACK_H