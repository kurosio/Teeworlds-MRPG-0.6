#ifndef GAME_SERVER_COMPONENT_GUILD_MEMBERS_CONTROLLER_H
#define GAME_SERVER_COMPONENT_GUILD_MEMBERS_CONTROLLER_H

#include "Requests/GuildRequestsManager.h"
#include "GuildMemberData.h"

class CGuildData;
class CGuildRequestsController;
using CGuildMembersContainer = std::map<int, CGuildMemberData*>;

// Enum for guild member results
enum class GUILD_MEMBER_RESULT : int
{
	JOIN_ALREADY_IN_GUILD,      // Result when a member tries to join a guild they are already a part of

	KICK_DOES_NOT_EXIST,        // Result when trying to kick a member who does not exist
	CANT_KICK_LEADER,           // Result when trying to kick the guild leader

	REQUEST_ALREADY_SEND,

	UNDEFINED_ERROR,			// Result wher undefined error
	SUCCESSFUL                  // Result when the operation is successful
};

class CGuildMembersController
{
	CGS* GS() const;

	CGuildData* m_pGuild {};
	CGuildRequestsController* m_pRequests {};
	CGuildMembersContainer m_apMembers {};

public:
	CGuildMembersController(CGuildData* pGuild, std::string&& MembersData);
	~CGuildMembersController();

	// Returns the pointer to the controller requests to join
	CGuildRequestsController* GetRequests() const { return m_pRequests; }

	// Get a guild member by account ID
	CGuildMemberData* Get(int AccountID);

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