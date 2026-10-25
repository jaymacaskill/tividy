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
 * @file WorkflowData.h
 * @brief The key/value payload that belongs to ONE workflow instance.
 */

#ifndef TIVIDY_CORE_WORKFLOWDATA_H
#define TIVIDY_CORE_WORKFLOWDATA_H

#include <map>
#include <string>

namespace tividy
{

	/**
	 * @brief String-to-string data bag. Value semantics: copying copies every entry, so two instances created from
	 * the same initial data never share state. The engine never interprets keys; only industry tasks and strategies do.
	 * Keys are lower_snake_case by convention.
	 */
	class WorkflowData
	{
		public:
			bool has(const std::string& key) const;

			/** @return the value. @throws NotFoundError if absent. */
			const std::string& get(const std::string& key) const;

			/** @return the value, or @p fallback if absent. */
			std::string getOr(const std::string& key, const std::string& fallback) const;

			/** Inserts or overwrites. @post get(key) == value. */
			void set(const std::string& key, const std::string& value);

			/** @return all entries ordered by key. */
			const std::map<std::string, std::string>& all() const;
		private:
			std::map<std::string, std::string> values;
	};
} // namespace tividy

#endif // TIVIDY_CORE_WORKFLOWDATA_H