#ifndef GAME_SERVER_CORE_TOOLS_EVENT_LISTENER_H
#define GAME_SERVER_CORE_TOOLS_EVENT_LISTENER_H

// forward
class CGS;
class IServer;
class CPlayer;
class CProfession;
class CCraftItem;
class CPlayerItem;
class CCharacter;
class CPlayerQuest;

// event listener
class IEventListener
{
	friend class CEventListenerManager;

public:
	virtual ~IEventListener() = default;

	enum Type
	{
		CharacterDamage,
		CharacterDeath,
		CharacterSpawn,
		PlayerProfessionUpgrade,
		PlayerProfessionLeveling,
		PlayerGotItem,
		PlayerLostItem,
		PlayerCraftItem,
		PlayerEquipItem,
		PlayerUnequipItem,
		PlayerEnchantItem,
		PlayerDurabilityItem,
		PlayerQuestChangeState,
	};

private:
	virtual void OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage) { }
	virtual void OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon) { }
	virtual void OnCharacterSpawn(CPlayer* pPlayer) { }
	virtual void OnPlayerProfessionUpgrade(CPlayer* pPlayer, int AttributeID) { }
	virtual void OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel) { }
	virtual void OnPlayerGotItem(CPlayer* pPlayer, CPlayerItem* pItem, int Got) { }
	virtual void OnPlayerLostItem(CPlayer* pPlayer, CPlayerItem* pItem, int Lost) { }
	virtual void OnPlayerCraftItem(CPlayer* pPlayer, CCraftItem* pCraft) { }
	virtual void OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem) { }
	virtual void OnPlayerUnequipItem(CPlayer* pPlayer, CPlayerItem* pItem) { }
	virtual void OnPlayerEnchantItem(CPlayer* pPlayer, CPlayerItem* pItem) { }
	virtual void OnPlayerDurabilityItem(CPlayer* pPlayer, CPlayerItem* pItem) { }
	virtual void OnPlayerQuestChangeState(CPlayer* pPlayer, CPlayerQuest* pQuest, QuestState NewState) { }
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
				else if constexpr(event == IEventListener::CharacterSpawn)
					listener->OnCharacterSpawn(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerProfessionUpgrade)
					listener->OnPlayerProfessionUpgrade(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerProfessionLeveling)
					listener->OnPlayerProfessionLeveling(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerGotItem)
					listener->OnPlayerGotItem(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerLostItem)
					listener->OnPlayerLostItem(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerCraftItem)
					listener->OnPlayerCraftItem(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerEquipItem)
					listener->OnPlayerEquipItem(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerUnequipItem)
					listener->OnPlayerUnequipItem(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerEnchantItem)
					listener->OnPlayerEnchantItem(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerDurabilityItem)
					listener->OnPlayerDurabilityItem(std::forward<Ts>(args)...);
				else if constexpr(event == IEventListener::PlayerQuestChangeState)
					listener->OnPlayerQuestChangeState(std::forward<Ts>(args)...);
			}
		}
	}
};

extern CEventListenerManager g_EventListenerManager;

#endif