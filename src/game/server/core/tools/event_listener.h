#ifndef GAME_SERVER_CORE_TOOLS_EVENT_LISTENER_H
#define GAME_SERVER_CORE_TOOLS_EVENT_LISTENER_H

// forward
class CGS;
class IServer;
class CPlayer;
class CPlayerItem;
class CCharacter;

// event listener
class IEventListener
{
public:
	virtual ~IEventListener() = default;

	enum Type
	{
		PlayerDeath,
		PlayerSpawn,
		PlayerUpgrade,
		PlayerEquipItem,
		PlayerUnequipItem,
		PlayerEnchantItem,
		PlayerDurabilityItem,
	};

	virtual void OnPlayerDeath(CPlayer* pPlayer, CPlayer* pKiller, int Weapon) {}
	virtual void OnPlayerSpawn(CPlayer* pPlayer) {}
	virtual void OnPlayerUpgrade(CPlayer* pPlayer, int AttributeID) {}
	virtual void OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem) {}
	virtual void OnPlayerUnequipItem(CPlayer* pPlayer, CPlayerItem* pItem) {}
	virtual void OnPlayerEnchantItem(CPlayer* pPlayer, CPlayerItem* pItem) {}
	virtual void OnPlayerDurabilityItem(CPlayer* pPlayer, CPlayerItem* pItem) {}
};

class CEventListenerManager
{
	std::mutex m_Mutex;
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
		std::lock_guard lock(m_Mutex);
		if(const auto it = m_vListeners.find(event); it != m_vListeners.end())
		{
			for(auto* listener : it->second)
			{
				if constexpr(event == IEventListener::PlayerDeath)
					listener->OnPlayerDeath(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerSpawn)
					listener->OnPlayerSpawn(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerUpgrade)
					listener->OnPlayerUpgrade(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerEquipItem)
					listener->OnPlayerEquipItem(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerUnequipItem)
					listener->OnPlayerUnequipItem(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerEnchantItem)
					listener->OnPlayerEnchantItem(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerDurabilityItem)
					listener->OnPlayerDurabilityItem(std::forward<Ts>(args)...);
			}
		}
	}
};

extern CEventListenerManager g_EventListenerManager;

#endif