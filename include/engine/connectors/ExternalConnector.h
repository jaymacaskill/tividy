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
 * @file ExternalConnector.h
 * @brief Adapter pattern Target, the registry that finds connectors by name, and a stub notifier.
 */

#ifndef TIVIDY_CONNECTORS_EXTERNALCONNECTOR_H
#define TIVIDY_CONNECTORS_EXTERNALCONNECTOR_H

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tividy
{

	/** A call to an external system in TIVIDY's neutral vocabulary. */
	struct ConnectorRequest
	{
		std::string operation;                        /**< e.g. "run_check". Defined by each connector. */
		std::map<std::string, std::string> params;
	};

	/** Reply from an external system in TIVIDY's neutral vocabulary. */
	struct ConnectorResult
	{
		static ConnectorResult ok(const std::string& message = "",
								  const std::map<std::string, std::string>& payload = std::map<std::string, std::string>());
		static ConnectorResult failure(const std::string& message, bool retryable);

		bool success;
		bool retryable;                               /**< Meaningful only when !success: true = transient. */
		std::string message;
		std::map<std::string, std::string> payload;
	};

	/**
	 * @brief Adapter pattern: Target. Every external system is reached through this interface. Adapters (in industry
	 * folders) translate invoke() into the vendor class's own API; the Adaptee is held by unique_ptr (Object Adapter).
	 */
	class ExternalConnector
	{
		public:
			virtual ~ExternalConnector();

			/** @return registry key, unique per Tividy. Constant for the object's lifetime. */
			virtual std::string name() const = 0;

			/** @brief Performs @p request. Never throws for ordinary vendor failures (returns failure(...) instead);
			 *  unknown operation -> failure("unknown operation: <op>", false). */
			virtual ConnectorResult invoke(const ConnectorRequest& request) = 0;
	};

	/** Owns every connector of one Tividy. */
	class ConnectorRegistry
	{
		public:
			/** Takes ownership. @throws EngineError on a duplicate name. */
			void registerConnector(std::unique_ptr<ExternalConnector> connector);

			bool has(const std::string& name) const;

			/** @throws NotFoundError. */
			ExternalConnector& require(const std::string& name);
		private:
			std::map<std::string, std::unique_ptr<ExternalConnector> > connectors;
	};

	/**
	 * @brief Stub e-mail/SMS used by default and in tests. Operation "notify" with params "recipient", "subject", "body"
	 * stores "recipient | subject | body" and succeeds; any other operation fails (non-retryable).
	 */
	class LoggingNotifier : public ExternalConnector
	{
		public:
			explicit LoggingNotifier(const std::string& connector_name = "notifier");

			std::string name() const override;
			ConnectorResult invoke(const ConnectorRequest& request) override;

			/** @return every stored line, oldest first (the TUI shows this as the notification outbox). */
			const std::vector<std::string>& sent() const;
		private:
			std::string name_ref;
			std::vector<std::string> sent_ref;
	};
} // namespace tividy

#endif // TIVIDY_CONNECTORS_EXTERNALCONNECTOR_H