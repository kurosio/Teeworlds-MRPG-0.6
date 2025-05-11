#include "rcon_processor.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>

#include "components/accounts/account_manager.h"
#include "components/guilds/guild_manager.h"
#include "components/Bots/BotManager.h"
#include "components/mails/mail_wrapper.h"

void RconProcessor::Init(IConsole* pConsole, IServer* pServer)
{
	// rcon commands
	pConsole->Register("set_world_time", "i[hour]", CFGFLAG_SERVER, ConSetWorldTime, pServer, "Set worlds time.");
	pConsole->Register("item_list", "", CFGFLAG_SERVER, ConItemList, pServer, "items list");
	pConsole->Register("give_item", "i[cid]i[itemid]i[count]i[enchant]i[mail]", CFGFLAG_SERVER, ConGiveItem, pServer, "Give item <clientid> <itemid> <count> <enchant> <mail 1=yes 0=no>");
	pConsole->Register("remove_item", "i[cid]i[itemid]i[count]", CFGFLAG_SERVER, ConRemItem, pServer, "Remove item <clientid> <itemid> <count>");
	pConsole->Register("disband_guild", "r[guildname]", CFGFLAG_SERVER, ConDisbandGuild, pServer, "Disband the guild with the name");
	pConsole->Register("say", "r[text]", CFGFLAG_SERVER, ConSay, pServer, "Say in chat");
	pConsole->Register("add_character", "i[cid]r[botname]", CFGFLAG_SERVER, ConAddCharacter, pServer, "(Warning) Add new bot on database or update if finding <clientid> <bot name>");
	pConsole->Register("sync_lines_for_translate", "", CFGFLAG_SERVER, ConSyncLinesForTranslate, pServer, "Perform sync lines in translated files. Order non updated translated to up");
	pConsole->Register("afk_list", "", CFGFLAG_SERVER, ConListAfk, pServer, "List all afk players");
	pConsole->Register("is_afk", "i[cid]", CFGFLAG_SERVER, ConCheckAfk, pServer, "Check if player is afk");
	pConsole->Register("ban_acc", "i[cid]s[time]r[reason]", CFGFLAG_SERVER, ConBanAcc, pServer, "Ban account, time format: d - days, h - hours, m - minutes, s - seconds, example: 3d15m");
	pConsole->Register("unban_acc", "i[banid]", CFGFLAG_SERVER, ConUnBanAcc, pServer, "UnBan account, pass ban id from bans_acc");
	pConsole->Register("bans_acc", "", CFGFLAG_SERVER, ConBansAcc, pServer, "Accounts bans");

	pConsole->Register("jail", "i[cid]i[sec]", CFGFLAG_SERVER, ConJail, pServer, "Imprison a player for a set number of seconds");
	pConsole->Register("unjail", "i[cid]", CFGFLAG_SERVER, ConUnjail, pServer, "Release the player from prison");

	// tools
	pConsole->Register("tele_by_mouse", "", CFGFLAG_SERVER, ConTeleportByMouse, pServer, "Teleport by mouse");
	pConsole->Register("tele_by_pos", "i[x]i[y]?i[world_id]", CFGFLAG_SERVER, ConTeleportByPos, pServer, "Teleport by pos");
	pConsole->Register("tele_by_client", "i[cid]", CFGFLAG_SERVER, ConTeleportByClient, pServer, "Teleport by client");
	pConsole->Register("position", "?i[cid]", CFGFLAG_SERVER, ConPosition, pServer, "Get position by client (default self position)");
	pConsole->Register("quest", "?s[state]?i[quest_id]", CFGFLAG_SERVER, ConQuest, pServer, "Accept or reset the quest with the specified ID");

	// chain's
	pConsole->Chain("sv_motd", ConchainSpecialMotdupdate, pServer);
}

static CGS* GetCommandResultGameServer(int ClientID, void* pUser)
{
	IServer* pServer = (IServer*)pUser;
	return (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));
}


void RconProcessor::ConJail(IConsole::IResult* pResult, void* pUser)
{
	const auto* pServer = (IServer*)pUser;
	const auto PrisonedCID = pResult->GetInteger(0);
	const auto FromCID = pResult->GetClientID();
	const auto Seconds = pResult->GetInteger(1);

	auto* pGS = GetCommandResultGameServer(PrisonedCID, pUser);
	const auto* pPlayer = pGS->GetPlayer(PrisonedCID);
	if(!pPlayer || !pPlayer->IsAuthed())
	{
		pGS->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "jail", "I can't find the player, or he's not authed");
		return;
	}

	// prison
	pPlayer->Account()->GetPrisonManager().Imprison(Seconds);
	pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "jail", "%s prisoned up player %s for %d seconds.",
		pServer->ClientName(FromCID), pServer->ClientName(PrisonedCID), Seconds);
}


