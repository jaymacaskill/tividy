#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "engine/core/Errors.h"
#include "engine/core/Outcome.h"
#include "engine/core/Participant.h"
#include "engine/items/StageItem.h"
#include "engine/items/WorkContext.h"
#include "engine/items/WorkItemDecorator.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/items/WorkItemVisitor.h"

// The WorkItemMemento constructor is private (friend: TaskItem, not written yet), so this file opens it for the tests only.
#define private public
#include "engine/items/WorkItemMemento.h"
#undef private

/*
The wrapped items here are fakes that log every call that reaches them as "<item id>:<operation>[:arguments]", so each test can check
exactly what a decorator forwarded, with which arguments, and in what order.
*/

// IMPORTANT: THIS FILE WILL HAVE CHANGES THAT MATCH THE CORRECT STANDARDS OF THE IMPLEMENTATION (StageItem has been merged)

namespace tividy
{
    namespace
    {
        std::vector<std::string> g_log;
        int alive = 0;
        int g_clones = 0;
        alignas(WorkContext) unsigned char context_storage[sizeof(WorkContext)];
        alignas(WorkContext) unsigned char other_context_storage[sizeof(WorkContext)];
        alignas(WorkItemVisitor) unsigned char visitor_storage[sizeof(WorkItemVisitor)];

        class Probe : public WorkItem
        {
            public:
                explicit Probe(const WorkItemSpec& spec)
                    : s(spec), st(ItemStatus::Created), assigned(TimePoint() + std::chrono::hours(7)), due(TimePoint() + std::chrono::hours(9)),
                      reversible(true), found(0), visited(0), made(0), restored(0), bound(0), fail_bind(false) { ++alive; }
                ~Probe() { --alive; }

                const WorkItemSpec& spec() const { return s; }
                ItemStatus status() const { return st; }
                const Id& assignee() const { return who; }
                TimePoint assignedAt() const { return assigned; }
                TimePoint dueAt() const { return due; }
                std::vector<WorkItem*> children() { return kids; }
                WorkItem& add(std::unique_ptr<WorkItem> child)
                {
                    log("add:" + child->spec().id());
                    owned.push_back(std::move(child));
                    return *owned.back();
                }
                WorkItem* find(const Id& id) { return id == s.id() ? this : (id == found_id ? found : 0); }
                void publish() { log("publish"); }
                void assign(const Id& p) { log("assign:" + p); }
                void start(const Participant& a) { log("start:" + a.id()); }
                void submit(const Participant& a, const Outcome&) { log("submit:" + a.id()); }
                void complete(const Participant& a, const Outcome&) { log("complete:" + a.id()); }
                void approve(const Participant& a, const Outcome&) { log("approve:" + a.id()); }
                void reject(const Participant& a, const std::string& r) { log("reject:" + a.id() + ":" + r); }
                void escalate(const std::string& r) { log("escalate:" + r); }
                void cancel(const std::string& r) { log("cancel:" + r); st = ItemStatus::Cancelled; }
                void skip(const std::string& r) { log("skip:" + r); st = ItemStatus::Skipped; }
                void expire(const std::string& c, const std::string& r) { log("expire:" + c + ":" + r); }
                std::unique_ptr<WorkItem> clone() const { ++g_clones; return std::unique_ptr<WorkItem>(new Probe(s)); }
                void accept(WorkItemVisitor& v) const { visited = &v; }
                std::unique_ptr<WorkItemIterator> createIterator() { return std::unique_ptr<WorkItemIterator>(new DepthFirstIterator(*this)); }
                std::unique_ptr<WorkItemMemento> createMemento() const
                {
                    std::unique_ptr<WorkItemMemento> m(new WorkItemMemento());
                    made = m.get();
                    return m;
                }
                void restore(const WorkItemMemento& m) { restored = &m; }
                bool isReversible() const { return reversible; }
                void bind(WorkContext& c)
                {
                    if (fail_bind || (bound != 0 && bound != &c)) throw InvalidStateError("probe is already bound to a different context");
                    bound = &c;
                }

