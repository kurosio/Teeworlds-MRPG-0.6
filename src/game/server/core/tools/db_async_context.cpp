#include "db_async_context.h"

#include <game/server/gamecontext.h>

DbAsync::CContextBase::CContextBase(int ClientID, int WorldID)
	: m_ClientID(ClientID), m_WorldID(WorldID == INITIALIZER_WORLD_ID ? ResolveClientWorldID(ClientID) : WorldID)
{
	if(auto* pGS = GS())
		if(auto* pPlayer = pGS->GetPlayer(ClientID, false))
			m_pConnectionLifetime = pPlayer->GetSharedData().m_pConnectionLifetime;
}

CGS* DbAsync::CContextBase::GS() const
{
	return static_cast<CGS*>(Instance::GameServer(m_WorldID));
}

CGS* DbAsync::CContextBase::GS(int WorldID) const
{
	return static_cast<CGS*>(Instance::GameServer(WorldID));
}

CPlayer* DbAsync::CContextBase::GetPlayer(bool CheckAuth, bool CheckCharacter) const
{
	auto Lifetime = m_pConnectionLifetime.lock();
	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
	auto* pPlayer = pGS ? pGS->GetPlayer(m_ClientID, CheckAuth, CheckCharacter) : nullptr;
	return Lifetime && pPlayer && Lifetime == pPlayer->GetSharedData().m_pConnectionLifetime ? pPlayer : nullptr;
}
