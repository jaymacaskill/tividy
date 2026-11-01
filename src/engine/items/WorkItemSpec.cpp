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
 * @file WorkItemSpec.cpp
 * @brief Implements WorkItemSpec, the fluent description of one work item: its id, roles, dependencies, deadline, review and retry rules.
 */

#ifndef TIVIDY_ITEMS_WORKITEMSPEC_CPP
#define TIVIDY_ITEMS_WORKITEMSPEC_CPP

#include "engine/items/WorkItemSpec.h"

namespace tividy
{
	WorkItemSpec::WorkItemSpec(const Id& id, const std::string& name) :
		id_ref(id), name_ref(name),
		review_required(false), automatic_ref(false),
		max_retries(2), due_after_seconds(0) { }

	WorkItemSpec& WorkItemSpec::requireRole(const std::string& role)
	{
		required_role = role;
		return *this;
	}

	WorkItemSpec& WorkItemSpec::reviewedBy(const std::string& role)
	{
		reviewer_role = role;
		return *this;
	}

	WorkItemSpec& WorkItemSpec::dependsOn(const Id& predecessor)
	{
		dependencies_ref.push_back(predecessor);
		return *this;
	}

	WorkItemSpec& WorkItemSpec::reviewRequired(bool required)
	{
		review_required = required;
		return *this;
	}

	WorkItemSpec& WorkItemSpec::automatic(bool is_automatic)
	{
		automatic_ref = is_automatic;
		return *this;
	}

	WorkItemSpec& WorkItemSpec::dueAfter(long seconds)
	{
		due_after_seconds = seconds;
		return *this;
	}

	WorkItemSpec& WorkItemSpec::onTimeout(const std::string& outcome_code)
	{
		timeout_code = outcome_code;
		return *this;
	}

	WorkItemSpec& WorkItemSpec::maxRetries(unsigned count)
	{
		max_retries = count;
		return *this;
	}

	const Id& WorkItemSpec::id() const
	{
		return id_ref;
	}

	const std::string& WorkItemSpec::name() const
	{
		return name_ref;
	}

	const std::string& WorkItemSpec::requiredRole() const
	{
		return required_role;
	}

	const std::string& WorkItemSpec::reviewerRole() const
	{
		return reviewer_role;
	}

	const std::vector<Id>& WorkItemSpec::dependencies() const
	{
		return dependencies_ref;
	}

	bool WorkItemSpec::requiresReview() const
	{
		return review_required;
	}

	bool WorkItemSpec::isAutomatic() const
	{
		return automatic_ref;
	}

	unsigned WorkItemSpec::maxRetries() const
	{
		return max_retries;
	}

	long WorkItemSpec::dueAfterSeconds() const
	{
		return due_after_seconds;
	}

	const std::string& WorkItemSpec::timeoutCode() const
	{
		return timeout_code;
	}
} // namespace tividy

#endif // TIVIDY_ITEMS_WORKITEMSPEC_CPP