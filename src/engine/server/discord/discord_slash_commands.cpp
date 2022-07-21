/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifdef CONF_DISCORD
#include "discord_main.h"

#include "discord_slash_commands.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

std::vector < DiscordCommands::Command > DiscordCommands::ms_aCommands;
std::map < std::string, int > DiscordCommands::ms_HelpCmdPage;

void DiscordCommands::InitCommands(DiscordJob* pDiscord)
{
	std::vector<SleepyDiscord::AppCommand> aDiscordCmd(pDiscord->getGlobalAppCommands(g_Config.m_SvDiscordApplicationID));
	auto CommandRegisteredID = [&aDiscordCmd](const char* pCmd) -> std::string
	{
		const auto pItem = std::find_if(aDiscordCmd.begin(), aDiscordCmd.end(), [pCmd](const SleepyDiscord::AppCommand& pDiscordCmd) { return pDiscordCmd.name == pCmd; });
		return pItem != aDiscordCmd.end() ? pItem->ID : "\0";
	};

	#define REG_DCMD(argsnum, name, argsformat, desc, callback, flags) \
		RegisterCommand<argsnum>(pDiscord, CommandRegisteredID(name), name, argsformat, desc, callback, flags)

	// commands important
	REG_DCMD(0, "help", "", "get help on commands.", CmdHelp, CMD_IMPORTANT);
	REG_DCMD(0, "connect", "", "help for connect your discord to account in game.", CmdConnect, CMD_IMPORTANT);
	REG_DCMD(0, "websites", "", "get links official/interested websites.", CmdWebsites, CMD_IMPORTANT);

	// commands game server
	REG_DCMD(0, "online", "", "show a list of players on the server.", CmdOnline, CMD_GAME);
	REG_DCMD(1, "stats", "s[nick]", "searching for players and displaying their personal MRPG cards.", CmdStats, CMD_GAME);
	REG_DCMD(0, "ranking", "", "show the ranking of players by level.", CmdRanking, CMD_GAME);
	REG_DCMD(0, "goldranking", "", "show the ranking of players by gold.", CmdRanking, CMD_GAME);

	// commands fun
	REG_DCMD(1, "avatar", "u[user]", "show user avatars.", CmdAvatar, CMD_FUN);

	#undef REG_DCMD
}

