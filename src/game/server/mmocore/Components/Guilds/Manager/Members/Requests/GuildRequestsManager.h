#ifndef GAME_SERVER_COMPONENT_GUILD_REUQUESTS_CONTROLLER_H
#define GAME_SERVER_COMPONENT_GUILD_REUQUESTS_CONTROLLER_H

#include "GuildRequestData.h"

class CGS;
class CGuildData;
class CGuildMemberData;
enum class GUILD_MEMBER_RESULT;
using GuildRequestsContainer = std::vector < CGuildRequestData* >;

// This class is a controller for managing guild requests to join a guild
class CGuildRequestsManager
{
	CGS* GS() const;

	CGuildData* m_pGuild {};
	GuildRequestsContainer m_aRequestsJoin {};

public:
	CGuildRequestsManager() = delete;
	CGuildRequestsManager(CGuildData* pGuild);
	~CGuildRequestsManager();

	// Getter for the join requests container
	const GuildRequestsContainer& GetContainer() const { return m_aRequestsJoin; }

	// Method for requesting to join the guild
	GUILD_MEMBER_RESULT Request(int FromUID);

	// Method for accepting a join request
	GUILD_MEMBER_RESULT Accept(int UserID, const CGuildMemberData* pFromMember = nullptr);

	// Method for denying a join request
	void Deny(int UserID, const CGuildMemberData* pFromMember = nullptr);

private:
	// Private method for initializing the controller
	void Init();
};

#endif