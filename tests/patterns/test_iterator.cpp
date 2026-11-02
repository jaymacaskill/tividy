#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "engine/core/Errors.h"
#include "engine/items/StageItem.h"
#include "engine/items/WorkContext.h"
#include "engine/items/WorkItem.h"
#include "engine/items/WorkItemDecorator.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemSpec.h"
#include "engine/items/WorkItemVisitor.h"

/*
Iterator pattern: WorkItemIterator (Iterator), DepthFirstIterator (ConcreteIterator), WorkItem/StageItem (Aggregate via createIterator()).
The tree nodes here are fakes that count how often children() is read and record any lifecycle call, so the tests can show that the
iterator only reads structure, reads it once per node, and never drives an item.
*/

namespace tividy
{
	namespace
	{
		int alive = 0;
		std::vector<std::string> g_calls;

		class Node : public WorkItem
		{
			public:
				explicit Node(const std::string& id) : s(id, "node"), st(ItemStatus::Created), children_reads(0) { ++alive; }
				~Node() { --alive; }
				const WorkItemSpec& spec() const { return s; }
				ItemStatus status() const { return st; }
				const Id& assignee() const { return none; }
				std::vector<WorkItem*> children() { ++children_reads; return kids; }
				WorkItem* find(const Id& id) { return id == s.id() ? this : 0; }
				void publish() { g_calls.push_back("publish"); }
				void assign(const Id&) { g_calls.push_back("assign"); }
				void start(const Participant&) { g_calls.push_back("start"); }
				void submit(const Participant&, const Outcome&) { g_calls.push_back("submit"); }
				void complete(const Participant&, const Outcome&) { g_calls.push_back("complete"); }
				void approve(const Participant&, const Outcome&) { g_calls.push_back("approve"); }
				void reject(const Participant&, const std::string&) { g_calls.push_back("reject"); }
				void escalate(const std::string&) { g_calls.push_back("escalate"); }
				void cancel(const std::string&) { g_calls.push_back("cancel"); }
				void skip(const std::string&) { g_calls.push_back("skip"); }
				void expire(const std::string&, const std::string&) { g_calls.push_back("expire"); }
				std::unique_ptr<WorkItem> clone() const { return std::unique_ptr<WorkItem>(); }
				void accept(WorkItemVisitor&) const { g_calls.push_back("accept"); }
				std::unique_ptr<WorkItemIterator> createIterator() { return std::unique_ptr<WorkItemIterator>(new DepthFirstIterator(*this)); }
				void bind(WorkContext&) { g_calls.push_back("bind"); }

				WorkItemSpec s;
				ItemStatus st;
				std::vector<WorkItem*> kids;
				int children_reads;
				Id none;
		};

		// Owns every node a test creates, so trees can be wired freely.
		class Forest
		{
			public:
				Node* node(const std::string& id) { pool.push_back(std::unique_ptr<Node>(new Node(id))); return pool.back().get(); }
				Node* child(Node* parent, const std::string& id) { Node* n = node(id); parent->kids.push_back(n); return n; }
			private:
				std::vector<std::unique_ptr<Node> > pool;
		};

		std::vector<std::string> walk(WorkItemIterator& it)
		{
			std::vector<std::string> ids;
			for (it.first(); !it.isDone(); it.next()) ids.push_back(it.currentItem().spec().id());
			return ids;
		}

		std::vector<std::string> ids(const std::string& a, const std::string& b = "", const std::string& c = "", const std::string& d = "",
									 const std::string& e = "", const std::string& f = "", const std::string& g = "", const std::string& h = "")
		{
			const std::string all[] = { a, b, c, d, e, f, g, h };
			std::vector<std::string> v;
			for (std::size_t i = 0; i < 8; ++i) if (!all[i].empty()) v.push_back(all[i]);
			return v;
		}

		// Independent reference: recursive pre-order written separately from the code under test.
		void reference(Node* n, std::vector<std::string>& out)
		{
			out.push_back(n->spec().id());
			for (std::size_t i = 0; i < n->kids.size(); ++i) reference(static_cast<Node*>(n->kids[i]), out);
		}

