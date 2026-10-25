/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 24 October 2026

WorkflowData.cpp
*/

#ifndef TIVIDY_CORE_WORKFLOWDATA_CPP
#define TIVIDY_CORE_WORKFLOWDATA_CPP

#include "engine/core/WorkflowData.h"
#include "engine/core/Errors.h"

namespace tividy
{
	const std::map<std::string, std::string>& WorkflowData::all() const
	{
		return values;
	}

	const std::string& WorkflowData::get(const std::string& key) const
	{
		std::map<std::string, std::string>::const_iterator it = values.find(key);
		if (it == values.end()) throw NotFoundError("workflow data key not found: " + key);

		return it->second;
	}

	bool WorkflowData::has(const std::string& key) const
	{
		return values.find(key) != values.end();
	}

	std::string WorkflowData::getOr(const std::string& key, const std::string& fallback) const
	{
		std::map<std::string, std::string>::const_iterator it = values.find(key);
		if (it == values.end()) return fallback;
		
		return it->second;
	}

	void WorkflowData::set(const std::string& key, const std::string& value)
	{
		values[key] = value;
	}
} // namespace tividy

#endif // TIVIDY_CORE_WORKFLOWDATA_CPP