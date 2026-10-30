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
 * @file TelecomNetworkAdapter.cpp
 * @brief NetworkVendor + NetworkProvisioningAdapter (simulate_failures hook) from TelecomParts.h
 */

#ifndef TIVIDY_TELECOM_NETWORK_ADAPTER_CPP
#define TIVIDY_TELECOM_NETWORK_ADAPTER_CPP

#include <stdexcept>

#include "industries/telecom/TelecomParts.h"

namespace tividy
{
	namespace telecom
	{
		NetworkVendor::NetworkVendor() : failures_left(0), provisioned(""), active("") { }

		void NetworkVendor::failNextCalls(unsigned n)
		{
			failures_left = n;
		}

		void NetworkVendor::provision(const std::string& service_ref, const std::string& /* product */)
		{
			if (failures_left > 0)
			{
				failures_left --;
				throw std::runtime_error("OSS unavailable");
			}

			provisioned = service_ref;
		}

		void NetworkVendor::activate(const std::string& service_ref)
		{
			if (failures_left > 0)
			{
				failures_left --;
				throw std::runtime_error("OSS unavailable");
			}

			active = service_ref;
		}

		bool NetworkVendor::isActive(const std::string& service_ref) const
		{
			return active == service_ref;
		}

		NetworkProvisioningAdapter::NetworkProvisioningAdapter(std::unique_ptr<NetworkVendor> vendor) : ExternalConnector(),
			vendor (std::move(vendor)) { }

		std::string NetworkProvisioningAdapter::name() const
		{
			return "network-provisioning";
		}

		ConnectorResult NetworkProvisioningAdapter::invoke(const ConnectorRequest& request)
		{
			if (request.operation == "provision")
			{
				std::map<std::string, std::string>::const_iterator ref = request.params.find("service_ref");
				if (ref == request.params.end())
					return ConnectorResult::failure("service_ref is required", false);

				std::map<std::string, std::string>::const_iterator prod = request.params.find("product");
				if (prod == request.params.end())
					return ConnectorResult::failure("product is required", false);

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
					vendor->provision(ref->second, prod->second);
					return ConnectorResult::ok("network provisioned");
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

			else if (request.operation == "activate")
			{
				std::map<std::string, std::string>::const_iterator ref = request.params.find("service_ref");
				if (ref == request.params.end())
					return ConnectorResult::failure("service_ref is required", false);

				try
				{
					vendor->activate(ref->second);
					return ConnectorResult::ok("service activated");
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
	} // namespace telecom
} // namespace tividy

#endif // TIVIDY_TELECOM_NETWORK_ADAPTER_CPP