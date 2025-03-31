#include "command_processor.h"

#include <engine/server.h>
#include <game/server/core/components/accounts/account_manager.h>
#include <game/server/core/components/guilds/guild_manager.h>
#include <game/server/core/components/houses/house_manager.h>
#include <game/server/core/components/groups/group_manager.h>

#include "components/houses/entities/house_door.h"
#include "game/server/gamecontext.h"

CCommandProcessor::CCommandProcessor(CGS* pGS)
{
	m_pGS = pGS;
	m_CommandManager.Init(m_pGS->Console());

	IServer* pServer = m_pGS->Server();
	AddCommand("login", "s[username] s[password]", ConChatLogin, pServer, "User login (authentication)");
	AddCommand("register", "s[username] s[password]", ConChatRegister, pServer, "Register a new account");

	// game commands
	AddCommand("group", "?s[element] ?s[post]", ConGroup, pServer, "Manage group settings");
	AddCommand("guild", "?s[element] ?s[name]", ConChatGuild, pServer, "Manage guild settings");
	AddCommand("house", "?s[element] ?s[subelement] ?i[number]", ConChatHouse, pServer, "Manage house settings");
	AddCommand("use_item", "i[item]", ConChatUseItem, pServer, "Use an item");
	AddCommand("use_skill", "i[skill]", ConChatUseSkill, pServer, "Use a skill");
	AddCommand("voucher", "r[voucher]", ConChatVoucher, pServer, "Activate a voucher");

	// information commands
	AddCommand("cmdlist", "?i[page]", ConChatCmdList, pServer, "Display the list of available commands");
	AddCommand("help", "", ConChatCmdList, pServer, "Get help on commands");
	AddCommand("rules", "", ConChatRules, pServer, "View game rules");
	AddCommand("info", "", ConChatWiki, pServer, "Game information/wiki");
	AddCommand("wiki", "", ConChatWiki, pServer, "Game information/wiki");
	AddCommand("bonuses", "", ConChatBonuses, pServer, "Information on bonuses");
}

CCommandProcessor::~CCommandProcessor()
{
	m_CommandManager.ClearCommands();
}

static CGS* GetCommandResultGameServer(int ClientID, void* pUser)
{
	IServer* pServer = (IServer*)pUser;
	return (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));
}

void CCommandProcessor::ConChatLogin(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	auto* pGS = GetCommandResultGameServer(ClientID, pUser);
	auto* pPlayer = pGS->GetPlayer(ClientID);

	if(!pPlayer)
		return;

	if(pPlayer->IsAuthed())
	{
		pGS->Chat(ClientID, "You're already signed in!");
		return;
	}

	const char* pUsername = pResult->GetString(0);
	const char* pPassword = pResult->GetString(1);
	pGS->Core()->AccountManager()->LoginAccount(ClientID, pUsername, pPassword);
}

void CCommandProcessor::ConChatRegister(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	auto* pGS = GetCommandResultGameServer(ClientID, pUser);
	auto* pPlayer = pGS->GetPlayer(ClientID);

	if(!pPlayer)
		return;

	if(pPlayer->IsAuthed())
	{
		pGS->Chat(ClientID, "Sign out first before you create a new account.");
		return;
	}

	const char* pUsername = pResult->GetString(0);
	const char* pPassword = pResult->GetString(1);
	pGS->Core()->AccountManager()->RegisterAccount(ClientID, pUsername, pPassword);
}

void CCommandProcessor::ConChatGuild(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	auto* pGS = GetCommandResultGameServer(ClientID, pUser);
	auto* pPlayer = pGS->GetPlayer(ClientID);

	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	// If the command element is "leave", leave the guild
	const std::string pElem = pResult->GetString(0);
	if(pElem.compare(0, 5, "leave") == 0)
	{
		if(!pPlayer->Account()->HasGuild())
		{
			pGS->Chat(ClientID, "You are not in the guild!");
			return;
		}

		const int AccountID = pPlayer->Account()->GetID();
		GuildResult Result = pPlayer->Account()->GetGuild()->GetMembers()->Kick(AccountID);
		if(Result == GuildResult::MEMBER_SUCCESSFUL)
		{
			pGS->Chat(ClientID, "You have left the guild!");
		}
		else if(Result == GuildResult::MEMBER_KICK_IS_OWNER)
		{
			pGS->Chat(ClientID, "You cannot leave the guild because you are the owner.");
		}
		return;
	}

	// If the command element is "create", create a new guild
	std::string Guildname = pResult->GetString(1);
	if(pElem.compare(0, 6, "create") == 0)
	{
		if(Guildname.length() > 8 || Guildname.length() < 3)
		{
			pGS->Chat(ClientID, "Guild name must contain '3-8 characters'.");
			return;
		}

		pGS->Core()->GuildManager()->Create(pPlayer, Guildname.c_str());
		return;
	}

	// Guild command list
	pGS->Chat(ClientID, mystd::aesthetic::boardPillar("Guild system", 7).c_str());
	pGS->Chat(ClientID, "/guild create <name> - create a new guild");
	pGS->Chat(ClientID, "/guild leave - leave the guild");

}

