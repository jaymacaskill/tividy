/**
 * @file Tividy.cpp
 * @brief Implements facade construction, pack installation, workflow startup and lookup operations.
 */
#include "engine/facade/Tividy.h"
#include "engine/core/Errors.h"
#include <stdexcept>
#include <utility>

namespace tividy
{
	/** @brief Uses the supplied simulated clock, or starts a new ManualClock at the current system time.
	 * @details Services holds references to the clock, participant directory and connector registry.
	 * CommandFactory construction and WorkflowInstance/IndustryPack destruction need their owners' definitions. */
	Tividy::Tividy(std::unique_ptr<ManualClock> simulated_clock) : clock(simulated_clock ? std::move(simulated_clock) : std::unique_ptr<ManualClock>(new ManualClock(SystemClock().now()))),
																   services{*clock, directory, connectors_ref}, counter(0) {}

	/** @brief Destroys the facade and its owned instances, packs and collaborators. */
	Tividy::~Tividy() = default;

	/** @brief Returns the keys of installed packs in map order. */
	std::vector<std::string> Tividy::packKeys() const
	{
		std::vector<std::string> keys;
		for (const auto &entry : packs)
			keys.push_back(entry.first);
		return keys;
	}

	/** @brief Finds an installed profile. @throws NotFoundError for an unknown key. */
	const ClientProfile &Tividy::profile(const std::string &pack_key) const
	{
		const auto it = profiles.find(pack_key);
		if (it == profiles.end())
			throw NotFoundError("unknown pack: " + pack_key);
		return it->second;
	}

	/** @brief Returns the key of the pack that registered the workflow.
	 * @throws NotFoundError if the workflow name is not in the library. */
	std::string Tividy::packOf(const std::string &workflow_name) const { return library.packKeyOf(workflow_name); }

	/** @brief Returns the workflow names registered in the library, in the library's order. */
	std::vector<std::string> Tividy::workflowNames() const { return library.names(); }

	/** @brief Returns instance ids in creation order. */
	std::vector<Id> Tividy::instanceIds() const { return order; }

	/** @brief Returns an owned instance. @throws NotFoundError for an unknown id. */
	WorkflowInstance &Tividy::instance(const Id &instance_id)
	{
		const auto it = instances.find(instance_id);
		if (it == instances.end())
			throw NotFoundError("unknown instance: " + instance_id);
		return *it->second;
	}

	/** @brief Provides const inspection of an owned instance. @throws NotFoundError for an unknown id. */
	const WorkflowInstance &Tividy::instance(const Id &instance_id) const
	{
		const auto it = instances.find(instance_id);
		if (it == instances.end())
			throw NotFoundError("unknown instance: " + instance_id);
		return *it->second;
	}

	/** @brief Installs a pack and registers the products it provides.
	 * @details Checks for a null or duplicate pack, stores the pack, then adds its participants,
	 * connectors, built workflow definition and client profile. The operation is not transactional:
	 * if a later step throws, earlier changes remain applied.
	 * @throws EngineError if @p pack is null or its key is already installed. */

	void Tividy::installPack(std::unique_ptr<IndustryPack> pack)
	{
		if (!pack)
			throw EngineError("cannot install null pack");

		const std::string key = pack->key();

		if (packs.count(key))
			throw EngineError("pack already installed: " + key);

		IndustryPack *installed = pack.get();
		packs.emplace(key, std::move(pack));

		for (const auto &participant : installed->createParticipants())
			directory.add(participant);

		auto new_connectors = installed->createConnectors();
		for (auto &connector : new_connectors)
			connectors_ref.registerConnector(std::move(connector));

		StandardWorkflowBuilder builder;
		auto director = installed->createDirector();
		library.add(key, director->build(builder));

		profiles[key] = installed->createProfile();
	}

	/** @brief Creates, records and starts a workflow instance, then returns its generated id.
	 * @details Ids use the sequence "inst-1", "inst-2", and so on. Each instance is created from
	 * the registered workflow definition and receives a notification observer. If startup throws
	 * after the instance is recorded, it remains owned by the facade and its id remains in creation order.
	 * @throws NotFoundError if @p workflow_name is not registered. */

	Id Tividy::startWorkflow(
		const std::string &workflow_name,
		const WorkflowData &data)
	{
		auto definition = library.get(workflow_name);

		const Id id = "inst-" + std::to_string(counter + 1);

		auto new_instance =
			definition->createInstance(id, data, services);

		new_instance->addObserver(
			std::unique_ptr<EventObserver>(
				new NotificationObserver(connectors_ref)));

		instances.emplace(id, std::move(new_instance));
		order.push_back(id);
		++counter;

		instances.at(id)->start();

		return id;
	}

} // namespace tividy
