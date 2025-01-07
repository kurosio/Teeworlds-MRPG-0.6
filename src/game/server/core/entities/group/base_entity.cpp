#include "base_entity.h"

#include <game/server/gamecontext.h>
#include <game/server/entities/character.h>

#include "entitiy_group.h"

CBaseEntity::CBaseEntity(CGameWorld* pGameWorld, const std::shared_ptr<CEntityGroup>& group, int EnttypeID, vec2 Pos, int Owner)
	: CEntity(pGameWorld, EnttypeID, Pos)
{
	group->AddEntity(this);

	m_GroupPtr = group;
	m_ClientID = Owner;
	m_Mask = CmaskAll();

	TriggerEvent(EventCreate);
	GameWorld()->InsertEntity(this);
}

CBaseEntity::~CBaseEntity()
{
	for(const int& ID : m_vIDs)
	{
		Server()->SnapFreeID(ID);
	}

	TriggerEvent(EventDestroy);

	if(const auto group = GetGroup())
	{
		group->RemoveEntity(this);
	}
}

void CBaseEntity::RegisterEvent(EventType Type, const std::function<void(CBaseEntity*)>& Callback)
{
	switch(Type)
	{
		case EventCreate:
			m_CreateCallback = Callback;
			break;
		case EventTick:
			m_TickCallback = Callback;
			break;
		case EventTickDeferred:
			m_TickDeferredCallback = Callback;
			break;
		case EventDestroy:
			m_DestroyCallback = Callback;
			break;
		default:
			break;
	}
}

void CBaseEntity::RegisterEvent(EventType Type, int NumIDs, const std::function<void(CBaseEntity*, int, std::vector<int>)>& Callback)
{
	if(Type == EventSnap)
	{
		m_vIDs.resize(NumIDs);
		for(int i = 0; i < NumIDs; i++)
		{
			m_vIDs[i] = Server()->SnapNewID();
		}

		m_SnapCallback = Callback;
	}
}

CPlayer* CBaseEntity::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID, false, true);
}

CCharacter* CBaseEntity::GetCharacter() const
{
	return GetPlayer() ? GetPlayer()->GetCharacter() : nullptr;
}

void CBaseEntity::Tick()
{
	if(m_ClientID != -1 && !GetPlayer())
	{
		MarkForDestroy();
		return;
	}

	TriggerEvent(EventTick);
}

void CBaseEntity::TickDeferred()
{
	TriggerEvent(EventTickDeferred);
}

void CBaseEntity::Snap(int SnappingClient)
{
	if(m_Mask == CmaskAll() || CmaskIsSet(m_Mask, SnappingClient))
		TriggerEvent(EventSnap, SnappingClient, m_vIDs);
}