void CCommandProcessor::ConChatHouse(IConsole::IResult* pResult, void* pUser)
{
	// Get the game server associated with the client ID
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	// If the player is not authenticated, return
	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	// If the player does not own a house, send a chat message and return
	CHouse* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
	{
		pGS->Chat(pPlayer->GetCID(), "You don't own a house!");
		return;
	}

	// If the command element is "sell", sell the house to the state
	const std::string pElem = pResult->GetString(0);
	if(pElem.compare(0, 4, "sell") == 0)
	{
		pHouse->Sell();
		return;
	}

	// If the command element is "doors", settings for the doors of the house
	const std::string pSubElem = pResult->GetString(1);
	if(pElem.compare(0, 5, "doors") == 0)
	{
		auto* pDoorManager = pHouse->GetDoorManager();

		// If the command element is "list", list all the doors in the house
		if(pSubElem.compare(0, 4, "list") == 0)
		{
			pGS->Chat(ClientID, mystd::aesthetic::boardPillar("Door list", 7).c_str());
			for(const auto& [Number, Door] : pDoorManager->GetContainer())
			{
				bool State = pDoorManager->GetContainer()[Number]->IsClosed();
				pGS->Chat(ClientID, "Number: '{}'. Name: {} ({})",
					Number, Door->GetName(), State ? Instance::Localize(ClientID, "closed") : Instance::Localize(ClientID, "opened"));
			}
			return;
		}

		// If the command element is "open_all", open all door's
		if(pSubElem.compare(0, 8, "open_all") == 0)
		{
			pDoorManager->OpenAll();
			pPlayer->m_VotesData.UpdateVotes(MENU_HOUSE);
			pGS->Chat(pPlayer->GetCID(), "All the doors of the house were open!");
			return;
		}

		// If the command element is "close_all", close all door's
		if(pSubElem.compare(0, 9, "close_all") == 0)
		{
			pDoorManager->CloseAll();
			pPlayer->m_VotesData.UpdateVotes(MENU_HOUSE);
			pGS->Chat(pPlayer->GetCID(), "All the doors of the house were closed!");
			return;
		}

		// If the command element is "reverse_all", reverse all door's
		if(pSubElem.compare(0, 11, "reverse_all") == 0)
		{
			pDoorManager->ReverseAll();
			pPlayer->m_VotesData.UpdateVotes(MENU_HOUSE);
			pGS->Chat(pPlayer->GetCID(), "All the doors of the house were reversed!");
			return;
		}

		// Get the door ID from the command result
		int Number = pResult->GetInteger(2);

		// If the command element is "reverse", reverse the state of the specified door
		if(pSubElem.compare(0, 7, "reverse") == 0)
		{
			// Check if the door ID is valid
			if(pDoorManager->GetContainer().find(Number) == pDoorManager->GetContainer().end())
			{
				pGS->Chat(pPlayer->GetCID(), "Number is either not listed or such a door does not exist.");
				return;
			}

			pDoorManager->Reverse(Number);
			pPlayer->m_VotesData.UpdateVotes(MENU_HOUSE);
			bool State = pDoorManager->GetContainer()[Number]->IsClosed();
			pGS->Chat(pPlayer->GetCID(), "Door '{}(Number {})' was '{}'!", pDoorManager->GetContainer()[Number]->GetName(), Number, State ? "closed" : "opened");
			return;
		}

		pGS->Chat(ClientID, mystd::aesthetic::boardPillar("Door controls", 6).c_str());
		pGS->Chat(ClientID, "/house doors list - list door's and ids");
		pGS->Chat(ClientID, "/house doors open_all - open all door's");
		pGS->Chat(ClientID, "/house doors close_all - close all door's");
		pGS->Chat(ClientID, "/house doors reverse_all - reverse all door's");
		pGS->Chat(ClientID, "/house doors reverse <number> - reverse door by id");
		return;
	}

	pGS->Chat(ClientID, mystd::aesthetic::boardPillar("House system", 7).c_str());
	pGS->Chat(ClientID, "/house doors - settings door's");
	pGS->Chat(ClientID, "/house sell - sell the house to the state");
}