                WorkItemSpec s;
                ItemStatus st;
                Id who;
                TimePoint assigned;
                TimePoint due;
                bool reversible;
                std::vector<WorkItem*> kids;
                std::vector<std::unique_ptr<WorkItem> > owned;
                Id found_id;
                WorkItem* found;
                mutable WorkItemVisitor* visited;
                mutable WorkItemMemento* made;
                const WorkItemMemento* restored;
                WorkContext* bound;
                bool fail_bind;
            private:
                void log(const std::string& what) { g_log.push_back(s.id() + ":" + what); }
        };

        class PassThrough : public WorkItemDecorator
        {
            public:
                explicit PassThrough(std::unique_ptr<WorkItem> c) : WorkItemDecorator(std::move(c)) { }
                std::unique_ptr<WorkItem> clone() const { return std::unique_ptr<WorkItem>(new PassThrough(cloneComponent())); }
                WorkContext& boundContext() const { return context(); }
        };

        WorkItemSpec plain(const std::string& id = "T") { return WorkItemSpec(id, "Task"); }

        WorkItemSpec guarded()
        {
            WorkItemSpec s("T", "Task");
            s.requireRole("manager");
            s.reviewedBy("lead");
            return s;
        }

        std::set<std::string> roles(const std::string& a = "", const std::string& b = "")
        {
            std::set<std::string> r;
            if (!a.empty()) r.insert(a);
            if (!b.empty()) r.insert(b);
            return r;
        }

        std::vector<std::string> log(const std::string& a = "", const std::string& b = "", const std::string& c = "")
        {
            std::vector<std::string> v;
            if (!a.empty()) v.push_back(a);
            if (!b.empty()) v.push_back(b);
            if (!c.empty()) v.push_back(c);
            return v;
        }
    }

    class DecoratorTest : public ::testing::Test
    {
        protected:
            DecoratorTest() : probe(new Probe(plain())), deco(std::unique_ptr<WorkItem>(probe)), actor("u1", "User", roles()) { }
            void SetUp() { g_log.clear(); g_clones = 0; }
            Probe* probe;
            PassThrough deco;
            Participant actor;
    };

    // ---- WorkItemDecorator: construction and ownership ----

    TEST(DecoratorConstructionTest, ANullComponentIsRejected)
    {
        EXPECT_THROW(PassThrough(std::unique_ptr<WorkItem>()), InvalidDefinitionError);
        EXPECT_THROW(SecureWorkItem(std::unique_ptr<WorkItem>()), InvalidDefinitionError);
    }

    TEST(DecoratorConstructionTest, TheDecoratorOwnsAndDestroysItsComponent)
    {
        {
            PassThrough d(std::unique_ptr<WorkItem>(new Probe(plain())));
            EXPECT_EQ(alive, 1);
        }
        EXPECT_EQ(alive, 0);
    }

    TEST(DecoratorConstructionTest, StackedDecoratorsDestroyTheWholeChain)
    {
        {
            PassThrough outer(std::unique_ptr<WorkItem>(new SecureWorkItem(std::unique_ptr<WorkItem>(new Probe(plain())))));
            EXPECT_EQ(alive, 1);
        }
        EXPECT_EQ(alive, 0);
    }

    // ---- WorkItemDecorator: forwarding ----

    TEST_F(DecoratorTest, ReadOnlyQueriesComeFromTheComponent)
    {
        probe->st = ItemStatus::InProgress;
        probe->who = "u9";
        probe->reversible = false;
        EXPECT_EQ(&deco.spec(), &probe->spec());
        EXPECT_EQ(deco.status(), ItemStatus::InProgress);
        EXPECT_EQ(deco.assignee(), "u9");
        EXPECT_EQ(deco.assignedAt(), probe->assigned);
        EXPECT_EQ(deco.dueAt(), probe->due);
        EXPECT_FALSE(deco.isReversible());
    }

