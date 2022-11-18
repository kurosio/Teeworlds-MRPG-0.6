#include <engine/console.h>
#include "CommandProcessor.h"

#include <engine/server.h>
#include <engine/shared/config.h>
#include "game/server/gamecontext.h"

#include <game/server/mmocore/Components/Accounts/AccountCore.h>
#include <game/server/mmocore/Components/Guilds/GuildCore.h>
#include <game/server/mmocore/Components/Houses/HouseCore.h>

CCommandProcessor::CCommandProcessor(CGS *pGS)
{
	m_pGS = pGS;
	m_CommandManager.Init(m_pGS->Console(), this, nullptr, nullptr);

	IServer* pServer = m_pGS->Server();
	AddCommand("login", "s[username] s[password]", ConChatLogin, pServer, "");
	AddCommand("register", "s[username] s[password]", ConChatRegister, pServer, "");

	// guild commands
	AddCommand("gexit", "", ConChatGuildExit, pServer, "");
	AddCommand("gcreate", "r[guildname]", ConChatGuildCreate, pServer, "");

	// house commands
	AddCommand("doorhouse", "", ConChatDoorHouse, pServer, "");
	AddCommand("sellhouse", "", ConChatSellHouse, pServer, "");

	// admin command
	AddCommand("pos", "", ConChatPosition, pServer, "");
	AddCommand("sound", "i[sound]", ConChatSound, pServer, "");
	AddCommand("effect", "s[effect] i[sec]", ConChatGiveEffect, pServer, "");

	// game command
	AddCommand("useitem", "i[item]", ConChatUseItem, pServer, "");
	AddCommand("useskill", "i[skill]", ConChatUseSkill, pServer, "");
	AddCommand("voucher", "r[voucher]", ConChatVoucher, pServer, "");
	AddCommand("coupon", "r[coupon]", ConChatVoucher, pServer, "");

	// information command
	AddCommand("cmdlist", "", ConChatCmdList, pServer, "");
	AddCommand("help", "", ConChatCmdList, pServer, "");
	AddCommand("rules", "", ConChatRules, pServer, "");
#ifdef CONF_DISCORD
	AddCommand("discord_connect", "s[DID]", ConChatDiscordConnect, pServer, "");
#endif
}

CCommandProcessor::~CCommandProcessor()
{
	m_CommandManager.ClearCommands();
}

void CCommandProcessor::ConChatLogin(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer)
		return;

	if (pPlayer->IsAuthed())
	{
		pGS->Chat(ClientID, "You're already signed in.");
		return;
	}

	char aUsername[16];
	char aPassword[16];
	str_copy(aUsername, pResult->GetString(0), sizeof(aUsername));
	str_copy(aPassword, pResult->GetString(1), sizeof(aPassword));

	pGS->Mmo()->Account()->LoginAccount(ClientID, aUsername, aPassword);
}

void CCommandProcessor::ConChatRegister(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer)
		return;

	if (pPlayer->IsAuthed())
	{
		pGS->Chat(ClientID, "Sign out first before you create a new account.");
		return;
	}

	char aUsername[16];
	char aPassword[16];
	str_copy(aUsername, pResult->GetString(0), sizeof(aUsername));
	str_copy(aPassword, pResult->GetString(1), sizeof(aPassword));

	pGS->Mmo()->Account()->RegisterAccount(ClientID, aUsername, aPassword);
}

#ifdef CONF_DISCORD
void CCommandProcessor::ConChatDiscordConnect(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->IsAuthed())
		return;

	char aDiscordDID[32];
	str_copy(aDiscordDID, pResult->GetString(0), sizeof(aDiscordDID));
	if (str_length(aDiscordDID) > 30 || str_length(aDiscordDID) < 10)
	{
		pGS->Chat(ClientID, "Discord ID must contain 10-30 characters.");
		return;
	}

	if(!str_is_number(aDiscordDID))
	{
		pGS->Chat(ClientID, "Discord ID can only contain numbers.");
		return;
	}

	pGS->Mmo()->Account()->DiscordConnect(ClientID, aDiscordDID);
}
#endif

void CCommandProcessor::ConChatGuildExit(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->IsAuthed() || !pPlayer->Acc().IsGuild())
		return;

	const int AccountID = pPlayer->Acc().m_UserID;
	pGS->Mmo()->Member()->ExitGuild(AccountID);
}

void CCommandProcessor::ConChatGuildCreate(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->IsAuthed() || pPlayer->Acc().IsGuild())
		return;

	char aGuildName[16];
	str_copy(aGuildName, pResult->GetString(0), sizeof(aGuildName));
	if (str_length(aGuildName) > 8 || str_length(aGuildName) < 3)
	{
		pGS->Chat(ClientID, "Guild name must contain 3-8 characters");
		return;
	}

	pGS->Mmo()->Member()->CreateGuild(pPlayer, aGuildName);
}

void CCommandProcessor::ConChatDoorHouse(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->IsAuthed())
		return;

	const int HouseID = pGS->Mmo()->House()->PlayerHouseID(pPlayer);
	pGS->Mmo()->House()->ChangeStateDoor(HouseID);
}

void CCommandProcessor::ConChatSellHouse(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->IsAuthed())
		return;

	// check owner house id
	const int HouseID = pGS->Mmo()->House()->PlayerHouseID(pPlayer);
	if(HouseID < 0)
	{
		pGS->Chat(ClientID, "You have no home.");
		return;
	}

	// sell house
	pGS->Mmo()->House()->SellHouse(HouseID);
}

