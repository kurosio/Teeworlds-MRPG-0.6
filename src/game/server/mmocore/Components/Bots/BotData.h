/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_BOT_DATA_H
#define GAME_SERVER_COMPONENT_BOT_DATA_H

/************************************************************************/
/*  Dialog struct (reworked 08.01.2022)                                 */
/************************************************************************/
class CDialog
{
public:
	class VariantText
	{
		std::string m_SaysName;

	public:
		std::string m_Text {};
		int m_Flag {};

		void Init(std::string SaysName) { m_SaysName = SaysName; }

		/* return nullptr in case the base name is not set, this also applies to dynamic data of the kind of player name */
		const char* GetSaysName() const
		{
			if(m_SaysName.empty())
				return nullptr;

			return m_SaysName.c_str();
		}
	};

private:
	std::deque <VariantText> m_aVariantText{};
	int m_Emote{};
	bool m_ActionStep{};

public:
	void Init(int BotID, std::string DialogueData, int Emote, bool ActionStep);

	/* this method should not be called through this function, it should be a reference */
	[[nodiscard]] VariantText* GetVariant();
	int GetEmote() const { return m_Emote; }
	bool IsRequestAction() const { return m_ActionStep; }
	std::deque<VariantText>& GetArrayText()  { return m_aVariantText; }
	bool IsEmptyDialog() const { return m_aVariantText.empty(); }

};

/************************************************************************/
/*  Global data information bot                                         */
/************************************************************************/
class DataBotInfo
{
public:
	char m_aNameBot[MAX_NAME_LENGTH]{};
	CTeeInfo m_TeeInfos{};
	int m_aEquipSlot[NUM_EQUIPPED]{};
	bool m_aVisibleActive[MAX_PLAYERS]{};

	static bool IsDataBotValid(int BotID) { return (ms_aDataBot.find(BotID) != ms_aDataBot.end()); }
	static std::map<int, DataBotInfo> ms_aDataBot;
};

/************************************************************************/
/*  Global data npc bot                                                 */
/************************************************************************/
class NpcBotInfo
{
public:
	bool m_Static{};
	vec2 m_Position{};
	int m_Emote{};
	int m_WorldID{};
	int m_BotID{};
	int m_Function{};
	int m_GiveQuestID{};
	std::vector<CDialog> m_aDialogs {};

	const char* GetName() const { return DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot; }
	static bool IsNpcBotValid(int MobID) { return ms_aNpcBot.find(MobID) != ms_aNpcBot.end() && DataBotInfo::IsDataBotValid(ms_aNpcBot[MobID].m_BotID); }
	static std::map<int, NpcBotInfo> ms_aNpcBot;
};

/************************************************************************/
/*  Global data quest bot                                               */
/************************************************************************/
class QuestBotInfo
{
public:
	char m_aGeneratedNickname[MAX_NAME_LENGTH]{};
	vec2 m_Position{};
	int m_QuestID{};
	int m_Step{};
	int m_WorldID{};
	int m_BotID{};
	int m_SubBotID{};
	int m_aItemSearch[2]{};
	int m_aItemSearchValue[2]{};
	int m_aItemGives[2]{};
	int m_aItemGivesValue[2]{};
	int m_aNeedMob[2]{};
	int m_aNeedMobValue[2]{};
	int m_InteractiveType{};
	int m_InteractiveTemp{};
	bool m_GenerateNick{};
	std::vector<CDialog> m_aDialogs {};

	const char* GetName() const { return DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot; }
	static bool IsQuestBotValid(int MobID) { return ms_aQuestBot.find(MobID) != ms_aQuestBot.end() && DataBotInfo::IsDataBotValid(ms_aQuestBot[MobID].m_BotID); }
	static std::map<int, QuestBotInfo> ms_aQuestBot;
};

/************************************************************************/
/*  Global data mob bot                                                 */
/************************************************************************/
class MobBotInfo
{
public:
	bool m_Boss{};
	int m_Power{};
	int m_Spread{};
	vec2 m_Position;
	int m_Level{};
	int m_RespawnTick{};
	int m_WorldID{};
	char m_aEffect[16]{};
	char m_aBehavior[32]{};
	int m_aDropItem[MAX_DROPPED_FROM_MOBS]{};
	int m_aValueItem[MAX_DROPPED_FROM_MOBS]{};
	float m_aRandomItem[MAX_DROPPED_FROM_MOBS]{};
	int m_BotID{};

	const char* GetName() const { return DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot; }
	static bool IsMobBotValid(int MobID) { return ms_aMobBot.find(MobID) != ms_aMobBot.end() && DataBotInfo::IsDataBotValid(ms_aMobBot[MobID].m_BotID); }
	static std::map<int, MobBotInfo> ms_aMobBot;
};

#endif
