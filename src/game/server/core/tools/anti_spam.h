#ifndef GAME_SERVER_CORE_TOOLS_ANTI_SPAM_H
#define GAME_SERVER_CORE_TOOLS_ANTI_SPAM_H

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <mutex>
#include <string>
#include <vector>

class CGS;
class CPlayer;
class IServer;

class CAntiSpam
{
	struct SBlockedWord
	{
		std::string m_Original;
		std::string m_Normalized;
	};

	struct SClientState
	{
		int m_JoinTick {};
		int m_LastMuteNotifyTick {};
		int m_LastChatTick {};
		int m_Warnings {};
		bool m_JoinMessagePending {};
		bool m_HasJoined {};
		char m_aLastMessage[256] {};
	};

	static std::vector<SBlockedWord> ms_vBlockedWords;
	static SClientState ms_aClients[MAX_CLIENTS];
	static std::mutex ms_Mutex;
	static bool ms_DatabaseInitialized;
	static int ms_PendingJoinMessages;

public:
	static void Init(IConsole* pConsole, IServer* pServer);
	static void InitDatabase();

	static void OnClientConnected(int ClientID);
	static void OnClientEnter(CGS* pGS, int ClientID, bool FirstEnter);
	static bool OnClientDrop(int ClientID);
	static void OnTick(CGS* pGS);

	static bool OnClientChat(CGS* pGS, CPlayer* pPlayer, const char* pMessage);

	static bool AddBlockedWord(const char* pText);
	static bool RemoveBlockedWord(const char* pText);
	static int BlockedWordsCount();

private:
	static void ClearPendingJoinMessage(int ClientID);
	static void SendJoinMessage(CGS* pGS, int ClientID);

	static void AddBlockedWordInternal(const std::string& Original);
	static std::string Normalize(const char* pText);
	static std::string FindBlockedWord(const std::string& Normalized);

	static void ConAddBlockedWord(IConsole::IResult* pResult, void* pUserData);
	static void ConRemoveBlockedWord(IConsole::IResult* pResult, void* pUserData);
	static void ConListBlockedWords(IConsole::IResult* pResult, void* pUserData);
};

#endif
