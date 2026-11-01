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

#ifndef TIVIDY_TESTS_WORKFLOW_INSTANCE_SNAPSHOT_FIXTURES_H
#define TIVIDY_TESTS_WORKFLOW_INSTANCE_SNAPSHOT_FIXTURES_H

// Structural collaborators for the real WorkflowInstanceSnapshot.cpp methods.
// The dedicated target excludes libtividy.a. Pending instance/task core, state,
// context, history and deadline methods are fixtures with the real header layouts.
// Iterator, values, events, audit, connectors, commands and assignment are real.
// These tests do not assert the pending runtime's transition or deadline algorithms.

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <utility>
#include "engine/core/Errors.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/runtime/WorkflowInstance.h"

struct InstanceSnapshotFixtureControls
{
	unsigned live_item_mementos = 0;
	unsigned sweep_calls = 0;
	unsigned flow_events = 0;
	std::size_t sweep_result = 0;
	bool sweep_throws = false;
	bool sweep_emits = false;
	tividy::WorkflowInstance* swept_instance = nullptr;
};

inline InstanceSnapshotFixtureControls& snapshotFixtureControls()
{
	static InstanceSnapshotFixtureControls value;
	return value;
}

inline void unexpectedSnapshotFixtureOperation()
{
	throw tividy::EngineError("snapshot operations must not execute item lifecycle work");
}

class InstanceSnapshotFixtureState : public tividy::WorkItemState
{
	public:
		explicit InstanceSnapshotFixtureState(tividy::ItemStatus value) : value(value) {}
		tividy::ItemStatus status() const override { return value; }
		static const InstanceSnapshotFixtureState& at(tividy::ItemStatus value)
		{
			static const std::map<tividy::ItemStatus, InstanceSnapshotFixtureState> states = {
				{tividy::ItemStatus::Created, InstanceSnapshotFixtureState(tividy::ItemStatus::Created)},
				{tividy::ItemStatus::Available, InstanceSnapshotFixtureState(tividy::ItemStatus::Available)},
				{tividy::ItemStatus::Assigned, InstanceSnapshotFixtureState(tividy::ItemStatus::Assigned)},
				{tividy::ItemStatus::InProgress, InstanceSnapshotFixtureState(tividy::ItemStatus::InProgress)},
				{tividy::ItemStatus::UnderReview, InstanceSnapshotFixtureState(tividy::ItemStatus::UnderReview)},
				{tividy::ItemStatus::Escalated, InstanceSnapshotFixtureState(tividy::ItemStatus::Escalated)},
				{tividy::ItemStatus::Completed, InstanceSnapshotFixtureState(tividy::ItemStatus::Completed)},
				{tividy::ItemStatus::Cancelled, InstanceSnapshotFixtureState(tividy::ItemStatus::Cancelled)},
				{tividy::ItemStatus::Skipped, InstanceSnapshotFixtureState(tividy::ItemStatus::Skipped)}
			};
			return states.at(value);
		}
		static void set(tividy::TaskItem& item, tividy::ItemStatus value) { moveTo(item, at(value)); }
		static void seedAssignee(tividy::TaskItem& item, const tividy::Id& id) { setAssignee(item, id); }
	private:
		tividy::ItemStatus value;
};

tividy::WorkItem::~WorkItem() = default;
tividy::TimePoint tividy::WorkItem::assignedAt() const { return TimePoint(); }
tividy::TimePoint tividy::WorkItem::dueAt() const { return TimePoint(); }
std::vector<tividy::WorkItem*> tividy::WorkItem::children() { return {}; }
tividy::WorkItem& tividy::WorkItem::add(std::unique_ptr<WorkItem>) { throw UnsupportedOperationError("not a stage"); }
std::unique_ptr<tividy::WorkItemMemento> tividy::WorkItem::createMemento() const { return {}; }
void tividy::WorkItem::restore(const WorkItemMemento&) {}
bool tividy::WorkItem::isReversible() const { return true; }
tividy::WorkItemState::~WorkItemState() = default;

