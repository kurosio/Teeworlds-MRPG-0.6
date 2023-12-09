/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "engine/server.h"

#include "WorldData.h"

#include "game/server/gamecontext.h"

void CWorldData::Init(int RespawnWorldID, int JailWorldID, int RequiredQuestID, std::deque<CWorldSwapData>&& Worlds)
{
	str_copy(m_aName, Server()->GetWorldName(m_ID), sizeof(m_aName));
	m_RequiredQuestID = RequiredQuestID;
	m_RespawnWorldID = RespawnWorldID;
	m_JailWorldID = JailWorldID;
	m_Swappers = std::move(Worlds);
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
	if(pSecondWorldData && pSecondWorldData->GetRequiredQuest() && !pPlayer->GetQuest(pSecondWorldData->GetRequiredQuest()->GetID())->IsCompleted())
	{
		pGS->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "Requires quest completion '{STR}'!", pSecondWorldData->GetRequiredQuest()->GetName());
		return;
	}

	pPlayer->GetTempData().SetTeleportPosition(pSwapper->GetSecondSwapPosition());
	pPlayer->ChangeWorld(pSwapper->GetSecondWorldID());
}

CWorldSwapData* CWorldData::GetSwapperByPos(vec2 Pos)
{
	auto pWorld = std::find_if(m_Swappers.begin(), m_Swappers.end(), [=](const CWorldSwapData& pItem)
	{ return pItem.GetFirstWorldID() == m_ID && distance(pItem.GetFirstSwapPosition(), Pos) < 400; });
	return (pWorld != m_Swappers.end()) ? &(*pWorld) : nullptr;
}

CQuestDescription* CWorldData::GetRequiredQuest() const
{
	const auto it = CQuestDescription::Data().find(m_RequiredQuestID);
	return it != CQuestDescription::Data().end() ? &it->second : nullptr;
}

CWorldData* CWorldData::GetRespawnWorld() const
{
	if(m_RespawnWorldID < 0 || m_RespawnWorldID > Data().size())
		return nullptr;

	return Data()[m_RespawnWorldID].get();
}

CWorldData* CWorldData::GetJailWorld() const
{
	if(m_JailWorldID < 0 || m_JailWorldID > Data().size())
		return nullptr;

	return Data()[m_JailWorldID].get();
}
