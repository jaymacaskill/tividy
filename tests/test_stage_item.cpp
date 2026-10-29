#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "engine/core/Errors.h"
#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"
#include "engine/items/StageItem.h"
#include "engine/items/WorkContext.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemMemento.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/items/WorkItemVisitor.h"

/*
TaskItem has no implementation yet, so the leaves here are fakes whose status the tests set directly. They log every call that
reaches them, which lets the tests check order and arguments of the cascades.
*/

namespace tividy
{
    namespace
    {
        std::vector<std::string> g_log;
        int alive = 0;
        alignas(WorkContext) unsigned char context_storage[sizeof(WorkContext)];

        class Leaf : public WorkItem
        {
            public:
                Leaf(const std::string& id, ItemStatus st = ItemStatus::Created) : s(id, "leaf"), st(st), bound_to(0) { ++alive; }
                ~Leaf() { --alive; }
                const WorkItemSpec& spec() const { return s; }
                ItemStatus status() const { return st; }
                const Id& assignee() const { return none; }
                WorkItem* find(const Id& id) { return id == s.id() ? this : 0; }
                void publish() { }
                void assign(const Id&) { }
                void start(const Participant&) { }
                void submit(const Participant&, const Outcome&) { }
                void complete(const Participant&, const Outcome&) { }
                void approve(const Participant&, const Outcome&) { }
                void reject(const Participant&, const std::string&) { }
                void escalate(const std::string&) { }
                void cancel(const std::string& reason) { g_log.push_back("cancel:" + s.id() + ":" + reason); st = ItemStatus::Cancelled; }
                void skip(const std::string& reason) { g_log.push_back("skip:" + s.id() + ":" + reason); st = ItemStatus::Skipped; }
                void expire(const std::string&, const std::string&) { }
                std::unique_ptr<WorkItem> clone() const { return std::unique_ptr<WorkItem>(new Leaf(s.id())); }
                void accept(WorkItemVisitor&) const { g_log.push_back("leaf:" + s.id()); }
                std::unique_ptr<WorkItemIterator> createIterator() { return std::unique_ptr<WorkItemIterator>(); }
                void bind(WorkContext& c) { bound_to = &c; }

                WorkItemSpec s;
                ItemStatus st;
                WorkContext* bound_to;
                Id none;
        };

        class Recorder : public WorkItemVisitor
        {
            public:
                void visitTask(const TaskItem&) { }
                void visitStage(const StageItem& stage) { g_log.push_back("stage:" + stage.spec().id()); }
        };

        Leaf* leaf(StageItem& stage, const std::string& id, ItemStatus st = ItemStatus::Created)
        {
            Leaf* l = new Leaf(id, st);
            stage.add(std::unique_ptr<WorkItem>(l));
            return l;
        }

        StageItem* nest(StageItem& stage, const std::string& id)
        {
            StageItem* n = new StageItem(WorkItemSpec(id, "nested"));
            stage.add(std::unique_ptr<WorkItem>(n));
            return n;
        }

