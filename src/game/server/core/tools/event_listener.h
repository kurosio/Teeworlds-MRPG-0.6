#ifndef GAME_SERVER_CORE_TOOLS_EVENT_LISTENER_H
#define GAME_SERVER_CORE_TOOLS_EVENT_LISTENER_H

// forward
class CGS;
class IServer;
class CPlayer;
class CCharacter;

// event listener
class IEventListener
{
public:
	virtual ~IEventListener() = default;

	enum class Type
	{
		PlayerDeath,
		PlayerSpawn,
		PlayerAttributeUpdate,
	};

	virtual void OnPlayerDeath(CPlayer* pPlayer, CPlayer* pKiller, int Weapon) {}
	virtual void OnPlayerSpawn(CPlayer* pPlayer) {}
	virtual void OnPlayerAttributeUpdate(CPlayer* pPlayer, int AttributeID, size_t Amount) {}
};

class CEventListenerManager
{
	std::mutex m_Mutex;
	std::unordered_map<IEventListener::Type, std::vector<IEventListener*>> m_vListeners;

public:
	void RegisterListener(IEventListener::Type event, IEventListener* listener)
	{
		std::unique_lock lock(m_Mutex);
		auto& listeners = m_vListeners[event];

		if(std::ranges::find(listeners, listener) == listeners.end())
		{
			listeners.push_back(listener);
		}
	}

	void UnregisterListener(IEventListener::Type event, IEventListener* listener)
	{
		std::unique_lock lock(m_Mutex);
		auto& listeners = m_vListeners[event];

		listeners.erase(std::ranges::remove(listeners, listener).begin(), listeners.end());
		if(listeners.empty())
		{
			m_vListeners.erase(event);
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
				if constexpr(event == IEventListener::Type::PlayerDeath)
				{
					listener->OnPlayerDeath(std::forward<Ts>(args)...);
				}
				else if constexpr(event == IEventListener::Type::PlayerSpawn)
				{
					listener->OnPlayerSpawn(std::forward<Ts>(args)...);
				}
				else if constexpr(event == IEventListener::Type::PlayerAttributeUpdate)
				{
					listener->OnPlayerAttributeUpdate(std::forward<Ts>(args)...);
				}
			}
		}
	}
};

#endif