#define SNAPSHOT_STATE_OPERATION(name, parameters) \
	void tividy::WorkItemState::name parameters const { unexpectedSnapshotFixtureOperation(); }
SNAPSHOT_STATE_OPERATION(publish, (TaskItem&))
SNAPSHOT_STATE_OPERATION(assign, (TaskItem&, const Id&))
SNAPSHOT_STATE_OPERATION(start, (TaskItem&, const Participant&))
SNAPSHOT_STATE_OPERATION(submit, (TaskItem&, const Participant&, const Outcome&))
SNAPSHOT_STATE_OPERATION(complete, (TaskItem&, const Participant&, const Outcome&))
SNAPSHOT_STATE_OPERATION(approve, (TaskItem&, const Participant&, const Outcome&))
SNAPSHOT_STATE_OPERATION(reject, (TaskItem&, const Participant&, const std::string&))
SNAPSHOT_STATE_OPERATION(escalate, (TaskItem&, const std::string&))
SNAPSHOT_STATE_OPERATION(cancel, (TaskItem&, const std::string&))
SNAPSHOT_STATE_OPERATION(skip, (TaskItem&, const std::string&))
SNAPSHOT_STATE_OPERATION(expire, (TaskItem&, const std::string&, const std::string&))
#undef SNAPSHOT_STATE_OPERATION

void tividy::WorkItemState::moveTo(TaskItem& item, const WorkItemState& next) { item.state = &next; }
void tividy::WorkItemState::setAssignee(TaskItem& item, const Id& id)
{
	item.assignee_ref = id;
	item.assigned_at = item.context().clock().now();
}

tividy::WorkItemMemento::WorkItemMemento()
	: status(ItemStatus::Created), assignee(), assigned_at(), rejections(0), failures(0),
	  escalations(0), last_outcome_kind(OutcomeKind::Accepted)
{
	++snapshotFixtureControls().live_item_mementos;
}
tividy::WorkItemMemento::~WorkItemMemento() { --snapshotFixtureControls().live_item_mementos; }

tividy::WorkContext::WorkContext(const Id& id, WorkflowData& data, EventPublisher& events,
	const Clock& clock, ConnectorRegistry& connectors, ParticipantDirectory& directory)
	: instance_id(id), data_ref(data), events_ref(events), clock_ref(clock),
	  connectors_ref(connectors), directory_ref(directory), irreversible(0) {}
const tividy::Id& tividy::WorkContext::instanceId() const { return instance_id; }
tividy::WorkflowData& tividy::WorkContext::data() { return data_ref; }
tividy::EventPublisher& tividy::WorkContext::events() { return events_ref; }
const tividy::Clock& tividy::WorkContext::clock() const { return clock_ref; }
tividy::ConnectorRegistry& tividy::WorkContext::connectors() { return connectors_ref; }
tividy::ParticipantDirectory& tividy::WorkContext::directory() { return directory_ref; }
void tividy::WorkContext::markIrreversible() { ++irreversible; }
unsigned tividy::WorkContext::irreversibleCount() const { return irreversible; }

tividy::TaskItem::TaskItem(const WorkItemSpec& spec)
	: spec_ref(spec), state(&InstanceSnapshotFixtureState::at(ItemStatus::Created)), context_ref(nullptr),
	  rejections_ref(0), failures_ref(0), escalations_ref(0), irreversible_performed(false) {}
