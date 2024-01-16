#ifndef GAME_SERVER_COMPONENT_GUILD_MEMBERS_CONTROLLER_H
#define GAME_SERVER_COMPONENT_GUILD_MEMBERS_CONTROLLER_H

#include "Requests/GuildRequestsManager.h"
#include "GuildMemberData.h"

// Forward declaration and alias
class CGuildData;
class CGuildRequestsManager;
using CGuildMembersContainer = std::map<int, CGuildMemberData*>;

// Enum class for representing different results of guild member operations
enum class GUILD_MEMBER_RESULT : int
{
	JOIN_ALREADY_IN_GUILD,      // Result when a member tries to join a guild they are already a part of
	KICK_DOES_NOT_EXIST,        // Result when trying to kick a member who does not exist
	CANT_KICK_LEADER,           // Result when trying to kick the guild leader
	REQUEST_ALREADY_SEND,       // Result when a member tries to send a join request to a guild they have already sent a request to
	NO_AVAILABLE_SLOTS,         // Result when there are no available slots in the guild for new members
	UNDEFINED_ERROR,            // Result when an undefined error occurs during the operation
	SUCCESSFUL                  // Result when the operation is successful
};

class CGuildMembersManager
{
	CGS* GS() const;

	CGuildData* m_pGuild {};
	CGuildRequestsManager* m_pRequests {};
	CGuildMembersContainer m_apMembers {};

public:
	CGuildMembersManager(CGuildData* pGuild, std::string&& MembersData);
	~CGuildMembersManager();

	// Returns the pointer to the controller requests to join
	CGuildRequestsManager* GetRequests() const { return m_pRequests; }

	// Get a guild member by account ID
	CGuildMemberData* Get(int AccountID);

	// Get the guild members container
	CGuildMembersContainer& GetContainer() { return m_apMembers; }

	// Join a guild by account ID
	[[nodiscard]] GUILD_MEMBER_RESULT Join(int AccountID);

	// Kick a guild member by account ID
	[[nodiscard]] GUILD_MEMBER_RESULT Kick(int AccountID);

	// This function checks if there are any free slots available
	bool HasFreeSlots() const;

	// This function returns the current number of slots being used and the total number of slots
	std::pair<int, int> GetCurrentSlots() const;

	// Save the guild members data
	void Save() const;

private:
	// Initialize the guild members controller
	void Init(std::string&& MembersData);
};

#endif