/************************************************************************/
/*  Important commands                                                  */
/************************************************************************/
void DiscordCommands::CmdHelp(SleepyDiscord::Interaction* pInteraction, DiscordJob *pDiscord, bool ResponseUpdate)
{
	SleepyDiscord::Interaction::Response response;
	response.type = ResponseUpdate ? SleepyDiscord::Interaction::Response::Type::UpdateMessage : SleepyDiscord::Interaction::Response::Type::ChannelMessageWithSource;

	const int MaxPage = 2;
	const std::string MessageID = pInteraction->message.ID;
	auto InformationCmdString = [](const int CmdTypeFlag, const bool Title)-> std::string
	{
		if(Title)
		{
			if(CmdTypeFlag == CMD_IMPORTANT)
				return "**Important commands:**";
			if(CmdTypeFlag == CMD_GAME)
				return "**Related game server commands:**";
			return "**Entertainment / Fun**";
		}

		std::string CmdInfo = "";
		for(auto& pCommand : ms_aCommands)
		{
			if(pCommand.m_TypeFlag & CmdTypeFlag)
				CmdInfo += "\n- **/" + std::string(pCommand.m_aCommand) + "** - " + std::string(pCommand.m_aCommandDesc);
		}
		return CmdInfo;
	};
	
	// embeds
	SleepyDiscord::Embed EmbedHelp;
	EmbedHelp.color = DC_DISCORD_INFO;
	switch(ms_HelpCmdPage[MessageID])
	{
		default:
		case 0:
		{
			EmbedHelp.title = InformationCmdString(CMD_IMPORTANT, true);
			EmbedHelp.description = InformationCmdString(CMD_IMPORTANT, false);
		} break;
		case 1:
		{
			EmbedHelp.title = InformationCmdString(CMD_GAME, true);
			EmbedHelp.description = InformationCmdString(CMD_GAME, false);
		} break;
		case 2:
		{
			EmbedHelp.title = InformationCmdString(CMD_FUN, true);
			EmbedHelp.description = InformationCmdString(CMD_FUN, false);
		} break;
	}
	response.data.embeds.push_back(EmbedHelp);

	// buttons
	auto pActionRow = std::make_shared<SleepyDiscord::ActionRow>();
	{
		pDiscord->CreateButton(pActionRow, "first-help-page", "<<", SleepyDiscord::ButtonStyle::Primary, ms_HelpCmdPage[MessageID] <= 0,
			[](DiscordJob* pDiscordCallback, SleepyDiscord::Interaction* pInteractionCallback)
			{
				ms_HelpCmdPage[pInteractionCallback->message.ID] = 0;
				CmdHelp(pInteractionCallback, pDiscordCallback, true);
			});
		
		pDiscord->CreateButton(pActionRow, "prev-help-page", "< Previous page", SleepyDiscord::ButtonStyle::Primary, ms_HelpCmdPage[MessageID] <= 0,
			[](DiscordJob* pDiscordCallback, SleepyDiscord::Interaction* pInteractionCallback)
			{
				ms_HelpCmdPage[pInteractionCallback->message.ID]--;
				CmdHelp(pInteractionCallback, pDiscordCallback, true);
			});

		pDiscord->CreateButton(pActionRow, "invisible", std::to_string(ms_HelpCmdPage[MessageID] + 1).c_str(), SleepyDiscord::ButtonStyle::Secondary, true);
		pDiscord->CreateButton(pActionRow, "next-help-page", "Next page >", SleepyDiscord::ButtonStyle::Primary, ms_HelpCmdPage[MessageID] >= MaxPage,
			[](DiscordJob* pDiscordCallback, SleepyDiscord::Interaction* pInteractionCallback)
			{
				ms_HelpCmdPage[pInteractionCallback->message.ID]++;
				CmdHelp(pInteractionCallback, pDiscordCallback, true);
			});

		pDiscord->CreateButton(pActionRow, "last-help-page", ">>", SleepyDiscord::ButtonStyle::Primary, ms_HelpCmdPage[MessageID] >= MaxPage,
			[MaxPage](DiscordJob* pDiscordCallback, SleepyDiscord::Interaction* pInteractionCallback)
		{
			ms_HelpCmdPage[pInteractionCallback->message.ID] = MaxPage;
			CmdHelp(pInteractionCallback, pDiscordCallback, true);
		});
	}
	if(!ResponseUpdate)
		ms_HelpCmdPage[MessageID] = 0;

	response.data.components.push_back(pActionRow);
	pDiscord->createInteractionResponse(pInteraction, pInteraction->token, response);
}

void DiscordCommands::CmdConnect(SleepyDiscord::Interaction* pInteraction, DiscordJob *pDiscord, bool ResponseUpdate)
{
	SleepyDiscord::Interaction::Response response;
	response.type = SleepyDiscord::Interaction::Response::Type::ChannelMessageWithSource;
	
	const CSqlString<64> DiscordID = sqlstr::CSqlString<64>(std::string(pInteraction->member.ID).c_str());
	ResultPtr pRes = SJK.SD("Nick", "tw_accounts_data", "WHERE DiscordID = '%s'", DiscordID.cstr());
	if(!pRes->rowsCount())
	{
		SleepyDiscord::Embed EmbedConnectInfo;
		EmbedConnectInfo.title = "Information on how to connect your account";
		EmbedConnectInfo.description = "_**Warning:**_"
			"\nDo not connect other people's discord accounts to your in-game account. This is similar to hacking your account in the game."
			"\n"
			"\n_**How to connect:**_"
			"\nYou need to enter in the game the command \"__/discord_connect <did>__\"."
			"\nYour personal Discord ID: " + std::string(pInteraction->member.ID);
		EmbedConnectInfo.color = DC_DISCORD_INFO;
		response.data.embeds.push_back(EmbedConnectInfo);

		SleepyDiscord::Embed EmbedWarning;
		EmbedWarning.description = "Your account is not connected to the game account.\nSee the information in /connect, to connect your account to Discord.";
		EmbedWarning.color = DC_DISCORD_WARNING;
		response.data.embeds.push_back(EmbedWarning);
		
		pDiscord->createInteractionResponse(pInteraction, pInteraction->token, response);
		return;
	}

	if(pRes->next())
	{

		const std::string Nick(pRes->getString("Nick").c_str());
		SleepyDiscord::Embed EmbedSuccess;
		EmbedSuccess.description = "Your account is connected to the game nickname [" + Nick + "].";
		EmbedSuccess.color = DC_DISCORD_SUCCESS;
		response.data.embeds.push_back(EmbedSuccess);
		
		pDiscord->createInteractionResponse(pInteraction, pInteraction->token, response);
	}
}

