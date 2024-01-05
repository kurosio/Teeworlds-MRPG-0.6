#ifndef GAME_SERVER_COMPONENT_GUILD_REUQUESTS_CONTROLLER_H
#define GAME_SERVER_COMPONENT_GUILD_REUQUESTS_CONTROLLER_H

#include "GuildRequestData.h"

class CGS;
class CGuildData;
class CGuildMemberData;
enum class GUILD_MEMBER_RESULT;
using GuildRequestsContainer = std::vector < CGuildRequestData* >;

class CGuildRequestsController
{
	CGS* GS() const;

	CGuildData* m_pGuild {};
	GuildRequestsContainer m_aRequestsJoin {};

public:
	CGuildRequestsController() = delete;
	CGuildRequestsController(CGuildData* pGuild);
	~CGuildRequestsController();

	// Get the guild invites container
	const GuildRequestsContainer& GetContainer() const { return m_aRequestsJoin; }

	GUILD_MEMBER_RESULT Request(int FromUID);
	GUILD_MEMBER_RESULT Accept(int UserID, const CGuildMemberData* pFromMember = nullptr);
	void Deny(int UserID, const CGuildMemberData* pFromMember = nullptr);

private:
	void Init();
};

#endif