/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifdef CONF_DISCORD
#ifndef ENGINE_DISCORD_COMMANDS_SERVER_H
#define ENGINE_DISCORD_COMMANDS_SERVER_H

enum
{
	CMD_INVISIBLE = 0,
	CMD_IMPORTANT = 1 << 0,
	CMD_GAME = 1 << 1,
	CMD_FUN = 1 << 2,
};

class DiscordCommands
{
	friend class DiscordJob;
	typedef void (*CommandCallback)(SleepyDiscord::Interaction*, class DiscordJob*, bool ResponseUpdate);

	/************************************************************************/
	/*  Important commands                                                  */
	/************************************************************************/
	static std::map<std::string, int> ms_HelpCmdPage;
	static void CmdHelp(SleepyDiscord::Interaction* , class DiscordJob*, bool ResponseUpdate);
	static void CmdConnect(SleepyDiscord::Interaction*, class DiscordJob*, bool ResponseUpdate);
	static void CmdWebsites(SleepyDiscord::Interaction*, class DiscordJob*, bool ResponseUpdate);


	/************************************************************************/
	/*  Game server commands                                                */
	/************************************************************************/
	static void CmdOnline(SleepyDiscord::Interaction*, class DiscordJob*, bool ResponseUpdate);
	static void CmdStats(SleepyDiscord::Interaction*, class DiscordJob*, bool ResponseUpdate);
	static void CmdRanking(SleepyDiscord::Interaction*, class DiscordJob*, bool ResponseUpdate);


	/************************************************************************/
	/*  Fun commands                                                        */
	/************************************************************************/
	static void CmdAvatar(SleepyDiscord::Interaction*, class DiscordJob*, bool ResponseUpdate);


	/************************************************************************/
	/*  Admin commands                                                      */
	/************************************************************************/


	/************************************************************************/
	/*  Engine commands                                                     */
	/************************************************************************/
public:
	struct Command
	{
		char m_aCommand[32];
		char m_aCommandDesc[256];
		int m_TypeFlag;
		CommandCallback m_pCallback;
	};

	static void InitCommands(class DiscordJob* pDiscord);
	static bool ExecuteCommand(class DiscordJob* pDiscord, SleepyDiscord::Interaction* pInteraction);

private:
	template< size_t ArrSize >
	static void RegisterCommand(class DiscordJob* pDiscord, std::string CommandID, const char* pName, const char* pArgs, const char* pDesc, CommandCallback pCallback, int FlagType);
	
	static std::vector < Command > ms_aCommands;
};

#endif
#endif