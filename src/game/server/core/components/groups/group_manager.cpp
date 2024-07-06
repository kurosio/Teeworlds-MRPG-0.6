/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "group_manager.h"
#include "group_data.h"

#include <game/server/core/tools/vote_optional.h>
#include <game/server/gamecontext.h>

void CGroupManager::OnInit()
{
	// Create a pointer to store the result of the database query
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_groups");
	while(pRes->next())
	{
		// Get the values of the columns for the current row
		GroupIdentifier ID = pRes->getInt("ID");
		int OwnerUID = pRes->getInt("OwnerUID");
		std::string StrAccountIDs = pRes->getString("AccountIDs").c_str();

		// Initialize a GroupData object with the retrieved values
		GroupData::CreateElement(ID).Init(OwnerUID, std::move(StrAccountIDs));
	}
}

void CGroupManager::OnPlayerLogin(CPlayer* pPlayer)
{
	// reinitialize group
	pPlayer->Account()->ReinitializeGroup();

	// check is first online player and set random free color
	if(pPlayer->Account()->HasGroup())
	{
		const auto pGroup = pPlayer->Account()->GetGroup();
		if(pGroup->GetOnlineCount() == 1)
			pGroup->UpdateRandomColor();
	}
}

GroupData* CGroupManager::CreateGroup(CPlayer* pPlayer) const
{
	// check valid player
	if(!pPlayer || !pPlayer->IsAuthed())
		return nullptr;

	// check if the player is already in a group
	if(pPlayer->Account()->GetGroup())
	{
		GS()->Chat(pPlayer->GetCID(), "You're already in a group!");
		return nullptr;
	}

	// get the highest group ID from the database
	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", TW_GROUPS_TABLE, "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1; // Increment the highest group ID by 1, or set to 1 if no previous group exists

	// initialize variables
	int OwnerUID = pPlayer->Account()->GetID();
	std::string StrAccountIDs = std::to_string(OwnerUID);

	// insert to database
	Database->Execute<DB::INSERT>(TW_GROUPS_TABLE, "(ID, OwnerUID, AccountIDs) VALUES ('%d', '%d', '%s')", InitID, OwnerUID, StrAccountIDs.c_str());

	// initialize the group
	auto& newGroup = GroupData::CreateElement(InitID);
	newGroup.Init(OwnerUID, StrAccountIDs);
	newGroup.UpdateRandomColor();
	pPlayer->Account()->ReinitializeGroup();

	// send message
	GS()->Chat(pPlayer->GetCID(), "The formation of the group took place!");
	return &newGroup;
}

GroupData* CGroupManager::GetGroupByID(GroupIdentifier ID) const
{
	// Check if the group ID exists in the group data
	if(GroupData::Data().find(ID) == GroupData::Data().end())
		return nullptr;

	// Return a pointer to the group data
	return &GroupData::Data()[ID];
}

// Function to display the group menu for a player
void CGroupManager::ShowGroupMenu(CPlayer* pPlayer) const
{
	// initialize variables
	int ClientID = pPlayer->GetCID();

	// information
	VoteWrapper VGroupCmd(ClientID,VWF_STYLE_STRICT_BOLD|VWF_SEPARATE|VWF_ALIGN_TITLE, "Group commands");
	VGroupCmd.Add("- /group Get all sub commands");
	VoteWrapper::AddEmptyline(ClientID);

	// group management
	GroupData* pGroup = pPlayer->Account()->GetGroup();
	VoteWrapper VGroup(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u273D Group Management");

	// check group valid
	if(!pGroup)
	{
		VGroup.AddOption("GROUP_CREATE", "Create a group");
		VoteWrapper::AddEmptyline(ClientID);
		return;
	}

	// group functions
	const bool IsOwner = pGroup->GetOwnerUID() == pPlayer->Account()->GetID();
	if(IsOwner)
	{
		VGroup.AddOption("GROUP_RANDOM_COLOR", "Random new group color");
		VGroup.AddOption("GROUP_DISBAND", "Disband group");
	}
	VGroup.AddOption("GROUP_KICK", pPlayer->Account()->GetID(), "Leave the group");
	VoteWrapper::AddEmptyline(ClientID);

	// group invites list
	VoteWrapper VGroupInvites(ClientID, VWF_STYLE_SIMPLE | VWF_SEPARATE_OPEN, "Players for invitation");
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pSearchPlayer = GS()->GetPlayer(i, true);
		if(pSearchPlayer && !pSearchPlayer->Account()->GetGroup())
			VGroupInvites.AddOption("GROUP_INVITE", i, "Invite {}", Server()->ClientName(i));
	}
	VoteWrapper::AddEmptyline(ClientID);

	// group member list
	VoteWrapper(ClientID).Add("\u2735 Members {} of {}", (int)pGroup->GetAccounts().size(), (int)MAX_GROUP_MEMBERS);
	for(auto& AID : pGroup->GetAccounts())
	{
		std::string PlayerName = Server()->GetAccountNickname(AID);
		bool HasInteraction = IsOwner && AID != pPlayer->Account()->GetID();
		VoteWrapper VMember(ClientID, VWF_UNIQUE, "{}{}", (AID == pGroup->GetOwnerUID() ? "*" : "\0"), PlayerName.c_str());
		if(HasInteraction)
		{
			VMember.AddOption("GROUP_KICK", AID, "Kick {}", PlayerName.c_str());
			VMember.AddOption("GROUP_CHANGE_OWNER", AID, "Transfer ownership {}", PlayerName.c_str());
		}
	}
	VoteWrapper::AddEmptyline(ClientID);
}