    TEST_F(DecoratorTest, EveryLifecycleOperationForwardsWithItsArguments)
    {
        deco.publish();
        deco.assign("u9");
        deco.start(actor);
        deco.submit(actor, Outcome::accepted());
        deco.complete(actor, Outcome::accepted());
        deco.approve(actor, Outcome::accepted());
        deco.reject(actor, "no");
        deco.escalate("help");
        deco.expire("late", "deadline");
        deco.skip("n/a");
        deco.cancel("stop");
        std::vector<std::string> expected;
        expected.push_back("T:publish");
        expected.push_back("T:assign:u9");
        expected.push_back("T:start:u1");
        expected.push_back("T:submit:u1");
        expected.push_back("T:complete:u1");
        expected.push_back("T:approve:u1");
        expected.push_back("T:reject:u1:no");
        expected.push_back("T:escalate:help");
        expected.push_back("T:expire:late:deadline");
        expected.push_back("T:skip:n/a");
        expected.push_back("T:cancel:stop");
        EXPECT_EQ(g_log, expected);
    }

    TEST_F(DecoratorTest, AddForwardsAndReturnsTheComponentsChild)
    {
        Probe* child = new Probe(plain("C"));
        WorkItem& returned = deco.add(std::unique_ptr<WorkItem>(child));
        EXPECT_EQ(&returned, child);
        EXPECT_EQ(g_log, log("T:add:C"));
    }

    TEST_F(DecoratorTest, ChildrenComeFromTheComponent)
    {
        Probe a(plain("A")), b(plain("B"));
        probe->kids.push_back(&a);
        probe->kids.push_back(&b);
        std::vector<WorkItem*> seen = deco.children();
        ASSERT_EQ(seen.size(), 2u);
        EXPECT_EQ(seen[0], &a);
        EXPECT_EQ(seen[1], &b);
    }

    TEST_F(DecoratorTest, AcceptHandsTheSameVisitorToTheComponent)
    {
        WorkItemVisitor& v = *reinterpret_cast<WorkItemVisitor*>(visitor_storage);
        const WorkItem& view = deco;
        view.accept(v);
        EXPECT_EQ(probe->visited, &v);
    }

    TEST_F(DecoratorTest, CreateIteratorComesFromTheComponent)
    {
        std::unique_ptr<WorkItemIterator> it = deco.createIterator();
        it->first();
        ASSERT_FALSE(it->isDone());
        EXPECT_EQ(&it->currentItem(), probe);
    }

    TEST_F(DecoratorTest, MementoAndRestoreGoToTheComponent)
    {
        std::unique_ptr<WorkItemMemento> m = deco.createMemento();
        EXPECT_TRUE(m.get() == probe->made);
        deco.restore(*m);
        EXPECT_EQ(probe->restored, m.get());
    }

    // ---- WorkItemDecorator: find returns the outermost wrapper ----

    TEST_F(DecoratorTest, FindReturnsTheWrapperWhenTheWrappedItemMatches)
    {
        EXPECT_EQ(deco.find("T"), &deco);
    }

    TEST_F(DecoratorTest, FindReturnsTheOutermostWrapperOfAStack)
    {
        Probe* inner = new Probe(plain("X"));
        PassThrough* middle = new PassThrough(std::unique_ptr<WorkItem>(inner));
        PassThrough outer((std::unique_ptr<WorkItem>(middle)));
        EXPECT_EQ(outer.find("X"), &outer);
        EXPECT_EQ(middle->find("X"), middle);
    }

    TEST_F(DecoratorTest, FindReturnsADescendantsOwnPointerUnchanged)
    {
        Probe other(plain("D"));
        probe->found_id = "D";
        probe->found = &other;
        EXPECT_EQ(deco.find("D"), &other);
    }

    TEST_F(DecoratorTest, FindReturnsNullForAnUnknownId)
    {
        EXPECT_TRUE(deco.find("nope") == 0);
    }

    // ---- WorkItemDecorator: bind and clone ----

    TEST_F(DecoratorTest, BindStoresTheContextAndForwardsIt)
    {
        WorkContext& ctx = *reinterpret_cast<WorkContext*>(context_storage);
        deco.bind(ctx);
        EXPECT_EQ(&deco.boundContext(), &ctx);
        EXPECT_EQ(probe->bound, &ctx);
    }

