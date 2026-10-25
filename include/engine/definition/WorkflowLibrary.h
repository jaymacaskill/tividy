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
 * @file WorkflowLibrary.h
 * @brief Store of published workflow definitions.
 */

#ifndef TIVIDY_DEFINITION_WORKFLOWLIBRARY_H
#define TIVIDY_DEFINITION_WORKFLOWLIBRARY_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/definition/WorkflowDefinition.h"

namespace tividy
{

	/**
	 * @brief Holds each definition by name, plus which pack published it. Definitions are shared (shared_ptr<const>) because
	 * running instances were cloned from them. Instances remember the version they were created from.
	 */
	class WorkflowLibrary
	{
		public:
			/** @pre definition != nullptr and validates. @throws InvalidDefinitionError if the name already exists. */
			void add(const std::string& pack_key, std::unique_ptr<WorkflowDefinition> definition);

			/** @throws NotFoundError. */
			std::shared_ptr<const WorkflowDefinition> get(const std::string& name) const;

			/** @return key of the pack that published @p name. @throws NotFoundError. */
			std::string packKeyOf(const std::string& name) const;

			/** @return all workflow names, sorted. */
			std::vector<std::string> names() const;
		private:
			struct Entry
			{
				std::string pack_key;
				std::shared_ptr<const WorkflowDefinition> definition;
			};

			std::map<std::string, Entry> entries;
	};
} // namespace tividy

#endif // TIVIDY_DEFINITION_WORKFLOWLIBRARY_H