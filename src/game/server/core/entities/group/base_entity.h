#ifndef GAME_SERVER_ENTITIES_BASE_ENTITY_H
#define GAME_SERVER_ENTITIES_BASE_ENTITY_H

#include <game/server/entity.h>

class CPlayer;
class CEntityGroup;

class CBaseEntity : public CEntity, public mystd::CConfigurable
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
	std::vector<int>& GetIDs() { return m_vIDs; }
	int GetSnapID() const { return GetID(); }
	void SetMask(int64_t Mask) { m_Mask = Mask; }
	int64_t GetMask() const { return m_Mask; }

protected:
	void TriggerEvent(EventType Type)
	{
		if(!m_GroupPtr.lock())
		{
			MarkForDestroy();
			return;
		}

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
	void TriggerEvent(EventType Type, int SnappingClient, const std::vector<int>& vIds)
	{
		if(!m_GroupPtr.lock())
		{
			MarkForDestroy();
			return;
		}

		if(Type == EventSnap)
		{
			if(m_SnapCallback)
				m_SnapCallback(this, SnappingClient, vIds);
		}
	}

	std::function<void(CBaseEntity*)> m_CreateCallback{};
	std::function<void(CBaseEntity*)> m_TickCallback{};
	std::function<void(CBaseEntity*)> m_TickDeferredCallback{};
	std::function<void(CBaseEntity*)> m_DestroyCallback{};
	std::function<void(CBaseEntity*, int, const std::vector<int>&)> m_SnapCallback{};

	int64_t m_Mask{};
	std::weak_ptr<CEntityGroup> m_GroupPtr{};
	std::vector<int> m_vIDs{};
};

#endif
