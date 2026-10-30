/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 26 October 2026
*/

/**
 * @file TelecomCreditAdapter.cpp
 * @brief implementation of CreditBureauVendor, CreditBureauAdapter from TelecomParts.h
 */

#ifndef TIVIDY_TELECOM_CREDIT_ADAPTER_CPP
#define TIVIDY_TELECOM_CREDIT_ADAPTER_CPP

#include <stdexcept>

#include "industries/telecom/TelecomParts.h"

namespace tividy
{
	namespace telecom
	{

		CreditBureauVendor::CreditBureauVendor() : failures_left(0) { }

		void CreditBureauVendor::failNextCalls(unsigned n)
		{
			failures_left = n;
		}

		int CreditBureauVendor::score(const std::string& id_number)
		{
			if (failures_left > 0)
			{
				failures_left --;
				throw std::runtime_error("bureau unavailable");
			}
			
			if (id_number.empty())
				throw std::invalid_argument("id_number must be non-empty");

			size_t l = id_number.size();
			char last = id_number.at(--l);

			if (last == '0') return 200;
			else if (last == '5') return 550;
			return 750;
		}

		CreditBureauAdapter::CreditBureauAdapter(std::unique_ptr<CreditBureauVendor> vendor) : ExternalConnector(),
			vendor(std::move(vendor)) { }

		std::string CreditBureauAdapter::name() const
		{
			return "credit-bureau";
		}

		ConnectorResult CreditBureauAdapter::invoke(const ConnectorRequest& request)
		{
			if (request.operation != "score")
				return ConnectorResult::failure("unknown operation: " + request.operation, false);

			std::map<std::string, std::string>::const_iterator id = request.params.find("id_number");
			if (id == request.params.end())
				return ConnectorResult::failure("id_number is required", false);

			try
			{
				int score = vendor->score(id->second);

				std::string band;
				if (score >= 650) band = "APPROVED";
				else if (score >= 400) band = "REFER";
				else band = "DECLINED";

				std::map<std::string, std::string> payload;
				payload["score"] = std::to_string(score);
				payload["band"]  = band;
				return ConnectorResult::ok("credit scored", payload);
			}
			catch (const std::invalid_argument& e)
			{
				return ConnectorResult::failure(e.what(), false);
			}
			catch (const std::runtime_error& e)
			{
				return ConnectorResult::failure(e.what(), true);
			}
			catch (const std::exception& e)
			{
				return ConnectorResult::failure(e.what(), false);
			}
		}
	} // namespace telecom
} // namespace tividy

#endif // TIVIDY_TELECOM_CREDIT_ADAPTER_CPP