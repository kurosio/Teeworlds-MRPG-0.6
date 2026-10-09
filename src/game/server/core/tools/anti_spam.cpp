#include "anti_spam.h"

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <algorithm>
#include <cctype>

std::vector<CAntiSpam::SBlockedWord> CAntiSpam::ms_vBlockedWords;
CAntiSpam::SClientState CAntiSpam::ms_aClients[MAX_CLIENTS];
std::mutex CAntiSpam::ms_Mutex;
bool CAntiSpam::ms_DatabaseInitialized = false;
int CAntiSpam::ms_PendingJoinMessages = 0;

namespace
{
	const char* const gs_apDefaultBlockedWords[] = {
		"t.me/",
		"telegram.me/",
		"telegram.org",
		"discord.gg/",
		"discord.com/invite",
		"discordapp.com/invite"
	};

	CGS* GetCommandGameServer(void* pUserData)
	{
		return (CGS*)((IServer*)pUserData)->GameServer(INITIALIZER_WORLD_ID);
	}
}

void CAntiSpam::Init(IConsole* pConsole, IServer* pServer)
{
	pConsole->Register("add_blocked_word", "r[word]", CFGFLAG_SERVER, ConAddBlockedWord, pServer, "Add a word to the chat blacklist");
	pConsole->Register("remove_blocked_word", "r[word]", CFGFLAG_SERVER, ConRemoveBlockedWord, pServer, "Remove a blocked word by value or by 1-based index");
	pConsole->Register("list_blocked_words", "?i[page]", CFGFLAG_SERVER, ConListBlockedWords, pServer, "List blocked words");

	dbg_msg("anti_spam", "loaded: join mute %ds, chat rate limit %ds, kick after %d blocked words, admin bypass %d",
		g_Config.m_SvJoinMuteTime, g_Config.m_SvChatRateLimit, g_Config.m_SvBlockedWordsMaxWarnings, (int)g_Config.m_SvBlockedWordsAdminBypass);
}

void CAntiSpam::InitDatabase()
{
	if(ms_DatabaseInitialized)
		return;

	ms_DatabaseInitialized = true;

	Database->Execute<DB::OTHER>([](bool)
	{
		Database->Prepare<DB::SELECT>("String", "tw_blocked_strings")->AtExecute([](ResultPtr pResult)
		{
			std::lock_guard<std::mutex> Guard(ms_Mutex);
			ms_vBlockedWords.clear();
			if(pResult)
			{
				while(pResult->next())
				{
					const std::string Original = pResult->getString("String");
					const std::string Normalized = Normalize(Original.c_str());
					if(!Normalized.empty())
						ms_vBlockedWords.push_back({ Original, Normalized });
				}
			}

			if(ms_vBlockedWords.empty())
			{
				for(const char* pDefault : gs_apDefaultBlockedWords)
					AddBlockedWordInternal(pDefault);
			}

			dbg_msg("anti_spam", "blacklist ready: %d blocked words", (int)ms_vBlockedWords.size());
		});
	}, "CREATE TABLE IF NOT EXISTS tw_blocked_strings (ID INT NOT NULL AUTO_INCREMENT, String VARCHAR(190) NOT NULL, PRIMARY KEY (ID), UNIQUE KEY uniq_blocked_string (String)) DEFAULT CHARSET=utf8mb4");
}

void CAntiSpam::OnClientConnected(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;

	ClearPendingJoinMessage(ClientID);
	ms_aClients[ClientID] = {};
}

void CAntiSpam::OnClientEnter(CGS* pGS, int ClientID, bool FirstEnter)
{
	if(!pGS || ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;

	if(!FirstEnter || ms_aClients[ClientID].m_HasJoined)
		return;

	ClearPendingJoinMessage(ClientID);
	ms_aClients[ClientID] = {};
	ms_aClients[ClientID].m_HasJoined = true;

	SClientState& State = ms_aClients[ClientID];
	State.m_JoinTick = pGS->Server()->Tick();

	if(g_Config.m_SvJoinMuteTime > 0 && !pGS->Server()->IsAuthed(ClientID))
	{
		State.m_JoinMessagePending = true;
		ms_PendingJoinMessages++;
		return;
	}

	SendJoinMessage(pGS, ClientID);
}

bool CAntiSpam::OnClientDrop(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;

	const bool bJoinMessagePending = ms_aClients[ClientID].m_JoinMessagePending;
	ClearPendingJoinMessage(ClientID);
	ms_aClients[ClientID] = {};
	return bJoinMessagePending;
}

void CAntiSpam::OnTick(CGS* pGS)
{
	if(!pGS || g_Config.m_SvJoinMuteTime <= 0 || ms_PendingJoinMessages <= 0)
		return;

	const int Tick = pGS->Server()->Tick();
	const int MuteTicks = g_Config.m_SvJoinMuteTime * pGS->Server()->TickSpeed();

	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		SClientState& State = ms_aClients[ClientID];
		if(!State.m_JoinMessagePending)
			continue;

		if(!pGS->Server()->ClientIngame(ClientID))
		{
			ClearPendingJoinMessage(ClientID);
			continue;
		}

		if(pGS->Server()->GetClientWorldID(ClientID) != pGS->GetWorldID())
			continue;

		if(Tick < State.m_JoinTick + MuteTicks)
			continue;

		ClearPendingJoinMessage(ClientID);
		SendJoinMessage(pGS, ClientID);
	}
}

