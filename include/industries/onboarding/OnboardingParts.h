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
 * @file OnboardingParts.h
 * @brief Onboarding's industry-specific classes: 2 tasks, 1 vendor system, 1 adapter.
 */

#ifndef TIVIDY_ONBOARDING_PARTS_H
#define TIVIDY_ONBOARDING_PARTS_H

#include <memory>
#include <string>

#include "engine/connectors/ExternalConnector.h"
#include "engine/items/TaskItem.h"

namespace tividy
{
	namespace onboarding
	{

		/**
		 * @brief Human task: HR checks identity and bank documents. (Template Method subclass.)
		 * validateOutcome: a Rejected outcome must have code "DOCS_INVALID" and a non-empty note, else
		 * ValidationError("rejected documents need a reason"). performWork: leaves the outcome unchanged.
		 * onCompleted: data["documents_verified"] = "yes" if the outcome is Accepted, otherwise "no".
		 */
		class VerifyDocumentsTask : public CloneableTask<VerifyDocumentsTask>
		{
			public:
				explicit VerifyDocumentsTask(const WorkItemSpec& spec);
			protected:
				void validateOutcome(const Outcome& outcome) const override;
				void performWork(const Participant& actor, Outcome& outcome) override;
				void onCompleted(const Outcome& outcome) override;
		};

		/**
		 * @brief Automatic task: background screening through connector "background-check".
		 * performWork: request {"run_check", {"national_id": data["national_id"]}}; a missing national_id throws ValidationError.
		 * Demo hook: while failures() == 0 the request also carries "simulate_failures" = data.getOr("simulate_failures", "0"), so starting an
		 * instance with simulate_failures=3 makes the vendor fail three times in a row (two retries, then escalation).
		 * If result.success is false throw ExternalFailure(result.message, result.retryable). If payload["status"] == "ISSUES" set
		 * outcome = Outcome::rejected("ISSUES_FOUND", "background check flagged"); otherwise leave Accepted.
		 * onCompleted: data["background_status"] = payload status. Not irreversible.
		 */
		class BackgroundCheckTask : public CloneableTask<BackgroundCheckTask>
		{
			public:
				explicit BackgroundCheckTask(const WorkItemSpec& spec);
			protected:
				void performWork(const Participant& actor, Outcome& outcome) override;
				void onCompleted(const Outcome& outcome) override;
			private:
				std::string last_status;
		};

		/**
		 * @brief Adaptee: the screening vendor's own (pretend third-party) class, simulated for tests and the demo.
		 * screen(): returns "FLAGGED" when the id ends with the digit '0', otherwise "CLEAR"; throws std::runtime_error("vendor unavailable")
		 * while failNextCalls(n) is active (the next n calls), then works again.
		 */
		class BackgroundCheckVendor
		{
			public:
				BackgroundCheckVendor();
				void failNextCalls(unsigned n);
				std::string screen(const std::string& national_id);
			private:
				unsigned failures_left;
		};

		/**
		 * @brief Object Adapter: ExternalConnector "background-check" over a BackgroundCheckVendor (held by unique_ptr).
		 * Operation "run_check" with param "national_id" (missing -> failure("national_id is required", false)). Optional param "simulate_failures"
		 * (integer text > 0): the adapter calls vendor.failNextCalls(n) before screening (demo/test hook). Success payload
		 * {"status": "CLEAR" or "ISSUES"} (FLAGGED becomes ISSUES). A std::runtime_error from the vendor becomes
		 * ConnectorResult::failure(what(), true) (retryable).
		 */
		class BackgroundCheckAdapter : public ExternalConnector
		{
			public:
				explicit BackgroundCheckAdapter(std::unique_ptr<BackgroundCheckVendor> vendor);
				std::string name() const override;
				ConnectorResult invoke(const ConnectorRequest& request) override;
			private:
				std::unique_ptr<BackgroundCheckVendor> vendor;
		};
	} // namespace onboarding
} // namespace tividy

#endif // TIVIDY_ONBOARDING_PARTS_H