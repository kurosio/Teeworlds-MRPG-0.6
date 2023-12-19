/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_MEMBERS_CONTROLLER_H
#define GAME_SERVER_COMPONENT_GUILD_MEMBERS_CONTROLLER_H

#include "GuildMemberData.h"

class CGuildData;
using CGuildMembersContainer = std::vector<CGuildMemberData>;

class CGuildMembersController
{
	CGuildData* m_pGuild {};
	CGuildMembersContainer m_aMembers {};

public:
	CGuildMembersController(CGuildData* pGuild);

	CGuildMembersContainer& GetMembers() { return m_aMembers; }

private:
	void InitMembers();
};

#endif
