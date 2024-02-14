/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "aethernet_manager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Guilds/GuildManager.h>

void CAethernetManager::OnInit()
{
	// Load aether data from the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_AETHERS);
	while(pRes->next())
	{
		std::string Name = pRes->getString("Name").c_str();
		vec2 Pos = vec2(pRes->getInt("TeleX"), pRes->getInt("TeleY"));
		int WorldID = pRes->getInt("WorldID");

		AetherIdentifier ID = pRes->getInt("ID");
		CAetherData::CreateElement(ID)->Init(Name.c_str(), Pos, WorldID);
	}

	// Sorted list of aethers by world ID
	if(s_vpAetherSortedList.empty())
	{
		// Loop through all the world data and initilize the sorted list how empty
		for(int i = 0; i < Server()->GetWorldsSize(); i++)
		{
			if(Server()->IsWorldType(i, WorldType::Default))
				s_vpAetherSortedList[i] = {};
		}

		// Loop through all the aether data and collect aethers by world ID
		for(const auto& pAether : CAetherData::Data())
		{
			if(s_vpAetherSortedList.find(pAether->GetWorldID()) != s_vpAetherSortedList.end())
			{
				int WorldID = pAether->GetWorldID();
				s_vpAetherSortedList[WorldID].push_back(pAether);
			}
		}
	}
}

void CAethernetManager::OnInitAccount(CPlayer* pPlayer)
{
	// Initialize the player's aether data
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_ACCOUNTS_AETHERS, "WHERE UserID = '%d'", pPlayer->Account()->GetID());
	while(pRes->next())
	{
		AetherIdentifier ID = pRes->getInt("AetherID");
		pPlayer->Account()->AddAether(ID);
	}
}

bool CAethernetManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	// Check if the player is trying to teleport to an aether
	if(PPSTR(CMD, "AETHER_TELEPORT") == 0)
	{
		// Assign the given VoteID to AetherID and AetherData object
		AetherIdentifier AetherID = VoteID;
		CAetherData* pAether = GetAetherByID(AetherID);

		// Check if the AetherData object does not exist
		if(!pAether)
		{
			GS()->Chat(ClientID, "Aether not found.");
			return true;
		}

		// Check if the player does not have access to the Aether
		if(!pPlayer->Account()->IsUnlockedAether(AetherID))
		{
			GS()->Chat(ClientID, "You don't have Aether access to this location.");
			return true;
		}

		// Check if the player has enough currency to spend
		const int& Fee = VoteID2;
		if(Fee <= 0 || pPlayer->Account()->SpendCurrency(Fee))
		{
			// Check if the player is in a different world than the Aether
			vec2 Position = pAether->GetPosition();
			if(!GS()->IsPlayerEqualWorld(ClientID, pAether->GetWorldID()))
			{
				// Set the teleport position in the player's temporary data
				pPlayer->GetTempData().SetTeleportPosition(Position);
				pPlayer->ChangeWorld(pAether->GetWorldID());
			}
			else
			{
				// Change the player's position to the Aether's position
				pPlayer->GetCharacter()->ChangePosition(Position);
				pPlayer->m_VotesData.UpdateCurrentVotes();
			}

			// Inform the player that they have been teleported to the Aether
			GS()->Chat(ClientID, "You have been teleported to the {STR} {STR}.", Server()->GetWorldName(pAether->GetWorldID()), pAether->GetName());
		}
		return true;
	}

	return false;
}

bool CAethernetManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	// Check if the character enters the Aether teleport tile
	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_AETHER_TELEPORT))
	{
		_DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_AETHERNET_LIST);
		UnlockLocationByPos(pChr->GetPlayer(), pChr->m_Core.m_Pos);
		return true;
	}
	// Check if the character exits the Aether teleport tile
	else if(pChr->GetHelper()->TileExit(IndexCollision, TILE_AETHER_TELEPORT))
	{
		_DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}

	return false;
}

bool CAethernetManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	if(Menulist == MENU_AETHERNET_LIST)
	{
		ShowMenu(pPlayer->GetCharacter());
		return true;
	}

	return false;
}

void CAethernetManager::ShowMenu(CCharacter* pChar) const
{
	CPlayer* pPlayer = pChar->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	// Default aether menu
	CVoteWrapper VAether(ClientID, VWFLAG_STYLE_STRICT_BOLD, "Aethernet information");
	VAether.Add("Total unlocked aethers: {VAL} of {VAL}.", pPlayer->Account()->GetAethers().size(), CAetherData::Data().size());
	VAether.AddItemValue(itGold);
	VAether.AddLine();
	CVoteWrapper::AddEmptyline(ClientID);

	// Add aether menu for each world
	for(auto& [WorldID, vAethers] : s_vpAetherSortedList)
	{
		int UnlockedPlayerZoneAethers = 0;
		CVoteWrapper VAetherElem(ClientID, VWFLAG_SEPARATE_OPEN | VWFLAG_STYLE_SIMPLE, "{STR} : Aethernet", Server()->GetWorldName(WorldID));

		VAetherElem.BeginDepthList();
		for(const auto& pAether : vAethers)
		{
			if(pPlayer->Account()->IsUnlockedAether(pAether->GetID()))
			{
				const int Fee = g_Config.m_SvAetherFee * (pAether->GetWorldID() + 1);
				VAetherElem.AddOption("AETHER_TELEPORT", pAether->GetID(), Fee, "{STR} (Fee {VAL} gold's)", pAether->GetName(), Fee);
				UnlockedPlayerZoneAethers++;
			}
		}
		VAetherElem.EndDepthList();

		VAetherElem.Add("Unlocked {INT} of {INT} zone aethers.", UnlockedPlayerZoneAethers, vAethers.size());
		CVoteWrapper::AddEmptyline(ClientID);
	}
}

void CAethernetManager::UnlockLocationByPos(CPlayer* pPlayer, vec2 Pos) const
{
	const int ClientID = pPlayer->GetCID();
	CAetherData* pAether = GetAetherByPos(Pos);

	// Unlock the Aether for the player
	if(pAether && !pPlayer->Account()->IsUnlockedAether(pAether->GetID()))
	{
		Database->Execute<DB::INSERT>(TW_ACCOUNTS_AETHERS, "(UserID, AetherID) VALUES ('%d', '%d')", pPlayer->Account()->GetID(), pAether->GetID());

		pPlayer->Account()->AddAether(pAether->GetID());
		GS()->Chat(ClientID, "You now have Aethernet access to the {STR}.", pAether->GetName());
		GS()->ChatDiscord(DC_SERVER_INFO, Server()->ClientName(ClientID), "Now have Aethernet access to the {STR}.", pAether->GetName());
	}
}

CAetherData* CAethernetManager::GetAetherByID(int AetherID) const
{
	const auto& iter = std::find_if(CAetherData::Data().begin(), CAetherData::Data().end(), [AetherID](const CAetherData* pAether)
	{
		return pAether->GetID() == AetherID;
	});
	return iter != CAetherData::Data().end() ? *iter : nullptr;
}

CAetherData* CAethernetManager::GetAetherByPos(vec2 Pos) const
{
	const auto& iter = std::find_if(CAetherData::Data().begin(), CAetherData::Data().end(), [Pos](const CAetherData* pAether)
	{
		return distance(pAether->GetPosition(), Pos) < 320.f;
	});
	return iter != CAetherData::Data().end() ? *iter : nullptr;
}
