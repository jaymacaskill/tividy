/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 28 October 2026

StageItem.cpp
*/

#ifndef TIVIDY_ITEMS_STAGEITEM_CPP
#define TIVIDY_ITEMS_STAGEITEM_CPP

#include "engine/items/StageItem.h"

#include <utility>

#include "engine/core/Errors.h"
#include "engine/core/Types.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemVisitor.h"

namespace tividy
{
	StageItem::StageItem(const WorkItemSpec& spec) : spec_ref(spec), children_ref() { }

	StageItem::~StageItem() { }

	const WorkItemSpec& StageItem::spec() const
	{
		return spec_ref;
	}

	ItemStatus StageItem::status() const
	{
		if (children_ref.empty()) return ItemStatus::Completed;

		bool all_skipped = true;
		bool all_settled = true;
		bool all_terminal = true;
		bool any_started = false;
		for (std::size_t i = 0; i < children_ref.size(); ++i)
		{
			const ItemStatus s = children_ref[i]->status();
			if (s != ItemStatus::Skipped) all_skipped = false;
			if (!isSettled(s)) all_settled = false;
			if (!isTerminal(s)) all_terminal = false;
			if (s != ItemStatus::Created) any_started = true;
		}

		if (all_skipped) return ItemStatus::Skipped;
		if (all_settled) return ItemStatus::Completed;
		if (all_terminal) return ItemStatus::Cancelled;
		if (any_started) return ItemStatus::InProgress;

		return ItemStatus::Created;
	}

	const Id& StageItem::assignee() const
	{
		static const Id nobody;

		return nobody;
	}

	std::vector<WorkItem*> StageItem::children()
	{
		std::vector<WorkItem*> list;
		for (std::size_t i = 0; i < children_ref.size(); ++i)
			list.push_back(children_ref[i].get());

		return list;
	}

	WorkItem& StageItem::add(std::unique_ptr<WorkItem> child)
	{
		if (!child) throw InvalidDefinitionError("Stage " + spec_ref.id() + " cannot add a null child");

		DepthFirstIterator walk(*child);
		for (walk.first(); !walk.isDone(); walk.next())
		{
			const Id id = walk.currentItem().spec().id();
			if (find(id) != 0)
				throw InvalidDefinitionError("Stage " + spec_ref.id() + " already contains an item with id " + id);
		}

		WorkItem& added = *child;
		children_ref.push_back(std::move(child));

		return added;
	}

	WorkItem* StageItem::find(const Id& id)
	{
		if (spec_ref.id() == id) return this;

		for (std::size_t i = 0; i < children_ref.size(); ++i)
		{
			WorkItem* hit = children_ref[i]->find(id);
			if (hit != 0) return hit;
		}

		return 0;
	}

	void StageItem::publish()
	{
		throw UnsupportedOperationError("a stage is driven by its children");
	}

	void StageItem::assign(const Id&)
	{
		throw UnsupportedOperationError("a stage is driven by its children");
	}

	void StageItem::start(const Participant&)
	{
		throw UnsupportedOperationError("a stage is driven by its children");
	}

	void StageItem::submit(const Participant&, const Outcome&)
	{
		throw UnsupportedOperationError("a stage is driven by its children");
	}

	void StageItem::complete(const Participant&, const Outcome&)
	{
		throw UnsupportedOperationError("a stage is driven by its children");
	}

	void StageItem::approve(const Participant&, const Outcome&)
	{
		throw UnsupportedOperationError("a stage is driven by its children");
	}

	void StageItem::reject(const Participant&, const std::string&)
	{
		throw UnsupportedOperationError("a stage is driven by its children");
	}

	void StageItem::escalate(const std::string&)
	{
		throw UnsupportedOperationError("a stage is driven by its children");
	}

	void StageItem::cancel(const std::string& reason)
	{
		for (std::size_t i = 0; i < children_ref.size(); ++i)
			if (!isTerminal(children_ref[i]->status())) children_ref[i]->cancel(reason);
	}

	void StageItem::skip(const std::string& reason)
	{
		for (std::size_t i = 0; i < children_ref.size(); ++i)
		{
			const ItemStatus s = children_ref[i]->status();
			if (s != ItemStatus::Created && s != ItemStatus::Available && s != ItemStatus::Skipped)
				throw InvalidStateError("Stage " + spec_ref.id() + " cannot be skipped: " + children_ref[i]->spec().id() +
										" is " + toString(s));
		}

		for (std::size_t i = 0; i < children_ref.size(); ++i)
		{
			const ItemStatus s = children_ref[i]->status();
			if (s == ItemStatus::Created || s == ItemStatus::Available) children_ref[i]->skip(reason);
		}
	}

	void StageItem::expire(const std::string&, const std::string&)
	{
		throw UnsupportedOperationError("a stage is driven by its children");
	}

	std::unique_ptr<WorkItem> StageItem::clone() const
	{
		std::unique_ptr<StageItem> copy(new StageItem(spec_ref));
		for (std::size_t i = 0; i < children_ref.size(); ++i)
			copy->children_ref.push_back(children_ref[i]->clone());

		return std::unique_ptr<WorkItem>(std::move(copy));
	}

	void StageItem::accept(WorkItemVisitor& visitor) const
	{
		visitor.visitStage(*this);
		for (std::size_t i = 0; i < children_ref.size(); ++i)
			children_ref[i]->accept(visitor);
	}

	std::unique_ptr<WorkItemIterator> StageItem::createIterator()
	{
		return std::unique_ptr<WorkItemIterator>(new DepthFirstIterator(*this));
	}

	void StageItem::bind(WorkContext& context)
	{
		for (std::size_t i = 0; i < children_ref.size(); ++i)
			children_ref[i]->bind(context);
	}
} // namespace tividy

#endif // TIVIDY_ITEMS_STAGEITEM_CPP