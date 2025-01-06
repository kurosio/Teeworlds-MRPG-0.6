/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "path_finder.h"

#include <game/server/core/components/worlds/world_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/tools/path_navigator.h>

CEntityPathArrow::CEntityPathArrow(CGameWorld* pGameWorld, int ClientID, float AreaClipped, vec2 SearchPos, int WorldID, 
									const std::weak_ptr<CQuestStep>& pStep, int ConditionType, int ConditionIndex)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PATH_FINDER, SearchPos, 0, ClientID), m_ConditionType(ConditionType), m_ConditionIndex(ConditionIndex), m_pStep(pStep)
{
	const auto PosTo = GS()->Core()->WorldManager()->FindPosition(WorldID, SearchPos);
	if(!PosTo.has_value())
	{
		MarkForDestroy();
		return;
	}
	m_PosTo = PosTo.value();
	m_AreaClipped = AreaClipped;
	GameWorld()->InsertEntity(this);

	// quest navigator finder
	CPlayer* pPlayer = GetPlayer();
	if(pPlayer && pPlayer->GetItem(itShowQuestStarNavigator)->IsEquipped())
	{
		new CEntityPathNavigator(&GS()->m_World, this, true, pPlayer->m_ViewPos, SearchPos, WorldID, true, CmaskOne(ClientID));
	}
}

void CEntityPathArrow::Destroy()
{
	if(const auto& pStep = GetQuestStep())
	{
		const auto sharedThis = shared_from_this();
		std::erase(pStep->m_vpEntitiesNavigator, sharedThis);
	}
}

void CEntityPathArrow::Tick()
{
	if(is_negative_vec(m_PosTo))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	CQuestStep* pStep = GetQuestStep();
	if(!pStep)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(m_ConditionType == CONDITION_DEFEAT_BOT && pStep->m_aMobProgress[m_ConditionIndex].m_Complete)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(m_ConditionType == CONDITION_MOVE_TO && pStep->m_aMoveActionProgress[m_ConditionIndex])
	{
		GameWorld()->DestroyEntity(this);
		return;
	}
}

void CEntityPathArrow::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient)
		return;

	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	if(m_AreaClipped > 1.f && distance(m_PosTo, pPlayer->m_ViewPos) < m_AreaClipped)
		return;

	m_Pos = pPlayer->GetCharacter()->m_Core.m_Pos;
	const auto FinalPos = m_Pos - normalize(m_Pos - m_PosTo) * clamp(distance(m_Pos, m_PosTo), 32.0f, 90.0f);
	GS()->SnapPickup(SnappingClient, GetID(), FinalPos, POWERUP_ARMOR);
}

CPlayer* CEntityPathArrow::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

CQuestStep* CEntityPathArrow::GetQuestStep() const
{
	if(const auto pStep = m_pStep.lock())
	{
		return pStep.get();
	}
	return nullptr;
}
