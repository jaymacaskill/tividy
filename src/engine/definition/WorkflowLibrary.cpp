/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 29 October 2026

WorkflowLibrary.cpp
*/

#ifndef TIVIDY_DEFINITION_WORKFLOWLIBRARY_CPP
#define TIVIDY_DEFINITION_WORKFLOWLIBRARY_CPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/definition/WorkflowLibrary.h"
#include "engine/definition/WorkflowDefinition.h"
#include "engine/core/Errors.h"

namespace tividy
{
    void WorkflowLibrary::add(const std::string& pack_key, std::unique_ptr<WorkflowDefinition> definition)
    {
        if (definition == nullptr)
        {
            throw InvalidDefinitionError("Definition cannot be NULL");
        }
    
        definition->validate();
    
        const std::string name = definition->name();
    
        Entry newEntry;
        newEntry.pack_key = pack_key;
        newEntry.definition = std::move(definition);
    
        if (!entries.insert(std::make_pair(name, std::move(newEntry))).second)
        {
            throw InvalidDefinitionError("Name already exists");
        }
    }
    
    std::shared_ptr<const WorkflowDefinition> WorkflowLibrary::get(const std::string& name) const
    {
        auto lookupEntry = entries.find(name);
    
        if (lookupEntry == entries.end())
        {
            throw NotFoundError("Definition with name " + name + " not found");
        }
    
        return lookupEntry->second.definition;
    }

    std::string WorkflowLibrary::packKeyOf(const std::string& name) const
    {
        auto lookupEntry = entries.find(name);
            
        if (lookupEntry == entries.end())
        {
            throw NotFoundError("Definition with name " + name + " not found");
        }

        return lookupEntry->second.pack_key;

    }

    std::vector<std::string> WorkflowLibrary::names() const
    {
        std::vector<std::string> list;

        for (auto entry : entries)
        {
            list.push_back(entry.first);
        }

        return list;
    }

} //namespace tividy

#endif //TIVIDY_DEFINITION_WORKFLOWLIBRARY_CPP