void DiscordCommands::CmdWebsites(SleepyDiscord::Interaction* pInteraction, DiscordJob* pDiscord, bool ResponseUpdate)
{
	SleepyDiscord::Interaction::Response response;
	response.type = SleepyDiscord::Interaction::Response::Type::ChannelMessageWithSource;

	SleepyDiscord::Embed EmbedWebsites;
	EmbedWebsites.title = "Interesting Links!";
	EmbedWebsites.description = "[Google docs](https://docs.google.com/spreadsheets/d/e/2PACX-1vQaxF94LaZ33qbkz_GrJO_5yRYuuvcRjsQH2ebViUcSnB77RsssWG08_QWj8WNef5NpR5G-xjVAXo_u/pubhtml) - Useful Wiki by **angrykiwi.\n";
	EmbedWebsites.color = DC_DISCORD_INFO;
	response.data.embeds.push_back(EmbedWebsites);

	auto pActionRow = std::make_shared<SleepyDiscord::ActionRow>();
	pDiscord->CreateButton(pActionRow, "https://mrpg.teeworlds.dev/", "Website", SleepyDiscord::ButtonStyle::Link);
	pDiscord->CreateButton(pActionRow, "https://mrpg.teeworlds.dev/wiki", "Wikipedia", SleepyDiscord::ButtonStyle::Link);
	pDiscord->CreateButton(pActionRow, "https://www.youtube.com/channel/UCPnAUuCtEf8Am7NXlmFBlwQ", "Youtube", SleepyDiscord::ButtonStyle::Link);
	response.data.components.push_back(pActionRow);
	
	pDiscord->createInteractionResponse(pInteraction, pInteraction->token, response);
}


/************************************************************************/
/*  Game server commands                                                */
/************************************************************************/
void DiscordCommands::CmdOnline(SleepyDiscord::Interaction* pInteraction, DiscordJob *pDiscord, bool ResponseUpdate)
{
	SleepyDiscord::Interaction::Response response;
	response.type = SleepyDiscord::Interaction::Response::Type::ChannelMessageWithSource;
	
	std::string Onlines = "";
	CGS* pGS = (CGS*)pDiscord->Server()->GameServer(MAIN_WORLD_ID);
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = pGS->GetPlayer(i);
		if(pPlayer)
		{
			std::string Nickname(pDiscord->Server()->ClientName(i));
			std::string Team(pPlayer->GetTeam() != TEAM_SPECTATORS ? "Ingame" : "Spectator");
			Onlines += std::string("**" + Nickname + "** - " + Team + "\n");
		}
	}

	SleepyDiscord::Embed EmbedOnlines;
	EmbedOnlines.title = "Online Server";
	EmbedOnlines.description = Onlines.empty() ? "Server is empty" : Onlines;
	EmbedOnlines.color = DC_DISCORD_INFO;
	response.data.embeds.push_back(EmbedOnlines);
	
	pDiscord->createInteractionResponse(pInteraction, pInteraction->token, response);
}

