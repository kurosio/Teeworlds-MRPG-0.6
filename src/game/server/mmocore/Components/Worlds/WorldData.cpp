/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "engine/server.h"

#include "WorldData.h"

#include "game/server/gamecontext.h"

std::list < CWorldSwapPosition > CWorldSwapPosition::ms_aWorldPositionLogic;

void CWorldData::Init(int RespawnWorldID, int RequiredQuestID, const std::deque<CWorldSwapData>& Worlds)
{
	str_copy(m_aName, Server()->GetWorldName(m_ID), sizeof(m_aName));
	m_RequiredQuestID = RequiredQuestID;
	m_RespawnWorldID = RespawnWorldID;
	m_Swappers = Worlds;
}

void CWorldData::Move(CPlayer* pPlayer)
{
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	CGS* pGS = dynamic_cast<CGS*>(Server()->GameServerPlayer(pPlayer->GetCID()));
	CWorldSwapData* pSwapper = GetSwapperByPos(pPlayer->GetCharacter()->GetPos());
	if(!pGS || !pSwapper)
		return;

	int ClientID = pPlayer->GetCID();
	CWorldData* pSecondWorldData = pGS->GetWorldData(pSwapper->GetSecondWorldID());
	if(pSecondWorldData && pSecondWorldData->GetRequiredQuest() && !pPlayer->GetQuest(pSecondWorldData->GetRequiredQuest()->GetID())->IsComplected())
	{
		pGS->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "Requires quest completion '{STR}'!", pSecondWorldData->GetRequiredQuest()->GetName());
		return;
	}

	pPlayer->GetTempData().m_TempTeleportPos = pSwapper->GetSecondSwapPosition();
	pPlayer->ChangeWorld(pSwapper->GetSecondWorldID());
}

CWorldSwapData* CWorldData::GetSwapperByPos(vec2 Pos)
{
	auto pWorld = std::find_if(m_Swappers.begin(), m_Swappers.end(), [=](const CWorldSwapData& pItem)
	{ return pItem.GetFirstWorldID() == m_ID && distance(pItem.GetFirstSwapPosition(), Pos) < 400; });
	if(pWorld != m_Swappers.end())
		return &(*pWorld);
	return nullptr;
}

CQuestDescription* CWorldData::GetRequiredQuest() const
{
	if(CQuestDescription::Data().find(m_RequiredQuestID) != CQuestDescription::Data().end())
		return &CQuestDescription::Data()[m_RequiredQuestID];
	return nullptr;
}

CWorldData* CWorldData::GetRespawnWorld() 
{
	auto p = std::find_if(Data().begin(), Data().end(), [this](const WorldDataPtr& p){return m_RespawnWorldID == p->GetID(); });
	return p != Data().end() ? (*p).get() : nullptr;
}
