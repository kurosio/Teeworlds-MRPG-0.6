#ifndef GAME_SERVER_CORE_RCON_PROCESSOR_H
#define GAME_SERVER_CORE_RCON_PROCESSOR_H

class IConsole;

class RconProcessor
{
	// rcon commands
	static void ConSetWorldTime(IConsole::IResult* pResult, void* pUserData);
	static void ConItemList(IConsole::IResult* pResult, void* pUserData);
	static void ConGiveItem(IConsole::IResult* pResult, void* pUserData);
	static void ConRemItem(IConsole::IResult* pResult, void* pUserData);
	static void ConDisbandGuild(IConsole::IResult* pResult, void* pUserData);
	static void ConSay(IConsole::IResult* pResult, void* pUserData);
	static void ConAddCharacter(IConsole::IResult* pResult, void* pUserData);
	static void ConSyncLinesForTranslate(IConsole::IResult* pResult, void* pUserData);
	static void ConListAfk(IConsole::IResult* pResult, void* pUserData);
	static void ConCheckAfk(IConsole::IResult* pResult, void* pUserData);
	static void ConBanAcc(IConsole::IResult* pResult, void* pUserData);
	static void ConUnBanAcc(IConsole::IResult* pResult, void* pUserData);
	static void ConBansAcc(IConsole::IResult* pResult, void* pUserData);

	static void ConTeleportByMouse(IConsole::IResult* pResult, void* pUserData);
	static void ConTeleportByPos(IConsole::IResult* pResult, void* pUserData);
	static void ConTeleportByClient(IConsole::IResult* pResult, void* pUserData);
	static void ConPosition(IConsole::IResult* pResult, void* pUserData);

	static void ConJail(IConsole::IResult* pResult, void* pUserData);
	static void ConUnjail(IConsole::IResult* pResult, void* pUserData);

	static void ConQuest(IConsole::IResult* pResult, void* pUserData);

	// chain's
	static void ConchainSpecialMotdupdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);
	static void ConchainGameinfoUpdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);

public:
	static void Init(IConsole* pConsole, IServer* pServer);
};

#endif