    TEST_F(DecoratorTest, ContextBeforeBindThrows)
    {
        EXPECT_THROW(deco.boundContext(), InvalidStateError);
    }

    // WorkItem::bind(): binding twice with a different context throws InvalidStateError. A refused bind must change nothing.

    TEST_F(DecoratorTest, ARefusedRebindLeavesTheDecoratorAndItsComponentOnTheEarlierContext)
    {
        WorkContext& a = *reinterpret_cast<WorkContext*>(context_storage);
        WorkContext& b = *reinterpret_cast<WorkContext*>(other_context_storage);
        deco.bind(a);
        EXPECT_THROW(deco.bind(b), InvalidStateError);
        EXPECT_EQ(&deco.boundContext(), &a);
        EXPECT_EQ(probe->bound, &a);
    }

    TEST_F(DecoratorTest, AFirstBindThatThrowsLeavesTheDecoratorUnbound)
    {
        WorkContext& a = *reinterpret_cast<WorkContext*>(context_storage);
        probe->fail_bind = true;
        EXPECT_THROW(deco.bind(a), InvalidStateError);
        EXPECT_THROW(deco.boundContext(), InvalidStateError);
        EXPECT_TRUE(probe->bound == 0);
    }

    TEST_F(DecoratorTest, BindingTheSameContextAgainIsHarmless)
    {
        WorkContext& a = *reinterpret_cast<WorkContext*>(context_storage);
        deco.bind(a);
        EXPECT_NO_THROW(deco.bind(a));
        EXPECT_EQ(&deco.boundContext(), &a);
    }

    TEST(DecoratorStackTest, ARefusedRebindLeavesEveryDecoratorInTheStackOnTheEarlierContext)
    {
        WorkContext& a = *reinterpret_cast<WorkContext*>(context_storage);
        WorkContext& b = *reinterpret_cast<WorkContext*>(other_context_storage);
        Probe* probe = new Probe(plain());
        PassThrough* inner = new PassThrough(std::unique_ptr<WorkItem>(probe));
        PassThrough outer((std::unique_ptr<WorkItem>(inner)));
        outer.bind(a);
        EXPECT_THROW(outer.bind(b), InvalidStateError);
        EXPECT_EQ(&outer.boundContext(), &a);
        EXPECT_EQ(&inner->boundContext(), &a);
        EXPECT_EQ(probe->bound, &a);
    }

    TEST_F(DecoratorTest, CloneWrapsAFreshCloneOfTheComponent)
    {
        std::unique_ptr<WorkItem> copy = deco.clone();
        EXPECT_EQ(g_clones, 1);
        EXPECT_NE(copy.get(), static_cast<WorkItem*>(&deco));
        EXPECT_EQ(copy->spec().id(), "T");
        copy->publish();
        EXPECT_TRUE(g_log.size() == 1u);
        EXPECT_EQ(deco.status(), ItemStatus::Created);
    }

    // ---- WorkItemDecorator: StageItem::skip looks through a decorated nested stage (the Phase 2 follow-up) ----

    TEST(DecoratorWithStageTest, SkipReachesATaskInsideADecoratedNestedStage)
    {
        g_log.clear();
        StageItem outer(WorkItemSpec("S", "Stage"));
        StageItem* inner = new StageItem(WorkItemSpec("N", "Nested"));
        Probe* task = new Probe(plain("T"));
        task->st = ItemStatus::Available;
        inner->add(std::unique_ptr<WorkItem>(task));
        outer.add(std::unique_ptr<WorkItem>(new PassThrough(std::unique_ptr<WorkItem>(inner))));
        ASSERT_EQ(inner->status(), ItemStatus::InProgress);
        EXPECT_NO_THROW(outer.skip("r"));
        EXPECT_EQ(g_log, log("T:skip:r"));
        EXPECT_EQ(outer.status(), ItemStatus::Skipped);
    }

