#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"
#include "engine/core/Types.h"
#include "engine/core/WorkflowData.h"
#include "engine/items/WorkItem.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemMemento.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/items/WorkItemVisitor.h"
#include "engine/policies/AssignmentStrategy.h"
#include "engine/policies/EscalationHandler.h"
#include "engine/policies/RoutingStrategy.h"

namespace tividy
{
    namespace
    {
        class FakeWorkItem : public WorkItem
        {
            public:
                explicit FakeWorkItem(const WorkItemSpec& s) : spec_ref(s) { }

                const WorkItemSpec& spec() const override { return spec_ref; }
                ItemStatus status() const override { return ItemStatus::Created; }
                const Id& assignee() const override { return empty_id; }
                WorkItem* find(const Id&) override { return 0; }

                void publish() override { }
                void assign(const Id&) override { }
                void start(const Participant&) override { }
                void submit(const Participant&, const Outcome&) override { }
                void complete(const Participant&, const Outcome&) override { }
                void approve(const Participant&, const Outcome&) override { }
                void reject(const Participant&, const std::string&) override { }
                void escalate(const std::string&) override { }
                void cancel(const std::string&) override { }
                void skip(const std::string&) override { }
                void expire(const std::string&, const std::string&) override { }

                std::unique_ptr<WorkItem> clone() const override { return std::unique_ptr<WorkItem>(); }
                void accept(WorkItemVisitor&) const override { }
                std::unique_ptr<WorkItemIterator> createIterator() override { return std::unique_ptr<WorkItemIterator>(); }
                void bind(WorkContext&) override { }

                WorkItem& add(std::unique_ptr<WorkItem>) override { return *this; }

            private:
                WorkItemSpec spec_ref;
                Id empty_id;
        };

        Participant make_participant(const std::string& id, const std::string& role)
        {
            std::set<std::string> roles;
            roles.insert(role);
            return Participant(id, id + "-name", roles);
        }

        // Static so EscalationRequest::data (which is "never null" per the header)
        // has a stable address across the whole test run.
        WorkflowData& static_data()
        {
            static WorkflowData d;
            return d;
        }

        EscalationRequest make_request(unsigned level)
        {
            EscalationRequest r;
            r.instance_id = "inst-1";
            r.item_id = "task-1";
            r.reason = "overdue";
            r.level = level;
            r.data = &static_data();
            return r;
        }

        TEST(EscalationHandlerTest, EmptyChainReturnsUnresolved)
        {
            RoleEscalationHandler h("senior-analyst", 1);
            EscalationResult r = h.handle(make_request(2));
            EXPECT_FALSE(r.resolved);
            EXPECT_TRUE(r.approver_role.empty());
            EXPECT_TRUE(r.handled_by.empty());
        }

        TEST(EscalationHandlerTest, SingleHandlerResolvesMatchingLevel)
        {
            RoleEscalationHandler h("senior-analyst", 1);
            EscalationResult r = h.handle(make_request(1));
            EXPECT_TRUE(r.resolved);
            EXPECT_EQ(r.approver_role, "senior-analyst");
            EXPECT_EQ(r.handled_by, "senior-analyst");
        }

        TEST(EscalationHandlerTest, ChainFallsThroughToMatchingLevel)
        {
            std::shared_ptr<const EscalationHandler> level2 =
                std::shared_ptr<const EscalationHandler>(new RoleEscalationHandler("compliance-officer", 2));
            std::shared_ptr<const EscalationHandler> chain =
                std::shared_ptr<const EscalationHandler>(new RoleEscalationHandler("senior-analyst", 1, level2));

            EscalationResult r = chain->handle(make_request(2));
            EXPECT_TRUE(r.resolved);
            EXPECT_EQ(r.approver_role, "compliance-officer");
        }

