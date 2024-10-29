/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "world_data.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>

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
	// check if the player is valid
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	auto* pGS = (CGS*)Server()->GameServerPlayer(pPlayer->GetCID());
	const auto* pSwapper = GetSwapperByPos(pPlayer->GetCharacter()->GetPos());

	// check if the swapper is valid
	if(!pGS || !pSwapper)
		return;

	const int ClientID = pPlayer->GetCID();
	const auto* pSecondWorldData = pGS->GetWorldData(pSwapper->GetSecondWorldID());

	// check if the player has the required level to move
	if(pSecondWorldData && pPlayer->Account()->GetLevel() < m_RequiredLevel)
	{
		pGS->Broadcast(ClientID, BroadcastPriority::MainInformation, 100, "You must be at least level {} to move!", m_RequiredLevel);
		return;
	}

	pPlayer->ChangeWorld(pSwapper->GetSecondWorldID(), pSwapper->GetSecondSwapPosition());
}

CWorldSwapData* CWorldData::GetSwapperByPos(vec2 Pos)
{
	const auto pWorld = std::ranges::find_if(m_Swappers, [&](const CWorldSwapData& pItem)
	{
		return pItem.GetFirstWorldID() == m_ID && distance(pItem.GetFirstSwapPosition(), Pos) < 400;
	});

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
