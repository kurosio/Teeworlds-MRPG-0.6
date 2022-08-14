/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AetherCore.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Guilds/GuildCore.h>
#include <game/server/mmocore/Components/Houses/HouseCore.h>

void CAetherCore::OnInit()
{
	const auto InitAethers = Sqlpool.Prepare<DB::SELECT>("*", "tw_aethers");
	InitAethers->AtExecute([this](IServer*, ResultPtr pRes)
	{
		while (pRes->next())
		{
			const int ID = pRes->getInt("ID");
			str_copy(CAetherData::ms_aTeleport[ID].m_aName, pRes->getString("Name").c_str(), sizeof(CAetherData::ms_aTeleport[ID].m_aName));
			CAetherData::ms_aTeleport[ID].m_TeleX = pRes->getInt("TeleX");
			CAetherData::ms_aTeleport[ID].m_TeleY = pRes->getInt("TeleY");
			CAetherData::ms_aTeleport[ID].m_WorldID = pRes->getInt("WorldID");
		}
		Job()->ShowLoadingProgress("Aethers", CAetherData::ms_aTeleport.size());
	});
}

void CAetherCore::OnInitAccount(CPlayer *pPlayer)
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_accounts_aethers", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	while(pRes->next())
	{
		const int TeleportID = pRes->getInt("AetherID");
		pPlayer->Acc().m_aAetherLocation[TeleportID] = true;
	}
}

bool CAetherCore::OnHandleVoteCommands(CPlayer *pPlayer, const char *CMD, const int VoteID, const int VoteID2, int Get, const char *GetText)
{
	const int ClientID = pPlayer->GetCID();

	// teleport
	if(PPSTR(CMD, "TELEPORT") == 0)
	{
		const int TeleportID = VoteID;
		const int Price = VoteID2;
		if(Price > 0 && !pPlayer->SpendCurrency(Price))
			return true;

		const vec2 Position = vec2(CAetherData::ms_aTeleport[TeleportID].m_TeleX, CAetherData::ms_aTeleport[TeleportID].m_TeleY);
		if(!GS()->IsPlayerEqualWorldID(ClientID, CAetherData::ms_aTeleport[TeleportID].m_WorldID))
		{
			pPlayer->GetTempData().m_TempTeleportPos = Position;
			pPlayer->ChangeWorld(CAetherData::ms_aTeleport[TeleportID].m_WorldID);
			return true;
		}

		pPlayer->GetCharacter()->ChangePosition(Position);
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	return false;
}

bool CAetherCore::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	if (pChr->GetHelper()->TileEnter(IndexCollision, TILE_AETHER_TELEPORT))
	{
		GS()->Chat(ClientID, "You can see menu in the votes!");
		pChr->m_Core.m_NoHookable = pChr->m_SkipDamage = true;
		UnlockLocation(pChr->GetPlayer(), pChr->m_Core.m_Pos);
		GS()->StrongUpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	else if (pChr->GetHelper()->TileExit(IndexCollision, TILE_AETHER_TELEPORT))
	{
		GS()->Chat(ClientID, "You left the active zone, menu is restored!");
		pChr->m_Core.m_NoHookable = pChr->m_SkipDamage = false;
		GS()->StrongUpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	return false;
}

bool CAetherCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	if (ReplaceMenu)
	{
		CCharacter* pChr = pPlayer->GetCharacter();
		if (!pChr || !pChr->IsAlive())
			return false;

		if (pChr->GetHelper()->BoolIndex(TILE_AETHER_TELEPORT))
		{
			ShowTeleportList(pChr);
			return true;
		}
		return false;
	}

	return false;
}

void CAetherCore::UnlockLocation(CPlayer *pPlayer, vec2 Pos)
{
	const int ClientID = pPlayer->GetCID();
	for (const auto& tl : CAetherData::ms_aTeleport)
	{
		if (distance(vec2(tl.second.m_TeleX, tl.second.m_TeleY), Pos) > 100 || pPlayer->Acc().m_aAetherLocation.find(tl.first) != pPlayer->Acc().m_aAetherLocation.end())
			continue;

		Sqlpool.Execute<DB::INSERT>("tw_accounts_aethers", "(UserID, AetherID) VALUES ('%d', '%d')", pPlayer->Acc().m_UserID, tl.first);
		GS()->Chat(ClientID, "You unlock aether {STR}!", CAetherData::ms_aTeleport[tl.first].m_aName);
		GS()->ChatDiscord(DC_SERVER_INFO, Server()->ClientName(ClientID), "Adventure unlock aether {STR}", CAetherData::ms_aTeleport[tl.first].m_aName);

		pPlayer->Acc().m_aAetherLocation[tl.first] = true;
		return;
	}
}

void CAetherCore::ShowTeleportList(CCharacter* pChar) const
{
	CPlayer* pPlayer = pChar->GetPlayer();
	const int ClientID = pPlayer->GetCID();
	GS()->ShowVotesItemValueInformation(pPlayer);
	GS()->AV(ClientID, "null");

	GS()->AVH(ClientID, TAB_AETHER, GOLDEN_COLOR, "Available aethers");
	if (Job()->Member()->GetGuildHouseID(pPlayer->Acc().m_GuildID) >= 1)
		GS()->AVM(ClientID, "MSPAWN", NOPE, TAB_AETHER, "Move to Guild House - free");
	if (Job()->House()->PlayerHouseID(pPlayer) >= 1)
		GS()->AVM(ClientID, "HSPAWN", NOPE, TAB_AETHER, "Move to Your House - free");

	for (const auto& tl : CAetherData::ms_aTeleport)
	{
		if (pPlayer->Acc().m_aAetherLocation.find(tl.first) == pPlayer->Acc().m_aAetherLocation.end())
			continue;

		const bool LocalTeleport = (GS()->IsPlayerEqualWorldID(ClientID, tl.second.m_WorldID) &&
			distance(pPlayer->GetCharacter()->m_Core.m_Pos, vec2(tl.second.m_TeleX, tl.second.m_TeleY)) < 120);
		if (LocalTeleport)
		{
			GS()->AVM(ClientID, "null", tl.first, TAB_AETHER, "[Local {STR}] : {STR}", tl.second.m_aName, Server()->GetWorldName(tl.second.m_WorldID));
			continue;
		}

		const int Price = g_Config.m_SvPriceTeleport * (tl.second.m_WorldID + 1);
		GS()->AVD(ClientID, "TELEPORT", tl.first, Price, TAB_AETHER, "[{STR}] : {STR} - {INT}gold",
			tl.second.m_aName, Server()->GetWorldName(tl.second.m_WorldID), Price);
	}
	GS()->AV(ClientID, "null");
}