        std::vector<std::string> ids(const std::vector<WorkItem*>& items)
        {
            std::vector<std::string> out;
            for (std::size_t i = 0; i < items.size(); ++i) out.push_back(items[i]->spec().id());
            return out;
        }
    }

// WorkItemVisitor.cpp is not written yet, so its destructor (the vtable anchor) is defined here. Delete this line when that file lands.
    WorkItemVisitor::~WorkItemVisitor() { }

#define EXPECT_UNSUPPORTED(call) \
    try { call; ADD_FAILURE() << #call " should have thrown"; } \
    catch (const UnsupportedOperationError& e) { EXPECT_STREQ(e.what(), "a stage is driven by its children"); }

    class StageItemTest : public ::testing::Test
    {
        protected:
            StageItemTest() : stage(WorkItemSpec("S", "Stage")), actor("u1", "User", std::set<std::string>()) { }
            void SetUp() { g_log.clear(); }
            StageItem stage;
            Participant actor;
    };

    // ---- identity ----

    TEST_F(StageItemTest, ReturnsItsSpecAndHasNoAssignee)
    {
        EXPECT_EQ(stage.spec().id(), "S");
        EXPECT_TRUE(stage.assignee().empty());
        EXPECT_TRUE(stage.children().empty());
    }

    TEST_F(StageItemTest, UsesTheWorkItemDefaultsForMementoAndReversibility)
    {
        EXPECT_TRUE(stage.createMemento() == 0);
        EXPECT_TRUE(stage.isReversible());
    }

    // ---- derived status: one test per rule, in order ----

    TEST_F(StageItemTest, Rule1_NoChildrenIsCompleted)
    {
        EXPECT_EQ(stage.status(), ItemStatus::Completed);
    }

    TEST_F(StageItemTest, Rule2_AllSkippedIsSkipped)
    {
        leaf(stage, "A", ItemStatus::Skipped);
        leaf(stage, "B", ItemStatus::Skipped);
        EXPECT_EQ(stage.status(), ItemStatus::Skipped);
    }

    TEST_F(StageItemTest, Rule3_AllSettledIsCompleted)
    {
        leaf(stage, "A", ItemStatus::Completed);
        leaf(stage, "B", ItemStatus::Skipped);
        EXPECT_EQ(stage.status(), ItemStatus::Completed);
    }

    TEST_F(StageItemTest, Rule3_AllCompletedIsCompleted)
    {
        leaf(stage, "A", ItemStatus::Completed);
        EXPECT_EQ(stage.status(), ItemStatus::Completed);
    }

    TEST_F(StageItemTest, Rule4_AllTerminalWithACancelledIsCancelled)
    {
        leaf(stage, "A", ItemStatus::Completed);
        leaf(stage, "B", ItemStatus::Cancelled);
        EXPECT_EQ(stage.status(), ItemStatus::Cancelled);
    }

    TEST_F(StageItemTest, Rule4_AllCancelledIsCancelled)
    {
        leaf(stage, "A", ItemStatus::Cancelled);
        leaf(stage, "B", ItemStatus::Cancelled);
        EXPECT_EQ(stage.status(), ItemStatus::Cancelled);
    }

    TEST_F(StageItemTest, Rule5_AnyChildPastCreatedIsInProgress)
    {
        const ItemStatus past[] = { ItemStatus::Available, ItemStatus::Assigned, ItemStatus::InProgress,
                                    ItemStatus::UnderReview, ItemStatus::Escalated, ItemStatus::Completed,
                                    ItemStatus::Cancelled, ItemStatus::Skipped };
        for (std::size_t i = 0; i < 8; ++i)
        {
            StageItem s(WorkItemSpec("X", "x"));
            leaf(s, "A", ItemStatus::Created);
            leaf(s, "B", past[i]);
            EXPECT_EQ(s.status(), ItemStatus::InProgress) << toString(past[i]);
        }
    }

    TEST_F(StageItemTest, Rule6_AllCreatedIsCreated)
    {
        leaf(stage, "A");
        leaf(stage, "B");
        EXPECT_EQ(stage.status(), ItemStatus::Created);
    }

    TEST_F(StageItemTest, StatusIsEvaluatedOnEveryCall)
    {
        Leaf* a = leaf(stage, "A");
        EXPECT_EQ(stage.status(), ItemStatus::Created);
        a->st = ItemStatus::InProgress;
        EXPECT_EQ(stage.status(), ItemStatus::InProgress);
        a->st = ItemStatus::Completed;
        EXPECT_EQ(stage.status(), ItemStatus::Completed);
    }

    TEST_F(StageItemTest, NestedStagesDeriveTheirStatusFromTheirOwnChildren)
    {
        StageItem* inner = nest(stage, "N");
        leaf(*inner, "B", ItemStatus::Completed);
        leaf(stage, "A", ItemStatus::Completed);
        EXPECT_EQ(inner->status(), ItemStatus::Completed);
        EXPECT_EQ(stage.status(), ItemStatus::Completed);
    }

    // ---- add / children / ownership ----

    TEST_F(StageItemTest, AddAppendsInOrderAndReturnsTheChild)
    {
        Leaf* a = new Leaf("A");
        WorkItem& returned = stage.add(std::unique_ptr<WorkItem>(a));
        leaf(stage, "B");
        leaf(stage, "C");
        EXPECT_EQ(&returned, a);
        std::vector<std::string> expected;
        expected.push_back("A"); expected.push_back("B"); expected.push_back("C");
        EXPECT_EQ(ids(stage.children()), expected);
    }

    TEST_F(StageItemTest, AddRejectsADuplicateIdAmongItsChildren)
    {
        leaf(stage, "A");
        EXPECT_THROW(stage.add(std::unique_ptr<WorkItem>(new Leaf("A"))), InvalidDefinitionError);
        EXPECT_EQ(stage.children().size(), 1u);
    }

    TEST_F(StageItemTest, AddRejectsTheStagesOwnId)
    {
        EXPECT_THROW(stage.add(std::unique_ptr<WorkItem>(new Leaf("S"))), InvalidDefinitionError);
    }

    TEST_F(StageItemTest, AddRejectsADuplicateDeepInTheTree)
    {
        StageItem* inner = nest(stage, "N");
        leaf(*inner, "B");
        EXPECT_THROW(stage.add(std::unique_ptr<WorkItem>(new Leaf("B"))), InvalidDefinitionError);
    }

    TEST_F(StageItemTest, AddRejectsASubtreeThatCollidesWithTheExistingTree)
    {
        leaf(stage, "A");
        StageItem* incoming = new StageItem(WorkItemSpec("N", "nested"));
        leaf(*incoming, "A");
        EXPECT_THROW(stage.add(std::unique_ptr<WorkItem>(incoming)), InvalidDefinitionError);
        EXPECT_EQ(stage.children().size(), 1u);
    }

    TEST_F(StageItemTest, AddRejectsANullChild)
    {
        EXPECT_THROW(stage.add(std::unique_ptr<WorkItem>()), InvalidDefinitionError);
    }

    TEST_F(StageItemTest, ARejectedChildIsDestroyedNotLeaked)
    {
        {
            StageItem s(WorkItemSpec("X", "x"));
            leaf(s, "A");
            EXPECT_THROW(s.add(std::unique_ptr<WorkItem>(new Leaf("A"))), InvalidDefinitionError);
            EXPECT_EQ(alive, 1);
        }
        EXPECT_EQ(alive, 0);
    }

    TEST_F(StageItemTest, DestroyingAStageDestroysItsWholeSubtree)
    {
        {
            StageItem s(WorkItemSpec("X", "x"));
            leaf(s, "A");
            StageItem* inner = nest(s, "N");
            leaf(*inner, "B");
            EXPECT_EQ(alive, 2);
        }
        EXPECT_EQ(alive, 0);
    }

    // ---- find ----

    TEST_F(StageItemTest, FindReturnsTheStageItself)
    {
        EXPECT_EQ(stage.find("S"), &stage);
    }

    TEST_F(StageItemTest, FindReturnsChildrenAtAnyDepth)
    {
        Leaf* a = leaf(stage, "A");
        StageItem* inner = nest(stage, "N");
        Leaf* b = leaf(*inner, "B");
        EXPECT_EQ(stage.find("A"), a);
        EXPECT_EQ(stage.find("N"), inner);
        EXPECT_EQ(stage.find("B"), b);
    }

    TEST_F(StageItemTest, FindReturnsNullForAnUnknownId)
    {
        leaf(stage, "A");
        EXPECT_TRUE(stage.find("nope") == 0);
    }

    // ---- operations a stage refuses ----

    TEST_F(StageItemTest, AStageIsDrivenByItsChildrenSoDirectLifecycleCallsThrow)
    {
        EXPECT_UNSUPPORTED(stage.publish());
        EXPECT_UNSUPPORTED(stage.assign("u1"));
        EXPECT_UNSUPPORTED(stage.start(actor));
        EXPECT_UNSUPPORTED(stage.submit(actor, Outcome::accepted()));
        EXPECT_UNSUPPORTED(stage.complete(actor, Outcome::accepted()));
        EXPECT_UNSUPPORTED(stage.approve(actor, Outcome::accepted()));
        EXPECT_UNSUPPORTED(stage.reject(actor, "no"));
        EXPECT_UNSUPPORTED(stage.escalate("help"));
        EXPECT_UNSUPPORTED(stage.expire("late", "deadline"));
    }

    // ---- cancel cascade ----

    TEST_F(StageItemTest, CancelCascadesToEveryNonTerminalChildInOrder)
    {
        leaf(stage, "A");
        leaf(stage, "B", ItemStatus::InProgress);
        leaf(stage, "C", ItemStatus::Completed);
        leaf(stage, "D", ItemStatus::Cancelled);
        leaf(stage, "E", ItemStatus::Skipped);
        stage.cancel("why");
        std::vector<std::string> expected;
        expected.push_back("cancel:A:why");
        expected.push_back("cancel:B:why");
        EXPECT_EQ(g_log, expected);
    }

    TEST_F(StageItemTest, CancelReachesNestedStages)
    {
        StageItem* inner = nest(stage, "N");
        leaf(*inner, "B", ItemStatus::Assigned);
        stage.cancel("stop");
        ASSERT_EQ(g_log.size(), 1u);
        EXPECT_EQ(g_log[0], "cancel:B:stop");
        EXPECT_EQ(stage.status(), ItemStatus::Cancelled);
    }

    TEST_F(StageItemTest, CancelOnAnEmptyStageDoesNothing)
    {
        EXPECT_NO_THROW(stage.cancel("x"));
        EXPECT_TRUE(g_log.empty());
    }

    // ---- skip cascade ----

    TEST_F(StageItemTest, SkipSkipsCreatedAndAvailableChildrenAndLeavesSkippedOnes)
    {
        leaf(stage, "A", ItemStatus::Created);
        leaf(stage, "B", ItemStatus::Available);
        leaf(stage, "C", ItemStatus::Skipped);
        stage.skip("n/a");
        std::vector<std::string> expected;
        expected.push_back("skip:A:n/a");
        expected.push_back("skip:B:n/a");
        EXPECT_EQ(g_log, expected);
        EXPECT_EQ(stage.status(), ItemStatus::Skipped);
    }

    TEST_F(StageItemTest, SkipThrowsIfAnyChildHasStarted)
    {
        const ItemStatus started[] = { ItemStatus::Assigned, ItemStatus::InProgress, ItemStatus::UnderReview,
                                       ItemStatus::Escalated, ItemStatus::Completed, ItemStatus::Cancelled };
        for (std::size_t i = 0; i < 6; ++i)
        {
            StageItem s(WorkItemSpec("X", "x"));
            leaf(s, "A");
            leaf(s, "B", started[i]);
            EXPECT_THROW(s.skip("r"), InvalidStateError) << toString(started[i]);
        }
    }

    TEST_F(StageItemTest, ARefusedSkipChangesNothing)
    {
        Leaf* a = leaf(stage, "A");
        leaf(stage, "B", ItemStatus::InProgress);
        EXPECT_THROW(stage.skip("r"), InvalidStateError);
        EXPECT_TRUE(g_log.empty());
        EXPECT_EQ(a->status(), ItemStatus::Created);
    }

    TEST_F(StageItemTest, SkipReachesNestedStages)
    {
        StageItem* inner = nest(stage, "N");
        leaf(*inner, "B");
        stage.skip("r");
        ASSERT_EQ(g_log.size(), 1u);
        EXPECT_EQ(g_log[0], "skip:B:r");
    }

    // ---- clone (Prototype) ----

    TEST_F(StageItemTest, CloneIsADeepCopyOfTheTemplate)
    {
        Leaf* a = leaf(stage, "A", ItemStatus::InProgress);
        StageItem* inner = nest(stage, "N");
        leaf(*inner, "B", ItemStatus::Completed);

        std::unique_ptr<WorkItem> copy = stage.clone();
        EXPECT_NE(copy.get(), &stage);
        EXPECT_EQ(copy->spec().id(), "S");
        EXPECT_EQ(copy->status(), ItemStatus::Created);
        std::vector<std::string> expected;
        expected.push_back("A"); expected.push_back("N");
        EXPECT_EQ(ids(copy->children()), expected);
        EXPECT_NE(copy->find("A"), a);
        EXPECT_NE(copy->find("N"), inner);
        EXPECT_TRUE(copy->find("B") != 0);
        EXPECT_NE(copy->find("B"), stage.find("B"));
    }

    TEST_F(StageItemTest, CloneSharesNoMutableStateWithTheOriginal)
    {
        Leaf* a = leaf(stage, "A");
        std::unique_ptr<WorkItem> copy = stage.clone();
        a->st = ItemStatus::InProgress;
        EXPECT_EQ(copy->status(), ItemStatus::Created);
        EXPECT_EQ(stage.status(), ItemStatus::InProgress);
    }

    TEST_F(StageItemTest, CloneOfAnEmptyStageIsAnEmptyStage)
    {
        std::unique_ptr<WorkItem> copy = stage.clone();
        EXPECT_TRUE(copy->children().empty());
        EXPECT_EQ(copy->spec().id(), "S");
    }

    TEST_F(StageItemTest, CloneDoesNotLeakOrDestroyTheOriginalsChildren)
    {
        leaf(stage, "A");
        {
            std::unique_ptr<WorkItem> copy = stage.clone();
            EXPECT_EQ(alive, 2);
        }
        EXPECT_EQ(alive, 1);
    }

    // ---- visitor ----

    TEST_F(StageItemTest, AcceptVisitsTheStageThenEachChildDepthFirst)
    {
        leaf(stage, "A");
        StageItem* inner = nest(stage, "N");
        leaf(*inner, "B");
        leaf(stage, "C");
        Recorder visitor;
        const StageItem& view = stage;
        view.accept(visitor);
        std::vector<std::string> expected;
        expected.push_back("stage:S"); expected.push_back("leaf:A"); expected.push_back("stage:N");
        expected.push_back("leaf:B"); expected.push_back("leaf:C");
        EXPECT_EQ(g_log, expected);
    }

    // ---- iterator ----

    TEST_F(StageItemTest, CreateIteratorWalksTheTreeDepthFirstFromTheStage)
    {
        leaf(stage, "A");
        StageItem* inner = nest(stage, "N");
        leaf(*inner, "B");
        leaf(stage, "C");
        std::unique_ptr<WorkItemIterator> it = stage.createIterator();
        std::vector<std::string> seen;
        for (it->first(); !it->isDone(); it->next()) seen.push_back(it->currentItem().spec().id());
        std::vector<std::string> expected;
        expected.push_back("S"); expected.push_back("A"); expected.push_back("N");
        expected.push_back("B"); expected.push_back("C");
        EXPECT_EQ(seen, expected);
    }

    // ---- bind ----

    TEST_F(StageItemTest, BindAttachesTheContextToEveryDescendant)
    {
        Leaf* a = leaf(stage, "A");
        StageItem* inner = nest(stage, "N");
        Leaf* b = leaf(*inner, "B");
        WorkContext& ctx = *reinterpret_cast<WorkContext*>(context_storage);
        stage.bind(ctx);
        EXPECT_EQ(a->bound_to, &ctx);
        EXPECT_EQ(b->bound_to, &ctx);
    }

    TEST_F(StageItemTest, BindOnAnEmptyStageIsHarmless)
    {
        WorkContext& ctx = *reinterpret_cast<WorkContext*>(context_storage);
        EXPECT_NO_THROW(stage.bind(ctx));
    }
}