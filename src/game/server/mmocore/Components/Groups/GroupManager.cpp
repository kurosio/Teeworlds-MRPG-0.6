/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GroupManager.h"

#include "GroupData.h"

#include <game/server/gamecontext.h>

// Initialization function for CGroupManager class
void CGroupManager::OnInit()
{
	// Create a pointer to store the result of the database query
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_groups");
	while(pRes->next())
	{
		// Get the values of the columns for the current row
		GroupIdentifier ID = pRes->getInt("ID");
		int OwnerUID = pRes->getInt("OwnerUID");
		int TeamColor = pRes->getInt("Color");
		std::string StrAccountIDs = pRes->getString("AccountIDs").c_str();

		// Initialize a GroupData object with the retrieved values
		GroupData(ID).Init(OwnerUID, TeamColor, StrAccountIDs);
	}
}

// This code is a method within a class called CGroupManager
// The purpose of this method is to initialize the group data for a player's account
// The method takes a pointer to a CPlayer object as a parameter
void CGroupManager::OnInitAccount(CPlayer* pPlayer)
{
	// Call the ReinitializeGroup() method of the player's account object 
	// to initialize the group data for the account
	pPlayer->Acc().ReinitializeGroup();
}

// Function to create a group for a player
// Input: Pointer to the player to create a group for
// Output: Pointer to GroupData representing the created group
GroupData* CGroupManager::CreateGroup(CPlayer* pPlayer) const
{
	// Check if the player is authenticated
	if(!pPlayer || !pPlayer->IsAuthed())
		return nullptr;

	// Check if the player is already in a group
	if(pPlayer->Acc().GetGroup())
	{
		GS()->Chat(pPlayer->GetCID(), "You're already in a group!");
		return nullptr;
	}

	// Get the highest group ID from the database
	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_groups", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1; // Increment the highest group ID by 1, or set to 1 if no previous group exists

	// Create a string with the player's account ID
	int Color = 1 + random_int() % 63;
	int OwnerUID = pPlayer->Acc().GetID();
	std::string StrAccountIDs = std::to_string(OwnerUID);

	// Insert the new group into the database
	Database->Execute<DB::INSERT>("tw_groups", "(ID, OwnerUID, Color, AccountIDs) VALUES ('%d', '%d', '%d', '%s')", InitID, OwnerUID, Color, StrAccountIDs.c_str());

	// Initialize the group data
	GroupData(InitID).Init(OwnerUID, Color, StrAccountIDs);
	pPlayer->Acc().ReinitializeGroup();

	// Inform the player that the group was created
	GS()->Chat(pPlayer->GetCID(), "The group was created!");
	return &GroupData::Data()[InitID];
}

// Function to display the group menu for a player
void CGroupManager::ShowGroupMenu(CPlayer* pPlayer)
{
	// Get the client ID of the player
	int ClientID = pPlayer->GetCID();

	// Group information
	GS()->AVH(ClientID, TAB_GROUP_COMMANDS, "‼ Group commands");
	GS()->AVM(ClientID, "null", NOPE, TAB_GROUP_COMMANDS, "/group - get all sub commands");
	GS()->AV(ClientID, "null");

	// Check if the player does not have a group
	if(!pPlayer->Acc().GetGroup())
	{
		// Show the option to create a new group in the clients available votes list
		GS()->AVL(ClientID, "GROUP_CREATE", "Create a new group.");
		return;
	}

	// Group list for interaction
	int HideID = 3200;
	GroupData* pGroup = pPlayer->Acc().GetGroup();
	bool IsOwner = pGroup->OwnerUID() == pPlayer->Acc().GetID();
	for(auto& AID : pGroup->GetAccounts())
	{
		// Get the player name for the account
		std::string PlayerName = Job()->PlayerName(AID);
		GS()->AVH(ClientID, HideID, "{STR}{STR}", (AID == pGroup->OwnerUID() ? "*" : "\0"), PlayerName.c_str());

		// Check if the current player is the owner or if the account belongs to the current player
		if(!IsOwner || (AID == pPlayer->Acc().GetID()))
		{
			// Add the only player name to the group list
			GS()->AVM(ClientID, "null", NOPE, HideID, "Interaction is not available", PlayerName.c_str());
		}
		else
		{
			// Add the options
			GS()->AVM(ClientID, "GROUP_KICK", AID, HideID, "Kick {STR}", PlayerName.c_str());
			GS()->AVM(ClientID, "GROUP_CHANGE_OWNER", AID, HideID, "Transfer ownership {STR}", PlayerName.c_str());
		}

		// Increment the value of HideID by 1
		HideID++;
	}
	GS()->AV(ClientID, "null");

	// Show group some interactions
	if(IsOwner)
	{
		// Display a message to change the colour of the group
		GS()->AVL(ClientID, "GROUP_CHANGE_COLOR", "Change the colour: ({INT}) current", pGroup->GetTeamColor());
		GS()->AVL(ClientID, "GROUP_DISBAND", "Disband group");
	}
	GS()->AVM(ClientID, "GROUP_KICK", pPlayer->Acc().GetID(), NOPE, "Leave the group");
	GS()->AV(ClientID, "null");

	// Player list for invition
	GroupIdentifier GroupID = pGroup->GetID();
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		// If the player does not exist, continue to the next iteration
		CPlayer* pSearchPlayer = GS()->GetPlayer(i, true);
		if(!pSearchPlayer)
			continue;

		// If the searched player does not belong to any group, send them an invitation
		GroupData* pSearchGroup = pSearchPlayer->Acc().GetGroup();
		if(!pSearchGroup)
		{
			GS()->AVD(ClientID, "GROUP_INVITE", i, GroupID, NOPE, "Invite {STR}", Server()->ClientName(i));
			continue;
		}

		// If the searched player belongs to a different group, send them a message indicating so
		if(pSearchGroup->GetID() != GroupID)
		{
			GS()->AVM(ClientID, "nope", NOPE, NOPE, "{STR} (another group)", Server()->ClientName(i));
		}
	}
}

