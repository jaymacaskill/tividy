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
*/

/**
 * @file WorkItemState.cpp
 * @brief Rejects operations that a concrete state has not explicitly allowed.
 */

#include "engine/core/Errors.h"
#include "engine/items/WorkItemState.h"

tividy::WorkItemState::~WorkItemState() = default;

void tividy::WorkItemState::publish(tividy::TaskItem&) const
{
	throw tividy::InvalidStateError("publish is not allowed while " + tividy::toString(status()));
}

void tividy::WorkItemState::assign(tividy::TaskItem&, const tividy::Id&) const
{
	throw tividy::InvalidStateError("assign is not allowed while " + tividy::toString(status()));
}

void tividy::WorkItemState::start(tividy::TaskItem&, const tividy::Participant&) const
{
	throw tividy::InvalidStateError("start is not allowed while " + tividy::toString(status()));
}

void tividy::WorkItemState::submit(tividy::TaskItem&, const tividy::Participant&, const tividy::Outcome&) const
{
	throw tividy::InvalidStateError("submit is not allowed while " + tividy::toString(status()));
}

void tividy::WorkItemState::complete(tividy::TaskItem&, const tividy::Participant&, const tividy::Outcome&) const
{
	throw tividy::InvalidStateError("complete is not allowed while " + tividy::toString(status()));
}

void tividy::WorkItemState::approve(tividy::TaskItem&, const tividy::Participant&, const tividy::Outcome&) const
{
	throw tividy::InvalidStateError("approve is not allowed while " + tividy::toString(status()));
}

void tividy::WorkItemState::reject(tividy::TaskItem&, const tividy::Participant&, const std::string&) const
{
	throw tividy::InvalidStateError("reject is not allowed while " + tividy::toString(status()));
}

void tividy::WorkItemState::escalate(tividy::TaskItem&, const std::string&) const
{
	throw tividy::InvalidStateError("escalate is not allowed while " + tividy::toString(status()));
}

void tividy::WorkItemState::cancel(tividy::TaskItem&, const std::string&) const
{
	throw tividy::InvalidStateError("cancel is not allowed while " + tividy::toString(status()));
}

void tividy::WorkItemState::skip(tividy::TaskItem&, const std::string&) const
{
	throw tividy::InvalidStateError("skip is not allowed while " + tividy::toString(status()));
}

void tividy::WorkItemState::expire(tividy::TaskItem&, const std::string&, const std::string&) const
{
	throw tividy::InvalidStateError("expire is not allowed while " + tividy::toString(status()));
}
