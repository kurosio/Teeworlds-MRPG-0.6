/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AetherManager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Guilds/GuildManager.h>

void CAetherManager::OnInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_AETHERS);
	while(pRes->next())
	{
		std::string Name = pRes->getString("Name").c_str();
		vec2 Pos = vec2(pRes->getInt("TeleX"), pRes->getInt("TeleY"));
		int WorldID = pRes->getInt("WorldID");

		AetherIdentifier ID = pRes->getInt("ID");
		CAetherData::CreateElement(ID)->Init(Name.c_str(), Pos, WorldID);
	}

	if(ms_vpAetherGroupCollector.empty())
	{
		for(const auto& pAether : CAetherData::Data())
		{
			int WorldID = pAether->GetWorldID();
			ms_vpAetherGroupCollector[WorldID].push_back(pAether);
		}
	}
}

void CAetherManager::OnInitAccount(CPlayer* pPlayer)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_ACCOUNTS_AETHERS, "WHERE UserID = '%d'", pPlayer->Account()->GetID());
	while(pRes->next())
	{
		AetherIdentifier ID = pRes->getInt("AetherID");
		pPlayer->Account()->AddAether(ID);
	}

}

bool CAetherManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	if(PPSTR(CMD, "AETHER_TELEPORT") == 0)
	{
		AetherIdentifier AetherID = VoteID;
		const int& Price = VoteID2;

		// Check if Price is greater than 0 and if the player has enough currency to spend
		if(Price > 0 && !pPlayer->Account()->SpendCurrency(Price))
			return true;

		// Check if pAether is null
		CAetherData* pAether = GetAetherByID(AetherID);
		if(!pAether)
			return true;

		// Check if the player is in a different world than pAether
		vec2 Position = pAether->GetPosition();
		if(!GS()->IsPlayerEqualWorld(ClientID, pAether->GetWorldID()))
		{
			// Change the player's world to pAether's world
			pPlayer->GetTempData().SetTeleportPosition(Position);
			pPlayer->ChangeWorld(pAether->GetWorldID());
			return true;
		}

		// Change the player's position to pAether's position
		pPlayer->GetCharacter()->ChangePosition(Position);
		GS()->UpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
		return true;
	}

	return false;
}

bool CAetherManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	// Check if the character enters the Aether teleport tile
	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_AETHER_TELEPORT))
	{
		_DEF_TILE_ENTER_ZONE_SEND_MSG_INFO(pPlayer);
		UnlockLocationByPos(pChr->GetPlayer(), pChr->m_Core.m_Pos);
		GS()->StrongUpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
		return true;
	}
	// Check if the character exits the Aether teleport tile
	else if(pChr->GetHelper()->TileExit(IndexCollision, TILE_AETHER_TELEPORT))
	{
		_DEF_TILE_EXIT_ZONE_SEND_MSG_INFO(pPlayer);
		GS()->StrongUpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
		return true;
	}

	return false;
}

bool CAetherManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	if(ReplaceMenu)
	{
		CCharacter* pChr = pPlayer->GetCharacter();
		if(!pChr || !pChr->IsAlive())
			return false;

		if(pChr->GetHelper()->BoolIndex(TILE_AETHER_TELEPORT))
		{
			ShowMenu(pChr);
			return true;
		}
		return false;
	}

	return false;
}

void CAetherManager::ShowMenu(CCharacter* pChar) const
{
	CPlayer* pPlayer = pChar->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	CVoteWrapper VAetherInfo(ClientID, BORDER_STRICT_BOLD);
	VAetherInfo.AddItemValue();
	VAetherInfo.AddIfOption(pPlayer->Account()->HasGuild() && pPlayer->Account()->GetGuild()->HasHouse(), "GUILD_HOUSE_SPAWN", "Move to Guild house - free");
	VAetherInfo.AddIfOption(pPlayer->Account()->HasHouse(), "HOUSE_SPAWN", "Move to your House - free");
	VAetherInfo.AddEmptyline();

	for(auto& [WorldID, vAethers] : ms_vpAetherGroupCollector)
	{
		CGS* pGS = (CGS*)Server()->GameServer(WorldID);
		if(WorldID == TUTORIAL_WORLD_ID || pGS->IsDungeon() || vAethers.empty())
			continue;

		/*int UnlockedPlayerZoneAethers = 0;
		CVoteWrapper VAethers(ClientID, HIDE_DEFAULT_OPEN | BORDER_SIMPLE, "{STR} : Shared aethers", Server()->GetWorldName(WorldID));
		for(const auto& Aether : vAethers)
		{
			if(pPlayer->Account()->m_aAetherLocation.find(Aether.GetID()) != pPlayer->Account()->m_aAetherLocation.end())
			{
				if(GS()->IsPlayerEqualWorld(ClientID, Aether.GetWorldID()) && distance(pPlayer->GetCharacter()->m_Core.m_Pos, Aether.GetPosition()) < 120)
					continue;

				const int Price = g_Config.m_SvPriceTeleport * (Aether.GetWorldID() + 1);
				VAethers.AddOption("AETHER_TELEPORT", Aether.GetID(), Price, "{STR} - {VAL}gold", Aether.GetName(), Price);
				UnlockedPlayerZoneAethers++;
			}
			else
			{
				VAethers.AddOption("AETHER_TELEPORT", -1, "{STR} - (locked)", Aether.GetName());
			}
		}
		VAethers.AddIf(VAethers.IsEmpty(), "No Aethers available.");
		VAethers.Add("Unlocked {INT} of {INT} zone aethers.", UnlockedPlayerZoneAethers, vAethers.size());
		CVoteWrapper::AddEmptyline(ClientID);*/
	}
}

void CAetherManager::UnlockLocationByPos(CPlayer* pPlayer, vec2 Pos) const
{
	const int ClientID = pPlayer->GetCID();

	CAetherData* pAether = GetAetherByPos(Pos);
	if(pAether && !pPlayer->Account()->IsUnlockedAether(pAether->GetID()))
	{
		Database->Execute<DB::INSERT>(TW_ACCOUNTS_AETHERS, "(UserID, AetherID) VALUES ('%d', '%d')", pPlayer->Account()->GetID(), pAether->GetID());

		pPlayer->Account()->AddAether(pAether->GetID());
		GS()->Chat(ClientID, "You now have Aethernet access to the {STR}.", pAether->GetName());
		GS()->ChatDiscord(DC_SERVER_INFO, Server()->ClientName(ClientID), "Now have Aethernet access to the {STR}.", pAether->GetName());
	}
}

CAetherData* CAetherManager::GetAetherByID(int AetherID) const
{
	const auto& iter = std::find_if(CAetherData::Data().begin(), CAetherData::Data().end(), [AetherID](const CAetherData* pAether)
	{
		return pAether->GetID() == AetherID;
	});
	return iter != CAetherData::Data().end() ? *iter : nullptr;
}

CAetherData* CAetherManager::GetAetherByPos(vec2 Pos) const
{
	const auto& iter = std::find_if(CAetherData::Data().begin(), CAetherData::Data().end(), [Pos](const CAetherData* pAether)
	{
		return distance(pAether->GetPosition(), Pos) < 100;
	});
	return iter != CAetherData::Data().end() ? *iter : nullptr;
}