void CCommandProcessor::ConGroup(IConsole::IResult* pResult, void* pUser)
{
	// Get the client ID from the result
	const int ClientID = pResult->GetClientID();

	// Get the server and game server objects
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	// Get the player object for the client
	CPlayer* pPlayer = pGS->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	// Get the requested element from the result
	const std::string pElem = pResult->GetString(0);

	// Check if the requested element is "create"
	if(pElem.compare(0, 6, "create") == 0)
	{
		// Create a group for the player
		pGS->Core()->GroupManager()->CreateGroup(pPlayer);
		pGS->UpdateVotesIfForAll(MENU_GROUP);
		return;
	}

	// Check if the requested element is "leave"
	GroupData* pGroup = pPlayer->Account()->GetGroup();
	if(pElem.compare(0, 5, "leave") == 0)
	{
		// Check group
		if(!pGroup)
		{
			pGS->Chat(ClientID, "You're not in a group!");
			return;
		}

		// Remove the player from the group
		pGroup->Remove(pPlayer->Account()->GetID());
		pGS->UpdateVotesIfForAll(MENU_GROUP);
		return;
	}

	// Check if the requested element is "list"
	if(pElem.compare(0, 4, "list") == 0)
	{
		// Check group
		if(!pGroup)
		{
			pGS->Chat(ClientID, "You're not in a group!");
			return;
		}

		// Display the group list for the player
		pGS->Chat(ClientID, mystd::aesthetic::boardPillar("Group membership list", 5).c_str());
		for(const auto& AID : pGroup->GetAccounts())
		{
			const char* Prefix = (pGroup->GetOwnerUID() == AID) ? "O: " : "\0";
			const std::string Nickname = Instance::Server()->GetAccountNickname(AID);
			pGS->Chat(ClientID, "{}{}", Prefix, Nickname.c_str());
		}
		return;
	}

	const char* Status = (pGroup ? Instance::Localize(ClientID, "in a group") : Instance::Localize(ClientID, "not in a group"));
	pGS->Chat(ClientID, mystd::aesthetic::boardPillar("Group system", 8).c_str());
	pGS->Chat(ClientID, "Current status: '{}'!", Status);
	pGS->Chat(ClientID, "/group create - create a new group");
	pGS->Chat(ClientID, "/group list - group membership list");
	pGS->Chat(ClientID, "/group leave - leave the group");
}

void CCommandProcessor::ConChatUseItem(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	// check valid
	const int ItemID = pResult->GetInteger(0);
	if(CItemDescription::Data().find(ItemID) == CItemDescription::Data().end())
	{
		pGS->Chat(ClientID, "There is no item with the entered 'Item Identifier'.");
		return;
	}

	pPlayer->GetItem(ItemID)->Use(1);
}

void CCommandProcessor::ConChatUseSkill(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	// check valid
	const int SkillID = pResult->GetInteger(0);
	if(CSkillDescription::Data().find(SkillID) == CSkillDescription::Data().end())
	{
		pGS->Chat(ClientID, "There is no skill with the entered 'Skill Identifier'.");
		return;
	}

	pPlayer->GetSkill(SkillID)->Use();
}

void CCommandProcessor::ConChatCmdList(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	constexpr int MaxPage = 2;
	const int Page = clamp(pResult->GetInteger(0), 1, MaxPage);
	pGS->Chat(ClientID, mystd::aesthetic::boardPillar(fmt_localize(ClientID, "Command list [{} of {}]", Page, MaxPage), 6).c_str());
	if(Page == 1)
	{
		pGS->Chat(ClientID, "/register <name> <pass> - new account.");
		pGS->Chat(ClientID, "/login <name> <pass> - log in account.");
		pGS->Chat(ClientID, "/voucher <voucher> - get voucher special items.");
		pGS->Chat(ClientID, "/use_skill <uid> - use skill by uid.");
		pGS->Chat(ClientID, "/use_item <uid> - use item by uid.");
		pGS->Chat(ClientID, "/wiki - wiki information about mod.");
		pGS->Chat(ClientID, "/bonuses - active bonuses.");

	}
	else if(Page == 2)
	{
		pGS->Chat(ClientID, "/rules - server rules.");
		pGS->Chat(ClientID, "/group - group system.");
		pGS->Chat(ClientID, "/guild - guild system.");
		pGS->Chat(ClientID, "/house - house system.");
		pGS->Chat(ClientID, "/tutorial - training challenge.");
		pGS->Chat(ClientID, "#<text> - perform an action.");
	}
}

