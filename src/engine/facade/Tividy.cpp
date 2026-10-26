/**
 * @file Tividy.cpp
 * @brief Role C facade construction/query support and orchestration scaffolds.
 * @details Team-plan split: Ishmael's TividyActions.cpp owns perform, execute, undo, commandFactory,
 * workFor, history, progress, signal, advanceTime, now, participants and connectors.
 */
#include "engine/facade/Tividy.h"
#include "engine/core/Errors.h"
#include <stdexcept>
#include <utility>

namespace tividy
{
	/** @brief Owns the supplied clock, or creates simulated time using the existing SystemClock abstraction.
	 * @details Services reference members declared before it; keep the header's member order unchanged.
	 * CommandFactory construction and WorkflowInstance/IndustryPack destruction need their owners' definitions. */
	Tividy::Tividy(std::unique_ptr<ManualClock> simulated_clock) :
		clock(simulated_clock ? std::move(simulated_clock) :
			std::unique_ptr<ManualClock>(new ManualClock(SystemClock().now()))),
		services{*clock, directory, connectors_ref}, counter(0) {}

	/** @brief Destroys owned instances, packs and collaborators in reverse member order. */
	Tividy::~Tividy() = default;

	/** @brief Lists installed pack keys; this implementation returns map key order. */
	std::vector<std::string> Tividy::packKeys() const
	{
		std::vector<std::string> keys;
		for (const auto& entry : packs) keys.push_back(entry.first);
		return keys;
	}

	/** @brief Finds an installed profile. @throws NotFoundError for an unknown key. */
	const ClientProfile& Tividy::profile(const std::string& pack_key) const
	{
		const auto it = profiles.find(pack_key);
		if (it == profiles.end()) throw NotFoundError("unknown pack: " + pack_key);
		return it->second;
	}

	/** @brief Delegates workflow-to-pack lookup to the owning library; preserves its error policy. */
	std::string Tividy::packOf(const std::string& workflow_name) const { return library.packKeyOf(workflow_name); }

	/** @brief Returns the library's sorted workflow names. */
	std::vector<std::string> Tividy::workflowNames() const { return library.names(); }

	/** @brief Returns the stored instance creation order. No instance is started by inspection. */
	std::vector<Id> Tividy::instanceIds() const { return order; }

	/** @brief Returns an owned instance. @throws NotFoundError for an unknown id. */
	WorkflowInstance& Tividy::instance(const Id& instance_id)
	{
		const auto it = instances.find(instance_id);
		if (it == instances.end()) throw NotFoundError("unknown instance: " + instance_id);
		return *it->second;
	}

	/** @brief Provides const inspection of an owned instance. @throws NotFoundError for an unknown id. */
	const WorkflowInstance& Tividy::instance(const Id& instance_id) const
	{
		const auto it = instances.find(instance_id);
		if (it == instances.end()) throw NotFoundError("unknown instance: " + instance_id);
		return *it->second;
	}

	/** @brief Will install a pack using the five steps in Tividy.h, preserving documented partial effects.
	 * @pre Non-null pack (null error reporting needs agreement).
	 * @details Depends on IndustryPack, StandardWorkflowBuilder, WorkflowDirector, WorkflowLibrary and ClientProfile.
	 * Invariant: duplicate pack rejection occurs before product registration; no transaction/rollback is promised.
	 * @throws std::logic_error Temporary scaffold; no registry entry or product is installed. */
	void Tividy::installPack(std::unique_ptr<IndustryPack> pack)
	{
		(void)pack;
		throw std::logic_error("Tividy::installPack awaits Connor's orchestration implementation");
	}

	/** @brief Will create/start an independent version-linked instance using the documented header sequence.
	 * @details Requires WorkflowDefinition::createInstance, observers and WorkflowInstance::start.
	 * Invariants: unique sequential ids; one insertion into creation order; caller data copied, not shared.
	 * @throws std::logic_error Temporary scaffold; no instance/id is fabricated. */
	Id Tividy::startWorkflow(const std::string& workflow_name, const WorkflowData& data)
	{
		(void)workflow_name;
		(void)data;
		throw std::logic_error("Tividy::startWorkflow awaits Connor's orchestration implementation");
	}
} // namespace tividy
