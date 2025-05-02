/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_BOT_DATA_H
#define GAME_SERVER_COMPONENT_BOT_DATA_H

#include <game/server/core/components/inventory/item_data.h>
#include "DialogsData.h"

// TODO: required rewrite full code
class CCollision;

/************************************************************************/
/*  Global data information bot                                         */
/************************************************************************/
class DataBotInfo
{
public:
	char m_aNameBot[MAX_NAME_LENGTH] {};
	CTeeInfo m_TeeInfos {};
	std::map<ItemType, int> m_vEquippedSlot {};
	bool m_aActiveByQuest[MAX_PLAYERS] {};
	DBSet m_EquippedModules {};

	static bool IsDataBotValid(int BotID) { return (ms_aDataBot.find(BotID) != ms_aDataBot.end()); }
	static std::map<int, DataBotInfo> ms_aDataBot;

	static class MobBotInfo* FindMobByBot(int BotID);
};

/************************************************************************/
/*  Global data npc bot                                                 */
/************************************************************************/
class NpcBotInfo
{
public:
	bool m_Static {};
	vec2 m_Position {};
	int m_Emote {};
	int m_WorldID {};
	int m_BotID {};
	int m_Function {};
	int m_GiveQuestID {};
	std::vector<CDialogStep> m_aDialogs {};

	const char* GetName() const { return DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot; }
	static bool IsValid(int MobID) { return ms_aNpcBot.find(MobID) != ms_aNpcBot.end() && DataBotInfo::IsDataBotValid(ms_aNpcBot[MobID].m_BotID); }
	static std::map<int, NpcBotInfo> ms_aNpcBot;
};

/************************************************************************/
/*  Global data quest bot                                               */
/************************************************************************/
class QuestBotInfo
{
public:
	bool m_AutoFinish {};
	vec2 m_Position {};
	int m_QuestID {};
	int m_StepPos {};
	int m_WorldID {};
	int m_BotID {};
	int m_ID {};
	bool m_HasAction {};
	std::string m_ScenarioJson {};
	std::vector<CDialogStep> m_aDialogs {};

	CItemsContainer m_RewardItems;

	struct TaskRequiredItems
	{
		enum class Type : short
		{
			DEFAULT,
			PICKUP,
			SHOW,
		};

		CItem m_Item {};
		Type m_Type {};
	};
	std::deque < TaskRequiredItems > m_vRequiredItems;

	struct TaskRequiredDefeat
	{
		int m_BotID {};
		int m_RequiredCount {};
	};
	std::deque < TaskRequiredDefeat > m_vRequiredDefeats;

	struct TaskAction
	{
		enum Types : unsigned int
		{
			EMPTY = 0,
			TFMOVING = 1 << 0,
			TFMOVING_PRESS = 1 << 1,
			TFMOVING_FOLLOW_PRESS = 1 << 2,
			TFPICKUP_ITEM = 1 << 3,
			TFREQUIRED_ITEM = 1 << 4,
			TFDEFEAT_MOB = 1 << 5,
		};

		struct DefeatMob
		{
			int m_BotID {};
			int m_AttributePower {};
			int m_WorldID {};
		};

		struct Interaction
		{
			vec2 m_Position {};
		};

		vec2 m_Position {};
		int m_WorldID {};
		int m_Step {};
		bool m_Navigator {};
		unsigned int m_TypeFlags {};
		CItem m_PickupItem {};
		CItem m_RequiredItem {};
		std::string m_CompletionText {};
		std::string m_TaskName {};
		int m_Cooldown {};

		int m_QuestBotID {};
		DefeatMob m_DefeatMobInfo {};
		Interaction m_Interaction {};

		bool IsHasDefeatMob() const { return m_DefeatMobInfo.m_BotID >= 1; };
	};

	std::deque < TaskAction > m_vRequiredMoveAction;
	bool IsAutoFinish() const { return m_AutoFinish; }

	const char* GetName() const { return DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot; }
	static bool IsValid(int MobID) { return ms_aQuestBot.find(MobID) != ms_aQuestBot.end() && DataBotInfo::IsDataBotValid(ms_aQuestBot[MobID].m_BotID); }
	void InitTasksFromJSON(CCollision* pCollision, const std::string& JsonData);

	static std::map<int, QuestBotInfo> ms_aQuestBot;
};

/************************************************************************/
/*  Global data mob bot                                                 */
/************************************************************************/
enum MobBehaviorFlags
{
	MOBFLAG_BEHAVIOR_SLEEPY = 1 << 1,
	MOBFLAG_BEHAVIOR_SLOWER = 1 << 2,
	MOBFLAG_BEHAVIOR_POISONOUS = 1 << 3,
	MOBFLAG_BEHAVIOR_NEUTRAL = 1 << 4,
};

class CMobDebuff
{
	float m_Chance {};
	std::string m_Effect {};
	std::tuple<int, int> m_Time {};

public:
	CMobDebuff() = default;
	CMobDebuff(float Chance, const std::string& Effect, const std::tuple<int, int>& Time)
		: m_Chance(Chance), m_Effect(Effect), m_Time(Time) {}

	enum TupleIndices
	{
		SECONDS = 0,
		RANGE = 1
	};

	const std::string& getEffect() const noexcept
	{
		return m_Effect;
	}

	int getTime() const
	{
		int Range = std::get<RANGE>(m_Time);
		int Time = std::get<SECONDS>(m_Time) - Range / 2;
		return Time + rand() % Range;
	}
	float getChance() const
	{
		return m_Chance;
	}
};

class MobBotInfo
{
	friend class CBotManager;
	std::deque < CMobDebuff > m_Effects;
	int64_t m_BehaviorsFlags {};

public:
	bool m_Boss {};
	int m_Power {};
	vec2 m_Position;
	int m_Level {};
	int m_RespawnTick {};
	int m_WorldID {};
	float m_Radius {};
	int m_aDropItem[MAX_DROPPED_FROM_MOBS] {};
	int m_aValueItem[MAX_DROPPED_FROM_MOBS] {};
	float m_aRandomItem[MAX_DROPPED_FROM_MOBS] {};
	int m_BotID {};

	void InitBehaviors(const DBSet& Behavior);
	void InitDebuffs(int Seconds, int Range, float Chance, const DBSet& buffSets);

	bool HasBehaviorFlag(int64_t Flag) const { return (m_BehaviorsFlags & Flag) != 0; }
	const char* GetName() const { return DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot; }
	std::deque < CMobDebuff >& GetDebuffs() { return m_Effects; }
	[[nodiscard]] CMobDebuff* GetRandomDebuff() { return m_Effects.empty() ? nullptr : &m_Effects[rand() % m_Effects.size()]; }

	static bool IsValid(int MobID) { return ms_aMobBot.find(MobID) != ms_aMobBot.end() && DataBotInfo::IsDataBotValid(ms_aMobBot[MobID].m_BotID); }
	static std::map<int, MobBotInfo> ms_aMobBot;
};

#endif