		void grow(Forest& forest, Node* parent, int depth, int fanout, int& counter)
		{
			if (depth == 0) return;
			for (int i = 0; i < fanout; ++i)
			{
				Node* c = forest.child(parent, "n" + std::to_string(++counter));
				grow(forest, c, depth - 1, fanout, counter);
			}
		}
	}

	class IteratorPatternTest : public ::testing::Test
	{
		protected:
			void SetUp() { g_calls.clear(); }
			Forest forest;
	};

	// ---- order ----

	TEST_F(IteratorPatternTest, ALoneItemYieldsJustItself)
	{
		Node* root = forest.node("R");
		DepthFirstIterator it(*root);
		EXPECT_EQ(walk(it), ids("R"));
	}

	TEST_F(IteratorPatternTest, VisitsTheRootFirstThenEachSubtreeInFullBeforeTheNextSibling)
	{
		Node* r = forest.node("R");
		Node* a = forest.child(r, "A");
		forest.child(a, "A1");
		forest.child(a, "A2");
		forest.child(r, "B");
		Node* c = forest.child(r, "C");
		Node* c1 = forest.child(c, "C1");
		forest.child(c1, "C11");
		DepthFirstIterator it(*r);
		EXPECT_EQ(walk(it), ids("R", "A", "A1", "A2", "B", "C", "C1", "C11"));
	}

	TEST_F(IteratorPatternTest, ChildrenAreVisitedInTheOrderTheyWereAdded)
	{
		Node* r = forest.node("R");
		forest.child(r, "third");
		forest.child(r, "first");
		forest.child(r, "second");
		DepthFirstIterator it(*r);
		EXPECT_EQ(walk(it), ids("R", "third", "first", "second"));
	}

	TEST_F(IteratorPatternTest, CurrentItemIsTheRealObjectNotACopy)
	{
		Node* r = forest.node("R");
		Node* a = forest.child(r, "A");
		Node* b = forest.child(a, "B");
		DepthFirstIterator it(*r);
		it.first();
		EXPECT_EQ(&it.currentItem(), r);
		it.next();
		EXPECT_EQ(&it.currentItem(), a);
		it.next();
		EXPECT_EQ(&it.currentItem(), b);
	}

	TEST_F(IteratorPatternTest, MatchesAnIndependentPreOrderOnALargerTree)
	{
		Node* r = forest.node("n0");
		int counter = 0;
		grow(forest, r, 5, 3, counter);
		std::vector<std::string> expected;
		reference(r, expected);
		DepthFirstIterator it(*r);
		std::vector<std::string> seen = walk(it);
		EXPECT_EQ(seen.size(), 364u);                       // 1 + 3 + 9 + 27 + 81 + 243
		EXPECT_EQ(seen, expected);
		EXPECT_EQ(std::set<std::string>(seen.begin(), seen.end()).size(), seen.size());   // every node exactly once
	}

	TEST_F(IteratorPatternTest, HandlesAVeryWideTree)
	{
		Node* r = forest.node("R");
		for (int i = 0; i < 1000; ++i) forest.child(r, "c" + std::to_string(i));
		DepthFirstIterator it(*r);
		std::vector<std::string> seen = walk(it);
		ASSERT_EQ(seen.size(), 1001u);
		EXPECT_EQ(seen[1], "c0");
		EXPECT_EQ(seen[1000], "c999");
	}

	TEST_F(IteratorPatternTest, HandlesAVeryDeepTree)
	{
		Node* r = forest.node("d0");
		Node* tip = r;
		for (int i = 1; i <= 500; ++i) tip = forest.child(tip, "d" + std::to_string(i));
		DepthFirstIterator it(*r);
		std::vector<std::string> seen = walk(it);
		ASSERT_EQ(seen.size(), 501u);
		EXPECT_EQ(seen.front(), "d0");
		EXPECT_EQ(seen.back(), "d500");
	}

	// ---- the Iterator protocol: first / next / isDone / currentItem ----

	TEST_F(IteratorPatternTest, IsNotDoneAtTheStartAndDoneAfterTheLastItem)
	{
		Node* r = forest.node("R");
		forest.child(r, "A");
		DepthFirstIterator it(*r);
		EXPECT_FALSE(it.isDone());
		it.first();
		EXPECT_FALSE(it.isDone());
		it.next();
		EXPECT_FALSE(it.isDone());
		it.next();
		EXPECT_TRUE(it.isDone());
	}