bool CAntiSpam::OnClientChat(CGS* pGS, CPlayer* pPlayer, const char* pMessage)
{
	if(!pGS || !pPlayer || !pMessage)
		return false;

	const int ClientID = pPlayer->GetCID();
	const int Tick = pGS->Server()->Tick();
	const int TickSpeed = pGS->Server()->TickSpeed();
	SClientState& State = ms_aClients[ClientID];
	const bool bBypass = pGS->Server()->IsAuthed(ClientID);

	if(pMessage[0] != '/')
	{
		if(!bBypass && g_Config.m_SvJoinMuteTime > 0 && State.m_JoinTick > 0)
		{
			const int MuteEndTick = State.m_JoinTick + g_Config.m_SvJoinMuteTime * TickSpeed;
			if(Tick < MuteEndTick)
			{
				if(Tick >= State.m_LastMuteNotifyTick)
				{
					State.m_LastMuteNotifyTick = Tick + TickSpeed;
					const int RemainingTicks = MuteEndTick - Tick;
					const int RemainingSeconds = maximum(1, (RemainingTicks + TickSpeed - 1) / TickSpeed);
					pGS->Chat(ClientID, "You are not permitted to talk for the next {} seconds.", RemainingSeconds);
				}
				return true;
			}
		}

		if(g_Config.m_SvChatRateLimit > 0 && Tick < State.m_LastChatTick + g_Config.m_SvChatRateLimit * TickSpeed)
			return true;
		State.m_LastChatTick = Tick;

		if(State.m_aLastMessage[0] != '\0' && str_comp(State.m_aLastMessage, pMessage) == 0)
			return true;
		str_copy(State.m_aLastMessage, pMessage, sizeof(State.m_aLastMessage));

		const std::string BlockedWord = FindBlockedWord(Normalize(pMessage));
		if(!BlockedWord.empty())
		{
			if(bBypass && g_Config.m_SvBlockedWordsAdminBypass)
			{
				pGS->Chat(ClientID, "Your message contains a blocked word ({}), it was sent because you are authenticated.", BlockedWord.c_str());
				return false;
			}

			State.m_Warnings++;
			if(g_Config.m_SvBlockedWordsMaxWarnings > 0 && State.m_Warnings >= g_Config.m_SvBlockedWordsMaxWarnings)
			{
				pGS->Server()->Kick(ClientID, "Blocked words in chat");
				return true;
			}

			pGS->Chat(ClientID, "Your message contains a blocked word ({}) and was not sent.", BlockedWord.c_str());
			return true;
		}
	}

	return false;
}

bool CAntiSpam::AddBlockedWord(const char* pText)
{
	if(!pText || pText[0] == '\0')
		return false;

	std::lock_guard<std::mutex> Guard(ms_Mutex);
	const std::string Normalized = Normalize(pText);
	if(Normalized.empty())
		return false;

	for(const auto& Entry : ms_vBlockedWords)
	{
		if(Entry.m_Normalized == Normalized)
			return false;
	}

	AddBlockedWordInternal(pText);
	return true;
}

bool CAntiSpam::RemoveBlockedWord(const char* pText)
{
	if(!pText || pText[0] == '\0')
		return false;

	std::lock_guard<std::mutex> Guard(ms_Mutex);
	if(ms_vBlockedWords.empty())
		return false;

	const bool bNumeric = std::all_of(pText, pText + str_length(pText), [](char c) { return c >= '0' && c <= '9'; });

	int Index = -1;
	if(bNumeric)
	{
		Index = str_toint(pText) - 1;
		if(Index < 0 || Index >= (int)ms_vBlockedWords.size())
			return false;
	}
	else
	{
		const std::string Normalized = Normalize(pText);
		for(int i = 0; i < (int)ms_vBlockedWords.size(); i++)
		{
			if(ms_vBlockedWords[i].m_Normalized == Normalized)
			{
				Index = i;
				break;
			}
		}
	}

	if(Index < 0)
		return false;

	const std::string Original = ms_vBlockedWords[Index].m_Original;
	ms_vBlockedWords.erase(ms_vBlockedWords.begin() + Index);
	Database->Execute<DB::REMOVE>("tw_blocked_strings", "WHERE String = '{}'", sqlstr::CSqlString<190>(Original.c_str()).cstr());
	return true;
}