tividy::TaskItem::~TaskItem() = default;
const tividy::WorkItemSpec& tividy::TaskItem::spec() const { return spec_ref; }
tividy::ItemStatus tividy::TaskItem::status() const { return state->status(); }
const tividy::Id& tividy::TaskItem::assignee() const { return assignee_ref; }
tividy::TimePoint tividy::TaskItem::assignedAt() const { return assigned_at; }
tividy::TimePoint tividy::TaskItem::dueAt() const { return TimePoint(); }
tividy::WorkItem* tividy::TaskItem::find(const Id& id) { return spec_ref.id() == id ? this : nullptr; }
void tividy::TaskItem::accept(WorkItemVisitor&) const { unexpectedSnapshotFixtureOperation(); }
std::unique_ptr<tividy::WorkItemIterator> tividy::TaskItem::createIterator()
{
	return std::unique_ptr<WorkItemIterator>(new DepthFirstIterator(*this));
}
void tividy::TaskItem::bind(WorkContext& value) { context_ref = &value; }
tividy::WorkContext& tividy::TaskItem::context() const
{
	if (!context_ref) throw InvalidStateError("unbound fixture task");
	return *context_ref;
}
bool tividy::TaskItem::isReversible() const { return !irreversible_performed; }
void tividy::TaskItem::validateOutcome(const Outcome&) const {}
void tividy::TaskItem::onCompleted(const Outcome&) {}
bool tividy::TaskItem::performsIrreversibleWork() const { return false; }

#define SNAPSHOT_TASK_OPERATION(name, parameters) \
	void tividy::TaskItem::name parameters { unexpectedSnapshotFixtureOperation(); }
SNAPSHOT_TASK_OPERATION(publish, ())
SNAPSHOT_TASK_OPERATION(assign, (const Id&))
SNAPSHOT_TASK_OPERATION(start, (const Participant&))
SNAPSHOT_TASK_OPERATION(submit, (const Participant&, const Outcome&))
SNAPSHOT_TASK_OPERATION(complete, (const Participant&, const Outcome&))
SNAPSHOT_TASK_OPERATION(approve, (const Participant&, const Outcome&))
SNAPSHOT_TASK_OPERATION(reject, (const Participant&, const std::string&))
SNAPSHOT_TASK_OPERATION(escalate, (const std::string&))
SNAPSHOT_TASK_OPERATION(cancel, (const std::string&))
SNAPSHOT_TASK_OPERATION(skip, (const std::string&))
SNAPSHOT_TASK_OPERATION(expire, (const std::string&, const std::string&))
#undef SNAPSHOT_TASK_OPERATION

std::unique_ptr<tividy::WorkItemMemento> tividy::TaskItem::createMemento() const
{
	std::unique_ptr<WorkItemMemento> result(new WorkItemMemento);
	result->status = status();
	result->assignee = assignee_ref;
	result->assigned_at = assigned_at;
	return result;
}
void tividy::TaskItem::restore(const WorkItemMemento& value)
{
	state = &InstanceSnapshotFixtureState::at(value.status);
	assignee_ref = value.assignee;
	assigned_at = value.assigned_at;
}

tividy::FlowController::FlowController(WorkflowInstance& owner, const DefinitionPolicies& selected)
	: instance(owner), policies(selected), draining(false) {}
tividy::FlowController::~FlowController() = default;
void tividy::FlowController::onEvent(const WorkflowEvent& event)
{
	++snapshotFixtureControls().flow_events;
	// Controlled seeding of terminal instance statuses; this is not runtime routing.
	if (event.type() == EventType::InstanceCompleted) instance.status_ref = InstanceStatus::Completed;
	if (event.type() == EventType::InstanceFailed) instance.status_ref = InstanceStatus::Failed;
}
tividy::DeadlineMonitor::DeadlineMonitor(WorkflowInstance& owner, const Clock& source)
	: instance(owner), clock(source) {}
std::size_t tividy::DeadlineMonitor::sweep()
{
	auto& control = snapshotFixtureControls();
	++control.sweep_calls;
	control.swept_instance = &instance;
	if (control.sweep_throws) throw EngineError("deadline fixture failure");
	if (control.sweep_emits)
	{
		WorkflowEvent event(EventType::DeadlineMissed, instance.id(), "leaf", clock.now());
		event.byActor("system").withMessage("deadline passed");
		instance.events().notify(event);
	}
	return control.sweep_result;
}
tividy::ActionHistory::ActionHistory(WorkflowInstance& owner) : instance(owner) {}
tividy::ActionHistory::~ActionHistory() = default;
std::size_t tividy::ActionHistory::size() const { return executed.size(); }
tividy::CommandResult tividy::ActionHistory::execute(std::unique_ptr<Command> command)
{
	const CommandResult result = command->execute();
	if (result.success) executed.push_back(std::move(command));
	return result;
}