        TEST(EscalationHandlerTest, FirstMatchingHandlerWins)
        {
            std::shared_ptr<const EscalationHandler> level2 =
                std::shared_ptr<const EscalationHandler>(new RoleEscalationHandler("compliance-officer", 2));
            std::shared_ptr<const EscalationHandler> chain =
                std::shared_ptr<const EscalationHandler>(new RoleEscalationHandler("senior-analyst", 1, level2));

            EscalationResult r = chain->handle(make_request(1));
            EXPECT_TRUE(r.resolved);
            EXPECT_EQ(r.approver_role, "senior-analyst");
        }

        TEST(EscalationHandlerTest, ChainWithNoMatchingLevelReturnsUnresolved)
        {
            std::shared_ptr<const EscalationHandler> level2 =
                std::shared_ptr<const EscalationHandler>(new RoleEscalationHandler("compliance-officer", 2));
            std::shared_ptr<const EscalationHandler> chain =
                std::shared_ptr<const EscalationHandler>(new RoleEscalationHandler("senior-analyst", 1, level2));

            EscalationResult r = chain->handle(make_request(3));
            EXPECT_FALSE(r.resolved);
            EXPECT_TRUE(r.approver_role.empty());
            EXPECT_TRUE(r.handled_by.empty());
        }

        TEST(RoleBasedAssignmentTest, PicksSmallestIdWhenMultipleMatch)
        {
            ParticipantDirectory dir;
            dir.add(make_participant("mentor-3", "mentor"));
            dir.add(make_participant("mentor-1", "mentor"));
            dir.add(make_participant("mentor-2", "mentor"));

            RoleBasedAssignment strategy;
            FakeWorkItem item(WorkItemSpec("assign_mentor", "Assign mentor"));
            WorkflowData data;

            EXPECT_EQ(strategy.chooseParticipant(item, "mentor", dir, data), "mentor-1");
        }

        TEST(RoleBasedAssignmentTest, ReturnsEmptyWhenNoMatch)
        {
            ParticipantDirectory dir;
            dir.add(make_participant("hr-1", "hr"));

            RoleBasedAssignment strategy;
            FakeWorkItem item(WorkItemSpec("assign_mentor", "Assign mentor"));
            WorkflowData data;

            EXPECT_EQ(strategy.chooseParticipant(item, "mentor", dir, data), "");
        }

        TEST(RoleBasedAssignmentTest, ReturnsEmptyForEmptyRole)
        {
            ParticipantDirectory dir;
            dir.add(make_participant("u1", "any"));

            RoleBasedAssignment strategy;
            FakeWorkItem item(WorkItemSpec("task", "Task"));
            WorkflowData data;

            EXPECT_EQ(strategy.chooseParticipant(item, "", dir, data), "");
        }

        TEST(DataDrivenAssignmentTest, UsesDataIdWhenParticipantHasRole)
        {
            ParticipantDirectory dir;
            dir.add(make_participant("mentor-1", "mentor"));
            dir.add(make_participant("mentor-2", "mentor"));

            WorkflowData data;
            data.set("assigned_mentor", "mentor-2");

            std::shared_ptr<const AssignmentStrategy> fallback(new RoleBasedAssignment());
            DataDrivenAssignment strategy("assigned_mentor", fallback);

            FakeWorkItem item(WorkItemSpec("assign_mentor", "Assign mentor"));
            EXPECT_EQ(strategy.chooseParticipant(item, "mentor", dir, data), "mentor-2");
        }

        TEST(DataDrivenAssignmentTest, FallsBackWhenParticipantLacksRole)
        {
            ParticipantDirectory dir;
            dir.add(make_participant("mentor-1", "mentor"));
            dir.add(make_participant("hr-1", "hr"));

            WorkflowData data;
            data.set("assigned_mentor", "hr-1");   // names a participant who lacks the mentor role

            std::shared_ptr<const AssignmentStrategy> fallback(new RoleBasedAssignment());
            DataDrivenAssignment strategy("assigned_mentor", fallback);

            WorkItemSpec spec("assign_mentor", "Assign mentor");
            spec.requireRole("mentor");
            FakeWorkItem item(spec);

            EXPECT_EQ(strategy.chooseParticipant(item, "mentor", dir, data), "mentor-1");
        }