void RconProcessor::ConUnjail(IConsole::IResult* pResult, void* pUser)
{
	const auto* pServer = (IServer*)pUser;
	const auto FromCID = pResult->GetClientID();
	const auto UnprisonedCID = pResult->GetInteger(0);

	auto* pGS = GetCommandResultGameServer(UnprisonedCID, pUser);
	const auto* pPlayer = pGS->GetPlayer(UnprisonedCID);
	if(!pPlayer || !pPlayer->IsAuthed())
	{
		pGS->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "jail", "I can't find the player, or he's not authed");
		return;
	}

	// unprisoned
	pPlayer->Account()->GetPrisonManager().Release();
	pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "jail", "%s unprisoned up player %s.",
		pServer->ClientName(FromCID), pServer->ClientName(UnprisonedCID));
}


void RconProcessor::ConTeleportByMouse(IConsole::IResult* pResult, void* pUser)
{
	const auto ClientID = pResult->GetClientID();
	auto* pGS = GetCommandResultGameServer(ClientID, pUser);
	const auto* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	// teleport by mouse
	const auto mousePos = pPlayer->GetCharacter()->GetMousePos();
	pPlayer->GetCharacter()->ChangePosition(mousePos);
}


void RconProcessor::ConTeleportByPos(IConsole::IResult* pResult, void* pUser)
{
	IServer* pServer = (IServer*)pUser;
	const auto ClientID = pResult->GetClientID();
	const auto PosX = pResult->GetInteger(0);
	const auto PosY = pResult->GetInteger(1);
	const auto WorldID = pResult->GetIntegerOr(2, pServer->GetClientWorldID(ClientID));

	auto* pGS = GetCommandResultGameServer(ClientID, pUser);
	const auto* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	// teleport by pos
	const auto NewPos = vec2(PosX, PosY);
	if(pServer->GetClientWorldID(ClientID) != WorldID)
	{
		pPlayer->GetSharedData().SetSpawnPosition(NewPos);
		pServer->ChangeWorld(ClientID, WorldID);
	}
	else
	{
		pPlayer->GetCharacter()->ChangePosition(NewPos);
	}
}


void RconProcessor::ConTeleportByClient(IConsole::IResult* pResult, void* pUser)
{
	IServer* pServer = (IServer*)pUser;
	auto ClientID = pResult->GetInteger(0);
	auto* pGS = GetCommandResultGameServer(ClientID, pUser);
	auto* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		pGS->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "teleport by client", "I can't find the player, or he's dead");
		return;
	}

	const auto NewPos = pPlayer->GetCharacter()->m_Core.m_Pos;
	const auto NewWorldID = pServer->GetClientWorldID(ClientID);

	ClientID = pResult->GetClientID();
	pGS = GetCommandResultGameServer(ClientID, pUser);
	pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	// teleport by client
	if(pPlayer->GetCurrentWorldID() != NewWorldID)
	{
		pPlayer->GetSharedData().SetSpawnPosition(NewPos);
		pServer->ChangeWorld(ClientID, NewWorldID);
	}
	else
	{
		pPlayer->GetCharacter()->ChangePosition(NewPos);
	}
}


void RconProcessor::ConPosition(IConsole::IResult* pResult, void* pUser)
{
	int ClientID = pResult->GetIntegerOr(0, pResult->GetClientID());
	auto* pGS = GetCommandResultGameServer(ClientID, pUser);
	const auto* pPlayer = pGS->GetPlayer(ClientID);

	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	const auto PosX = round_to_int(pPlayer->GetCharacter()->m_Core.m_Pos.x);
	const auto PosY = round_to_int(pPlayer->GetCharacter()->m_Core.m_Pos.y);
	const int MapPosX = PosX / 32;
	const int MapPosY = PosY / 32;
	pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "client position", "CID: %d Pos: %d %d(%d %d) World: %s(%d)",
		ClientID, MapPosX, MapPosY, PosX, PosY, pGS->Server()->GetWorldName(pGS->GetWorldID()), pGS->GetWorldID());
}


void RconProcessor::ConSetWorldTime(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	const int Hour = pResult->GetInteger(0);
	const auto pServer = (IServer*)pUserData;

	// set offset game time
	pServer->SetOffsetGameTime(Hour);
}


void RconProcessor::ConItemList(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	const auto pServer = (IServer*)pUserData;
	const auto pSelf = (CGS*)pServer->GameServer();

	// show list of items
	for(auto& [ID, Item] : CItemDescription::Data())
	{
		pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "item_list",
			"ID: %d | Name: %s | %s", ID, Item.GetName(), Item.IsEnchantable() ? "Enchantable" : "Default stack");
	}
}


