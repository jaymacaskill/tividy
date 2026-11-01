#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Outcome.h"
#include "engine/core/Types.h"
#include "engine/events/Observer.h"
#include "engine/events/WorkflowEvent.h"
#include "engine/runtime/Observers.h"

namespace tividy
{
	namespace
	{
		WorkflowEvent makeEvent(EventType type, const Id& item_id = "item-1")
		{
			return WorkflowEvent(type, "inst-1", item_id, TimePoint());
		}

		class RecordingObserver : public EventObserver
		{
			public:
				explicit RecordingObserver(const std::string& n, std::vector<std::string>* log = nullptr) : name_ref(n), log_ref(log) { }
				void onEvent(const WorkflowEvent& event) override
				{
					received_ref.push_back(event);
					if (log_ref != nullptr)
					{
						log_ref->push_back(name_ref);
					}
				}
				const std::vector<WorkflowEvent>& received() const { return received_ref; }
			private:
				std::string name_ref;
				std::vector<std::string>* log_ref;
				std::vector<WorkflowEvent> received_ref;
		};

		class ThrowingObserver : public EventObserver
		{
			public:
				void onEvent(const WorkflowEvent&) override
				{
					throw std::runtime_error("observer failure");
				}
		};

		class AttachingObserver : public EventObserver
		{
			public:
				AttachingObserver(EventSubject* s, EventObserver* o) : subject_ref(s), to_attach(o) { }
				void onEvent(const WorkflowEvent&) override
				{
					subject_ref->attach(to_attach);
				}
			private:
				EventSubject* subject_ref;
				EventObserver* to_attach;
		};

		class DetachingObserver : public EventObserver
		{
			public:
				explicit DetachingObserver(EventSubject* s) : subject_ref(s), received_ref(0) { }
				void onEvent(const WorkflowEvent&) override
				{
					received_ref++;
					subject_ref->detach(this);
				}
				unsigned received() const { return received_ref; }
			private:
				EventSubject* subject_ref;
				unsigned received_ref;
		};

		class RecordingConnector : public ExternalConnector
		{
			public:
				RecordingConnector(const std::string& n, std::vector<ConnectorRequest>* log, bool succeed = true) : name_ref(n), log_ref(log), succeed_ref(succeed) { }
				std::string name() const override { return name_ref; }
				ConnectorResult invoke(const ConnectorRequest& request) override
				{
					log_ref->push_back(request);
					if (succeed_ref)
					{
						return ConnectorResult::ok("sent");
					}
					return ConnectorResult::failure("unavailable", true);
				}
			private:
				std::string name_ref;
				std::vector<ConnectorRequest>* log_ref;
				bool succeed_ref;
		};

		std::vector<ConnectorRequest> notifyOnce(const WorkflowEvent& event)
		{
			std::vector<ConnectorRequest> requests;
			ConnectorRegistry registry;
			registry.registerConnector(std::unique_ptr<ExternalConnector>(new RecordingConnector("notifier", &requests)));
			NotificationObserver observer(registry);
			EventPublisher publisher;
			publisher.attach(&observer);
			publisher.notify(event);
			return requests;
		}

		void expectSingleNotification(const std::vector<ConnectorRequest>& requests, const std::string& recipient, EventType type, const std::string& body)
		{
			ASSERT_EQ(requests.size(), 1u);
			EXPECT_EQ(requests[0].operation, "notify");
			EXPECT_EQ(requests[0].params.at("recipient"), recipient);
			EXPECT_EQ(requests[0].params.at("subject"), toString(type));
			EXPECT_EQ(requests[0].params.at("body"), body);
		}
	}

	TEST(ObserverPatternTest, NewPublisherHasNoObservers)
	{
		EventPublisher publisher;
		EXPECT_EQ(publisher.observerCount(), 0u);
	}

	TEST(ObserverPatternTest, AttachRegistersObserver)
	{
		EventPublisher publisher;
		RecordingObserver a("a");
		RecordingObserver b("b");
		publisher.attach(&a);
		EXPECT_EQ(publisher.observerCount(), 1u);
		publisher.attach(&b);
		EXPECT_EQ(publisher.observerCount(), 2u);
	}

