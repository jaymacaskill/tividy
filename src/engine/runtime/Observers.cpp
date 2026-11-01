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
 * @file Observers.cpp
 * @brief Implements the event observers: AuditLog and AuditObserver (a record of every event) and NotificationObserver (sends events to a connector).
 */

#ifndef TIVIDY_RUNTIME_OBSERVERS_CPP
#define TIVIDY_RUNTIME_OBSERVERS_CPP

#include <ctime>
#include <sstream>
#include <iomanip>

#include "engine/events/WorkflowEvent.h"
#include "engine/runtime/Observers.h"

namespace tividy
{
	void AuditLog::append(const AuditEntry& entry)
	{
		entries_ref.push_back(entry);
	}

	const std::vector<AuditEntry>& AuditLog::entries() const
	{
		return entries_ref;
	}

	std::string AuditLog::format(const AuditEntry& entry)
	{
		const std::time_t t = std::chrono::system_clock::to_time_t(entry.when);
		const std::tm* tm_utc = std::gmtime(&t);

		char ts[24] = "1970-01-01 00:00:00";
		if (tm_utc != 0)
			std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_utc);

		std::ostringstream line;
		line << std::left
			<< std::setw(19) << ts << "  "
			<< std::setw(18) << toString(entry.type) << "  "
			<< std::setw(15) << entry.actor_id << "  "
			<< std::setw(20) << entry.item_id << "  "
			<< entry.message;
		return line.str();
	}

	AuditObserver::AuditObserver(AuditLog& log) : log(log) { }

	void AuditObserver::onEvent(const WorkflowEvent& event)
	{
		AuditEntry entry;
		entry.when = event.when();
		entry.type = event.type();
		entry.item_id = event.itemId();
		entry.actor_id = event.actorId().empty() ? std::string("system") : event.actorId();
		entry.message = event.message().empty() ? toString(event.type()) : event.message();
		log.append(entry);
	}

	NotificationObserver::NotificationObserver(ConnectorRegistry& connectors, const std::string& connector_name) :
		connectors(connectors), connector_name(connector_name) { }

	void NotificationObserver::onEvent(const WorkflowEvent& event)
	{
		std::string recipient;
		switch (event.type())
		{
			case EventType::ItemAssigned:
				recipient = event.actorId();
				break;
			case EventType::ItemEscalated:
				recipient = "escalation";
				break;
			case EventType::ItemFailed:
				recipient = "escalation";
				break;
			case EventType::InstanceCompleted:
				recipient = "owner";
				break;
			case EventType::InstanceFailed:
				recipient = "owner";
				break;
			default: return;   // events we don't notify about
		}

		if (!connectors.has(connector_name)) return;

		ConnectorRequest req;
		req.operation = "notify";
		req.params["recipient"] = recipient;
		req.params["subject"] = toString(event.type());
		req.params["body"] = event.message();

		try
		{
			connectors.require(connector_name).invoke(req);
		}
		catch (...)
		{
			// Notifications are best-effort. A failed connector must never
			// stop the engine or affect workflow progress.
		}
	}
} // namespace tividy

#endif // TIVIDY_RUNTIME_OBSERVERS_CPP