void DiscordCommands::CmdStats(SleepyDiscord::Interaction* pInteraction, DiscordJob *pDiscord, bool ResponseUpdate)
{
	SleepyDiscord::Interaction::Response response;
	response.type = SleepyDiscord::Interaction::Response::Type::ChannelMessageWithSource;
	
	const char* pSearchNick = pInteraction->data.options.at(0).value.GetString();
	const bool Found = pDiscord->SendGenerateMessage(pInteraction->user, pInteraction->channelID, "Discord MRPG Card", pSearchNick);
	if(!Found)
	{
		SleepyDiscord::Embed EmbedWarning;
		EmbedWarning.description = "Accounts containing [" + std::string(pSearchNick) + "] among nicknames were not found on the server.";
		EmbedWarning.color = DC_DISCORD_WARNING;
		response.data.embeds.push_back(EmbedWarning);
		
		pDiscord->createInteractionResponse(pInteraction, pInteraction->token, response);
		return;
	}
	
	response.data.content = "The request is formulat";
	pDiscord->createInteractionResponse(pInteraction, pInteraction->token, response);
}

void DiscordCommands::CmdRanking(SleepyDiscord::Interaction* pInteraction, DiscordJob *pDiscord, bool ResponseUpdate)
{
	SleepyDiscord::Interaction::Response response;
	response.type = SleepyDiscord::Interaction::Response::Type::ChannelMessageWithSource;
	
	std::string Names = "";
	std::string Levels = "";
	std::string GoldValues = "";
	const bool GoldRanking = pInteraction->data.name == "goldranking";
	SleepyDiscord::Embed EmbedRanking;
	EmbedRanking.title = GoldRanking ? "Ranking by Gold" : "Ranking by Level";
	EmbedRanking.color = DC_DISCORD_INFO;

	ResultPtr pRes = SJK.SD("a.ID, ad.Nick AS `Nick`, ad.Level AS `Level`, g.Value AS `Gold`", "tw_accounts a", "JOIN tw_accounts_data ad ON a.ID = ad.ID LEFT JOIN tw_accounts_items g ON a.ID = g.UserID AND g.ItemID = 1 ORDER BY %s LIMIT 10", (GoldRanking ? "g.Value DESC, ad.Level DESC" : "ad.Level DESC, g.Value DESC"));
	while(pRes->next())
	{
		Names += std::to_string((int)pRes->getRow()) + ". **" + pRes->getString("Nick").c_str() + "**\n";
		Levels += std::to_string(pRes->getInt("Level")) + "\n";
		GoldValues += std::to_string(pRes->getInt("Gold")) + "\n";
	}

	EmbedRanking.fields.emplace_back("Name", Names, true);
	EmbedRanking.fields.emplace_back("Level", Levels, true);
	EmbedRanking.fields.emplace_back("Gold", GoldValues, true);

	response.data.embeds.push_back(EmbedRanking);
	pDiscord->createInteractionResponse(pInteraction, pInteraction->token, response);
}


/************************************************************************/
/*  Fun commands                                                        */
/************************************************************************/
void DiscordCommands::CmdAvatar(SleepyDiscord::Interaction* pInteraction, DiscordJob *pDiscord, bool ResponseUpdate)
{
	SleepyDiscord::Interaction::Response response;
	response.type = SleepyDiscord::Interaction::Response::Type::ChannelMessageWithSource;

	const char* pSearchNick = pInteraction->data.options.at(0).value.GetString();
	SleepyDiscord::User User = pDiscord->getUser(pSearchNick);
	
	response.data.content = User.avatarUrl();
	pDiscord->createInteractionResponse(pInteraction, pInteraction->token, response);
}

/************************************************************************/
/*  Admin commands                                                      */
/************************************************************************/



