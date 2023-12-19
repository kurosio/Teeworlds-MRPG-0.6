/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_MEMBERS_CONTROLLER_H
#define GAME_SERVER_COMPONENT_GUILD_MEMBERS_CONTROLLER_H

#include "GuildMemberData.h"

class CGuildData;
using CGuildMembersContainer = std::vector<CGuildMemberData*>;

class CGuildMembersController
{
	CGS* GS() const;

	CGuildData* m_pGuild {};
	CGuildMembersContainer m_apMembers {};

public:
	CGuildMembersController(CGuildData* pGuild);
	~CGuildMembersController();

	CGuildMembersContainer& GetContainer() { return m_apMembers; }

	bool Kick(int AccountID);
	bool Join(int AccountID);

private:
	void InitMembers();
	CGuildMemberData* GetMember(int AccountID);
};

#endif
