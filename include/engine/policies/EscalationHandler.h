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
 * @file EscalationHandler.h
 * @brief Chain of Responsibility: who must decide on an escalated item?
 */

#ifndef TIVIDY_POLICIES_ESCALATIONHANDLER_H
#define TIVIDY_POLICIES_ESCALATIONHANDLER_H

#include <memory>
#include <string>

#include "engine/core/WorkflowData.h"
#include "engine/core/Types.h"

namespace tividy
{

	/** The request travelling down the chain. */
	struct EscalationRequest
	{
		Id instance_id;
		Id item_id;
		std::string reason;
		unsigned level;              /**< 1 for the item's first escalation, 2 for its second, ... (= ItemEscalated event count()). */
		const WorkflowData* data;    /**< Instance data, read-only; never null. */
	};

	/** The chain's answer. */
	struct EscalationResult
	{
		bool resolved;               /**< true if some handler chose an approver. */
		std::string approver_role;   /**< Role that must decide; meaningful only when resolved. */
		std::string handled_by;      /**< Name of the handler that resolved it ("" when unresolved). */
	};

	/**
	 * @brief Handler (GoF "Handler" participant) of a Chain of Responsibility built at definition time.
	 *
	 * handle() asks canHandle(); if true it returns resolve(request); otherwise it forwards to the successor;
	 * with no successor it returns {false, "", ""}. Chains are immutable once shared (stateless handlers), so
	 * one chain serves every instance. An unresolved escalation leaves the item Escalated and publishes an audit
	 * note; a human with the right role can still act on it.
	 */
	class EscalationHandler
	{
		public:
			explicit EscalationHandler(std::shared_ptr<const EscalationHandler> successor =
									   std::shared_ptr<const EscalationHandler>());
			virtual ~EscalationHandler();

			/** @brief Template of the chain walk described above. Never throws. */
			EscalationResult handle(const EscalationRequest& request) const;
		protected:
			/** @return true if this handler is responsible for @p request. Pure function. */
			virtual bool canHandle(const EscalationRequest& request) const = 0;

			/** @return a resolved result. @pre canHandle(request). */
			virtual EscalationResult resolve(const EscalationRequest& request) const = 0;
		private:
			std::shared_ptr<const EscalationHandler> successor;
	};

	/**
	 * @brief Generic concrete handler: handles requests of exactly one level and names one approver role.
	 * Example: RoleEscalationHandler("hiring-manager", 1, RoleEscalationHandler("hr-head", 2)).
	 */
	class RoleEscalationHandler : public EscalationHandler
	{
		public:
			RoleEscalationHandler(const std::string& approver_role, unsigned level,
								  std::shared_ptr<const EscalationHandler> successor =
								  std::shared_ptr<const EscalationHandler>());
		protected:
			bool canHandle(const EscalationRequest& request) const override;   /**< request.level == level. */

			EscalationResult resolve(const EscalationRequest& request) const override;
		private:
			std::string approver_role;
			unsigned level;
	};
} // namespace tividy

#endif // TIVIDY_POLICIES_ESCALATIONHANDLER_H