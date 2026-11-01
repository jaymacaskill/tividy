/** @file WorkItemVisitor.cpp
 * @brief Read-only task status aggregation for workflow progress.
 */
#include "engine/items/WorkItemVisitor.h"
#include "engine/items/TaskItem.h"

namespace tividy
{
	WorkItemVisitor::~WorkItemVisitor() = default;
	ProgressVisitor::ProgressVisitor() : total_ref(0) {}
	std::size_t ProgressVisitor::total() const { return total_ref; }
	std::size_t ProgressVisitor::count(ItemStatus status) const
	{
		const auto entry = counts.find(status);
		return entry == counts.end() ? 0 : entry->second;
	}

	/** @brief Stages are intentionally ignored by the documented leaf-only progress metric. */
	void ProgressVisitor::visitStage(const StageItem&) {}

	/** @brief Record one task using its observable status. */
	void ProgressVisitor::visitTask(const TaskItem& task)
	{
		++counts[task.status()];
		++total_ref;
	}

	/** @brief Completed and skipped tasks are settled; an empty tree is fully settled. */
	double ProgressVisitor::percentSettled() const
	{
		if (total_ref == 0) return 100.0;
		return 100.0 * (static_cast<double>(count(ItemStatus::Completed))
			+ static_cast<double>(count(ItemStatus::Skipped))) / total_ref;
	}
}
