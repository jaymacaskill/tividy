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
 * @file Observers.h
 * @brief Concrete observers: the audit log (and its writer) and notifications.
 */

#ifndef TIVIDY_RUNTIME_OBSERVERS_H
#define TIVIDY_RUNTIME_OBSERVERS_H

#include <string>
#include <vector>

#include "engine/connectors/ExternalConnector.h"
#include "engine/events/Observer.h"

namespace tividy
{

	/** One immutable history row. */
	struct AuditEntry
	{
		TimePoint when;
		EventType type;
		Id item_id;       /**< "" for instance-level events. */
		Id actor_id;      /**< "system" when the engine acted. */
		std::string message;
	};

	/** Append-only history of ONE instance. Never edited; an undo appends ItemReverted. */
	class AuditLog
	{
		public:
			void append(const AuditEntry& entry);

			const std::vector<AuditEntry>& entries() const;

			/** @return one display line: "YYYY-MM-DD HH:MM:SS  <type>  <actor>  <item>  <message>" (UTC, fixed-width columns). */
			static std::string format(const AuditEntry& entry);
		private:
			std::vector<AuditEntry> entries_ref;
	};

	/**
	 * @brief Concrete Observer and the SINGLE writer of the audit log.
	 * onEvent(): converts the event to an AuditEntry (actor "system" if actor_id is empty; message from the event, or toString(type)
	 * if empty) and appends it. Never throws.
	 */
	class AuditObserver : public EventObserver
	{
		public:
			explicit AuditObserver(AuditLog& log);

			void onEvent(const WorkflowEvent& event) override;
		private:
			AuditLog& log;
	};

	/**
	 * @brief Sends a message through the connector named @c connector_name for: ItemAssigned (recipient = the assignee),
	 * ItemEscalated and ItemFailed (recipient "escalation"), InstanceCompleted and InstanceFailed (recipient "owner").
	 * Request: operation "notify", params {recipient, subject = toString(type), body = event.message()}.
	 * If the connector is not registered, does nothing; failures are ignored (notifications are best-effort). Never throws.
	 */
	class NotificationObserver : public EventObserver
	{
		public:
			NotificationObserver(ConnectorRegistry& connectors, const std::string& connector_name = "notifier");

			void onEvent(const WorkflowEvent& event) override;
		private:
			ConnectorRegistry& connectors;
			std::string connector_name;
	};
} // namespace tividy

#endif // TIVIDY_RUNTIME_OBSERVERS_H