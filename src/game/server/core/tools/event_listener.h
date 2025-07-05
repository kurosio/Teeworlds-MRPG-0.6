#ifndef GAME_SERVER_CORE_TOOLS_EVENT_LISTENER_H
#define GAME_SERVER_CORE_TOOLS_EVENT_LISTENER_H

// forward
class CGS;
class IServer;
class CPlayer;
class CAccountData;
class CProfession;
class CCraftItem;
class CPlayerItem;
class CCharacter;
class CPlayerQuest;

#define LIST_OF_ALL_EVENTS(XEV) \
    XEV(CharacterDamage,          OnCharacterDamage,         CPlayer* pFrom, CPlayer* pTo, int Damage) \
    XEV(CharacterDeath,           OnCharacterDeath,          CPlayer* pVictim, CPlayer* pKiller, int Weapon) \
    XEV(CharacterSpawn,           OnCharacterSpawn,          CPlayer* pPlayer) \
    XEV(PlayerLogin,              OnPlayerLogin,             CPlayer* pPlayer, CAccountData* pAccount) \
    XEV(PlayerChat,               OnPlayerChat,              CPlayer* pPlayer, const char* pMessage) \
    XEV(PlayerProfessionUpgrade,  OnPlayerProfessionUpgrade, CPlayer* pPlayer, int AttributeID) \
    XEV(PlayerProfessionLeveling, OnPlayerProfessionLeveling,CPlayer* pPlayer, CProfession* pProfession, int NewLevel) \
    XEV(PlayerProfessionChange,   OnPlayerProfessionChange,  CPlayer* pPlayer, CProfession* pOldProf, CProfession* pNewProf) \
    XEV(PlayerGotItem,            OnPlayerGotItem,           CPlayer* pPlayer, CPlayerItem* pItem, int Got) \
    XEV(PlayerLostItem,           OnPlayerLostItem,          CPlayer* pPlayer, CPlayerItem* pItem, int Lost) \
    XEV(PlayerCraftItem,          OnPlayerCraftItem,         CPlayer* pPlayer, CCraftItem* pCraft) \
    XEV(PlayerEquipItem,          OnPlayerEquipItem,         CPlayer* pPlayer, CPlayerItem* pItem) \
    XEV(PlayerUnequipItem,        OnPlayerUnequipItem,       CPlayer* pPlayer, CPlayerItem* pItem) \
    XEV(PlayerEnchantItem,        OnPlayerEnchantItem,       CPlayer* pPlayer, CPlayerItem* pItem) \
    XEV(PlayerDurabilityItem,     OnPlayerDurabilityItem,    CPlayer* pPlayer, CPlayerItem* pItem, int OldDurability) \
    XEV(PlayerQuestChangeState,   OnPlayerQuestChangeState,  CPlayer* pPlayer, CPlayerQuest* pQuest, QuestState NewState)

// event listener
class IEventListener
{
public:
	virtual ~IEventListener() = default;

	enum Type
	{
#define XDEF(name, func, ...) name,
		LIST_OF_ALL_EVENTS(XDEF)
#undef XDEF
	};

#define XDEF(name, func, ...) virtual void func(__VA_ARGS__) {}
	LIST_OF_ALL_EVENTS(XDEF)
#undef XDEF
};

class CEventListenerManager
{
	std::mutex m_Mutex;
	std::unordered_map<IEventListener::Type, std::unordered_set<IEventListener*>> m_Listeners;

public:
	void RegisterListener(IEventListener::Type event, IEventListener* listener)
	{
		std::unique_lock lock(m_Mutex);
		m_Listeners[event].insert(listener);
	}

	void UnregisterListener(IEventListener::Type event, IEventListener* listener)
	{
		std::unique_lock lock(m_Mutex);
		if(auto it = m_Listeners.find(event); it != m_Listeners.end())
		{
			it->second.erase(listener);
			if(it->second.empty())
				m_Listeners.erase(it);
		}
	}

	void LogRegisteredEvents()
	{
		dbg_msg("EventListenerManager", "Registered events and their listeners:");

		std::unique_lock lock(m_Mutex);
		for(const auto& [event, listeners] : m_Listeners)
			dbg_msg("EventListenerManager", "Event: %d, Listeners count: %zu", event, listeners.size());
	}

	template <IEventListener::Type event, typename... Ts>
	void Notify(Ts&&... args)
	{
		static constexpr auto EventDispatchTable = std::tuple {
#define XDEF(name, func, ...) &IEventListener::func,
			LIST_OF_ALL_EVENTS(XDEF)
#undef XDEF
		};

		std::unique_lock lock(m_Mutex);
		if(const auto it = m_Listeners.find(event); it != m_Listeners.end())
		{
			// safety copy listeners / e.g. in cases where the event call is recursive, or in cases where the subscriber unsubscribe in time.
			const auto pfnMember = std::get<static_cast<size_t>(event)>(EventDispatchTable);
			auto listenersCopy = it->second;
			lock.unlock();

			for(auto* pListener : listenersCopy)
				(pListener->*pfnMember)(std::forward<Ts>(args)...);
		}
	}
};

extern CEventListenerManager g_EventListenerManager;
#undef LIST_OF_ALL_EVENTS

// scoped event listener (RAII)
class ScopedEventListener
{
	IEventListener* m_pListener {};
	std::vector<IEventListener::Type> m_vEvents {};
	bool m_IsRegistered {};

public:
	ScopedEventListener() = default;
	~ScopedEventListener()
	{
		Unregister();
	}

	template<typename... TEvents>
	void Init(IEventListener* pListener, TEvents... events)
	{
		static_assert(std::conjunction_v<std::is_same<TEvents, IEventListener::Type>...>,
			"All events must be type IEventListener::Type");

		Unregister();
		m_pListener = pListener;
		m_vEvents.clear();
		m_vEvents.reserve(sizeof...(events));
		(m_vEvents.push_back(events), ...);
	}

	void Init(IEventListener* pListener, const std::vector<IEventListener::Type>& events)
	{
		Unregister();
		m_pListener = pListener;
		m_vEvents = events;
	}

	void Register()
	{
		if(!m_IsRegistered && m_pListener && !m_vEvents.empty())
		{
			for(const auto& event : m_vEvents)
				g_EventListenerManager.RegisterListener(event, m_pListener);

			m_IsRegistered = true;
		}
	}

	void Unregister()
	{
		if(m_IsRegistered && m_pListener)
		{
			for(const auto& event : m_vEvents)
				g_EventListenerManager.UnregisterListener(event, m_pListener);

			m_IsRegistered = false;
		}
	}

	// disable copying and allow move
	ScopedEventListener(const ScopedEventListener&) = delete;
	ScopedEventListener& operator=(const ScopedEventListener&) = delete;
	ScopedEventListener(ScopedEventListener&&) = default;
	ScopedEventListener& operator=(ScopedEventListener&&) = default;
};

#endif