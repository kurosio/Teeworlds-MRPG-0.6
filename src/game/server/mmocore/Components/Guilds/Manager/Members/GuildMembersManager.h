#ifndef GAME_SERVER_COMPONENT_GUILD_MEMBERS_CONTROLLER_H
#define GAME_SERVER_COMPONENT_GUILD_MEMBERS_CONTROLLER_H

#include "GuildMemberData.h"

class CGuildData;
using CGuildMembersContainer = std::map<int, CGuildMemberData*>;

class CGuildMembersController
{
	CGS* GS() const;

	CGuildData* m_pGuild {};
	CGuildMembersContainer m_apMembers {};

public:
	CGuildMembersController(CGuildData* pGuild, std::string&& MembersData);
	~CGuildMembersController();

	// Get a guild member by account ID
	CGuildMemberData* GetMember(int AccountID);

	// Get the guild members container
	CGuildMembersContainer& GetContainer() { return m_apMembers; }

	// Join a guild by account ID
	GUILD_MEMBER_RESULT Join(int AccountID);

	// Kick a guild member by account ID
	GUILD_MEMBER_RESULT Kick(int AccountID);

	// Save the guild members data
	void Save() const;

private:
	// Initialize the guild members controller
	void Init(std::string&& MembersData);
};

#endif