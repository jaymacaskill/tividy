/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 22 October 2026
*/

/**
 * @file Observer.h
 * @brief Observer pattern: EventObserver, EventSubject, EventPublisher.
 */

#ifndef TIVIDY_EVENTS_OBSERVER_H
#define TIVIDY_EVENTS_OBSERVER_H

#include <vector>

#include "engine/events/WorkflowEvent.h"

namespace tividy
{

	/**
	 * @brief Observer (GoF "Observer" participant).
	 *
	 * Reacts to events. Concrete observers: FlowController (routing/assignment), AuditObserver,
	 * NotificationObserver, AnalyticsObserver, plus any a client adds.
	 */
	class EventObserver
	{
		public:
			virtual ~EventObserver();

			/**
			 * @brief Push-model update.
			 * @param event The fact that just happened. Valid only during the call; copy it to keep it.
			 * @details Must not throw. If it must signal a problem it records it itself. Must not detach
			 *          itself or others from the publisher during the call.
			 */
			virtual void onEvent(const WorkflowEvent& event) = 0;
	};

	/** Subject (GoF "Subject" participant): the attach/detach/notify interface. */
	class EventSubject
	{
		public:
			virtual ~EventSubject();

			/** Registers a non-owning observer. @pre observer != nullptr. Attaching the same pointer twice has no effect. */
			virtual void attach(EventObserver* observer) = 0;

			/** Unregisters. No effect if not attached. */
			virtual void detach(EventObserver* observer) = 0;

			/**
			 * @brief Delivers @p event to every attached observer, in attach order.
			 * @details Iterates over a COPY of the observer list so attach/detach during delivery is safe.
			 *          An exception escaping one observer is caught and swallowed so the remaining observers
			 *          still receive the event.
			 */
			virtual void notify(const WorkflowEvent& event) = 0;
	};

	/**
	 * @brief ConcreteSubject: the per-instance event channel.
	 *
	 * Each WorkflowInstance owns exactly one EventPublisher, which keeps instances isolated.
	 * Work items depend only on this class (through WorkContext), never on any listener.
	 * The publisher does NOT own its observers; WorkflowInstance owns them and guarantees they
	 * outlive the publisher.
	 */
	class EventPublisher : public EventSubject
	{
		public:
			EventPublisher();

			void attach(EventObserver* observer) override;
			void detach(EventObserver* observer) override;
			void notify(const WorkflowEvent& event) override;

			/** @return number of attached observers (for tests). */
			std::size_t observerCount() const;
		private:
			std::vector<EventObserver*> observers;
	};
} // namespace tividy

#endif // TIVIDY_EVENTS_OBSERVER_H