tividy::WorkflowInstance::WorkflowInstance(const Id& id, const std::string& name, unsigned version,
	const WorkflowData& data, std::unique_ptr<WorkItem> root, const DefinitionPolicies& selected, Services& services)
	: id_ref(id), definition_name(name), definition_version(version), status_ref(InstanceStatus::Created),
	  data_ref(data), audit_observer(audit_ref), root_ref(std::move(root)),
	  context_ref(id, data_ref, events_ref, services.clock, services.connectors, services.directory),
	  flow(*this, selected), deadlines(*this, services.clock), actions_ref(*this)
{
	root_ref->bind(context_ref);
	events_ref.attach(&audit_observer);
	events_ref.attach(&flow);
}
tividy::WorkflowInstance::~WorkflowInstance() = default;
const tividy::Id& tividy::WorkflowInstance::id() const { return id_ref; }
const std::string& tividy::WorkflowInstance::definitionName() const { return definition_name; }
unsigned tividy::WorkflowInstance::definitionVersion() const { return definition_version; }
tividy::InstanceStatus tividy::WorkflowInstance::status() const { return status_ref; }
tividy::WorkflowData& tividy::WorkflowInstance::data() { return data_ref; }
const tividy::WorkflowData& tividy::WorkflowInstance::data() const { return data_ref; }
tividy::WorkItem& tividy::WorkflowInstance::root() { return *root_ref; }
const tividy::WorkItem& tividy::WorkflowInstance::root() const { return *root_ref; }
tividy::WorkItem* tividy::WorkflowInstance::findItem(const Id& id) { return root_ref->find(id); }
tividy::EventPublisher& tividy::WorkflowInstance::events() { return events_ref; }
tividy::AuditLog& tividy::WorkflowInstance::audit() { return audit_ref; }
const tividy::AuditLog& tividy::WorkflowInstance::audit() const { return audit_ref; }
tividy::ActionHistory& tividy::WorkflowInstance::actions() { return actions_ref; }
tividy::WorkContext& tividy::WorkflowInstance::context() { return context_ref; }
void tividy::WorkflowInstance::start() { status_ref = InstanceStatus::Running; }
void tividy::WorkflowInstance::cancel(const std::string&) { status_ref = InstanceStatus::Cancelled; }

class SnapshotFixtureTask : public tividy::TaskItem
{
	public:
		explicit SnapshotFixtureTask(const tividy::Id& id) : TaskItem(tividy::WorkItemSpec(id, id)) {}
		mutable unsigned captures = 0;
		unsigned restores = 0;
		unsigned bindings = 0;
		bool omit_snapshot = false;
		bool fail_capture = false;
		std::unique_ptr<tividy::WorkItemMemento> createMemento() const override
		{
			++captures;
			if (fail_capture) throw tividy::EngineError("capture fixture failure");
			if (omit_snapshot) return {};
			return TaskItem::createMemento();
		}
		void restore(const tividy::WorkItemMemento& value) override { ++restores; TaskItem::restore(value); }
		void bind(tividy::WorkContext& value) override { ++bindings; TaskItem::bind(value); }
		void setRuntime(tividy::ItemStatus value, const tividy::Id& assignee = "")
		{
			InstanceSnapshotFixtureState::seedAssignee(*this, assignee);
			InstanceSnapshotFixtureState::set(*this, value);
		}
		tividy::WorkContext& boundContext() const { return context(); }
		std::unique_ptr<tividy::WorkItem> clone() const override
		{
			unexpectedSnapshotFixtureOperation();
			return {};
		}
	protected:
		void performWork(const tividy::Participant&, tividy::Outcome&) override { unexpectedSnapshotFixtureOperation(); }
};

