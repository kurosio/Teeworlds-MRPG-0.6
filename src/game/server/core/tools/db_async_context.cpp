#include "db_async_context.h"

#include <game/server/gamecontext.h>

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
	return GS()->GetPlayer(GetClientID(), CheckAuth, CheckCharacter);
}
