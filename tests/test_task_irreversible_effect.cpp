/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 1 November 2026
*/

/**
 * @file test_task_irreversible_effect.cpp
 * @brief Committed-effect barriers through real tasks, snapshots and commands.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <vector>

#include "engine/commands/ActionHistory.h"
#include "engine/commands/WorkItemCommands.h"
#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Clock.h"
#include "engine/core/Errors.h"
#include "engine/core/Participant.h"
#include "engine/events/Observer.h"
#include "engine/items/StageItem.h"
#include "engine/items/TaskItem.h"
#include "engine/items/WorkContext.h"
#include "engine/runtime/WorkflowInstance.h"

namespace
{
	struct EffectScenario
	{
		unsigned work_calls = 0;
		unsigned external_effects = 0;
		unsigned marking_calls = 1;
		bool early_mark = true;
		bool fallback = true;
		bool reject_validation = false;
		bool fail_before_commit = false;
		bool fail_after_commit = false;
		bool throw_after_commit = false;
		bool fail_completion = false;
	};

	class EffectTask : public tividy::CloneableTask<EffectTask>
	{
		public:
			explicit EffectTask(EffectScenario& scenario)
				: CloneableTask<EffectTask>(tividy::WorkItemSpec("effect", "Effect").requireRole("worker")),
				  scenario(scenario) {}
		protected:
			void validateOutcome(const tividy::Outcome&) const override
			{
				if (scenario.reject_validation)
					throw tividy::ValidationError("outcome rejected");
			}
			void performWork(const tividy::Participant&, tividy::Outcome&) override
			{
				++scenario.work_calls;
				if (scenario.fail_before_commit)
					throw tividy::ExternalFailure("no commit", true);
				++scenario.external_effects;
				if (scenario.early_mark)
				{
					for (unsigned call = 0; call < scenario.marking_calls; ++call)
						markIrreversibleEffect();
				}
				if (scenario.fail_after_commit)
					throw tividy::ExternalFailure("committed reply has no reference", true);
				if (scenario.throw_after_commit)
					throw std::logic_error("post-commit processing failed");
			}
			bool performsIrreversibleWork() const override { return scenario.fallback; }
			void onCompleted(const tividy::Outcome&) override
			{
				if (scenario.fail_completion)
					throw tividy::ExternalFailure("completion hook failed", false);
			}
		private:
			EffectScenario& scenario;
	};

	class EffectEvents : public tividy::EventObserver
	{
		public:
			void onEvent(const tividy::WorkflowEvent& event) override { values.push_back(event); }
			std::vector<tividy::WorkflowEvent> values;
	};

	class TaskIrreversibleEffect : public ::testing::Test
	{
		protected:
			TaskIrreversibleEffect()
				: clock(tividy::TimePoint(std::chrono::seconds(1234))),
				  context("instance", data, events, clock, connectors, directory),
				  actor("worker", "Worker", {"worker"})
			{
				directory.add(actor);
				events.attach(&recorder);
			}
			void prepare(tividy::WorkItem& item)
			{
				item.bind(context);
				item.publish();
				item.assign(actor.id());
				item.start(actor);
				recorder.values.clear();
			}
			tividy::ManualClock clock;
			tividy::WorkflowData data;
			tividy::EventPublisher events;
			tividy::ConnectorRegistry connectors;
			tividy::ParticipantDirectory directory;
			tividy::WorkContext context;
			tividy::Participant actor;
			EffectEvents recorder;
	};

	TEST_F(TaskIrreversibleEffect, CommittedEffectSurvivesPostCommitTechnicalFailure)
	{
		EffectScenario scenario;
		scenario.fail_after_commit = true;
		EffectTask item(scenario);
		prepare(item);
		EXPECT_NO_THROW(item.complete(actor, tividy::Outcome::accepted()));
		EXPECT_EQ(1u, scenario.external_effects);
		EXPECT_EQ(1u, context.irreversibleCount());
		EXPECT_FALSE(item.isReversible());
		EXPECT_EQ(1u, item.failures());
		EXPECT_EQ(tividy::ItemStatus::InProgress, item.status());
		ASSERT_EQ(1u, recorder.values.size());
		EXPECT_EQ(tividy::EventType::ItemFailed, recorder.values[0].type());
		EXPECT_EQ("RETRYABLE", recorder.values[0].outcome().code());
	}

	TEST_F(TaskIrreversibleEffect, RepeatedEarlyMarksAndSuccessfulFallbackCountOnce)
	{
		EffectScenario scenario;
		scenario.marking_calls = 3;
		EffectTask item(scenario);
		prepare(item);
		item.complete(actor, tividy::Outcome::accepted());
		EXPECT_EQ(1u, scenario.external_effects);
		EXPECT_EQ(1u, context.irreversibleCount());
		EXPECT_FALSE(item.isReversible());
		EXPECT_EQ(tividy::ItemStatus::Completed, item.status());
	}

	TEST_F(TaskIrreversibleEffect, EachRetryRecordsTheEffectCommittedByThatAttempt)
	{
		EffectScenario scenario;
		scenario.fail_after_commit = true;
		EffectTask item(scenario);
		prepare(item);
		item.complete(actor, tividy::Outcome::accepted());
		item.complete(actor, tividy::Outcome::accepted());
		EXPECT_EQ(2u, scenario.external_effects);
		EXPECT_EQ(2u, context.irreversibleCount());
		EXPECT_EQ(2u, item.failures());
		scenario.fail_after_commit = false;
		item.complete(actor, tividy::Outcome::accepted());
		EXPECT_EQ(3u, scenario.external_effects);
		EXPECT_EQ(3u, context.irreversibleCount());
		EXPECT_EQ(2u, item.failures());
		EXPECT_EQ(tividy::ItemStatus::Completed, item.status());
	}

	TEST_F(TaskIrreversibleEffect, FailureBeforeCommitDoesNotInventAnEffect)
	{
		EffectScenario scenario;
		scenario.fail_before_commit = true;
		EffectTask item(scenario);
		prepare(item);
		item.complete(actor, tividy::Outcome::accepted());
		EXPECT_EQ(0u, scenario.external_effects);
		EXPECT_EQ(0u, context.irreversibleCount());
		EXPECT_TRUE(item.isReversible());
	}

	TEST_F(TaskIrreversibleEffect, ValidationFailureStopsBeforeWorkAndBarrier)
	{
		EffectScenario scenario;
		scenario.reject_validation = true;
		EffectTask item(scenario);
		prepare(item);
		EXPECT_THROW(item.complete(actor, tividy::Outcome::accepted()), tividy::ValidationError);
		EXPECT_EQ(0u, scenario.work_calls);
		EXPECT_EQ(0u, scenario.external_effects);
		EXPECT_EQ(0u, context.irreversibleCount());
		EXPECT_TRUE(item.isReversible());
		EXPECT_TRUE(recorder.values.empty());
	}

	TEST_F(TaskIrreversibleEffect, ExistingSuccessfulWorkFallbackStillRecordsItsEffect)
	{
		EffectScenario scenario;
		scenario.early_mark = false;
		EffectTask item(scenario);
		prepare(item);
		item.complete(actor, tividy::Outcome::accepted());
		EXPECT_EQ(1u, scenario.external_effects);
		EXPECT_EQ(1u, context.irreversibleCount());
		EXPECT_FALSE(item.isReversible());
	}

	TEST_F(TaskIrreversibleEffect, EarlyMarkDoesNotDependOnTheFallbackHook)
	{
		EffectScenario scenario;
		scenario.fallback = false;
		EffectTask item(scenario);
		prepare(item);
		item.complete(actor, tividy::Outcome::accepted());
		EXPECT_EQ(1u, context.irreversibleCount());
		EXPECT_FALSE(item.isReversible());
	}

	TEST_F(TaskIrreversibleEffect, UnrelatedExceptionAfterCommitPreservesTheBarrier)
	{
		EffectScenario scenario;
		scenario.throw_after_commit = true;
		EffectTask item(scenario);
		prepare(item);
		EXPECT_THROW(item.complete(actor, tividy::Outcome::accepted()), std::logic_error);
		EXPECT_EQ(1u, scenario.external_effects);
		EXPECT_EQ(1u, context.irreversibleCount());
		EXPECT_FALSE(item.isReversible());
		EXPECT_EQ(0u, item.failures());
		EXPECT_TRUE(recorder.values.empty());
	}

	TEST_F(TaskIrreversibleEffect, RestoringRuntimeStateCannotRemoveACommittedEffect)
	{
		EffectScenario scenario;
		scenario.fail_after_commit = true;
		EffectTask item(scenario);
		prepare(item);
		const auto before = item.createMemento();
		item.complete(actor, tividy::Outcome::accepted());
		const auto event_count = recorder.values.size();
		item.restore(*before);
		EXPECT_EQ(tividy::ItemStatus::InProgress, item.status());
		EXPECT_EQ(0u, item.failures());
		EXPECT_EQ(1u, scenario.external_effects);
		EXPECT_EQ(1u, context.irreversibleCount());
		EXPECT_FALSE(item.isReversible());
		EXPECT_EQ(event_count, recorder.values.size());
		scenario.fail_after_commit = false;
		item.complete(actor, tividy::Outcome::accepted());
		EXPECT_EQ(2u, context.irreversibleCount());
	}

	TEST_F(TaskIrreversibleEffect, PrototypeCopyStartsWithItsOwnUnrecordedAttempt)
	{
		EffectScenario scenario;
		EffectTask prototype(scenario);
		auto copy = prototype.clone();
		ASSERT_TRUE(copy);
		EXPECT_EQ(tividy::ItemStatus::Created, copy->status());
		EXPECT_TRUE(copy->isReversible());
		prepare(*copy);
		copy->complete(actor, tividy::Outcome::accepted());
		EXPECT_EQ(1u, scenario.external_effects);
		EXPECT_EQ(1u, context.irreversibleCount());
		EXPECT_FALSE(copy->isReversible());
		EXPECT_TRUE(prototype.isReversible());
		EXPECT_EQ(tividy::ItemStatus::Created, prototype.status());
	}

	TEST_F(TaskIrreversibleEffect, CompletionFailurePreservesTheSingleEarlyBarrier)
	{
		EffectScenario scenario;
		scenario.fail_completion = true;
		EffectTask item(scenario);
		prepare(item);
		EXPECT_THROW(item.complete(actor, tividy::Outcome::accepted()), tividy::ExternalFailure);
		EXPECT_EQ(1u, scenario.external_effects);
		EXPECT_EQ(1u, context.irreversibleCount());
		EXPECT_FALSE(item.isReversible());
		EXPECT_EQ(0u, item.failures());
	}

	struct CommitReply
	{
		unsigned effects = 0;
		bool succeeds = true;
		bool has_reference = false;
	};

	// A normal connector extension records a commit, then supplies its vendor reply.
	class CommitConnector : public tividy::ExternalConnector
	{
		public:
			explicit CommitConnector(CommitReply& reply) : reply(reply) {}
			std::string name() const override { return "commit-api"; }
			tividy::ConnectorResult invoke(const tividy::ConnectorRequest&) override
			{
				if (!reply.succeeds)
					return tividy::ConnectorResult::failure("rejected before commit", false);
				++reply.effects;
				if (reply.has_reference)
					return tividy::ConnectorResult::ok("committed", {{"reference", "ref-1"}});
				return tividy::ConnectorResult::ok("committed");
			}
		private:
			CommitReply& reply;
	};

	class ReplyTask : public tividy::CloneableTask<ReplyTask>
	{
		public:
			ReplyTask() : CloneableTask<ReplyTask>(tividy::WorkItemSpec("commit", "Commit").requireRole("worker")) {}
		protected:
			void performWork(const tividy::Participant&, tividy::Outcome&) override
			{
				const auto reply = context().connectors().require("commit-api").invoke(tividy::ConnectorRequest());
				if (!reply.success)
					throw tividy::ExternalFailure(reply.message, reply.retryable);
				markIrreversibleEffect();
				if (reply.payload.count("reference") == 0)
					throw tividy::ExternalFailure("committed reply has no reference", false);
			}
			bool performsIrreversibleWork() const override { return true; }
	};

	class IrreversibleEffectHistory : public ::testing::Test
	{
		protected:
			IrreversibleEffectHistory()
				: clock(tividy::TimePoint(std::chrono::seconds(1234))),
				  services{clock, directory, connectors}, actor("worker", "Worker", {"worker"})
			{
				directory.add(actor);
				connectors.registerConnector(std::unique_ptr<tividy::ExternalConnector>(new CommitConnector(reply)));
			}
			void start()
			{
				tividy::DefinitionPolicies policies;
				policies.assignment = std::make_shared<tividy::RoleBasedAssignment>();
				std::unique_ptr<tividy::StageItem> root(new tividy::StageItem(tividy::WorkItemSpec("root", "Root")));
				root->add(std::unique_ptr<tividy::WorkItem>(new ReplyTask));
				instance.reset(new tividy::WorkflowInstance("instance", "workflow", 1, tividy::WorkflowData(),
					std::unique_ptr<tividy::WorkItem>(root.release()), policies, services));
				instance->start();
				ASSERT_TRUE(instance->actions().execute(std::unique_ptr<tividy::Command>(
					new tividy::StartCommand(*instance, "commit", actor))).success);
			}
			tividy::ManualClock clock;
			tividy::ParticipantDirectory directory;
			tividy::ConnectorRegistry connectors;
			tividy::Services services;
			tividy::Participant actor;
			CommitReply reply;
			std::unique_ptr<tividy::WorkflowInstance> instance;
	};

	TEST_F(IrreversibleEffectHistory, MalformedSuccessfulReplyBlocksCommandUndo)
	{
		start();
		ASSERT_TRUE(instance->actions().execute(std::unique_ptr<tividy::Command>(
			new tividy::CompleteCommand(*instance, "commit", actor, tividy::Outcome::accepted()))).success);
		EXPECT_EQ(1u, reply.effects);
		EXPECT_EQ(1u, instance->context().irreversibleCount());
		EXPECT_FALSE(instance->findItem("commit")->isReversible());
		EXPECT_FALSE(instance->actions().undoLast().success);
		EXPECT_EQ(2u, instance->actions().size());
		EXPECT_EQ(1u, reply.effects);
	}

	TEST_F(IrreversibleEffectHistory, CompleteSuccessfulReplyRecordsOnlyOneBarrier)
	{
		reply.has_reference = true;
		start();
		ASSERT_TRUE(instance->actions().execute(std::unique_ptr<tividy::Command>(
			new tividy::CompleteCommand(*instance, "commit", actor, tividy::Outcome::accepted()))).success);
		EXPECT_EQ(tividy::InstanceStatus::Completed, instance->status());
		EXPECT_EQ(1u, reply.effects);
		EXPECT_EQ(1u, instance->context().irreversibleCount());
		EXPECT_FALSE(instance->actions().undoLast().success);
	}

	TEST_F(IrreversibleEffectHistory, ConnectorFailureBeforeCommitRemainsUndoable)
	{
		reply.succeeds = false;
		start();
		ASSERT_TRUE(instance->actions().execute(std::unique_ptr<tividy::Command>(
			new tividy::CompleteCommand(*instance, "commit", actor, tividy::Outcome::accepted()))).success);
		EXPECT_EQ(0u, reply.effects);
		EXPECT_EQ(0u, instance->context().irreversibleCount());
		EXPECT_TRUE(instance->actions().undoLast().success);
		EXPECT_EQ(tividy::ItemStatus::InProgress, instance->findItem("commit")->status());
		EXPECT_EQ(0u, reply.effects);
	}
}