bool CGroupManager::OnPlayerMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MENU_GROUP)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		ShowGroupMenu(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

static void CallbackVoteOptionalGroupInvite(CPlayer* pPlayer, int Extra1, int Extra2, bool Accepted)
{
	// initialize variables
	CGS* pGS = pPlayer->GS();
	int ClientID = pPlayer->GetCID();
	int& InvitedCID = Extra1;
	int& GroupID = Extra2;

	// check valid group
	GroupData* pGroup = pGS->Core()->GroupManager()->GetGroupByID(GroupID);
	if(!pGroup)
		return;

	// check selected option
	if(Accepted)
	{
		pGroup->Add(pPlayer->Account()->GetID());
		pGS->Chat(ClientID, "You've accepted the invitation!");
		pGS->Chat(InvitedCID, "{} accepted your invitation!", pGS->Server()->ClientName(ClientID));
	}
	else
	{
		pGS->Chat(ClientID, "You declined the invitation!");
		pGS->Chat(InvitedCID, "{} declined your invitation!", pGS->Server()->ClientName(ClientID));
	}
}

bool CGroupManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	// create new group
	if(PPSTR(pCmd, "GROUP_CREATE") == 0)
	{
		// try to create group
		if(CreateGroup(pPlayer))
			GS()->UpdateVotesIfForAll(MENU_GROUP);
		return true;
	}

	// invite to enter group
	if(PPSTR(pCmd, "GROUP_INVITE") == 0)
	{
		// initialize variables
		const int InvitedCID = Extra1;
		const auto pGroup = pPlayer->Account()->GetGroup();

		// check valid group
		if(!pGroup)
		{
			GS()->Chat(ClientID, "You're not in a group!");
			return true;
		}

		// check is owner
		if(pGroup->GetOwnerUID() != pPlayer->Account()->GetID())
		{
			GS()->Chat(ClientID, "You're not the owner of the group!");
			return true;
		}

		// check is full
		if(pGroup->IsFull())
		{
			GS()->Chat(ClientID, "The group is full!");
			return true;
		}

		// try invite player
		const GroupIdentifier GroupID = pGroup->GetID();
		if(CPlayer* pInvitedPlayer = GS()->GetPlayer(InvitedCID, true))
		{
			// is invited player has group
			if(pInvitedPlayer->Account()->HasGroup())
			{
				GS()->Chat(ClientID, "This player is already in another group.");
				return true;
			}

			// create invite vote optional
			const auto pOption = CVoteOptional::Create(InvitedCID, ClientID, GroupID, 15,
			                                           "Join to {} group?", Server()->ClientName(ClientID));
			pOption->RegisterCallback(CallbackVoteOptionalGroupInvite);

			// send messages
			GS()->Chat(ClientID, "You've invited {} to join your group!", Server()->ClientName(InvitedCID));
			GS()->Chat(InvitedCID, "You have been invited by the {} to join the group.", Server()->ClientName(ClientID));
		}

		return true;
	}

	// change group owner
	if(PPSTR(pCmd, "GROUP_CHANGE_OWNER") == 0)
	{
		// initialize variables
		const int AccountID = Extra1;

		// check player group valid
		if(const auto pGroup = pPlayer->Account()->GetGroup())
		{
			// check is owner player
			if(pGroup->GetOwnerUID() != pPlayer->Account()->GetID())
			{
				GS()->Chat(ClientID, "You're not the owner of the group!");
				return true;
			}

			// change owner
			pGroup->ChangeOwner(AccountID);
			GS()->UpdateVotesIfForAll(MENU_GROUP);
		}

		return true;
	}

	// kick from group
	if(PPSTR(pCmd, "GROUP_KICK") == 0)
	{
		// initialize variables
		const int AccountID = Extra1;

		// check player group valid
		if(const auto pGroup = pPlayer->Account()->GetGroup())
		{
			// check is owner player
			if(pGroup->GetOwnerUID() != pPlayer->Account()->GetID())
			{
				GS()->Chat(ClientID, "You're not the owner of the group!");
				return true;
			}

			// remove
			pGroup->Remove(AccountID);
			GS()->UpdateVotesIfForAll(MENU_GROUP);
		}

		return true;
	}

	// get random free color
	if(PPSTR(pCmd, "GROUP_RANDOM_COLOR") == 0)
	{
		// check player group valid
		if(const auto pGroup = pPlayer->Account()->GetGroup())
		{
			// check is owner player
			if(pGroup->GetOwnerUID() != pPlayer->Account()->GetID())
			{
				GS()->Chat(ClientID, "You're not the owner of the group!");
				return true;
			}

			// update random color
			pGroup->UpdateRandomColor();
			GS()->UpdateVotesIfForAll(MENU_GROUP);
			GS()->Chat(ClientID, "The group color has been updated!");
		}

		return true;
	}

	// disband group
	if(PPSTR(pCmd, "GROUP_DISBAND") == 0)
	{
		// check player group valid
		if(const auto pGroup = pPlayer->Account()->GetGroup())
		{
			// check is owner player
			if(pGroup->GetOwnerUID() != pPlayer->Account()->GetID())
			{
				GS()->Chat(ClientID, "You're not the owner of the group!");
				return true;
			}

			// disband group
			pGroup->Disband();
			GS()->UpdateVotesIfForAll(MENU_GROUP);
		}

		return true;
	}

	return false;
}
