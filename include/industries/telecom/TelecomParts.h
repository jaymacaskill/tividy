/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 23 October 2026
*/

/**
 * @file TelecomParts.h
 * @brief Telecom's industry-specific classes: 2 tasks, 2 vendor systems, 2 adapters, 1 assignment strategy.
 */

#ifndef TIVIDY_TELECOM_PARTS_H
#define TIVIDY_TELECOM_PARTS_H

#include <memory>
#include <string>

#include "engine/connectors/ExternalConnector.h"
#include "engine/items/TaskItem.h"
#include "engine/policies/AssignmentStrategy.h"

namespace tividy
{
	namespace telecom
	{

		/**
		 * @brief Automatic task: credit assessment through connector "credit-bureau".
		 * performWork: request {"score", {"id_number": data["customer_id_number"]}}; missing key -> ValidationError. A failed result throws
		 * ExternalFailure(message, retryable). On success store data["credit_score"] and map payload "band":
		 * "APPROVED" -> leave Accepted; "REFER" -> outcome = Outcome::escalated("CREDIT_REFER", "score needs manual review");
		 * "DECLINED" -> outcome = Outcome::rejected("CREDIT_DECLINED", "credit declined"). Not irreversible.
		 */
		class CreditCheckTask : public CloneableTask<CreditCheckTask>
		{
			public:
				explicit CreditCheckTask(const WorkItemSpec& spec);
			protected:
				void performWork(const Participant& actor, Outcome& outcome) override;
		};

		/**
		 * @brief Automatic task: configures the network and switches the service on through connector "network-provisioning".
		 * performWork: request {"provision", {"service_ref": data["customer_ref"], "product": data["product"]}}, then (only if that succeeded)
		 * {"activate", {"service_ref": data["customer_ref"]}}. A failed result throws ExternalFailure(message, retryable) (if the first call
		 * succeeded and the second failed, the retry repeats both; the vendor is idempotent). Stores data["service_status"] = "ACTIVE".
		 * Demo hook: while failures() == 0 the "provision" request also carries "simulate_failures" = data.getOr("simulate_failures", "0").
		 * performsIrreversibleWork() == true.
		 */
		class ProvisionServiceTask : public CloneableTask<ProvisionServiceTask>
		{
			public:
				explicit ProvisionServiceTask(const WorkItemSpec& spec);
			protected:
				void performWork(const Participant& actor, Outcome& outcome) override;
				bool performsIrreversibleWork() const override;
		};

		/**
		 * @brief Adaptee (simulated credit bureau). score(): 200 if the id ends in '0', 550 if it ends in '5', otherwise 750.
		 * Throws std::runtime_error("bureau unavailable") while failNextCalls(n) is active.
		 */
		class CreditBureauVendor
		{
			public:
				CreditBureauVendor();
				void failNextCalls(unsigned n);
				int score(const std::string& id_number);
			private:
				unsigned failures_left;
		};

		/**
		 * @brief Adaptee (simulated OSS provisioning system). provision() and activate() record the call; both throw
		 * std::runtime_error("OSS unavailable") while failNextCalls(n) is active. isActive(service_ref) tells whether activate() succeeded for @p service_ref.
		 */
		class NetworkVendor
		{
			public:
				NetworkVendor();
				void failNextCalls(unsigned n);
				void provision(const std::string& service_ref, const std::string& product);
				void activate(const std::string& service_ref);
				bool isActive(const std::string& service_ref) const;
			private:
				unsigned failures_left;
				std::string provisioned;
				std::string active;
		};

		/**
		 * @brief Object Adapter: connector "credit-bureau". Operation "score", param "id_number" (missing -> failure(..., false)).
		 * Payload {"score": "<int>", "band": "APPROVED" (>= 650) | "REFER" (400..649) | "DECLINED" (< 400)}.
		 * runtime_error -> failure(what(), true).
		 */
		class CreditBureauAdapter : public ExternalConnector
		{
			public:
				explicit CreditBureauAdapter(std::unique_ptr<CreditBureauVendor> vendor);
				std::string name() const override;
				ConnectorResult invoke(const ConnectorRequest& request) override;
			private:
				std::unique_ptr<CreditBureauVendor> vendor;
		};

		/**
		 * @brief Object Adapter: connector "network-provisioning". Operations "provision" {"service_ref","product"} and "activate"
		 * {"service_ref"}; success payload empty. runtime_error -> failure(what(), true). "provision" accepts the optional demo param
		 * "simulate_failures" (integer text > 0): the adapter calls vendor.failNextCalls(n) first.
		 */
		class NetworkProvisioningAdapter : public ExternalConnector
		{
			public:
				explicit NetworkProvisioningAdapter(std::unique_ptr<NetworkVendor> vendor);
				std::string name() const override;
				ConnectorResult invoke(const ConnectorRequest& request) override;
			private:
				std::unique_ptr<NetworkVendor> vendor;
		};

		/**
		 * @brief ConcreteStrategy (assignment): field technicians are matched by region.
		 * For @p role: candidates = directory.withRole(role). If role == "field-technician", keep only participants whose attribute("region")
		 * equals data["region"]. Return the smallest id among the remaining; "" if none (also for an empty role).
		 */
		class RegionalTechnicianAssignment : public AssignmentStrategy
		{
			public:
				Id chooseParticipant(const WorkItem& item, const std::string& role,
									 const ParticipantDirectory& directory, const WorkflowData& data) const override;
		};
	} // namespace telecom
} // namespace tividy

#endif // TIVIDY_TELECOM_PARTS_H