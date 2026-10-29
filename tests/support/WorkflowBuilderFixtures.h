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

#ifndef TIVIDY_TESTS_WORKFLOW_BUILDER_FIXTURES_H
#define TIVIDY_TESTS_WORKFLOW_BUILDER_FIXTURES_H

// Structural StageItem and visitor-destructor fixtures for the dedicated builder
// unit-test executable. Link the real builder, definition, validator, director,
// base item, memento, iterator and policies explicitly, without libtividy.a.
// No runtime state/cascade behavior is supplied or tested by these fixtures.
// Global id validation is deliberately left to the real definition validator.

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include "engine/core/Errors.h"
#include "engine/items/StageItem.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemVisitor.h"

struct BuilderStageProbe
{
	static std::vector<tividy::StageItem*> live;
	static bool failNextAdd;
};

std::vector<tividy::StageItem*> BuilderStageProbe::live;
bool BuilderStageProbe::failNextAdd = false;

tividy::WorkItemVisitor::~WorkItemVisitor() = default;
tividy::StageItem::StageItem(const WorkItemSpec& spec) : spec_ref(spec)
{
	BuilderStageProbe::live.push_back(this);
}
tividy::StageItem::~StageItem()
{
	auto& live = BuilderStageProbe::live;
	live.erase(std::find(live.begin(), live.end(), this));
}
const tividy::WorkItemSpec& tividy::StageItem::spec() const { return spec_ref; }
tividy::ItemStatus tividy::StageItem::status() const { return ItemStatus::Created; }
const tividy::Id& tividy::StageItem::assignee() const
{
	static const Id empty;
	return empty;
}
std::vector<tividy::WorkItem*> tividy::StageItem::children()
{
	std::vector<WorkItem*> result;
	for (const auto& child : children_ref) result.push_back(child.get());
	return result;
}
tividy::WorkItem& tividy::StageItem::add(std::unique_ptr<WorkItem> child)
{
	if (BuilderStageProbe::failNextAdd)
	{
		BuilderStageProbe::failNextAdd = false;
		throw EngineError("injected stage insertion failure");
	}
	if (!child) throw InvalidDefinitionError("null fixture child");
	children_ref.push_back(std::move(child));
	return *children_ref.back();
}
tividy::WorkItem* tividy::StageItem::find(const Id& id)
{
	if (spec_ref.id() == id) return this;
	for (const auto& child : children_ref)
		if (WorkItem* found = child->find(id)) return found;
	return nullptr;
}
void tividy::StageItem::accept(WorkItemVisitor& visitor) const
{
	visitor.visitStage(*this);
	for (const auto& child : children_ref) child->accept(visitor);
}
std::unique_ptr<tividy::WorkItemIterator> tividy::StageItem::createIterator()
{
	return std::unique_ptr<WorkItemIterator>(new DepthFirstIterator(*this));
}
std::unique_ptr<tividy::WorkItem> tividy::StageItem::clone() const
{
	throw EngineError("unexpected template clone");
}
void tividy::StageItem::bind(WorkContext&) { throw EngineError("unexpected template bind"); }
void tividy::StageItem::publish() { throw EngineError("unexpected lifecycle call"); }
void tividy::StageItem::assign(const Id&) { throw EngineError("unexpected lifecycle call"); }
void tividy::StageItem::start(const Participant&) { throw EngineError("unexpected lifecycle call"); }
void tividy::StageItem::submit(const Participant&, const Outcome&) { throw EngineError("unexpected lifecycle call"); }
void tividy::StageItem::complete(const Participant&, const Outcome&) { throw EngineError("unexpected lifecycle call"); }
void tividy::StageItem::approve(const Participant&, const Outcome&) { throw EngineError("unexpected lifecycle call"); }
void tividy::StageItem::reject(const Participant&, const std::string&) { throw EngineError("unexpected lifecycle call"); }
void tividy::StageItem::escalate(const std::string&) { throw EngineError("unexpected lifecycle call"); }
void tividy::StageItem::cancel(const std::string&) { throw EngineError("unexpected lifecycle call"); }
void tividy::StageItem::skip(const std::string&) { throw EngineError("unexpected lifecycle call"); }
void tividy::StageItem::expire(const std::string&, const std::string&) { throw EngineError("unexpected lifecycle call"); }

#endif
