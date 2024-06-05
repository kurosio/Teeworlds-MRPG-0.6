#include "command_processor.h"

#include <engine/server.h>
#include <game/server/core/components/Accounts/AccountManager.h>
#include <game/server/core/components/guilds/guild_manager.h>
#include <game/server/core/components/houses/house_manager.h>
#include <game/server/core/components/groups/group_manager.h>

#include "components/houses/entities/house_door.h"
#include "game/server/gamecontext.h"

void ConAddMultipleOrbite(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(pResult->GetClientID()));

	CPlayer* pPlayer = pGS->GetPlayer(pResult->GetClientID());
	if(!pPlayer || !pPlayer->IsAuthed() || !pPlayer->GetCharacter())
		return;

	const int Orbite = pResult->GetInteger(0);
	const int Type = pResult->GetInteger(1);
	const int Subtype = pResult->GetInteger(2);
	pPlayer->GetCharacter()->AddMultipleOrbite(Orbite, Type, Subtype);
}

CCommandProcessor::CCommandProcessor(CGS* pGS)
{
	m_pGS = pGS;
	m_CommandManager.Init(m_pGS->Console(), this, nullptr, nullptr);

	IServer* pServer = m_pGS->Server();
	AddCommand("login", "s[username] s[password]", ConChatLogin, pServer, "");
	AddCommand("register", "s[username] s[password]", ConChatRegister, pServer, "");

	// guild commands
	AddCommand("guild", "?s[element] ?s[name]", ConChatGuild, pServer, "");

	// house commands
	AddCommand("house", "?s[element] ?s[subelement] ?i[number]", ConChatHouse, pServer, "");

	// admin command
	AddCommand("pos", "", ConChatPosition, pServer, "");
	AddCommand("sound", "i[sound]", ConChatSound, pServer, "");
	AddCommand("effect", "s[effect] i[sec]", ConChatGiveEffect, pServer, "");

	// group command
	AddCommand("group", "?s[element] ?s[post]", ConGroup, pServer, "");

	// game command
	AddCommand("useitem", "i[item]", ConChatUseItem, pServer, "");
	AddCommand("useskill", "i[skill]", ConChatUseSkill, pServer, "");
	AddCommand("voucher", "r[voucher]", ConChatVoucher, pServer, "");
	AddCommand("coupon", "r[coupon]", ConChatVoucher, pServer, "");
	AddCommand("tutorial", "", ConChatTutorial, pServer, "");

	// information command
	AddCommand("cmdlist", "?i[page]", ConChatCmdList, pServer, "");
	AddCommand("help", "?i[page]", ConChatCmdList, pServer, "");
	AddCommand("rules", "", ConChatRules, pServer, "");

	AddCommand("add_multiple_orbite", "i[orbite] i[type] i[subtype]", ConAddMultipleOrbite, pServer, "");

	// discord command
#ifdef CONF_DISCORD
	AddCommand("discord_connect", "s[DID]", ConChatDiscordConnect, pServer, "");
#endif
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
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(pPlayer)
	{
		if(pPlayer->IsAuthed())
		{
			pGS->Chat(ClientID, "You're already signed in!");
			return;
		}

		char aUsername[16];
		char aPassword[16];
		str_copy(aUsername, pResult->GetString(0), sizeof(aUsername));
		str_copy(aPassword, pResult->GetString(1), sizeof(aPassword));

		pGS->Core()->AccountManager()->LoginAccount(ClientID, aUsername, aPassword);
	}
}

void CCommandProcessor::ConChatRegister(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	if(pPlayer->IsAuthed())
	{
		pGS->Chat(ClientID, "Sign out first before you create a new account.");
		return;
	}

	char aUsername[16];
	char aPassword[16];
	str_copy(aUsername, pResult->GetString(0), sizeof(aUsername));
	str_copy(aPassword, pResult->GetString(1), sizeof(aPassword));

	pGS->Core()->AccountManager()->RegisterAccount(ClientID, aUsername, aPassword);
}