        TEST(DataDrivenAssignmentTest, FallsBackWhenDataKeyAbsent)
        {
            ParticipantDirectory dir;
            dir.add(make_participant("mentor-1", "mentor"));

            WorkflowData data;

            std::shared_ptr<const AssignmentStrategy> fallback(new RoleBasedAssignment());
            DataDrivenAssignment strategy("assigned_mentor", fallback);

            FakeWorkItem item(WorkItemSpec("assign_mentor", "Assign mentor"));
            EXPECT_EQ(strategy.chooseParticipant(item, "mentor", dir, data), "mentor-1");
        }

        TEST(DataDrivenAssignmentTest, FallsBackWhenDataIdNamesUnknownParticipant)
        {
            ParticipantDirectory dir;
            dir.add(make_participant("mentor-1", "mentor"));

            WorkflowData data;
            data.set("assigned_mentor", "ghost-user");

            std::shared_ptr<const AssignmentStrategy> fallback(new RoleBasedAssignment());
            DataDrivenAssignment strategy("assigned_mentor", fallback);

            FakeWorkItem item(WorkItemSpec("assign_mentor", "Assign mentor"));
            EXPECT_EQ(strategy.chooseParticipant(item, "mentor", dir, data), "mentor-1");
        }

        TEST(DataDrivenAssignmentTest, ReturnsEmptyWhenFallbackIsNull)
        {
            ParticipantDirectory dir;
            WorkflowData data;
            data.set("assigned_mentor", "ghost-user");

            DataDrivenAssignment strategy("assigned_mentor",
                std::shared_ptr<const AssignmentStrategy>());

            FakeWorkItem item(WorkItemSpec("assign_mentor", "Assign mentor"));
            EXPECT_EQ(strategy.chooseParticipant(item, "mentor", dir, data), "");
        }

        TEST(DependencyRoutingTest, ReturnsEmptyDecision)
        {
            DependencyRouting routing;
            FakeWorkItem item(WorkItemSpec("task", "Task"));
            WorkflowData data;

            RoutingDecision d = routing.decide(item, Outcome::accepted(), data);
            EXPECT_TRUE(d.activate.empty());
            EXPECT_TRUE(d.skip.empty());
        }

        TEST(OutcomeTableRoutingTest, MatchesConfiguredRule)
        {
            OutcomeTableRouting routing;
            routing.addRule("CREDIT_DECLINED",
                            std::vector<Id>{"notify_declined"},
                            std::vector<Id>{"provision"});

            FakeWorkItem item(WorkItemSpec("task", "Task"));
            WorkflowData data;
            RoutingDecision d = routing.decide(item, Outcome::rejected("CREDIT_DECLINED"), data);

            ASSERT_EQ(d.activate.size(), 1u);
            EXPECT_EQ(d.activate[0], "notify_declined");
            ASSERT_EQ(d.skip.size(), 1u);
            EXPECT_EQ(d.skip[0], "provision");
        }

        TEST(OutcomeTableRoutingTest, FallsBackToDefaultWhenKeyUnmatched)
        {
            OutcomeTableRouting routing;
            routing.addRule("SOMETHING_ELSE", std::vector<Id>{"x"}, std::vector<Id>());
            routing.setDefault(std::vector<Id>{"y"}, std::vector<Id>{"z"});

            FakeWorkItem item(WorkItemSpec("task", "Task"));
            WorkflowData data;
            RoutingDecision d = routing.decide(item, Outcome::accepted("UNMATCHED"), data);

            ASSERT_EQ(d.activate.size(), 1u);
            EXPECT_EQ(d.activate[0], "y");
            ASSERT_EQ(d.skip.size(), 1u);
            EXPECT_EQ(d.skip[0], "z");
        }

