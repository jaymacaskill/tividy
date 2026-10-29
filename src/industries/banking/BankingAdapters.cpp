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

BankingAdapters.cpp
*/

/**
 * @file BankingAdapters.cpp
 * @brief Implements CoreBankingVendor, CardSchemeVendor, CoreBankingAdapter, CardSchemeAdapter
 * from BankingParts.h
 */

#ifndef TIVIDY_BANKING_ADAPTERS_CPP
#define TIVIDY_BANKING_ADAPTERS_CPP

#include <stdexcept>
#include <cmath>

#include "industries/banking/BankingParts.h"

namespace tividy
{
	namespace
	{
		bool isIssuedLedgerRef(const std::string& ref, unsigned issued)
		{
			const std::string prefix = "LEDGER-";
			if (ref.size() <= prefix.size() || ref.compare(0, prefix.size(), prefix) != 0)
				return false;
			const std::string digits = ref.substr(prefix.size());
			if (digits.size() > 9 || digits[0] == '0'
				|| digits.find_first_not_of("0123456789") != std::string::npos)
				return false;
			return std::stoul(digits) <= issued;
		}
	}
	namespace banking
	{
		/*
			We make use of a test double, simulating calls a bank may make,
			but we don't have their actual logic, however, it could be wired in here
			There are unused parameters, they're there to show the typical format of a bank's calls
		*/

		CoreBankingVendor::CoreBankingVendor() : failures_left(0), counter(0) { }

		void CoreBankingVendor::failNextCalls(unsigned n)
		{
			failures_left = n;
		}

		std::string CoreBankingVendor::postProvisionalCredit(const std::string& /* card_ref */, const std::string& amount)
		{
			if (failures_left > 0)
			{
				failures_left --;
				throw std::runtime_error("core banking unavailable");
			}

			double value = 0.0;
			std::size_t used = 0;
			try { value = std::stod(amount, &used); }
			catch (const std::exception&) { throw std::invalid_argument("amount must be a positive number"); }
			
			if (used != amount.size() || !std::isfinite(value) || !(value > 0.0))
				throw std::invalid_argument("amount must be a positive number");

			counter ++;
			return "LEDGER-" + std::to_string(counter);
		}

		void CoreBankingVendor::reverse(const std::string& ledger_ref)
		{
			if (failures_left > 0)
			{
				failures_left --;
				throw std::runtime_error("core banking unavailable");
			}

			if (!isIssuedLedgerRef(ledger_ref, counter))
				throw std::invalid_argument("unknown ledger reference");
		}

		void CoreBankingVendor::finalise(const std::string& ledger_ref)
		{
			if (failures_left > 0)
			{
				failures_left --;
				throw std::runtime_error("core banking unavailable");
			}

			if (!isIssuedLedgerRef(ledger_ref, counter))
				throw std::invalid_argument("unknown ledger reference");
		}

		CardSchemeVendor::CardSchemeVendor() : failures_left(0) { }

		void CardSchemeVendor::failNextCalls(unsigned n)
		{
			failures_left = n;
		}

		std::string CardSchemeVendor::fileChargeback(const std::string& transaction_id, const std::string& /* reason_code */)
		{
			if (failures_left > 0)
			{
				failures_left --;
				throw std::runtime_error("scheme unavailable");
			}
			return "CASE-" + transaction_id;
		}

		CoreBankingAdapter::CoreBankingAdapter(std::unique_ptr<CoreBankingVendor> vendor) : ExternalConnector() ,
			vendor(std::move(vendor)) { }

		std::string CoreBankingAdapter::name() const
		{
			return "core-banking";
		}

		ConnectorResult CoreBankingAdapter::invoke(const ConnectorRequest& request)
		{
			if (request.operation == "post_provisional_credit")
			{
				std::map<std::string, std::string>::const_iterator ref = request.params.find("card_ref");
				std::map<std::string, std::string>::const_iterator amt = request.params.find("amount");
				
				if (ref == request.params.end())
					return ConnectorResult::failure("card_ref is required", false);
				if (amt == request.params.end())
					return ConnectorResult::failure("amount is required", false);

				std::map<std::string, std::string>::const_iterator sim = request.params.find("simulate_failures");
				if (sim != request.params.end())
				{
					int n = 0;
					try { n = std::stoi(sim->second); }
					catch (const std::exception&) { n = 0; }
					if (n > 0) vendor->failNextCalls(static_cast<unsigned>(n));
				}

				try
				{
					std::string ledger = vendor->postProvisionalCredit(ref->second, amt->second);
					std::map<std::string, std::string> payload;
					payload["ledger_ref"] = ledger;
					return ConnectorResult::ok("provisional credit posted", payload);
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

			if (request.operation == "reverse_credit")
			{
				std::map<std::string, std::string>::const_iterator ref = request.params.find("ledger_ref");
				if (ref == request.params.end())
					return ConnectorResult::failure("ledger_ref is required", false);

				try
				{
					vendor->reverse(ref->second);
					return ConnectorResult::ok("credit reversed");
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

			if (request.operation == "finalise_credit")
			{
				std::map<std::string, std::string>::const_iterator ref = request.params.find("ledger_ref");
				if (ref == request.params.end())
					return ConnectorResult::failure("ledger_ref is required", false);

				try
				{
					vendor->finalise(ref->second);
					return ConnectorResult::ok("credit finalised");
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

			return ConnectorResult::failure("unknown operation: " + request.operation, false);
		}

		CardSchemeAdapter::CardSchemeAdapter(std::unique_ptr<CardSchemeVendor> vendor) : ExternalConnector() ,
			vendor(std::move(vendor)) { }

		std::string CardSchemeAdapter::name() const
		{
			return "card-scheme";
		}

		ConnectorResult CardSchemeAdapter::invoke(const ConnectorRequest& request)
		{
			if (request.operation != "file_chargeback")
				return ConnectorResult::failure("unknown operation: " + request.operation, false); // our industry scope is disputes, specific operations would need to be added in

			std::map<std::string, std::string>::const_iterator transact = request.params.find("transaction_id");
			std::map<std::string, std::string>::const_iterator reason = request.params.find("reason_code");

			if (transact == request.params.end())
				return ConnectorResult::failure("transaction_id is required", false);
			if (reason == request.params.end())
				return ConnectorResult::failure("reason_code is required", false);
			// bank request must have <transaction_id, id> and <reason_code, code> in the parameters map
			
			std::map<std::string, std::string>::const_iterator sim = request.params.find("simulate_failures");
			if (sim != request.params.end())
			{
				int n = 0;
				try { n = std::stoi(sim->second); }
				catch (const std::exception&) { n = 0; }
				if (n > 0) vendor->failNextCalls(static_cast<unsigned>(n));
			} // the failure simulation for demos

			try
			{
				std::string case_id = vendor->fileChargeback(transact->second, reason->second);
				std::map<std::string, std::string> payload;
				payload["scheme_case"] = case_id;
				return ConnectorResult::ok("chargeback filed", payload);
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
	} // namespace banking
} // namespace tividy

#endif // TIVIDY_BANKING_ADAPTERS_CPP