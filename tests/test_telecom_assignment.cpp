#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>

#include "engine/core/Participant.h"
#include "engine/core/WorkflowData.h"
#include "engine/items/WorkItem.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemMemento.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/items/WorkItemVisitor.h"
#include "industries/telecom/TelecomParts.h"

namespace tividy
{
    namespace telecom
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
                std::unique_ptr<WorkItemMemento> createMemento() const override { return std::unique_ptr<WorkItemMemento>(); }
                void restore(const WorkItemMemento&) override { }
                bool isReversible() const override { return true; }
                void bind(WorkContext&) override { }
                WorkItem& add(std::unique_ptr<WorkItem>) override { return *this; }

            private:
                WorkItemSpec spec_ref;
                Id empty_id;
            };

            Participant make_participant(const std::string& id,
                                        const std::string& role,
                                        const std::string& region = "")
            {
                std::set<std::string> roles;
                roles.insert(role);
                Participant p(id, id + "-name", roles);
                if (!region.empty())
                    p.setAttribute("region", region);
                return p;
            }

            WorkflowData data_with_region(const std::string& region)
            {
                WorkflowData d;
                d.set("region", region);
                return d;
            }
        } // namespace

        TEST(RegionalTechnicianAssignmentTest, OrdinaryRoleSelectsFirstEligible)
        {
            ParticipantDirectory dir;
            dir.add(make_participant("care-3", "customer-care"));
            dir.add(make_participant("care-1", "customer-care"));
            dir.add(make_participant("care-2", "customer-care"));

            RegionalTechnicianAssignment strategy;
            FakeWorkItem item(WorkItemSpec("task", "Task"));
            WorkflowData data;

            EXPECT_EQ(strategy.chooseParticipant(item, "customer-care", dir, data), "care-1");
        }

        TEST(RegionalTechnicianAssignmentTest, TechnicianFromCorrectRegionIsSelected)
        {
            ParticipantDirectory dir;
            dir.add(make_participant("tech-gp-1", "field-technician", "gauteng"));
            dir.add(make_participant("tech-wc-1", "field-technician", "western-cape"));

            RegionalTechnicianAssignment strategy;
            FakeWorkItem item(WorkItemSpec("task", "Task"));
            WorkflowData data = data_with_region("western-cape");

            EXPECT_EQ(strategy.chooseParticipant(item, "field-technician", dir, data), "tech-wc-1");
        }

        TEST(RegionalTechnicianAssignmentTest, NoTechnicianInRegionReturnsEmpty)
        {
            ParticipantDirectory dir;
            dir.add(make_participant("tech-gp-1", "field-technician", "gauteng"));

            RegionalTechnicianAssignment strategy;
            FakeWorkItem item(WorkItemSpec("task", "Task"));
            WorkflowData data = data_with_region("limpopo");

            EXPECT_EQ(strategy.chooseParticipant(item, "field-technician", dir, data), "");
        }

        TEST(RegionalTechnicianAssignmentTest, MissingRoleOrRegionReturnsEmpty)
        {
            ParticipantDirectory dir;
            dir.add(make_participant("tech-gp-1", "field-technician", "gauteng"));

            RegionalTechnicianAssignment strategy;
            FakeWorkItem item(WorkItemSpec("task", "Task"));
            WorkflowData data = data_with_region("gauteng");

            EXPECT_EQ(strategy.chooseParticipant(item, "", dir, data), "");

            WorkflowData no_region;
            EXPECT_EQ(strategy.chooseParticipant(item, "field-technician", dir, no_region), "");
        }
    } // namespace telecom
} // namespace tividy