#ifdef CONF_DISCORD
void CCommandProcessor::ConChatDiscordConnect(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	char aDiscordDID[32];
	str_copy(aDiscordDID, pResult->GetString(0), sizeof(aDiscordDID));
	if(str_length(aDiscordDID) > 30 || str_length(aDiscordDID) < 10)
	{
		pGS->Chat(ClientID, "Discord ID must contain 10-30 characters.");
		return;
	}

	if(!string_to_number(aDiscordDID, 0, std::numeric_limits<int>::max()))
	{
		pGS->Chat(ClientID, "Discord ID can only contain numbers.");
		return;
	}

	pGS->Core()->AccountManager()->DiscordConnect(ClientID, aDiscordDID);
}
#endif

void CCommandProcessor::ConChatGuild(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
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
			pGS->Chat(ClientID, "Guild name must contain 3-8 characters");
			return;
		}

		pGS->Core()->GuildManager()->Create(pPlayer, Guildname.c_str());
		return;
	}

	// Guild command list
	pGS->Chat(ClientID, "{} Guild system {}", Tools::Aesthetic::B_PILLAR(7, false), Tools::Aesthetic::B_PILLAR(7, true));
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
			pGS->Chat(ClientID, "{} Door list {}", Tools::Aesthetic::B_PILLAR(7, false), Tools::Aesthetic::B_PILLAR(7, true));
			for(const auto& [Number, Door] : pDoorManager->GetContainer())
			{
				bool State = pDoorManager->GetContainer()[Number]->IsClosed();
				pGS->Chat(ClientID, "Number: {}. Name: {} ({})", 
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
			pGS->Chat(pPlayer->GetCID(), "Door {}(Number {}) was {}!", pDoorManager->GetContainer()[Number]->GetName(), Number, State ? "closed" : "opened");
			return;
		}

		pGS->Chat(ClientID, "{} Door controls {}", Tools::Aesthetic::B_PILLAR(6, false), Tools::Aesthetic::B_PILLAR(6, true));
		pGS->Chat(ClientID, "/house doors list - list door's and ids");
		pGS->Chat(ClientID, "/house doors open_all - open all door's");
		pGS->Chat(ClientID, "/house doors close_all - close all door's");
		pGS->Chat(ClientID, "/house doors reverse_all - reverse all door's");
		pGS->Chat(ClientID, "/house doors reverse <number> - reverse door by id");
		return;
	}

	pGS->Chat(ClientID, "{} House system {}", Tools::Aesthetic::B_PILLAR(7, false), Tools::Aesthetic::B_PILLAR(7, true));
	pGS->Chat(ClientID, "/house doors - settings door's");
	pGS->Chat(ClientID, "/house sell - sell the house to the state");
}

void CCommandProcessor::ConChatPosition(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->GetCharacter() || !pGS->Server()->IsAuthed(ClientID))
		return;

	const int PosX = static_cast<int>(pPlayer->GetCharacter()->m_Core.m_Pos.x) / 32;
	const int PosY = static_cast<int>(pPlayer->GetCharacter()->m_Core.m_Pos.y) / 32;
	pGS->Chat(ClientID, "[{}] Position X: {} Y: {}.", pGS->Server()->GetWorldName(pGS->GetWorldID()), PosX, PosY);
	dbg_msg("cmd_pos", "%0.f %0.f WorldID: %d", pPlayer->GetCharacter()->m_Core.m_Pos.x, pPlayer->GetCharacter()->m_Core.m_Pos.y, pGS->GetWorldID());
}

void CCommandProcessor::ConChatSound(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->GetCharacter() || !pGS->Server()->IsAuthed(ClientID))
		return;

	const int SoundID = clamp(pResult->GetInteger(0), 0, 40);
	pGS->CreateSound(pPlayer->GetCharacter()->m_Core.m_Pos, SoundID);
}

