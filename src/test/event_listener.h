#ifndef TEST_EVENT_LISTENER_H
#define TEST_EVENT_LISTENER_H

#include <algorithm>
#include <mutex>
#include <unordered_map>

#include <base/system.h>

// forward

// event listener
class IEventListener
{
public:
	virtual ~IEventListener() = default;

	enum Type
	{
		CharacterDamage,
		CharacterDeath,
	};

	virtual void OnCharacterDamage(int Damage) { }
	virtual void OnCharacterDeath(int Weapon) { }
};

class CEventListenerManager
{
	std::recursive_mutex m_Mutex;
	std::unordered_map<IEventListener::Type, std::vector<IEventListener*>> m_vListeners;
	std::unordered_map<IEventListener::Type, size_t> m_EventListenerCounts;

public:
	void RegisterListener(IEventListener::Type event, IEventListener* listener)
	{
		std::unique_lock lock(m_Mutex);
		auto& listeners = m_vListeners[event];

		if(std::ranges::find(listeners, listener) == listeners.end())
		{
			listeners.push_back(listener);
			m_EventListenerCounts[event]++;
		}
	}

	void UnregisterListener(IEventListener::Type event, IEventListener* listener)
	{
		std::unique_lock lock(m_Mutex);
		auto& listeners = m_vListeners[event];
		listeners.erase(std::ranges::remove(listeners, listener).begin(), listeners.end());

		if(!listeners.empty())
		{
			m_EventListenerCounts[event]--;
		}
		else
		{
			m_vListeners.erase(event);
			m_EventListenerCounts.erase(event);
		}
	}

	void LogRegisteredEvents()
	{
		std::unique_lock lock(m_Mutex);
		dbg_msg("EventListenerManager", "Registered events and their listeners:");

		for(const auto& [event, listeners] : m_vListeners)
		{
			dbg_msg("EventListenerManager", "Event: %d, Listeners count: %zu", event, listeners.size());
		}
	}

	template <IEventListener::Type event, typename... Ts>
	void Notify(Ts&&... args)
	{
		std::unique_lock lock(m_Mutex);
		if(const auto it = m_vListeners.find(event); it != m_vListeners.end())
		{
			for(auto* listener : it->second)
			{
				if constexpr(event == IEventListener::CharacterDamage)
					listener->OnCharacterDamage(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::CharacterDeath)
					listener->OnCharacterDeath(std::forward<Ts>(args)...);
			}
		}
	}
};

extern CEventListenerManager g_EventListenerManager;

#endif