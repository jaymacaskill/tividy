/** @file WorkItemVisitor.cpp
 * @brief Phase 1 Role C Visitor construction/query support; aggregation remains personal TDD work.
 */
#include "engine/items/WorkItemVisitor.h"
#include <stdexcept>

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

	/** @brief Will aggregate the real TaskItem status. No counter is modified by this scaffold. */
	void ProgressVisitor::visitTask(const TaskItem&)
	{
		throw std::logic_error("ProgressVisitor::visitTask awaits Connor's aggregation implementation");
	}

	/** @brief Will report the documented settled percentage, including the empty-tree boundary. */
	double ProgressVisitor::percentSettled() const
	{
		throw std::logic_error("ProgressVisitor::percentSettled awaits Connor's aggregation implementation");
	}
}
