#ifndef GAME_SERVER_ENTITIES_BASE_ENTITY_H
#define GAME_SERVER_ENTITIES_BASE_ENTITY_H

#include <game/server/core/utilites.h>
#include <game/server/entity.h>

class CPlayer;
class CEntityGroup;
using ConfigVariant = std::variant<int, float, vec2, std::string>;

class CBaseEntity : public CEntity, public mrpgstd::CConfigurable
{
public:
	enum EventType
	{
		EventCreate,
		EventTick,
		EventTickDeferred,
		EventSnap,
		EventDestroy
	};

	CBaseEntity(CGameWorld* pGameWorld, const std::shared_ptr<CEntityGroup>& group, int EnttypeID, vec2 Pos, int Owner);
	~CBaseEntity() override;

	CPlayer* GetPlayer() const;
	CCharacter* GetCharacter() const;

	void Tick() override;
	void TickDeferred() override;
	void Snap(int SnappingClient) override;

	void RegisterEvent(EventType Type, const std::function<void(CBaseEntity*)>& Callback);
	void RegisterEvent(EventType Type, int NumIDs, const std::function<void(CBaseEntity*, int, std::vector<int>)>& Callback);
	std::shared_ptr<CEntityGroup> GetGroup() const { return m_GroupPtr.lock(); }

	bool IsEnabledEvent(EventType Type) const { return !m_vDisabledEvents.contains(Type); }
	void DisableEvent(EventType Type)
	{
		if(IsEnabledEvent(Type))
			m_vDisabledEvents.emplace(Type);
	}
	void EnableEvent(EventType Type)
	{
		if(!IsEnabledEvent(Type))
			m_vDisabledEvents.erase(Type);
	}
	std::vector<int>& GetIDs() { return m_vIDs; }

protected:
	void TriggerEvent(EventType Type)
	{
		if(!IsEnabledEvent(Type))
			return;

		if(Type == EventCreate)
		{
			if(m_CreateCallback)
			{
				m_CreateCallback(this);
				m_CreateCallback = nullptr;
			}
		}
		else if(Type == EventTick)
		{
			if(m_TickCallback)
				m_TickCallback(this);
		}
		else if(Type == EventTickDeferred)
		{
			if(m_TickDeferredCallback)
				m_TickDeferredCallback(this);
		}
		else if(Type == EventDestroy)
		{
			if(m_DestroyCallback)
				m_DestroyCallback(this);
		}
	}

	struct HitCharactersEventCallback
	{
		std::function<void(CBaseEntity*, std::vector<CCharacter*>)> Callback;
		float HitRadius;
	};

	struct HitEntitiesEventCallback
	{
		std::function<void(CBaseEntity*, std::vector<CEntity*>)> Callback;
		float HitRadius;
	};

	std::function<void(CBaseEntity*)> m_CreateCallback;
	std::function<void(CBaseEntity*)> m_TickCallback;
	std::function<void(CBaseEntity*)> m_TickDeferredCallback;
	std::function<void(CBaseEntity*)> m_DestroyCallback;
	std::function<void(CBaseEntity*, int, const std::vector<int>&)> m_SnapCallback;

	std::set<int> m_vDisabledEvents;
	std::weak_ptr<CEntityGroup> m_GroupPtr;
	std::vector<int> m_vIDs;
};

#endif