        TEST(OutcomeTableRoutingTest, ReturnsEmptyWhenNoMatchAndNoDefault)
        {
            OutcomeTableRouting routing;

            FakeWorkItem item(WorkItemSpec("task", "Task"));
            WorkflowData data;
            RoutingDecision d = routing.decide(item, Outcome::accepted("X"), data);

            EXPECT_TRUE(d.activate.empty());
            EXPECT_TRUE(d.skip.empty());
        }

        TEST(OutcomeTableRoutingTest, AddRuleChains)
        {
            OutcomeTableRouting routing;
            routing.addRule("A", std::vector<Id>{"a"}, std::vector<Id>())
                .addRule("B", std::vector<Id>{"b"}, std::vector<Id>());

            FakeWorkItem item(WorkItemSpec("task", "Task"));
            WorkflowData data;

            RoutingDecision ra = routing.decide(item, Outcome::accepted("A"), data);
            ASSERT_EQ(ra.activate.size(), 1u);
            EXPECT_EQ(ra.activate[0], "a");

            RoutingDecision rb = routing.decide(item, Outcome::accepted("B"), data);
            ASSERT_EQ(rb.activate.size(), 1u);
            EXPECT_EQ(rb.activate[0], "b");
        }

        TEST(DataValueRoutingTest, MatchesByDataValue)
        {
            DataValueRouting routing("install_type");
            routing.addRule("SELF_INSTALL", std::vector<Id>{"ship"}, std::vector<Id>{"schedule_tech"});

            FakeWorkItem item(WorkItemSpec("choose_install", "Choose install"));
            WorkflowData data;
            data.set("install_type", "SELF_INSTALL");

            RoutingDecision d = routing.decide(item, Outcome::accepted(), data);
            ASSERT_EQ(d.activate.size(), 1u);
            EXPECT_EQ(d.activate[0], "ship");
            ASSERT_EQ(d.skip.size(), 1u);
            EXPECT_EQ(d.skip[0], "schedule_tech");
        }

        TEST(DataValueRoutingTest, FallsBackToDefaultWhenValueUnmatched)
        {
            DataValueRouting routing("install_type");
            routing.addRule("SELF_INSTALL", std::vector<Id>{"ship"}, std::vector<Id>());
            routing.setDefault(std::vector<Id>{"schedule_tech"}, std::vector<Id>{"ship"});

            FakeWorkItem item(WorkItemSpec("choose_install", "Choose install"));
            WorkflowData data;
            data.set("install_type", "TECHNICIAN");

            RoutingDecision d = routing.decide(item, Outcome::accepted(), data);
            ASSERT_EQ(d.activate.size(), 1u);
            EXPECT_EQ(d.activate[0], "schedule_tech");
        }

        TEST(DataValueRoutingTest, MissingDataKeyUsesDefault)
        {
            DataValueRouting routing("install_type");
            routing.setDefault(std::vector<Id>{"schedule_tech"}, std::vector<Id>{"ship"});

            FakeWorkItem item(WorkItemSpec("choose_install", "Choose install"));
            WorkflowData data;

            RoutingDecision d = routing.decide(item, Outcome::accepted(), data);
            ASSERT_EQ(d.activate.size(), 1u);
            EXPECT_EQ(d.activate[0], "schedule_tech");
        }

        TEST(DataValueRoutingTest, MissingDataKeyWithoutDefaultIsEmpty)
        {
            DataValueRouting routing("install_type");

            FakeWorkItem item(WorkItemSpec("choose_install", "Choose install"));
            WorkflowData data;

            RoutingDecision d = routing.decide(item, Outcome::accepted(), data);
            EXPECT_TRUE(d.activate.empty());
            EXPECT_TRUE(d.skip.empty());
        }
    } // namespace
} // namespace tividy