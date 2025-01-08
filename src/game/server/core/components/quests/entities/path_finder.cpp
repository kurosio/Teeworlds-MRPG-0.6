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
	m_pEntNavigator = nullptr;
	GameWorld()->InsertEntity(this);

	// quest navigator finder
	auto* pPlayer = GetPlayer();
	if(pPlayer && pPlayer->GetItem(itShowQuestStarNavigator)->IsEquipped())
		m_pEntNavigator = new CEntityPathNavigator(&GS()->m_World, m_ClientID, true, SearchPos, WorldID, true, CmaskOne(ClientID));
}

CEntityPathArrow::~CEntityPathArrow()
{
	if(const auto& pStep = GetQuestStep())
	{
		std::erase_if(pStep->m_vpEntitiesNavigator, [this](const auto* pEntPtr) 
		{
			return pEntPtr == this;
		});
	}

	if(m_pEntNavigator)
	{
		delete m_pEntNavigator;
		m_pEntNavigator = nullptr;
	}
}

void CEntityPathArrow::Tick()
{
	if(is_negative_vec(m_PosTo))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	auto* pPlayer = GetPlayer();
	if(!pPlayer)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	auto* pStep = GetQuestStep();
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

	if(!pPlayer->GetCharacter())
		return;

	m_Pos = pPlayer->GetCharacter()->m_Core.m_Pos;
}

void CEntityPathArrow::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient)
		return;

	if(m_AreaClipped > 1.f && distance(m_PosTo, m_Pos) < m_AreaClipped)
		return;

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