	TEST(ObserverPatternTest, AttachingSamePointerTwiceHasNoEffect)
	{
		EventPublisher publisher;
		RecordingObserver a("a");
		publisher.attach(&a);
		publisher.attach(&a);
		EXPECT_EQ(publisher.observerCount(), 1u);

		publisher.notify(makeEvent(EventType::ItemAvailable));
		EXPECT_EQ(a.received().size(), 1u);
	}

	TEST(ObserverPatternTest, DetachUnregistersObserver)
	{
		EventPublisher publisher;
		RecordingObserver a("a");
		publisher.attach(&a);
		publisher.detach(&a);
		EXPECT_EQ(publisher.observerCount(), 0u);

		publisher.notify(makeEvent(EventType::ItemAvailable));
		EXPECT_TRUE(a.received().empty());
	}

	TEST(ObserverPatternTest, DetachUnknownObserverHasNoEffect)
	{
		EventPublisher publisher;
		RecordingObserver a("a");
		RecordingObserver b("b");
		publisher.attach(&a);
		EXPECT_NO_THROW(publisher.detach(&b));
		EXPECT_EQ(publisher.observerCount(), 1u);
	}

	TEST(ObserverPatternTest, NotifyWithNoObserversDoesNothing)
	{
		EventPublisher publisher;
		EXPECT_NO_THROW(publisher.notify(makeEvent(EventType::InstanceStarted, "")));
	}

	TEST(ObserverPatternTest, NotifyDeliversToEveryObserverInAttachOrder)
	{
		EventPublisher publisher;
		std::vector<std::string> log;
		RecordingObserver a("a", &log);
		RecordingObserver b("b", &log);
		RecordingObserver c("c", &log);
		publisher.attach(&b);
		publisher.attach(&a);
		publisher.attach(&c);

		publisher.notify(makeEvent(EventType::ItemAssigned));
		ASSERT_EQ(log.size(), 3u);
		EXPECT_EQ(log[0], "b");
		EXPECT_EQ(log[1], "a");
		EXPECT_EQ(log[2], "c");
	}

	TEST(ObserverPatternTest, ObserverReceivesEventsInPublishOrder)
	{
		EventPublisher publisher;
		RecordingObserver a("a");
		publisher.attach(&a);

		publisher.notify(makeEvent(EventType::ItemAvailable));
		publisher.notify(makeEvent(EventType::ItemAssigned));
		publisher.notify(makeEvent(EventType::ItemStarted));
		ASSERT_EQ(a.received().size(), 3u);
		EXPECT_EQ(a.received()[0].type(), EventType::ItemAvailable);
		EXPECT_EQ(a.received()[1].type(), EventType::ItemAssigned);
		EXPECT_EQ(a.received()[2].type(), EventType::ItemStarted);
	}

	TEST(ObserverPatternTest, NotifyPushesFullEventPayload)
	{
		EventPublisher publisher;
		RecordingObserver a("a");
		publisher.attach(&a);

		WorkflowEvent event = makeEvent(EventType::ItemSubmitted, "verify-docs");
		event.byActor("u1").withOutcome(Outcome::rejected("DOCS_INVALID", "missing id")).withMessage("Verify documents submitted").withCount(0);
		publisher.notify(event);

		ASSERT_EQ(a.received().size(), 1u);
		const WorkflowEvent& got = a.received()[0];
		EXPECT_EQ(got.type(), EventType::ItemSubmitted);
		EXPECT_EQ(got.instanceId(), "inst-1");
		EXPECT_EQ(got.itemId(), "verify-docs");
		EXPECT_EQ(got.actorId(), "u1");
		EXPECT_EQ(got.outcome().kind(), OutcomeKind::Rejected);
		EXPECT_EQ(got.outcome().code(), "DOCS_INVALID");
		EXPECT_EQ(got.message(), "Verify documents submitted");
		EXPECT_EQ(got.count(), 0u);
		EXPECT_EQ(got.when(), TimePoint());
	}