    TEST(DecoratorWithStageTest, ADecoratedStageStillExposesItsChildren)
    {
        StageItem* inner = new StageItem(WorkItemSpec("N", "Nested"));
        inner->add(std::unique_ptr<WorkItem>(new Probe(plain("T"))));
        PassThrough deco((std::unique_ptr<WorkItem>(inner)));
        ASSERT_EQ(deco.children().size(), 1u);
        EXPECT_EQ(deco.children()[0]->spec().id(), "T");
    }

    // ---- SecureWorkItem: role checks before start, submit, complete ----

    class SecureTest : public ::testing::Test
    {
        protected:
            SecureTest()
                : probe(new Probe(guarded())), secure(std::unique_ptr<WorkItem>(probe)),
                  boss("u1", "Boss", roles("manager", "lead")), clerk("u2", "Clerk", roles()), lead("u3", "Lead", roles("lead")) { }
            void SetUp() { g_log.clear(); g_clones = 0; }
            Probe* probe;
            SecureWorkItem secure;
            Participant boss, clerk, lead;
    };

    TEST_F(SecureTest, AnActorWithTheRequiredRoleMayStartSubmitAndComplete)
    {
        secure.start(boss);
        secure.submit(boss, Outcome::accepted());
        secure.complete(boss, Outcome::accepted());
        EXPECT_EQ(g_log, log("T:start:u1", "T:submit:u1", "T:complete:u1"));
    }

    TEST_F(SecureTest, AnActorWithoutTheRoleIsRefusedWithTheDocumentedMessage)
    {
        try
        {
            secure.start(clerk);
            FAIL() << "start() should have thrown";
        }
        catch (const PermissionDeniedError& e)
        {
            EXPECT_STREQ(e.what(), "u2 may not start 'Task' (requires role manager)");
        }
        EXPECT_THROW(secure.submit(clerk, Outcome::accepted()), PermissionDeniedError);
        EXPECT_THROW(secure.complete(clerk, Outcome::accepted()), PermissionDeniedError);
    }

    TEST_F(SecureTest, EachRefusedOperationNamesItselfInTheMessage)
    {
        try { secure.submit(clerk, Outcome::accepted()); FAIL(); }
        catch (const PermissionDeniedError& e) { EXPECT_STREQ(e.what(), "u2 may not submit 'Task' (requires role manager)"); }
        try { secure.complete(clerk, Outcome::accepted()); FAIL(); }
        catch (const PermissionDeniedError& e) { EXPECT_STREQ(e.what(), "u2 may not complete 'Task' (requires role manager)"); }
    }

    TEST_F(SecureTest, ARefusedCallChangesNothing)
    {
        EXPECT_THROW(secure.start(clerk), PermissionDeniedError);
        EXPECT_THROW(secure.submit(clerk, Outcome::accepted()), PermissionDeniedError);
        EXPECT_THROW(secure.complete(clerk, Outcome::accepted()), PermissionDeniedError);
        EXPECT_TRUE(g_log.empty());
    }

    TEST(SecureNoRoleTest, AnItemWithNoRequiredRoleAcceptsAnyActor)
    {
        g_log.clear();
        Participant anyone("u2", "Anyone", roles());
        SecureWorkItem open(std::unique_ptr<WorkItem>(new Probe(plain())));
        open.start(anyone);
        open.submit(anyone, Outcome::accepted());
        open.complete(anyone, Outcome::accepted());
        EXPECT_EQ(g_log, log("T:start:u2", "T:submit:u2", "T:complete:u2"));
    }

    // ---- SecureWorkItem: reviewer checks before approve and reject ----

    TEST_F(SecureTest, AReviewerWithTheReviewerRoleMayApproveAndReject)
    {
        secure.approve(lead, Outcome::accepted());
        secure.reject(lead, "no");
        EXPECT_EQ(g_log, log("T:approve:u3", "T:reject:u3:no"));
    }