// give the item to the player
void RconProcessor::ConGiveItem(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	const int ClientID = clamp(pResult->GetInteger(0), 0, MAX_PLAYERS - 1);
	const ItemIdentifier ItemID = pResult->GetInteger(1);
	const int Value = pResult->GetInteger(2);
	const int Enchant = pResult->GetInteger(3);
	const int ByMailbox = pResult->GetInteger(4);
	const auto pServer = (IServer*)pUserData;
	const auto pSelf = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	// check valid item
	if(!CItemDescription::Data().contains(ItemID))
	{
		pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "give_item", "Item with ID %d not found. Use command for list \"item_list\".", ItemID);
		return;
	}

	// check valid player
	if(CPlayer* pPlayer = pSelf->GetPlayer(ClientID, true))
	{
		if(ByMailbox == 0)
		{
			pPlayer->GetItem(ItemID)->Add(Value, 0, Enchant);
			return;
		}

		MailWrapper Mail("Console", pPlayer->Account()->GetID(), "The sender heavens.");
		Mail.AddDescLine("Sent from console");
		Mail.AttachItem(CItem(ItemID, Value, Enchant));
		Mail.Send();
	}
}


void RconProcessor::ConDisbandGuild(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	const auto pServer = (IServer*)pUserData;
	const auto pSelf = (CGS*)pServer->GameServer(INITIALIZER_WORLD_ID);
	const char* pGuildName = pResult->GetString(0);
	const CGuild* pGuild = pSelf->Core()->GuildManager()->GetGuildByName(pGuildName);

	// check valid guild
	if(!pGuild)
	{
		pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "guild_disband", "%s, no such guild has been found.", pGuildName);
		return;
	}

	// disband
	pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "guild_disband", "Guild with identifier %d and by the name of %s has been disbanded.", pGuild->GetID(), pGuildName);
	pSelf->Core()->GuildManager()->Disband(pGuild->GetID());
}


void RconProcessor::ConRemItem(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	const int ClientID = clamp(pResult->GetInteger(0), 0, MAX_PLAYERS - 1);
	const ItemIdentifier ItemID = pResult->GetInteger(1);
	const int Value = pResult->GetInteger(2);
	const auto pServer = (IServer*)pUserData;
	const auto pSelf = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	// check valid player
	if(CPlayer* pPlayer = pSelf->GetPlayer(ClientID, true))
	{
		// success remove item
		if(pPlayer->GetItem(ItemID)->Remove(Value))
		{
			pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "rem_item", "Item with ID %d(%d) has been removed from the player.", ItemID, Value);
			return;
		}

		// item not found
		pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "rem_item", "Item with ID %d not found in the player's inventory.", ItemID);
	}
}


void RconProcessor::ConSay(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	const auto pServer = (IServer*)pUserData;
	const auto pSelf = (CGS*)pServer->GameServer();

	// send chat
	pSelf->SendChat(-1, CHAT_ALL, pResult->GetString(0));
}


void RconProcessor::ConAddCharacter(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	const int ClientID = pResult->GetInteger(0);
	const auto pServer = (IServer*)pUserData;
	const auto pSelf = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	// we check if there is a player
	if(const CPlayer* pPlayer = pSelf->GetPlayer(ClientID, true); !pPlayer)
	{
		pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "add_character", "Player not found or isn't logged in");
		return;
	}

	// add a new kind of bot
	pSelf->Core()->BotManager()->ConAddCharacterBot(ClientID, pResult->GetString(1));
}


void RconProcessor::ConSyncLinesForTranslate(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	const auto pServer = (IServer*)pUserData;
	const auto pSelf = (CGS*)pServer->GameServer();

	// start thread for sync lines
	std::thread(&CMmoController::SyncLocalizations, pSelf->Core()).detach();
}


void RconProcessor::ConListAfk(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	int Counter = 0;
	const auto pServer = (IServer*)pUserData;
	auto pSelf = (CGS*)pServer->GameServer();

	for(int i = 0; i < MAX_PLAYERS; ++i)
	{
		// check client in-game
		if(pServer->ClientIngame(i))
		{
			// check afk state
			pSelf = (CGS*)pServer->GameServer(pServer->GetClientWorldID(i));
			if(const CPlayer* pPlayer = pSelf->GetPlayer(i); pPlayer && pPlayer->IsAfk())
			{
				// write information about afk
				pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "AFK", "id=%d name='%s' afk_time='%ld's", i, pServer->ClientName(i), pPlayer->GetAfkTime());
				Counter++;
			}
		}
	}

	// total afk players
	pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "AFK", "%d afk players in total", Counter);
}