	TEST(ObserverPatternTest, ExceptionFromOneObserverDoesNotStopOthers)
	{
		EventPublisher publisher;
		RecordingObserver first("first");
		ThrowingObserver thrower;
		RecordingObserver last("last");
		publisher.attach(&first);
		publisher.attach(&thrower);
		publisher.attach(&last);

		EXPECT_NO_THROW(publisher.notify(makeEvent(EventType::ItemCompleted)));
		EXPECT_EQ(first.received().size(), 1u);
		EXPECT_EQ(last.received().size(), 1u);
	}

	TEST(ObserverPatternTest, ObserverAttachedDuringDeliveryMissesCurrentEvent)
	{
		EventPublisher publisher;
		RecordingObserver late("late");
		AttachingObserver attacher(&publisher, &late);
		publisher.attach(&attacher);

		EXPECT_NO_THROW(publisher.notify(makeEvent(EventType::ItemAvailable)));
		EXPECT_EQ(publisher.observerCount(), 2u);
		EXPECT_TRUE(late.received().empty());

		publisher.notify(makeEvent(EventType::ItemAssigned));
		ASSERT_EQ(late.received().size(), 1u);
		EXPECT_EQ(late.received()[0].type(), EventType::ItemAssigned);
	}

	TEST(ObserverPatternTest, ObserverDetachingItselfDuringDeliveryDoesNotBreakDelivery)
	{
		EventPublisher publisher;
		DetachingObserver detacher(&publisher);
		RecordingObserver after("after");
		publisher.attach(&detacher);
		publisher.attach(&after);

		EXPECT_NO_THROW(publisher.notify(makeEvent(EventType::ItemAvailable)));
		EXPECT_EQ(detacher.received(), 1u);
		EXPECT_EQ(after.received().size(), 1u);
		EXPECT_EQ(publisher.observerCount(), 1u);

		publisher.notify(makeEvent(EventType::ItemAssigned));
		EXPECT_EQ(detacher.received(), 1u);
		EXPECT_EQ(after.received().size(), 2u);
	}

	TEST(ObserverPatternTest, IndependentObserversAllReactToSameEvent)
	{
		std::vector<ConnectorRequest> requests;
		ConnectorRegistry registry;
		registry.registerConnector(std::unique_ptr<ExternalConnector>(new RecordingConnector("notifier", &requests)));
		AuditLog log;
		AuditObserver audit(log);
		NotificationObserver notification(registry);
		RecordingObserver recorder("recorder");

		EventPublisher publisher;
		publisher.attach(&audit);
		publisher.attach(&notification);
		publisher.attach(&recorder);
		publisher.notify(makeEvent(EventType::ItemAssigned).byActor("u1").withMessage("assigned"));

		EXPECT_EQ(log.entries().size(), 1u);
		EXPECT_EQ(requests.size(), 1u);
		EXPECT_EQ(recorder.received().size(), 1u);
	}

	TEST(ObserverPatternTest, WorksThroughSubjectInterface)
	{
		EventPublisher publisher;
		EventSubject& subject = publisher;
		RecordingObserver a("a");
		EventObserver* observer = &a;

		subject.attach(observer);
		subject.notify(makeEvent(EventType::ItemEscalated));
		subject.detach(observer);
		subject.notify(makeEvent(EventType::ItemCancelled));

		ASSERT_EQ(a.received().size(), 1u);
		EXPECT_EQ(a.received()[0].type(), EventType::ItemEscalated);
		EXPECT_EQ(publisher.observerCount(), 0u);
	}

	TEST(AuditObserverTest, AppendsOneEntryPerEvent)
	{
		AuditLog log;
		AuditObserver observer(log);
		EventPublisher publisher;
		publisher.attach(&observer);
		EXPECT_TRUE(log.entries().empty());

		TimePoint when = TimePoint() + std::chrono::seconds(1793190896);
		publisher.notify(WorkflowEvent(EventType::ItemAssigned, "inst-1", "verify-docs", when).byActor("u1").withMessage("Verify documents assigned"));
		publisher.notify(makeEvent(EventType::ItemStarted).byActor("u1").withMessage("started"));
		publisher.notify(makeEvent(EventType::ItemSubmitted).byActor("u1").withMessage("submitted"));

		ASSERT_EQ(log.entries().size(), 3u);
		const AuditEntry& first = log.entries()[0];
		EXPECT_EQ(first.when, when);
		EXPECT_EQ(first.type, EventType::ItemAssigned);
		EXPECT_EQ(first.item_id, "verify-docs");
		EXPECT_EQ(first.actor_id, "u1");
		EXPECT_EQ(first.message, "Verify documents assigned");
		EXPECT_EQ(log.entries()[1].type, EventType::ItemStarted);
		EXPECT_EQ(log.entries()[2].type, EventType::ItemSubmitted);
	}