void CCommandProcessor::ConChatGiveEffect(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	IServer* pServer = (IServer*)pUser;
	CGS* pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(pPlayer && pPlayer->IsAuthed() && pGS->Server()->IsAuthed(ClientID))
		pPlayer->GiveEffect(pResult->GetString(0), pResult->GetInteger(1));
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
		pGS->Chat(ClientID, "{} Group membership list {}", Tools::Aesthetic::B_PILLAR(5, false), Tools::Aesthetic::B_PILLAR(5, true));
		for(const auto& AID : pGroup->GetAccounts())
		{
			const char* Prefix = (pGroup->GetOwnerUID() == AID) ? "O: " : "\0";
			const std::string Nickname = Instance::Server()->GetAccountNickname(AID);
			pGS->Chat(ClientID, "{}{}", Prefix, Nickname.c_str());
		}
		return;
	}

	const char* Status = (pGroup ? Instance::Localize(ClientID, "in a group") : Instance::Localize(ClientID, "not in a group"));
	pGS->Chat(ClientID, "{} Group system {}", Tools::Aesthetic::B_PILLAR(8, false), Tools::Aesthetic::B_PILLAR(8, true));
	pGS->Chat(ClientID, "Current status: {}!", Status);
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
		pGS->Chat(ClientID, "There is no item with the entered Item Identifier.");
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
		pGS->Chat(ClientID, "There is no skill with the entered Skill Identifier.");
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
	pGS->Chat(ClientID, "{} Command list [{} of {}] {}", Tools::Aesthetic::B_PILLAR(6, false), Page, MaxPage, Tools::Aesthetic::B_PILLAR(6, true));
	if(Page == 1)
	{
		pGS->Chat(ClientID, "/register <name> <pass> - new account.");
		pGS->Chat(ClientID, "/login <name> <pass> - log in account.");
		pGS->Chat(ClientID, "/rules - server rules.");
		pGS->Chat(ClientID, "/tutorial - training challenge.");
		pGS->Chat(ClientID, "/voucher <voucher> - get voucher special items.");
		pGS->Chat(ClientID, "/useskill <uid> - use skill by uid.");
		pGS->Chat(ClientID, "/useitem <uid> - use item by uid.");
	}
	else if(Page == 2)
	{
		pGS->Chat(ClientID, "/group - group system.");
		pGS->Chat(ClientID, "/guild - guild system.");
		pGS->Chat(ClientID, "/house - house system.");
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
	pGS->Chat(ClientID, "{} Server rules {}", Tools::Aesthetic::B_FLOWER(false), Tools::Aesthetic::B_FLOWER(true));
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

void CCommandProcessor::ConChatVoucher(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	if(pResult->NumArguments() != 1)
	{
		pGS->Chat(ClientID, "Use: /voucher <voucher>");
		return;
	}

	char aVoucher[32];
	str_copy(aVoucher, pResult->GetString(0), sizeof(aVoucher));
	pGS->Core()->AccountManager()->UseVoucher(ClientID, aVoucher);
}

void CCommandProcessor::ConChatTutorial(IConsole::IResult* pResult, void* pUser)
{
	const int ClientID = pResult->GetClientID();
	CGS* pGS = GetCommandResultGameServer(ClientID, pUser);

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	if(pGS->IsPlayerEqualWorld(ClientID, TUTORIAL_WORLD_ID))
	{
		pGS->Chat(ClientID, "You're already taking a training challenge!");
		return;
	}

	pGS->Chat(ClientID, "You have begun the training challenge!");
	pPlayer->ChangeWorld(TUTORIAL_WORLD_ID);
}


/************************************************************************/
/*  Command system                                                      */
/************************************************************************/

void CCommandProcessor::ChatCmd(const char* pMessage, CPlayer* pPlayer)
{
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
			GS()->Chat(ClientID, "Use: /{} {}", pCommand->m_pName, aArgsDesc);
		}
		return;
	}

	GS()->Chat(ClientID, "Command {} not found!", pMessage);
}

// Function to add a new command to the command processor
void CCommandProcessor::AddCommand(const char* pName, const char* pParams, IConsole::FCommandCallback pfnFunc, void* pUser, const char* pHelp)
{
	// Register the command in the console
	GS()->Console()->Register(pName, pParams, CFGFLAG_CHAT, pfnFunc, pUser, pHelp);

	// Add the command to the command manager
	m_CommandManager.AddCommand(pName, pHelp, pParams, pfnFunc, pUser);
}