    TEST_F(SecureTest, AReviewerWithoutTheRoleIsRefused)
    {
        try
        {
            secure.approve(clerk, Outcome::accepted());
            FAIL() << "approve() should have thrown";
        }
        catch (const PermissionDeniedError& e)
        {
            EXPECT_STREQ(e.what(), "u2 may not approve 'Task' (requires role lead)");
        }
        try
        {
            secure.reject(clerk, "no");
            FAIL() << "reject() should have thrown";
        }
        catch (const PermissionDeniedError& e)
        {
            EXPECT_STREQ(e.what(), "u2 may not reject 'Task' (requires role lead)");
        }
        EXPECT_TRUE(g_log.empty());
    }

    TEST(SecureNoRoleTest, AnItemWithNoReviewerRoleAcceptsAnyReviewer)
    {
        g_log.clear();
        Participant anyone("u2", "Anyone", roles());
        SecureWorkItem open(std::unique_ptr<WorkItem>(new Probe(plain())));
        open.approve(anyone, Outcome::accepted());
        open.reject(anyone, "no");
        EXPECT_EQ(g_log, log("T:approve:u2", "T:reject:u2:no"));
    }

    // ---- SecureWorkItem: an escalated item is decided by the assigned approver only ----

    TEST_F(SecureTest, OnAnEscalatedItemTheAssignedApproverMayDecideWithoutTheReviewerRole)
    {
        probe->st = ItemStatus::Escalated;
        probe->who = "u2";
        secure.approve(clerk, Outcome::accepted());
        secure.reject(clerk, "no");
        EXPECT_EQ(g_log, log("T:approve:u2", "T:reject:u2:no"));
    }

    TEST_F(SecureTest, OnAnEscalatedItemAnyoneElseIsRefusedEvenWithTheReviewerRole)
    {
        probe->st = ItemStatus::Escalated;
        probe->who = "u2";
        try
        {
            secure.approve(lead, Outcome::accepted());
            FAIL() << "approve() should have thrown";
        }
        catch (const PermissionDeniedError& e)
        {
            EXPECT_STREQ(e.what(), "u3 may not approve 'Task' (only the assigned approver u2 may decide an escalated item)");
        }
        EXPECT_THROW(secure.reject(lead, "no"), PermissionDeniedError);
        EXPECT_TRUE(g_log.empty());
    }

    TEST_F(SecureTest, TheReviewerRoleCheckAppliesAgainOnceTheItemIsNoLongerEscalated)
    {
        probe->st = ItemStatus::InProgress;
        probe->who = "u2";
        EXPECT_THROW(secure.approve(clerk, Outcome::accepted()), PermissionDeniedError);
    }

    // ---- SecureWorkItem: system-initiated operations are never checked ----

    TEST_F(SecureTest, SystemOperationsAreForwardedUnchecked)
    {
        secure.publish();
        secure.assign("u2");
        secure.escalate("help");
        secure.expire("late", "deadline");
        secure.skip("n/a");
        secure.cancel("stop");
        ASSERT_EQ(g_log.size(), 6u);
        EXPECT_EQ(g_log[0], "T:publish");
        EXPECT_EQ(g_log[1], "T:assign:u2");
        EXPECT_EQ(g_log[2], "T:escalate:help");
        EXPECT_EQ(g_log[3], "T:expire:late:deadline");
        EXPECT_EQ(g_log[4], "T:skip:n/a");
        EXPECT_EQ(g_log[5], "T:cancel:stop");
    }

    // ---- SecureWorkItem: clone and stacking ----

    TEST_F(SecureTest, ACloneIsStillSecuredAndWrapsAFreshComponent)
    {
        std::unique_ptr<WorkItem> copy = secure.clone();
        EXPECT_EQ(g_clones, 1);
        EXPECT_THROW(copy->start(clerk), PermissionDeniedError);
        copy->start(boss);
        EXPECT_EQ(g_log, log("T:start:u1"));
    }

    TEST_F(SecureTest, TheRoleCheckReadsTheSpecThroughOtherDecorators)
    {
        PassThrough outer(std::unique_ptr<WorkItem>(new SecureWorkItem(std::unique_ptr<WorkItem>(new Probe(guarded())))));
        EXPECT_THROW(outer.start(clerk), PermissionDeniedError);
        outer.start(boss);
        EXPECT_EQ(g_log, log("T:start:u1"));
    }
}