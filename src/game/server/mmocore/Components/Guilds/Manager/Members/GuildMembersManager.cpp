/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildMembersManager.h"

CGuildMembersController::CGuildMembersController(CGuildData* pGuild) : m_pGuild(pGuild)
{
	m_aMembers.reserve(MAX_GUILD_PLAYERS);
}

void CGuildMembersController::InitMembers()
{

}
