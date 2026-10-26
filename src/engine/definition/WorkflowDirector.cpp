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

WorkflowDirector.cpp
*/

#ifndef TIVIDY_DEFINITION_WORKFLOWDIRECTOR_CPP
#define TIVIDY_DEFINITION_WORKFLOWDIRECTOR_CPP

/** note: WorkflowDirector public interface is located within the WorkflowBuilder interface */
#include "engine/definition/WorkflowBuilder.h"

namespace tividy
{
	WorkflowDirector::~WorkflowDirector() { }
} // namespace tividy

std::unique_ptr<tividy::WorkflowDefinition> tividy::WorkflowDirector::build(
	tividy::WorkflowBuilder& builder) const
{
	builder.reset(workflowName(), version(), description());
	construct(builder);
	return builder.getResult();
}

#endif // TIVIDY_DEFINITION_WORKFLOWDIRECTOR_CPP
