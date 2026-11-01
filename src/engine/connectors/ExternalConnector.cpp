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
 * @file ExternalConnector.cpp
 * @brief Implements the Adapter side of the engine: ConnectorResult, ConnectorRegistry and the LoggingNotifier connector.
 */

#ifndef TIVIDY_CONNECTORS_EXTERNALCONNECTOR_CPP
#define TIVIDY_CONNECTORS_EXTERNALCONNECTOR_CPP

#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Errors.h"

namespace tividy
{
	ConnectorResult ConnectorResult::ok(const std::string& message, const std::map<std::string, std::string>& payload)
	{
		ConnectorResult o;
		o.success = true;
		o.message = message;
		o.payload = payload;
		o.retryable = false;

		return o;
	}

	ConnectorResult ConnectorResult::failure(const std::string& message, bool retryable)
	{
		ConnectorResult f;
		f.message = message;
		f.retryable = retryable;
		f.success = false;

		return f;
	}

	ExternalConnector::~ExternalConnector() { }

	void ConnectorRegistry::registerConnector(std::unique_ptr<ExternalConnector> connector)
	{
		std::map<std::string, std::unique_ptr<ExternalConnector>>::iterator it = connectors.find(connector->name());
		if (it != connectors.end()) throw EngineError("Connector " + connector->name() + " already present in this registry");
		connectors[connector->name()] = std::move(connector);
	}

	bool ConnectorRegistry::has(const std::string& name) const
	{
		std::map<std::string, std::unique_ptr<ExternalConnector>>::const_iterator it = connectors.find(name);
		return it != connectors.end();
	}

	ExternalConnector& ConnectorRegistry::require(const std::string& name)
	{
		std::map<std::string, std::unique_ptr<ExternalConnector>>::const_iterator it = connectors.find(name);
		if (it == connectors.end()) throw NotFoundError("Connector with key " + name + " was not found");
		return *(it->second);
	}

	LoggingNotifier::LoggingNotifier(const std::string& connector_name)
	{
		name_ref = connector_name;
	}

	std::string LoggingNotifier::name() const
	{
		return name_ref;
	}

	ConnectorResult LoggingNotifier::invoke(const ConnectorRequest& request)
	{
		if (request.operation != "notify")
			return ConnectorResult::failure("unknown operation: " + request.operation, false);

		std::string recipient, subject, body;
		std::map<std::string, std::string>::const_iterator it;

		it = request.params.find("recipient");
		if (it != request.params.end()) recipient = it->second;

		it = request.params.find("subject");
		if (it != request.params.end()) subject = it->second;

		it = request.params.find("body");
		if (it != request.params.end()) body = it->second;

		std::string line = recipient + " | " + subject + " | " + body;
		sent_ref.push_back(line);
		return ConnectorResult::ok(line);
	}

	const std::vector<std::string>& LoggingNotifier::sent() const
	{
		return sent_ref;
	}
} // namespace tividy

#endif // TIVIDY_CONNECTORS_EXTERNALCONNECTOR_CPP