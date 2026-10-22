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
 * @file WorkItemIterator.h
 * @brief Iterator pattern: traverse the item tree without exposing how children are stored.
 */

#ifndef TIVIDY_ITEMS_WORKITEMITERATOR_H
#define TIVIDY_ITEMS_WORKITEMITERATOR_H

#include <cstddef>
#include <vector>

namespace tividy
{
	class WorkItem;

	/**
	 * @brief Iterator (GoF "Iterator"). Usage: for (it->first(); !it->isDone(); it->next()) use(it->currentItem());
	 * Takes a snapshot of the order at construction; the tree's structure must not change during iteration (statuses may).
	 */
	class WorkItemIterator
	{
		public:
			virtual ~WorkItemIterator();

			virtual void first() = 0;

			/** @pre !isDone(). */
			virtual void next() = 0;

			virtual bool isDone() const = 0;

			/** @pre !isDone(). @throws EngineError otherwise. */
			virtual WorkItem& currentItem() const = 0;
	};

	/** ConcreteIterator: pre-order depth-first over a subtree (root, then children in order) using WorkItem::children(). */
	class DepthFirstIterator : public WorkItemIterator
	{
		public:
			/** @param root Subtree root; must outlive the iterator. */
			explicit DepthFirstIterator(WorkItem& root);

			void first() override;
			void next() override;
			bool isDone() const override;
			WorkItem& currentItem() const override;
		private:
			std::vector<WorkItem*> order;
			std::size_t position;
	};
} // namespace tividy

#endif // TIVIDY_ITEMS_WORKITEMITERATOR_H