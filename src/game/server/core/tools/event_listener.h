#ifndef GAME_SERVER_CORE_TOOLS_EVENT_LISTENER_H
#define GAME_SERVER_CORE_TOOLS_EVENT_LISTENER_H
#include <typeindex>

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
};

class EventCharacterDieListener : public IEventListener
{
public:
	virtual void HandleEvent(CPlayer* pKiller, CPlayer* pVictim) = 0;
};

class CEventListenerManager
{
	std::mutex m_Mutex;
	std::unordered_map<std::type_index, std::vector<IEventListener*>> m_vListeners;

public:
	template <typename EventListenerType>
	void RegisterListener(IEventListener* listener)
	{
		std::lock_guard lock(m_Mutex);
		m_vListeners[typeid(EventListenerType)].push_back(listener);
	}

	template <typename EventListenerType>
	void UnregisterListener(IEventListener* listener)
	{
		std::lock_guard lock(m_Mutex);
		auto& listeners = m_vListeners[typeid(EventListenerType)];
		std::erase(listeners, listener);
	}

	template <typename EventListenerType, typename... Ts>
	void Notify(Ts&&... args)
	{
		std::lock_guard lock(m_Mutex);
		if(const auto it = m_vListeners.find(typeid(EventListenerType)); it != m_vListeners.end())
		{
			const auto listeners = it->second;
			for(auto* listener : listeners)
				static_cast<EventListenerType*>(listener)->HandleEvent(std::forward<Ts>(args)...);
		}
	}
};

#endif