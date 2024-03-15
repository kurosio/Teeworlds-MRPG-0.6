/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "engine/server.h"

#include "world_data.h"

#include "game/server/gamecontext.h"

void CWorldData::Init(int RespawnWorldID, int JailWorldID, int RequiredLevel, std::deque<CWorldSwapData>&& Worlds)
{
	str_copy(m_aName, Server()->GetWorldName(m_ID), sizeof(m_aName));
	m_RequiredLevel = RequiredLevel;
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
	if(pSecondWorldData && pPlayer->Account()->GetLevel() < m_RequiredLevel)
	{
		pGS->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "You must be at least level {INT} to moved!", m_RequiredLevel);
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

CWorldData* CWorldData::GetRespawnWorld() const
{
	if(m_RespawnWorldID < 0 || m_RespawnWorldID > (int)Data().size())
		return nullptr;

	return Data()[m_RespawnWorldID].get();
}

CWorldData* CWorldData::GetJailWorld() const
{
	if(m_JailWorldID < 0 || m_JailWorldID >(int)Data().size())
		return nullptr;

	return Data()[m_JailWorldID].get();
}