	TEST_F(IteratorPatternTest, FirstRestartsFromTheRootAtAnyPoint)
	{
		Node* r = forest.node("R");
		forest.child(r, "A");
		forest.child(r, "B");
		DepthFirstIterator it(*r);
		it.first();
		it.next();
		it.next();
		it.first();                                          // part-way through
		EXPECT_EQ(it.currentItem().spec().id(), "R");
		for (; !it.isDone(); it.next()) { }
		it.first();                                          // after finishing
		EXPECT_FALSE(it.isDone());
		EXPECT_EQ(it.currentItem().spec().id(), "R");
	}

	TEST_F(IteratorPatternTest, TheSameIteratorCanBeWalkedTwiceWithTheSameResult)
	{
		Node* r = forest.node("R");
		forest.child(forest.child(r, "A"), "A1");
		forest.child(r, "B");
		DepthFirstIterator it(*r);
		std::vector<std::string> once = walk(it);
		std::vector<std::string> twice = walk(it);
		EXPECT_EQ(once, twice);
		EXPECT_EQ(once, ids("R", "A", "A1", "B"));
	}

	TEST_F(IteratorPatternTest, StaysDoneOnceFinished)
	{
		Node* r = forest.node("R");
		DepthFirstIterator it(*r);
		it.first();
		it.next();
		EXPECT_TRUE(it.isDone());
		EXPECT_TRUE(it.isDone());
	}

	TEST_F(IteratorPatternTest, NextAndCurrentItemThrowAnEngineErrorWhenDone)
	{
		Node* r = forest.node("R");
		DepthFirstIterator it(*r);
		it.first();
		it.next();
		ASSERT_TRUE(it.isDone());
		EXPECT_THROW(it.next(), EngineError);
		EXPECT_THROW(it.currentItem(), EngineError);
		try { it.next(); FAIL(); }
		catch (const EngineError& e) { EXPECT_NE(std::string(e.what()).find("next()"), std::string::npos); }
		try { it.currentItem(); FAIL(); }
		catch (const EngineError& e) { EXPECT_NE(std::string(e.what()).find("currentItem()"), std::string::npos); }
	}

	TEST_F(IteratorPatternTest, AFailedNextDoesNotMoveThePosition)
	{
		Node* r = forest.node("R");
		DepthFirstIterator it(*r);
		it.first();
		it.next();
		EXPECT_THROW(it.next(), EngineError);
		EXPECT_TRUE(it.isDone());
		it.first();
		EXPECT_EQ(it.currentItem().spec().id(), "R");
	}

	// ---- snapshot behaviour (the header: the order is captured at construction, statuses may change) ----

	TEST_F(IteratorPatternTest, ReadsEachNodesChildrenExactlyOnceAtConstruction)
	{
		Node* r = forest.node("R");
		Node* a = forest.child(r, "A");
		Node* b = forest.child(a, "B");
		DepthFirstIterator it(*r);
		EXPECT_EQ(r->children_reads, 1);
		EXPECT_EQ(a->children_reads, 1);
		EXPECT_EQ(b->children_reads, 1);
		walk(it);
		walk(it);
		EXPECT_EQ(r->children_reads, 1);
		EXPECT_EQ(a->children_reads, 1);
		EXPECT_EQ(b->children_reads, 1);
	}

	TEST_F(IteratorPatternTest, ItemsAddedAfterConstructionAreNotVisited)
	{
		Node* r = forest.node("R");
		forest.child(r, "A");
		DepthFirstIterator it(*r);
		forest.child(r, "late");
		EXPECT_EQ(walk(it), ids("R", "A"));
	}

	TEST_F(IteratorPatternTest, StatusChangesDuringTheWalkAreSeenLive)
	{
		Node* r = forest.node("R");
		Node* a = forest.child(r, "A");
		DepthFirstIterator it(*r);
		it.first();
		it.next();
		a->st = ItemStatus::InProgress;
		EXPECT_EQ(it.currentItem().status(), ItemStatus::InProgress);
	}

