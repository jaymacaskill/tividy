/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 27 October 2026

TelecomTasks.cpp
*/

#ifndef TIVIDY_TELECOM_TELECOMTASKS_CPP
#define TIVIDY_TELECOM_TELECOMTASKS_CPP

#include "industries/telecom/TelecomParts.h"
#include "engine/core/Errors.h"
#include "engine/items/WorkContext.h"

namespace tividy
{
	namespace telecom
	{
		namespace
		{
			// call the connector and turn a failed reply into the exception the Template Method expects
			ConnectorResult callConnector(WorkContext& context, const std::string& connector, const ConnectorRequest& request)
			{
				ConnectorResult result = context.connectors().require(connector).invoke(request);
				if (!result.success) throw ExternalFailure(result.message, result.retryable);

				return result;
			}

			// a connector that reports success but leaves out the value we need is a broken contract, retrying will not fix it
			std::string requirePayload(const ConnectorResult& result, const std::string& key)
			{
				std::map<std::string, std::string>::const_iterator it = result.payload.find(key);
				if (it == result.payload.end()) throw ExternalFailure("connector reply is missing " + key, false);

				return it->second;
			}

			bool isBlank(const WorkflowData& data, const std::string& key)
			{
				return data.getOr(key, "") == "";
			}
		} // anonymous namespace

		// CreditCheckTask

		CreditCheckTask::CreditCheckTask(const WorkItemSpec& spec) :
			CloneableTask<CreditCheckTask>(spec) { }

		void CreditCheckTask::performWork(const Participant& /* actor */, Outcome& outcome)
		{
			WorkflowData& data = context().data();
			if (isBlank(data, "customer_id_number")) throw ValidationError("customer_id_number is required");

			ConnectorRequest request;
			request.operation = "score";
			request.params["id_number"] = data.get("customer_id_number");

			ConnectorResult result = callConnector(context(), "credit-bureau", request);
			data.set("credit_score", requirePayload(result, "score"));

			// the band decides what happens next, the outcome code is what the routing branches on
			const std::string band = requirePayload(result, "band");
			if (band == "APPROVED") return; // outcome stays Accepted

			if (band == "REFER") outcome = Outcome::escalated("CREDIT_REFER", "score needs manual review");
			else if (band == "DECLINED") outcome = Outcome::rejected("CREDIT_DECLINED", "credit declined");
			else throw ExternalFailure("unknown credit band: " + band, false);
		}

		// ProvisionServiceTask

		ProvisionServiceTask::ProvisionServiceTask(const WorkItemSpec& spec) :
			CloneableTask<ProvisionServiceTask>(spec) { }

		void ProvisionServiceTask::performWork(const Participant& /* actor */, Outcome& /* outcome */)
		{
			WorkflowData& data = context().data();
			if (isBlank(data, "customer_ref") || isBlank(data, "product")) throw ValidationError("customer_ref and product are required");

			ConnectorRequest provision;
			provision.operation = "provision";
			provision.params["service_ref"] = data.get("customer_ref");
			provision.params["product"] = data.get("product");

			// demo hook: only the first attempt asks the vendor to fail, so the retry goes through
			if (failures() == 0) provision.params["simulate_failures"] = data.getOr("simulate_failures", "0");

			callConnector(context(), "network-provisioning", provision);

			// only reached if provisioning worked. if activate fails the retry repeats both calls, that is fine because the vendor is idempotent
			ConnectorRequest activate;
			activate.operation = "activate";
			activate.params["service_ref"] = data.get("customer_ref");

			callConnector(context(), "network-provisioning", activate);

			data.set("service_status", "ACTIVE");
		}

		bool ProvisionServiceTask::performsIrreversibleWork() const
		{
			return true; // the customer's service is live now
		}
	} // namespace telecom
} // namespace tividy

#endif // TIVIDY_TELECOM_TELECOMTASKS_CPP