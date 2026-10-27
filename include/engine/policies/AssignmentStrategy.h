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
 * @file AssignmentStrategy.h
 * @brief Strategy: who should do this work?
 */

#ifndef TIVIDY_POLICIES_ASSIGNMENTSTRATEGY_H
#define TIVIDY_POLICIES_ASSIGNMENTSTRATEGY_H

#include <memory>
#include <string>

#include "engine/core/Participant.h"
#include "engine/core/WorkflowData.h"
#include "engine/items/WorkItem.h"

namespace tividy
{

	/**
	 * @brief Strategy (GoF "Strategy"). Context = FlowController. Stateless/immutable, shared by all instances of a definition.
	 */
	class AssignmentStrategy
	{
		public:
			virtual ~AssignmentStrategy();

			/**
			 * @param item      The item that needs someone.
			 * @param role      Role the participant must hold: item.spec().requiredRole(), or the escalation approver role.
			 *                  Empty means "anyone".
			 * @param directory All known participants.
			 * @param data      The instance's data (for data-driven rules).
			 * @return the chosen participant's id, or "" if nobody is eligible (the item stays Available).
			 * @details Deterministic (ties -> smallest id) and read-only.
			 */
			virtual Id chooseParticipant(const WorkItem& item, const std::string& role,
										 const ParticipantDirectory& directory, const WorkflowData& data) const = 0;
	};

	/** The holder of @p role with the smallest id. @p role must be non-empty (an empty role returns ""). */
	class RoleBasedAssignment : public AssignmentStrategy
	{
		public:
			Id chooseParticipant(const WorkItem& item, const std::string& role,
								 const ParticipantDirectory& directory, const WorkflowData& data) const override;
	};

	/**
	 * @brief Rule computed from workflow data: the participant id is stored under @c data_key (e.g. "mentor_id").
	 * If the key is absent, or names a participant who lacks @p role (when role is non-empty), the fallback decides.
	 * @note The fallbackis set at construction and immutable afterwards. This prevents the caller from
	 * accidentally creating a fallback cycle (A → B → A) which would cause infinite recursion.
	 * ChainedDataDrivenAssignmentinstances are permitted and terminate at the first
	 * non-DataDrivenAssignmentfallback, but twoDataDrivenAssignments with the samedata_key` will do the same lookup
	 * twice which is redundant, but not infinite.
	 */
	class DataDrivenAssignment : public AssignmentStrategy
	{
		public:
			/** @param data_key Data key holding a participant id, e.g. "mentor_id".
			 *  @param fallback Used when the data rule cannot decide; must not be null. */
			DataDrivenAssignment(const std::string& data_key, std::shared_ptr<const AssignmentStrategy> fallback);

			Id chooseParticipant(const WorkItem& item, const std::string& role,
								 const ParticipantDirectory& directory, const WorkflowData& data) const override;
		private:
			std::string data_key;
			std::shared_ptr<const AssignmentStrategy> fallback;
	};
} // namespace tividy

#endif // TIVIDY_POLICIES_ASSIGNMENTSTRATEGY_H