	TEST_F(IteratorPatternTest, WalkingNeverDrivesAnItem)
	{
		Node* r = forest.node("R");
		forest.child(forest.child(r, "A"), "B");
		DepthFirstIterator it(*r);
		walk(it);
		EXPECT_TRUE(g_calls.empty());
	}

	// ---- through the Aggregate (createIterator) and the abstract interface ----

	TEST_F(IteratorPatternTest, AStageHandsOutAnIteratorRootedAtItself)
	{
		StageItem stage(WorkItemSpec("S", "Stage"));
		stage.add(std::unique_ptr<WorkItem>(new Node("A")));
		StageItem* inner = new StageItem(WorkItemSpec("N", "Nested"));
		inner->add(std::unique_ptr<WorkItem>(new Node("B")));
		stage.add(std::unique_ptr<WorkItem>(inner));
		stage.add(std::unique_ptr<WorkItem>(new Node("C")));
		std::unique_ptr<WorkItemIterator> it = stage.createIterator();
		EXPECT_EQ(walk(*it), ids("S", "A", "N", "B", "C"));
	}

	TEST_F(IteratorPatternTest, AnIteratorRootedAtASubtreeStaysInsideIt)
	{
		StageItem stage(WorkItemSpec("S", "Stage"));
		stage.add(std::unique_ptr<WorkItem>(new Node("A")));
		StageItem* inner = new StageItem(WorkItemSpec("N", "Nested"));
		inner->add(std::unique_ptr<WorkItem>(new Node("B")));
		stage.add(std::unique_ptr<WorkItem>(inner));
		stage.add(std::unique_ptr<WorkItem>(new Node("C")));
		DepthFirstIterator it(*inner);
		EXPECT_EQ(walk(it), ids("N", "B"));
	}

	TEST_F(IteratorPatternTest, ADecoratedStageIsVisitedOnceThenItsChildren)
	{
		StageItem stage(WorkItemSpec("S", "Stage"));
		stage.add(std::unique_ptr<WorkItem>(new Node("A")));
		StageItem* inner = new StageItem(WorkItemSpec("N", "Nested"));
		inner->add(std::unique_ptr<WorkItem>(new Node("B")));
		WorkItem& decorated = stage.add(std::unique_ptr<WorkItem>(new SecureWorkItem(std::unique_ptr<WorkItem>(inner))));
		stage.add(std::unique_ptr<WorkItem>(new Node("C")));
		std::unique_ptr<WorkItemIterator> it = stage.createIterator();
		std::vector<std::string> seen = walk(*it);
		EXPECT_EQ(seen, ids("S", "A", "N", "B", "C"));       // N appears once, not once for the wrapper and once for the stage
		it->first();
		it->next();
		it->next();
		EXPECT_EQ(&it->currentItem(), &decorated);            // the visited node is the outermost wrapper
	}

	TEST_F(IteratorPatternTest, ADecoratorHandsOutAnIteratorRootedAtTheWrappedItem)
	{
		StageItem* inner = new StageItem(WorkItemSpec("N", "Nested"));
		inner->add(std::unique_ptr<WorkItem>(new Node("B")));
		SecureWorkItem secure((std::unique_ptr<WorkItem>(inner)));
		std::unique_ptr<WorkItemIterator> it = secure.createIterator();
		EXPECT_EQ(walk(*it), ids("N", "B"));
	}

	TEST_F(IteratorPatternTest, IsUsableAndDestroyableThroughTheBaseInterface)
	{
		{
			Node* r = forest.node("R");
			forest.child(r, "A");
			std::unique_ptr<WorkItemIterator> it(new DepthFirstIterator(*r));
			EXPECT_EQ(walk(*it), ids("R", "A"));
		}
		EXPECT_EQ(alive, 2);                                  // the forest still owns both nodes; the iterator owned nothing
	}

	TEST_F(IteratorPatternTest, TwoIteratorsOverTheSameTreeAreIndependent)
	{
		Node* r = forest.node("R");
		forest.child(r, "A");
		forest.child(r, "B");
		DepthFirstIterator one(*r);
		DepthFirstIterator two(*r);
		one.first();
		two.first();
		one.next();
		one.next();
		EXPECT_EQ(one.currentItem().spec().id(), "B");
		EXPECT_EQ(two.currentItem().spec().id(), "R");
	}
}