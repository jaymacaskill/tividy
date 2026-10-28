/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 25 October 2026
*/

/**
 * @file WorkItemIterator.cpp
 * @brief Iterator pattern: pre-order depth-first traversal over a WorkItem subtree.
 */

#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItem.h"
#include "engine/core/Errors.h"

namespace tividy
{
	namespace
	{
		// File-local helper to collect nodes in pre-order
		void collectPreOrder(WorkItem& node, std::vector<WorkItem*>& out)
		{
			out.push_back(&node);
			std::vector<WorkItem*> children = node.children();
			for (std::size_t i = 0; i < children.size(); ++i)
			{
				collectPreOrder(*children[i], out);
			}
		}
	}

	WorkItemIterator::~WorkItemIterator() = default;

	DepthFirstIterator::DepthFirstIterator(WorkItem& root)
		: order(), position(0)
	{
		collectPreOrder(root, order);
	}

	void DepthFirstIterator::first()
	{
		position = 0;
	}

	void DepthFirstIterator::next()
	{
		if (isDone())
		{
			throw EngineError("DepthFirstIterator::next() called when iterator is done");
		}
		++position;
	}

	bool DepthFirstIterator::isDone() const
	{
		return position >= order.size();
	}

	WorkItem& DepthFirstIterator::currentItem() const
	{
		if (isDone())
		{
			throw EngineError("DepthFirstIterator::currentItem() called when iterator is done");
		}
		return *order[position];
	}
} // namespace tividy