	TEST(AuditObserverTest, LogIsAppendOnly)
	{
		AuditLog log;
		AuditObserver observer(log);
		observer.onEvent(makeEvent(EventType::ItemAvailable, "a").withMessage("first"));
		AuditEntry first = log.entries()[0];

		observer.onEvent(makeEvent(EventType::ItemAssigned, "b").byActor("u1").withMessage("second"));
		observer.onEvent(makeEvent(EventType::ItemStarted, "c").byActor("u1").withMessage("third"));

		ASSERT_EQ(log.entries().size(), 3u);
		EXPECT_EQ(log.entries()[0].type, first.type);
		EXPECT_EQ(log.entries()[0].item_id, first.item_id);
		EXPECT_EQ(log.entries()[0].actor_id, first.actor_id);
		EXPECT_EQ(log.entries()[0].message, first.message);
		EXPECT_EQ(log.entries()[0].when, first.when);
		EXPECT_EQ(log.entries()[1].message, "second");
		EXPECT_EQ(log.entries()[2].message, "third");
	}

	TEST(AuditObserverTest, InstanceEventHasEmptyItemId)
	{
		AuditLog log;
		AuditObserver observer(log);
		observer.onEvent(makeEvent(EventType::InstanceStarted, "").withMessage("started from demo v1"));

		ASSERT_EQ(log.entries().size(), 1u);
		EXPECT_EQ(log.entries()[0].item_id, "");
	}

	TEST(AuditObserverTest, ActorDefaultsToSystemWhenEventHasNoActor)
	{
		AuditLog log;
		AuditObserver observer(log);
		observer.onEvent(makeEvent(EventType::ItemAvailable).withMessage("available"));

		ASSERT_EQ(log.entries().size(), 1u);
		EXPECT_EQ(log.entries()[0].actor_id, "system");
	}

	TEST(AuditObserverTest, MessageDefaultsToTypeNameWhenEventHasNoMessage)
	{
		AuditLog log;
		AuditObserver observer(log);
		observer.onEvent(makeEvent(EventType::ItemCompleted).byActor("u1"));

		ASSERT_EQ(log.entries().size(), 1u);
		EXPECT_EQ(log.entries()[0].message, toString(EventType::ItemCompleted));
	}

	TEST(AuditObserverTest, FormatProducesLineWithAllFieldsInOrder)
	{
		AuditEntry entry;
		entry.when = TimePoint() + std::chrono::seconds(1793190896);
		entry.type = EventType::ItemCompleted;
		entry.item_id = "verify-docs";
		entry.actor_id = "u1";
		entry.message = "Verify documents completed";

		std::string line = AuditLog::format(entry);
		std::size_t at_time = line.find("2026-10-28 12:34:56");
		std::size_t at_type = line.find(toString(EventType::ItemCompleted));
		std::size_t at_actor = line.find("u1");
		std::size_t at_item = line.find("verify-docs");
		std::size_t at_message = line.find("Verify documents completed");

		ASSERT_NE(at_time, std::string::npos);
		ASSERT_NE(at_type, std::string::npos);
		ASSERT_NE(at_actor, std::string::npos);
		ASSERT_NE(at_item, std::string::npos);
		ASSERT_NE(at_message, std::string::npos);
		EXPECT_EQ(at_time, 0u);
		EXPECT_LT(at_time, at_type);
		EXPECT_LT(at_type, at_actor);
		EXPECT_LT(at_actor, at_item);
		EXPECT_LT(at_item, at_message);
	}