/************************************************************************/
/*  Engine commands                                                     */
/************************************************************************/
template<size_t ArrSize>
void DiscordCommands::RegisterCommand(DiscordJob* pDiscord, std::string CommandID, const char* pName, const char* pArgs, const char* pDesc, CommandCallback pCallback, int FlagType)
{
	Command NewCommand;
	str_copy(NewCommand.m_aCommand, pName, sizeof(NewCommand.m_aCommand));
	str_copy(NewCommand.m_aCommandDesc, pDesc, sizeof(NewCommand.m_aCommandDesc));
	NewCommand.m_TypeFlag = FlagType;
	NewCommand.m_pCallback = pCallback;
	ms_aCommands.push_back(NewCommand);

	const bool RequiresUpdate = (bool)(CommandID != "\0");
	if(ArrSize > 0)
	{
		int StringPos = 0;
		size_t HandledOption = 0;
		bool RequiredOption = true;
		std::array<SleepyDiscord::AppCommand::Option, ArrSize> Option;
		while(*pArgs)
		{
			if(HandledOption >= ArrSize)
				break;

			bool ValidArgs = false;
			if(*pArgs == '?')
			{
				RequiredOption = false;
			}
			else if(*pArgs == 'u')
			{
				Option.at(HandledOption).description = std::to_string(HandledOption + 1) + ". person that you want.";
				Option.at(HandledOption).type = SleepyDiscord::AppCommand::Option::Type::USER;
			}
			else if(*pArgs == 'b')
			{
				Option.at(HandledOption).description = std::to_string(HandledOption + 1) + ". select 1 - yes / 0 - no.";
				Option.at(HandledOption).type = SleepyDiscord::AppCommand::Option::Type::BOOLEAN;
			}
			else if(*pArgs == 'i')
			{
				Option.at(HandledOption).description = std::to_string(HandledOption + 1) + ". enter a number.";
				Option.at(HandledOption).type = SleepyDiscord::AppCommand::Option::Type::INTEGER;
			}
			else if(*pArgs == 'c')
			{
				Option.at(HandledOption).description = std::to_string(HandledOption + 1) + ". select a channel.";
				Option.at(HandledOption).type = SleepyDiscord::AppCommand::Option::Type::CHANNEL;
			}
			else if(*pArgs == 'r')
			{
				Option.at(HandledOption).description = std::to_string(HandledOption + 1) + ". select a role.";
				Option.at(HandledOption).type = SleepyDiscord::AppCommand::Option::Type::ROLE;
			}
			else if(*pArgs == 's')
			{
				Option.at(HandledOption).description = std::to_string(HandledOption + 1) + ". enter text.";
				Option.at(HandledOption).type = SleepyDiscord::AppCommand::Option::Type::STRING;
			}
			else if(*pArgs == '[')
			{
				char aName[64];
				str_format(aName, sizeof(aName), "%.*s", str_span(&pArgs[StringPos], "]"), &pArgs[StringPos]);
				Option.at(HandledOption).name = aName;
				pArgs += str_span(&pArgs[StringPos - 1], "]");
				ValidArgs = true;
			}

			if(ValidArgs)
			{
				if(RequiredOption)
				{
					Option.at(HandledOption).isRequired = RequiredOption;
					RequiredOption = false;
				}
				HandledOption++;
			}
			StringPos++;
			pArgs++;
		}

		dbg_msg("discord_command", "%s %s is performed", RequiresUpdate ? "updating" : "registration", pName);
		if(RequiresUpdate)
			pDiscord->editGlobalAppCommand(g_Config.m_SvDiscordApplicationID, CommandID, pName, pDesc, Option);
		else
			pDiscord->createGlobalAppCommand(g_Config.m_SvDiscordApplicationID, pName, pDesc, Option);
		sleep_pause(500); // pause for disable many requests to discord api
	}
	else
	{
		dbg_msg("discord_command", "%s %s is performed", RequiresUpdate ? "updating" : "registration", pName);
		if(RequiresUpdate)
			pDiscord->editGlobalAppCommand(g_Config.m_SvDiscordApplicationID, CommandID, pName, pDesc);
		else
			pDiscord->createGlobalAppCommand(g_Config.m_SvDiscordApplicationID, pName, pDesc);
		sleep_pause(500); // pause for disable many requests to discord api
	}
}

bool DiscordCommands::ExecuteCommand(DiscordJob* pDiscord, SleepyDiscord::Interaction* pInteraction)
{
	for(auto& pCommand : ms_aCommands)
	{
		if(pInteraction->data.name == pCommand.m_aCommand)
		{
			pCommand.m_pCallback(pInteraction, pDiscord, false);
			return true;
		}
	}
	return false;
}
#endif