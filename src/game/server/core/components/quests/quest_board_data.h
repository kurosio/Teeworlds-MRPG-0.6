/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_QUESTS_QUEST_BOARD_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_QUESTS_QUEST_BOARD_DATA_H

#include "quest_data.h"

constexpr auto TW_QUEST_BOARDS_TABLE = "tw_quest_boards";
constexpr auto TW_QUESTS_DAILY_BOARD_LIST = "tw_quests_board_list";

// This class represents the daily board for quests in a game.
class CQuestsBoard : public MultiworldIdentifiableData< std::map< int, CQuestsBoard* > >
{
	int m_ID {};
	std::string m_Name {};
	vec2 m_Pos {};
	int m_WorldID {};
	std::deque<CQuestDescription*> m_vQuestList {};

public:
	CQuestsBoard() = default;

	static CQuestsBoard* CreateElement(int ID)
	{
		const auto pData = new CQuestsBoard();
		pData->m_ID = ID;
		return m_pData[ID] = pData;
	}

	void Init(const std::string& Name, vec2 Pos, int WorldID)
	{
		m_Name = Name;
		m_Pos = Pos;
		m_WorldID = WorldID;
	}

	int GetID() const { return m_ID; }
	const char* GetName() const { return m_Name.c_str(); }
	vec2 GetPos() const { return m_Pos; }
	int GetWorldID() const { return m_WorldID; }
	std::deque<CQuestDescription*>& GetQuestList() { return m_vQuestList; }
};

#endif