int CAntiSpam::BlockedWordsCount()
{
	std::lock_guard<std::mutex> Guard(ms_Mutex);
	return (int)ms_vBlockedWords.size();
}

void CAntiSpam::AddBlockedWordInternal(const std::string& Original)
{
	const std::string Normalized = Normalize(Original.c_str());
	if(Normalized.empty())
		return;

	for(const auto& Entry : ms_vBlockedWords)
	{
		if(Entry.m_Normalized == Normalized)
			return;
	}

	ms_vBlockedWords.push_back({ Original, Normalized });
	Database->Execute<DB::INSERT>("tw_blocked_strings", "(String) VALUES ('{}')", sqlstr::CSqlString<190>(Original.c_str()).cstr());
}

std::string CAntiSpam::Normalize(const char* pText)
{
	std::string Result;
	if(!pText)
		return Result;

	Result.reserve(str_length(pText));

	for(const char* p = pText; *p != '\0';)
	{
		if(*p == '^' && p[1] != '\0')
		{
			p += 2;
			continue;
		}

		unsigned char Character = (unsigned char)std::tolower((unsigned char)*p);
		switch(Character)
		{
			case '0': Character = 'o'; break;
			case '1': Character = 'i'; break;
			case '3': Character = 'e'; break;
			case '4': Character = 'a'; break;
			case '5': Character = 's'; break;
			case '7': Character = 't'; break;
			case '@': Character = 'a'; break;
			case '$': Character = 's'; break;
			default: break;
		}

		Result.push_back((char)Character);
		p++;
	}

	return Result;
}

std::string CAntiSpam::FindBlockedWord(const std::string& Normalized)
{
	if(Normalized.empty())
		return std::string();

	std::lock_guard<std::mutex> Guard(ms_Mutex);
	for(const auto& Entry : ms_vBlockedWords)
	{
		if(!Entry.m_Normalized.empty() && Normalized.find(Entry.m_Normalized) != std::string::npos)
			return Entry.m_Original;
	}

	return std::string();
}

void CAntiSpam::ClearPendingJoinMessage(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !ms_aClients[ClientID].m_JoinMessagePending)
		return;

	ms_aClients[ClientID].m_JoinMessagePending = false;
	ms_PendingJoinMessages = maximum(0, ms_PendingJoinMessages - 1);
}

void CAntiSpam::SendJoinMessage(CGS* pGS, int ClientID)
{
	pGS->Chat(-1, "'{~}' entered and joined the {~}", pGS->Server()->ClientName(ClientID), g_Config.m_SvGamemodeName);
}

void CAntiSpam::ConAddBlockedWord(IConsole::IResult* pResult, void* pUserData)
{
	CGS* pGS = GetCommandGameServer(pUserData);
	const char* pWord = pResult->GetString(0);

	if(AddBlockedWord(pWord))
		pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "anti_spam", "Added blocked word '%s' (total: %d).", pWord, BlockedWordsCount());
	else
		pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "anti_spam", "Cannot add blocked word '%s' (empty or already present).", pWord);
}

void CAntiSpam::ConRemoveBlockedWord(IConsole::IResult* pResult, void* pUserData)
{
	CGS* pGS = GetCommandGameServer(pUserData);
	const char* pWord = pResult->GetString(0);

	if(RemoveBlockedWord(pWord))
		pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "anti_spam", "Removed blocked word '%s' (total: %d).", pWord, BlockedWordsCount());
	else
		pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "anti_spam", "Blocked word '%s' was not found. Use list_blocked_words.", pWord);
}

void CAntiSpam::ConListBlockedWords(IConsole::IResult* pResult, void* pUserData)
{
	CGS* pGS = GetCommandGameServer(pUserData);

	std::vector<std::string> aWords;
	{
		std::lock_guard<std::mutex> Guard(ms_Mutex);
		aWords.reserve(ms_vBlockedWords.size());
		for(const auto& Entry : ms_vBlockedWords)
			aWords.push_back(Entry.m_Original);
	}

	constexpr int PerPage = 10;
	const int Total = (int)aWords.size();
	const int MaxPage = maximum(1, (Total + PerPage - 1) / PerPage);
	const int Page = clamp(pResult->GetIntegerOr(0, 1), 1, MaxPage);

	pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "anti_spam", "Blocked words: %d total. Page %d/%d", Total, Page, MaxPage);

	const int Start = (Page - 1) * PerPage;
	const int End = minimum(Total, Start + PerPage);
	for(int i = Start; i < End; i++)
		pGS->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "anti_spam", "%d. %s", i + 1, aWords[i].c_str());
}
