#ifndef GAME_SERVER_CORE_TOOLS_SAFE_CHAT_H
#define GAME_SERVER_CORE_TOOLS_SAFE_CHAT_H

#include <base/system.h>
#include <engine/storage.h>

class CSafeChat
{
	static constexpr const char* ms_pFilename = "chat_filter.cfg";

public:
	static bool ContainsBlockedLink(const char* pMessage)
	{
		char aList[512];
		str_copy(aList, g_Config.m_SvChatBlockedStrings, sizeof(aList));

		char* pToken = strtok(aList, ",");
		while(pToken)
		{
			if(str_find_nocase(pMessage, pToken) != nullptr)
				return true;

			pToken = strtok(nullptr, ",");
		}
		return false;
	}

	static bool IsJoinCooldownActive(int* pPlayerTickArray, int CurrentTick)
	{
		return pPlayerTickArray[JoinProtection] > CurrentTick;
	}

	// call once at server startup to restore saved blocked strings list
	static void Load(class IStorageEngine* pStorage)
	{
		if(const auto File = pStorage->OpenFile(ms_pFilename, IOFLAG_READ, IStorageEngine::TYPE_ABSOLUTE))
		{
			char* pResult = io_read_all_str(File);
			io_close(File);
			if(pResult)
			{
				str_copy(g_Config.m_SvChatBlockedStrings, pResult, sizeof(g_Config.m_SvChatBlockedStrings));
				free(pResult);
			}
		}
	}

	// call every time the blocked strings list changes to persist it to disk
	static void Save(class IStorageEngine* pStorage)
	{
		if(const auto File = pStorage->OpenFile(ms_pFilename, IOFLAG_WRITE, IStorageEngine::TYPE_ABSOLUTE))
		{
			io_write(File, g_Config.m_SvChatBlockedStrings, str_length(g_Config.m_SvChatBlockedStrings));
			io_close(File);
		}
	}
};

#endif