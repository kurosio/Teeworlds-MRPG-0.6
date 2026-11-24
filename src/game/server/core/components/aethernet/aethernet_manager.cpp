/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "aethernet_manager.h"

#include "aethernet_db.h"
#include <game/server/core/tools/db_async_context.h>

#include <game/server/gamecontext.h>
#include <generated/server_data.h>

#include <game/server/core/components/guilds/guild_manager.h>

void CAethernetManager::OnPreInit()
{
	// Load aether data from the database
	auto Aethers = AethernetDb::ListAethers();
	while(Aethers.Next())
	{
		std::string Name = Aethers.GetString("Name").c_str();
		vec2 Pos = vec2(Aethers.GetInt("TeleX"), Aethers.GetInt("TeleY"));
		int WorldID = Aethers.GetInt("WorldID");

		AetherIdentifier ID = Aethers.GetInt("ID");
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

void CAethernetManager::OnRegisterAccountLoadStages(CPlayer* pPlayer)
{
	auto* pAccount = pPlayer->Account();
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pAccount->GetID();
	pAccount->AddLoadStage("aethers", [ClientID, AccountID](CAccountData::CLoadDone Done)
	{
		auto pContext = DbAsync::MakeContext(ClientID);
		AethernetDb::ListUnlocked(AccountID).Then([pContext, Done = std::move(Done)](Db::CResultSet&& Unlocked) mutable
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer || !Unlocked.Ok())
			{
				Done(false);
				return;
			}
			while(Unlocked.Next())
				pPlayer->Account()->AddAether(Unlocked.GetInt("AetherID"));
			Done(true);
		});
	});
}

bool CAethernetManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	// Check if the player is trying to teleport to an aether
	if(PPSTR(pCmd, "AETHER_TELEPORT") == 0)
	{
        // Assign the given Extra1 to AetherID and AetherData object
        AetherIdentifier AetherID = GetIfExists<int>(Extras, 0, NOPE);
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
        const int& Fee = GetIfExists<int>(Extras, 1, 0);

		if(Fee <= 0 || pPlayer->Account()->SpendCurrency(Fee))
		{
			vec2 Position = pAether->GetPosition();
			if(!GS()->IsPlayerInWorld(ClientID, pAether->GetWorldID()))
			{
				GS()->CreateSound(Position, SOUND_SFX_TELEPORT);
				pPlayer->ChangeWorld(pAether->GetWorldID(), Position);
			}
			else
			{
				GS()->CreateSound(Position, SOUND_SFX_TELEPORT);
				pPlayer->GetCharacter()->ChangePosition(Position);
				pPlayer->m_VotesData.UpdateCurrentVotes();
			}

			// Inform the player that they have been teleported to the Aether
			GS()->Chat(ClientID, "You have been teleported to the '{} {}'.", Server()->GetWorldName(pAether->GetWorldID()), pAether->GetName());
		}
		return true;
	}

	return false;
}

void CAethernetManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();

	HANDLE_TILE_VOTE_MENU(pPlayer, pChr, TILE_AETHER_TELEPORT, MENU_AETHERNET_LIST,
		{ UnlockLocationByPos(pChr->GetPlayer(), pChr->m_Core.m_Pos); },
		{}
	);
}

bool CAethernetManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
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
	VoteWrapper VAether(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Aethernet information");
	VAether.Add("Total unlocked aethers: {} of {}.", pPlayer->Account()->GetAethers().size(), CAetherData::Data().size());
	VAether.AddItemValue(itGold);
	VoteWrapper::AddEmptyline(ClientID);

	// Add aether menu for each world
	for(auto& [WorldID, vAethers] : s_vpAetherSortedList)
	{
		int UnlockedPlayerZoneAethers = 0;
		VoteWrapper VAetherElem(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "{} : Aethernet", Server()->GetWorldName(WorldID));
		{
			VAetherElem.BeginDepth();
			for(const auto& pAether : vAethers)
			{
				if(pPlayer->Account()->IsUnlockedAether(pAether->GetID()))
				{
					const int Fee = g_Config.m_SvTeleportFeePerDistance * (pAether->GetWorldID() + 1);
					VAetherElem.AddOption("AETHER_TELEPORT", MakeAnyList(pAether->GetID(), Fee), "{} (Fee {$} gold)", pAether->GetName(), Fee);
					UnlockedPlayerZoneAethers++;
				}
			}
			VAetherElem.EndDepth();
		}
		VAetherElem.Add("Unlocked {} of {} zone aethers.", UnlockedPlayerZoneAethers, vAethers.size());
		VoteWrapper::AddEmptyline(ClientID);
	}
}

void CAethernetManager::UnlockLocationByPos(CPlayer* pPlayer, vec2 Pos) const
{
	const int ClientID = pPlayer->GetCID();
	CAetherData* pAether = GetAetherByPos(Pos);

	// Unlock the Aether for the player
	if(pAether && !pPlayer->Account()->IsUnlockedAether(pAether->GetID()))
	{
		AethernetDb::Unlock(pPlayer->Account()->GetID(), pAether->GetID());

		pPlayer->Account()->AddAether(pAether->GetID());
		GS()->Chat(ClientID, "You now have Aethernet access to the '{}'.", pAether->GetName());
	}
}

CAetherData* CAethernetManager::GetAetherByID(int AetherID) const
{
	const auto& iter = std::ranges::find_if(CAetherData::Data(), [AetherID](const CAetherData* pAether)
	                                        { return pAether->GetID() == AetherID; });
	return iter != CAetherData::Data().end() ? *iter : nullptr;
}

CAetherData* CAethernetManager::GetAetherByPos(vec2 Pos) const
{
	const auto& iter = std::ranges::find_if(CAetherData::Data(), [Pos](const CAetherData* pAether)
	                                        { return distance(pAether->GetPosition(), Pos) < 320.f; });
	return iter != CAetherData::Data().end() ? *iter : nullptr;
}
