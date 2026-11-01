/*
Aidan du Bruyn (25040104)
Connor Human (22752154)
Ishmael Ngwasheng (24642542)
Jay Macaskill (25198387)
Keval Govind (24904407)
Ruan van Tonder (25284607)
Thabo Ngwabi (24742512)

COS 214 (Software Modelling) TIVIDY
Last Modified: 31 October 2026
*/

/**
 * @file Observer.cpp
 * @brief Implements the Observer plumbing: EventPublisher keeps the list of observers and notifies each one of every event.
 */

#ifndef TIVIDY_EVENTS_OBSERVER_CPP
#define TIVIDY_EVENTS_OBSERVER_CPP

#include "engine/events/Observer.h"

#include <algorithm>

namespace tividy
{
	EventObserver::~EventObserver() { }

	EventSubject::~EventSubject() { }

	EventPublisher::EventPublisher() : EventSubject() { }

	void EventPublisher::attach(EventObserver* observer) // no duplicates
	{
		if (std::find(observers.begin(), observers.end(), observer) == observers.end())
			observers.push_back(observer);
	}

	void EventPublisher::detach(EventObserver* observer)
	{
		std::vector<EventObserver*>::iterator it = observers.begin();
		while (it != observers.end())
		{
			if (*it == observer)
			{
				observers.erase(it);
				return;
			}
			it ++;
		}
	}

	void EventPublisher::notify(const WorkflowEvent& event)
	{
		std::vector<EventObserver*> copy = observers;
		for (EventObserver* o : copy)
		{
			try { o->onEvent(event); } catch (...) { /* here we catch everything and keep notifying */ }
		}
	}

	std::size_t EventPublisher::observerCount() const
	{
		return observers.size();
	}
} // namespace tividy

#endif // TIVIDY_EVENTS_OBSERVER_CPP