void RconProcessor::ConCheckAfk(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	int ClientID = pResult->GetInteger(0);
	const auto pServer = (IServer*)pUserData;
	auto pSelf = (CGS*)pServer->GameServer();

	// check client in-game
	if(pServer->ClientIngame(ClientID))
	{
		// check afk state
		pSelf = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));
		if(const CPlayer* pPlayer = pSelf->GetPlayer(ClientID); pPlayer && pPlayer->IsAfk())
		{
			// write information about afk
			pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "AFK", "id=%d name='%s' afk_time='%ld's", ClientID, pServer->ClientName(ClientID), pPlayer->GetAfkTime());
			return;
		}
	}

	// if not found information about afk
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "AFK", "No such player or he's not afk");
}


void RconProcessor::ConBanAcc(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	const int ClientID = pResult->GetInteger(0);
	const auto pServer = (IServer*)pUserData;
	const auto pSelf = (CGS*)pServer->GameServer();

	// check valid timeperiod
	CTimePeriod time(pResult->GetString(1));
	if(time.isZero())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Time bad formatted or equals zero!");
		return;
	}

	// check player
	CPlayer* pPlayer = pSelf->GetPlayer(ClientID, true);
	if(!pPlayer)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Player not found or isn't logged in");
		return;
	}

	// ban account
	pSelf->Core()->AccountManager()->BanAccount(pPlayer, time, pResult->GetString(2));
}


void RconProcessor::ConUnBanAcc(IConsole::IResult* pResult, void* pUserData)
{
	const auto pServer = (IServer*)pUserData;
	const auto pSelf = (CGS*)pServer->GameServer();

	// unban account by banid
	pSelf->Core()->AccountManager()->UnBanAccount(pResult->GetInteger(0));
}


void RconProcessor::ConBansAcc(IConsole::IResult* pResult, void* pUserData)
{
	// initialize variables
	int Counter = 0;
	const auto pServer = (IServer*)pUserData;
	const auto pSelf = (CGS*)pServer->GameServer(INITIALIZER_WORLD_ID);

	// collects banned accounts
	for(const auto& p : pSelf->Core()->AccountManager()->BansAccount())
	{
		// write information about afk
		pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "BansAccount", "ban_id=%d name='%s' ban_until='%s' reason='%s'", p.id, p.nickname.c_str(), p.until.c_str(), p.reason.c_str());
		Counter++;
	}

	// total bans
	pSelf->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "BansAccount", "%d bans in total", Counter);
}


void RconProcessor::ConQuest(IConsole::IResult* pResult, void* pUser)
{
	const auto ClientID = pResult->GetClientID();
	const char* pStatus = pResult->GetString(0);
	auto* pGS = GetCommandResultGameServer(ClientID, pUser);
	auto* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
	{
		pGS->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "quest", "Log in to account after which use.");
		return;
	}
	auto TryGetValidPlayerQuest = [pGS, pPlayer, pResult]() -> CPlayerQuest*
	{
		const auto QuestID = pResult->GetInteger(1);
		if(!CQuestDescription::Data().contains(QuestID))
		{
			pGS->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "quest", "The quest with this ID does not exist.");
			return nullptr;
		}
		return pPlayer->GetQuest(QuestID);
	};

	if(str_comp(pStatus, "list") == 0)
	{
		for(auto& [ID, pQuestInfo] : CQuestDescription::Data())
		{
			auto* pPlayerQuest = pPlayer->GetQuest(ID);
			pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "quest_list",
				"ID: %d | Name: %s | State: %s", ID, pQuestInfo->GetName(), GetQustStateName(pPlayerQuest->GetState()));
		}
	}
	else if(str_comp(pStatus, "accept") == 0)
	{
		auto* pPlayerQuest = TryGetValidPlayerQuest();
		if(pPlayerQuest)
		{
			if(pPlayerQuest->IsAccepted() || pPlayerQuest->IsCompleted())
				pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "quest", "The quest is either completed or already accepted.");
			if(pPlayerQuest->Accept())
				pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "quest", "Quest '%s:%d' successful accepted!",
					pPlayerQuest->Info()->GetName(), pPlayerQuest->GetID());
		}
	}
	else if(str_comp(pStatus, "reset") == 0)
	{
		auto* pPlayerQuest = TryGetValidPlayerQuest();
		if(pPlayerQuest)
		{
			if(pPlayerQuest->IsAccepted() || pPlayerQuest->IsCompleted())
			{
				pPlayerQuest->Reset();
				pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "quest", "Quest '%s:%d' successful refused!",
					pPlayerQuest->Info()->GetName(), pPlayerQuest->GetID());
			}
		}
	}
	else
	{
		pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "quest", "Use 'quest <list, accept, reset> <quest_id>.");
	}
}


void RconProcessor::ConchainSpecialMotdupdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		const auto pSelf = (IServer*)pUserData;
		pSelf->SendMotd(-1, g_Config.m_SvMotd);
	}
}


void RconProcessor::ConchainGameinfoUpdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		return;
	}
}