class SnapshotFixtureStage : public tividy::WorkItem
{
	public:
		explicit SnapshotFixtureStage(const tividy::Id& id = "root") : value(id, id) {}
		mutable unsigned captures = 0;
		unsigned restores = 0;
		unsigned bindings = 0;
		unsigned iterators = 0;
		bool fail_iterator = false;
		const tividy::WorkItemSpec& spec() const override { return value; }
		tividy::ItemStatus status() const override { return tividy::ItemStatus::Created; }
		const tividy::Id& assignee() const override { static const tividy::Id empty; return empty; }
		std::vector<tividy::WorkItem*> children() override
		{
			std::vector<tividy::WorkItem*> result;
			for (auto& child : owned) result.push_back(child.get());
			return result;
		}
		tividy::WorkItem& add(std::unique_ptr<tividy::WorkItem> child) override
		{
			owned.push_back(std::move(child));
			return *owned.back();
		}
		tividy::WorkItem* find(const tividy::Id& id) override
		{
			if (value.id() == id) return this;
			for (auto& child : owned) if (auto* found = child->find(id)) return found;
			return nullptr;
		}
		std::unique_ptr<tividy::WorkItemMemento> createMemento() const override { ++captures; return {}; }
		void restore(const tividy::WorkItemMemento&) override { ++restores; }
		std::unique_ptr<tividy::WorkItemIterator> createIterator() override
		{
			++iterators;
			if (fail_iterator) throw tividy::EngineError("iterator fixture failure");
			return std::unique_ptr<tividy::WorkItemIterator>(new tividy::DepthFirstIterator(*this));
		}
		void bind(tividy::WorkContext& context) override
		{
			++bindings;
			for (auto& child : owned) child->bind(context);
		}
		void reverseChildren() { std::reverse(owned.begin(), owned.end()); }
		std::unique_ptr<tividy::WorkItem> clone() const override { unexpectedSnapshotFixtureOperation(); return {}; }
		void accept(tividy::WorkItemVisitor&) const override { unexpectedSnapshotFixtureOperation(); }
		void publish() override { unexpectedSnapshotFixtureOperation(); }
		void assign(const tividy::Id&) override { unexpectedSnapshotFixtureOperation(); }
		void start(const tividy::Participant&) override { unexpectedSnapshotFixtureOperation(); }
		void submit(const tividy::Participant&, const tividy::Outcome&) override { unexpectedSnapshotFixtureOperation(); }
		void complete(const tividy::Participant&, const tividy::Outcome&) override { unexpectedSnapshotFixtureOperation(); }
		void approve(const tividy::Participant&, const tividy::Outcome&) override { unexpectedSnapshotFixtureOperation(); }
		void reject(const tividy::Participant&, const std::string&) override { unexpectedSnapshotFixtureOperation(); }
		void escalate(const std::string&) override { unexpectedSnapshotFixtureOperation(); }
		void cancel(const std::string&) override { unexpectedSnapshotFixtureOperation(); }
		void skip(const std::string&) override { unexpectedSnapshotFixtureOperation(); }
		void expire(const std::string&, const std::string&) override { unexpectedSnapshotFixtureOperation(); }
	private:
		tividy::WorkItemSpec value;
		std::vector<std::unique_ptr<tividy::WorkItem> > owned;
};

class SnapshotFixtureWrapper : public SnapshotFixtureStage
{
	public:
		explicit SnapshotFixtureWrapper(std::unique_ptr<tividy::WorkItem> inner)
			: SnapshotFixtureStage(inner->spec().id()), inner(std::move(inner)) {}
		const tividy::WorkItemSpec& spec() const override { return inner->spec(); }
		tividy::ItemStatus status() const override { return inner->status(); }
		const tividy::Id& assignee() const override { return inner->assignee(); }
		std::vector<tividy::WorkItem*> children() override { return inner->children(); }
		tividy::WorkItem* find(const tividy::Id& id) override { return id == spec().id() ? this : inner->find(id); }
		std::unique_ptr<tividy::WorkItemMemento> createMemento() const override { ++captures; return inner->createMemento(); }
		void restore(const tividy::WorkItemMemento& value) override { ++restores; inner->restore(value); }
		void bind(tividy::WorkContext& context) override { ++bindings; inner->bind(context); }
	private:
		std::unique_ptr<tividy::WorkItem> inner;
};

#endif
