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
 * @file WorkItemSpec.h
 * @brief The definition-time description of a work item (what it IS, not where it is now).
 */

#ifndef TIVIDY_ITEMS_WORKITEMSPEC_H
#define TIVIDY_ITEMS_WORKITEMSPEC_H

#include <string>
#include <vector>

#include "engine/core/Types.h"

namespace tividy
{

	/**
	 * @brief Plain value object. Split out of WorkItem so decorators forward ONE accessor and cloning copies it
	 * trivially. Setters return *this: WorkItemSpec("verify_docs","Verify documents").requireRole("hr").reviewedBy("hr-head").
	 */
	class WorkItemSpec
	{
		public:
			/** @param id Unique inside the definition, lower_snake_case. @param name Display name. */
			WorkItemSpec(const Id& id, const std::string& name);

			/** Role allowed to start/submit/complete. Empty = anyone. */
			WorkItemSpec& requireRole(const std::string& role);

			/** Role allowed to approve()/reject(). Required when reviewRequired(true). */
			WorkItemSpec& reviewedBy(const std::string& role);

			/** Id of an item that must be settled (Completed or Skipped) before this one may become Available. May repeat. */
			WorkItemSpec& dependsOn(const Id& predecessor);

			/** true: complete() is illegal; the worker submit()s and a reviewer approve()s. Default false. */
			WorkItemSpec& reviewRequired(bool required);

			/** true: FlowController starts and completes the item itself once it is assigned. Must not be reviewRequired. Default false. */
			WorkItemSpec& automatic(bool is_automatic);

			/** Retries allowed: technical failures before escalation, AND reviewer rejections before the next reject cancels. Default 2. */
			WorkItemSpec& maxRetries(unsigned count);

			const Id& id() const;
			const std::string& name() const;
			const std::string& requiredRole() const;
			const std::string& reviewerRole() const;
			const std::vector<Id>& dependencies() const;
			bool requiresReview() const;
			bool isAutomatic() const;
			unsigned maxRetries() const;

		private:
			Id id_ref;
			std::string name_ref;
			std::string required_role;
			std::string reviewer_role;
			std::vector<Id> dependencies_ref;
			bool review_required;
			bool automatic_ref;
			unsigned max_retries;
	};
} // namespace tividy

#endif // TIVIDY_ITEMS_WORKITEMSPEC_H