	TEST(NotificationObserverTest, ItemAssignedNotifiesAssignee)
	{
		std::vector<ConnectorRequest> requests = notifyOnce(makeEvent(EventType::ItemAssigned).byActor("u7").withMessage("Verify documents assigned"));
		expectSingleNotification(requests, "u7", EventType::ItemAssigned, "Verify documents assigned");
	}

	TEST(NotificationObserverTest, ItemEscalatedNotifiesEscalation)
	{
		std::vector<ConnectorRequest> requests = notifyOnce(makeEvent(EventType::ItemEscalated).byActor("u1").withMessage("deadline missed").withCount(1));
		expectSingleNotification(requests, "escalation", EventType::ItemEscalated, "deadline missed");
	}

	TEST(NotificationObserverTest, ItemFailedNotifiesEscalation)
	{
		std::vector<ConnectorRequest> requests = notifyOnce(makeEvent(EventType::ItemFailed).withMessage("vendor down").withCount(1));
		expectSingleNotification(requests, "escalation", EventType::ItemFailed, "vendor down");
	}

	TEST(NotificationObserverTest, InstanceCompletedNotifiesOwner)
	{
		std::vector<ConnectorRequest> requests = notifyOnce(makeEvent(EventType::InstanceCompleted, "").withMessage("workflow completed"));
		expectSingleNotification(requests, "owner", EventType::InstanceCompleted, "workflow completed");
	}

	TEST(NotificationObserverTest, InstanceFailedNotifiesOwner)
	{
		std::vector<ConnectorRequest> requests = notifyOnce(makeEvent(EventType::InstanceFailed, "").withMessage("item cancelled: verify-docs"));
		expectSingleNotification(requests, "owner", EventType::InstanceFailed, "item cancelled: verify-docs");
	}

	TEST(NotificationObserverTest, UnrelatedEventsTriggerNothing)
	{
		const EventType unrelated[] = {
			EventType::InstanceStarted, EventType::InstanceCancelled,
			EventType::ItemAvailable, EventType::ItemStarted, EventType::ItemSubmitted, EventType::ItemCompleted,
			EventType::ItemRejected, EventType::ItemCancelled, EventType::ItemSkipped, EventType::ItemReverted,
			EventType::DeadlineMissed, EventType::ExternalSignal, EventType::EngineNote
		};
		for (EventType type : unrelated)
		{
			EXPECT_TRUE(notifyOnce(makeEvent(type).byActor("u1").withMessage("x")).empty()) << toString(type);
		}
	}

	TEST(NotificationObserverTest, MissingConnectorDoesNotThrow)
	{
		ConnectorRegistry registry;
		NotificationObserver observer(registry);
		EventPublisher publisher;
		publisher.attach(&observer);
		EXPECT_NO_THROW(observer.onEvent(makeEvent(EventType::ItemAssigned).byActor("u1")));
		EXPECT_NO_THROW(publisher.notify(makeEvent(EventType::InstanceCompleted, "")));
	}

	TEST(NotificationObserverTest, ConnectorFailureIsIgnored)
	{
		std::vector<ConnectorRequest> requests;
		ConnectorRegistry registry;
		registry.registerConnector(std::unique_ptr<ExternalConnector>(new RecordingConnector("notifier", &requests, false)));
		NotificationObserver observer(registry);

		EXPECT_NO_THROW(observer.onEvent(makeEvent(EventType::ItemAssigned).byActor("u1")));
		EXPECT_EQ(requests.size(), 1u);
	}

	TEST(NotificationObserverTest, UsesNamedConnector)
	{
		std::vector<ConnectorRequest> default_requests;
		std::vector<ConnectorRequest> sms_requests;
		ConnectorRegistry registry;
		registry.registerConnector(std::unique_ptr<ExternalConnector>(new RecordingConnector("notifier", &default_requests)));
		registry.registerConnector(std::unique_ptr<ExternalConnector>(new RecordingConnector("sms", &sms_requests)));
		NotificationObserver observer(registry, "sms");

		observer.onEvent(makeEvent(EventType::ItemAssigned).byActor("u1").withMessage("assigned"));
		EXPECT_TRUE(default_requests.empty());
		expectSingleNotification(sms_requests, "u1", EventType::ItemAssigned, "assigned");
	}
} // namespace tividy