void CCommandProcessor::ConChatRules(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	// translate to russian
	pGS->Chat(ClientID, mystd::aesthetic::boardFlower("Server rules").c_str());
	pGS->Chat(ClientID, "- Don't use racist words");
	pGS->Chat(ClientID, "- Don't spam messages");
	pGS->Chat(ClientID, "- Don't block other players");
	pGS->Chat(ClientID, "- Don't abuse bugs");
	pGS->Chat(ClientID, "- Don't use third party software which give you unfair advantage (bots, clickers, macros)");
	pGS->Chat(ClientID, "- Don't share unwanted/malicious/advertisement links");
	pGS->Chat(ClientID, "- Don't use multiple accounts");
	pGS->Chat(ClientID, "- Don't share your account credentials (username, password)");
	pGS->Chat(ClientID, "- The admins and moderators will mute/kick/ban per discretion");
}

void CCommandProcessor::ConChatBonuses(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	auto* pGS = GetCommandResultGameServer(ClientID, pUser);
	auto* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	// send bonuses info
	pGS->SendMenuMotd(pPlayer, MOTD_MENU_BONUSES_INFO);
}

void CCommandProcessor::ConChatWiki(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	// send wiki info
	pGS->SendMenuMotd(pPlayer, MOTD_MENU_WIKI_INFO);
}

void CCommandProcessor::ConChatVoucher(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	if(pResult->NumArguments() != 1)
	{
		pGS->Chat(ClientID, "Use: '/voucher <voucher>'.");
		return;
	}

	char aVoucher[32];
	str_copy(aVoucher, pResult->GetString(0), sizeof(aVoucher));
	pGS->Core()->AccountManager()->UseVoucher(ClientID, aVoucher);
}

/************************************************************************/
/*  Command system                                                      */
/************************************************************************/

void CCommandProcessor::ProcessClientChatCommand(int ClientID, const char* pMessage)
{
	// prepare command buffer
	int charIndex = 0;
	char aCmdBuf[256] = { 0 };
	for(int i = 1; i < str_length(pMessage); i++)
	{
		if(pMessage[i] != ' ')
		{
			aCmdBuf[charIndex] = pMessage[i];
			charIndex++;
		}
		else
			break;
	}

	// search command info
	const IConsole::CCommandInfo* pCommandInfo = GS()->Console()->GetCommandInfo(aCmdBuf, CFGFLAG_CHAT, false);
	if(pCommandInfo)
	{
		int errorArgs;
		GS()->Console()->ExecuteLineFlag(pMessage + 1, CFGFLAG_CHAT, ClientID, false, &errorArgs);
		if(errorArgs)
		{
			char argsDesc[256];
			GS()->Console()->ParseArgumentsDescription(pCommandInfo->m_pParams, argsDesc, sizeof(argsDesc));
			GS()->Chat(ClientID, "Use: '/{} {}'", pCommandInfo->m_pName, argsDesc);
		}
	}
	else
	{
		GS()->Chat(ClientID, "Command '{}' not found!", pMessage);
	}
}


void CCommandProcessor::AddCommand(const char* pName, const char* pParams, IConsole::FCommandCallback pfnFunc, void* pUser, const char* pHelp)
{
	GS()->Console()->Register(pName, pParams, CFGFLAG_CHAT, pfnFunc, pUser, pHelp);
	m_CommandManager.AddCommand(pName, pHelp, pParams, pfnFunc, pUser);
}


void CCommandProcessor::SendClientCommandsInfo(CGS* pGS, int ClientID) const
{
	// send start group command
	{
		CNetMsg_Sv_CommandInfoGroupStart msg;
		pGS->Server()->SendPackMsg(&msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}

	// send allowed commands
	for(const IConsole::CCommandInfo* pCmd = pGS->Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_USER, CFGFLAG_CHAT); pCmd != nullptr;
		pCmd = pCmd->NextCommandInfo(IConsole::ACCESS_LEVEL_USER, CFGFLAG_CHAT))
	{
		CNetMsg_Sv_CommandInfo msg;
		msg.m_pName = pCmd->m_pName;
		msg.m_pArgsFormat = pCmd->m_pParams;
		msg.m_pHelpText = pCmd->m_pHelp;
		pGS->Server()->SendPackMsg(&msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}

	// send end group command
	{
		CNetMsg_Sv_CommandInfoGroupEnd msg;
		pGS->Server()->SendPackMsg(&msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
}