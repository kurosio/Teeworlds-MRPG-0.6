/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_BOT_DATA_H
#define GAME_SERVER_COMPONENT_BOT_DATA_H

/************************************************************************/
/*  Dialog struct                                                       */
/************************************************************************/
struct DialogData
{
	char m_aText[512];
	int m_Emote;
	bool m_RequestAction;
	int m_Flag;

	void LoadFlags();
};

/************************************************************************/
/*  Global data information bot                                         */
/************************************************************************/
struct DataBotInfo
{
	char m_aNameBot[MAX_NAME_ARRAY_SIZE];
	CTeeInfo m_TeeInfos;
	int m_aEquipSlot[MAX_EQUIPPED_SLOTS_BOTS];
	bool m_aVisibleActive[MAX_PLAYERS];

	static bool IsDataBotValid(int BotID)
	{
		return (ms_aDataBot.find(BotID) != ms_aDataBot.end());
	}
	static std::map<int, DataBotInfo> ms_aDataBot;
};

/************************************************************************/
/*  Global data npc bot                                                 */
/************************************************************************/
struct NpcBotInfo
{
	bool m_Static;
	vec2 m_Position;
	int m_Emote;
	int m_WorldID;
	int m_BotID;
	int m_Function;
	int m_GivesQuestID;
	std::vector<DialogData> m_aDialog;

	const char* GetName() const
	{
		return DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot;
	}

	static bool IsNpcBotValid(int MobID)
	{
		return ms_aNpcBot.find(MobID) != ms_aNpcBot.end() && DataBotInfo::IsDataBotValid(ms_aNpcBot[MobID].m_BotID);
	}
	static std::map<int, NpcBotInfo> ms_aNpcBot;
};

/************************************************************************/
/*  Global data quest bot                                               */
/************************************************************************/
struct QuestBotInfo
{
	char m_aGeneratedNickname[MAX_NAME_LENGTH];
	vec2 m_Position;
	int m_QuestID;
	int m_Step;
	int m_WorldID;
	int m_BotID;
	int m_SubBotID;

	int m_aItemSearch[2];
	int m_aItemSearchValue[2];

	int m_aItemGives[2];
	int m_aItemGivesValue[2];

	int m_aNeedMob[2];
	int m_aNeedMobValue[2];

	int m_InteractiveType;
	int m_InteractiveTemp;
	bool m_GenerateNick;
	std::vector<DialogData> m_aDialog;

	const char* GetName() const
	{
		return DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot;
	}

	static bool IsQuestBotValid(int MobID)
	{
		return ms_aQuestBot.find(MobID) != ms_aQuestBot.end() && DataBotInfo::IsDataBotValid(ms_aQuestBot[MobID].m_BotID);
	}
	static std::map<int, QuestBotInfo> ms_aQuestBot;
};

/************************************************************************/
/*  Global data mob bot                                                 */
/************************************************************************/
struct MobBotInfo
{
	bool m_Boss;
	int m_Power;
	int m_Spread;
	vec2 m_Position;
	int m_Level;
	int m_RespawnTick;
	int m_WorldID;
	char m_aEffect[16];
	char m_aBehavior[32];

	int m_aDropItem[MAX_DROPPED_FROM_MOBS];
	int m_aValueItem[MAX_DROPPED_FROM_MOBS];
	float m_aRandomItem[MAX_DROPPED_FROM_MOBS];
	int m_BotID;

	const char* GetName() const
	{
		return DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot;
	}

	static bool IsMobBotValid(int MobID)
	{
		return ms_aMobBot.find(MobID) != ms_aMobBot.end() && DataBotInfo::IsDataBotValid(ms_aMobBot[MobID].m_BotID);
	}
	static std::map<int, MobBotInfo> ms_aMobBot;
};

#endif