void CCommandProcessor::ConChatPosition(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->GetCharacter() || !pGS->Server()->IsAuthed(ClientID))
		return;

	const int PosX = (int)pPlayer->GetCharacter()->m_Core.m_Pos.x / 32;
	const int PosY = (int)pPlayer->GetCharacter()->m_Core.m_Pos.y/32;
	pGS->Chat(ClientID, "[{STR}] Position X: {INT} Y: {INT}.", pGS->Server()->GetWorldName(pGS->GetWorldID()), PosX, PosY);
	dbg_msg("cmd_pos", "%0.f %0.f WorldID: %d", pPlayer->GetCharacter()->m_Core.m_Pos.x, pPlayer->GetCharacter()->m_Core.m_Pos.y, pGS->GetWorldID());
}

void CCommandProcessor::ConChatSound(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->GetCharacter() || !pGS->Server()->IsAuthed(ClientID))
		return;

	const int SoundID = clamp(pResult->GetInteger(0), 0, 40);
	pGS->CreateSound(pPlayer->GetCharacter()->m_Core.m_Pos, SoundID);
}

void CCommandProcessor::ConChatGiveEffect(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if(pPlayer && pPlayer->IsAuthed())
		pPlayer->GiveEffect(pResult->GetString(0), pResult->GetInteger(1));
}

void CCommandProcessor::ConChatUseItem(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->IsAuthed())
		return;

	// check valid
	const int ItemID = pResult->GetInteger(0);
	if(CItemDescription::Data().find(ItemID) == CItemDescription::Data().end())
	{
		pGS->Chat(ClientID, "There is no item with the entered Item Identifier.");
		return;
	}

	pPlayer->GetItem(ItemID)->Use(1);
}

void CCommandProcessor::ConChatUseSkill(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->IsAuthed())
		return;

	// check valid
	const int SkillID = pResult->GetInteger(0);
	if(CSkillDescription::Data().find(SkillID) == CSkillDescription::Data().end())
	{
		pGS->Chat(ClientID, "There is no skill with the entered Skill Identifier.");
		return;
	}

	pPlayer->GetSkill(SkillID)->Use();
}

void CCommandProcessor::ConChatCmdList(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer)
		return;

	pGS->Chat(ClientID, "Command List / Help");
	pGS->Chat(ClientID, "/register <name> <pass> - new account.");
	pGS->Chat(ClientID, "/login <name> <pass> - log in account.");
	pGS->Chat(ClientID, "/rules - server rules.");
	pGS->Chat(ClientID, "Another information see Wiki Page.");
}

void CCommandProcessor::ConChatRules(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer)
		return;

	pGS->Chat(ClientID, "Server rules");
	pGS->Chat(ClientID, "- Don't abuse bugs");
	pGS->Chat(ClientID, "- Don't use bots or other external software which give you unfair advantages");
	pGS->Chat(ClientID, "- Don't use multiple accounts");
	pGS->Chat(ClientID, "- Don't share your account credentials (username, password)");
	pGS->Chat(ClientID, "- Do not use ads, that is not part of the game");
}

void CCommandProcessor::ConChatVoucher(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->IsAuthed())
		return;

	if (pResult->NumArguments() != 1)
	{
		pGS->Chat(ClientID, "Use: /voucher <voucher>", pPlayer);
		return;
	}

	char aVoucher[32];
	str_copy(aVoucher, pResult->GetString(0), sizeof(aVoucher));
	pGS->Mmo()->Account()->UseVoucher(ClientID, aVoucher);
}


/************************************************************************/
/*  Command system                                                      */
/************************************************************************/

void CCommandProcessor::ChatCmd(const char* pMessage, CPlayer* pPlayer)
{
	LastChat(pPlayer);
	const int ClientID = pPlayer->GetCID();

	int Char = 0;
	char aCommand[256] = { 0 };
	for(int i = 1; i < str_length(pMessage); i++)
	{
		if(pMessage[i] != ' ')
		{
			aCommand[Char] = pMessage[i];
			Char++;
			continue;
		}
		break;
	}

	const IConsole::CCommandInfo* pCommand = GS()->Console()->GetCommandInfo(aCommand, CFGFLAG_CHAT, false);
	if(pCommand)
	{
		int ErrorArgs;
		GS()->Console()->ExecuteLineFlag(pMessage + 1, CFGFLAG_CHAT, ClientID, false, &ErrorArgs);
		if(ErrorArgs)
		{
			char aArgsDesc[256];
			GS()->Console()->ParseArgumentsDescription(pCommand->m_pParams, aArgsDesc, sizeof(aArgsDesc));
			GS()->Chat(ClientID, "Use: /{STR} {STR}", pCommand->m_pName, aArgsDesc);
		}
		return;
	}

	GS()->Chat(ClientID, "Command {STR} not found!", pMessage);
}

void CCommandProcessor::AddCommand(const char* pName, const char* pParams, IConsole::FCommandCallback pfnFunc, void* pUser, const char* pHelp)
{
	GS()->Console()->Register(pName, pParams, CFGFLAG_CHAT, pfnFunc, pUser, pHelp);
	if(!m_CommandManager.AddCommand(pName, pHelp, pParams, pfnFunc, pUser))
		dbg_msg("chat_cmd", "added command: '/%s' :: WorldID %d", pName, m_pGS->GetWorldID());
	else
		dbg_msg("chat_cmd", "failed to add command: '/%s' :: WorldID %d", pName, m_pGS->GetWorldID());
}

void CCommandProcessor::LastChat(CPlayer *pPlayer)
{
	if(pPlayer->m_aPlayerTick[TickState::LastChat] + GS()->Server()->TickSpeed() <= GS()->Server()->Tick())
		pPlayer->m_aPlayerTick[TickState::LastChat] = GS()->Server()->Tick();
}