bool CGroupManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();

	if(ReplaceMenu)
		return false;

	if(Menulist == MENU_GROUP)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;
		ShowGroupMenu(pPlayer);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	return false;
}

static void CallbackVoteOptionalGroupInvite(CPlayer* pPlayer, int OptionID, int OptionID2, int Option)
{
	// Get variables
	CGS* pGS = pPlayer->GS();
	int ClientID = pPlayer->GetCID();

	// Get references to the option IDs
	int& InvitedCID = OptionID;
	int& GroupID = OptionID2;

	// Check if the group ID exists in the group data
	if(GroupData::Data().find(GroupID) == GroupData::Data().end())
		return;

	// Check the selected option
	if(Option == 1)
	{
		// Send chat messages to the player and the invited player
		pGS->Chat(ClientID, "You've accepted the invitation!");
		pGS->Chat(InvitedCID, "{STR} accepted your invitation!", pGS->Server()->ClientName(ClientID));

		// Add the player to the group
		GroupData::Data()[GroupID].Add(pPlayer->Acc().GetID());
	}
	else
	{
		// Send chat messages to the player and the invited player
		pGS->Chat(ClientID, "You declined the invitation!");
		pGS->Chat(InvitedCID, "{STR} declined your invitation!", pGS->Server()->ClientName(ClientID));
	}
}

bool CGroupManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	// Check if the command is "GROUP_CREATE"
	if(PPSTR(CMD, "GROUP_CREATE") == 0)
	{
		// If the group creation is successful
		if(CreateGroup(pPlayer))
		{
			// Update votes for all players with the GROUP menu
			GS()->StrongUpdateVotesForAll(MENU_GROUP);
		}
		return true;
	}

	if(PPSTR(CMD, "GROUP_INVITE") == 0)
	{
		const int InvitedCID = VoteID;
		const int GroupID = VoteID2;

		// Check if the player being invited exists
		CPlayer* pInvitedPlayer = GS()->GetPlayer(InvitedCID, true);
		if(pInvitedPlayer)
		{
			// Check if the player being invited already belongs to another group
			if(pInvitedPlayer->Acc().HasGroup())
			{
				GS()->Chat(ClientID, "This player is already in another group.");
				return true;
			}

			// Create vote optional
			CVoteEventOptional* pOption = pInvitedPlayer->CreateVoteOptional(ClientID, GroupID, 15, "Join to {STR} group?", Server()->ClientName(ClientID));
			pOption->RegisterCallback(CallbackVoteOptionalGroupInvite);

			// Send a chat message to the player inviting them to join the group
			GS()->Chat(ClientID, "You've invited {STR} to join your group!", Server()->ClientName(InvitedCID));

			// Send chat messages to the player being invited informing them of the invitation and how to join the group
			GS()->Chat(InvitedCID, "You have been invited by the players {STR} to join the group.", Server()->ClientName(ClientID));
		}
		return true;
	}

	// Check if the command is for changing the owner of a group
	if(PPSTR(CMD, "GROUP_CHANGE_OWNER") == 0)
	{
		// Set the AccountID to the value of VoteID
		const int AccountID = VoteID;

		// Get the group data of the player's account
		GroupData* pGroup = pPlayer->Acc().GetGroup();

		// If the player is in a group
		if(pGroup)
		{
			// Change the owner of the group to the specified AccountID
			pGroup->ChangeOwner(AccountID);

			// Update the votes for all players
			GS()->StrongUpdateVotesForAll(MENU_GROUP);
		}

		return true;
	}

	// Check if the command is "GROUP_KICK"
	if(PPSTR(CMD, "GROUP_KICK") == 0)
	{
		// Get the AccountID from VoteID
		const int AccountID = VoteID;

		// Get the player's group
		GroupData* pGroup = pPlayer->Acc().GetGroup();
		if(pGroup) // If the player has a group
		{
			// Remove the account from the group
			pGroup->Remove(AccountID);

			// Update votes for all players in the menu GROUP
			GS()->StrongUpdateVotesForAll(MENU_GROUP);
		}

		return true; // Return true to indicate success
	}

	// Check if the command is "GROUP_CHANGE_COLOR"
	if(PPSTR(CMD, "GROUP_CHANGE_COLOR") == 0)
	{
		if(Get <= 1 || Get > 63)
		{
			GS()->Chat(ClientID, "Write a number between 2 and 63 in the reason!");
			return true;
		}

		// Get the player's group data
		GroupData* pGroup = pPlayer->Acc().GetGroup();

		// If the player is in a group
		if(pGroup)
		{
			// Change the group's color
			pGroup->ChangeColor(Get);

			// Update the votes for all players in the group menu
			GS()->StrongUpdateVotesForAll(MENU_GROUP);
		}

		return true;
	}

	// Check if the command is "GROUP_DISBAND"
	if(PPSTR(CMD, "GROUP_DISBAND") == 0)
	{
		// Get the group data of the player's account
		GroupData* pGroup = pPlayer->Acc().GetGroup();

		// Check if the player has a group and if they are the owner of the group
		if(pGroup && pGroup->OwnerUID() == pPlayer->Acc().GetID())
		{
			// Disband the group
			pGroup->Disband();

			// Update votes for all players for the group menu
			GS()->StrongUpdateVotesForAll(MENU_GROUP);
		}

		return true;
	}

	return false;
}
