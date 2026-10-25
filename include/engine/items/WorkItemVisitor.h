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
 * @file WorkItemVisitor.h
 * @brief Visitor pattern: operations over the item tree without editing the item classes.
 */

#ifndef TIVIDY_ITEMS_WORKITEMVISITOR_H
#define TIVIDY_ITEMS_WORKITEMVISITOR_H

#include <cstddef>
#include <map>

#include "engine/core/Types.h"

namespace tividy
{
	class TaskItem;
	class StageItem;

	/** Visitor (GoF "Visitor"). Elements: TaskItem, StageItem. Read-only. Decorators are transparent (accept() forwards). */
	class WorkItemVisitor
	{
		public:
			virtual ~WorkItemVisitor();

			virtual void visitTask(const TaskItem& task) = 0;

			/** Called once per stage BEFORE its children (pre-order). */
			virtual void visitStage(const StageItem& stage) = 0;
	};

	/** ConcreteVisitor: counts TaskItems by status; stages are ignored (their status is derived). */
	class ProgressVisitor : public WorkItemVisitor
	{
		public:
			ProgressVisitor();

			void visitTask(const TaskItem& task) override;
			void visitStage(const StageItem& stage) override;

			std::size_t total() const;                 /**< Tasks visited. */

			std::size_t count(ItemStatus status) const;

			/** @return 100 * (Completed + Skipped) / total, or 100.0 if total == 0. */
			double percentSettled() const;
		private:
			std::map<ItemStatus, std::size_t> counts;
			std::size_t total_ref;
	};
} // namespace tividy

#endif // TIVIDY_ITEMS_WORKITEMVISITOR_H