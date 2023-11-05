/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_DAILY_BOARD_DATA_H
#define GAME_SERVER_COMPONENT_QUEST_DAILY_BOARD_DATA_H

#include "QuestDataInfo.h"

using QuestDailyBoardIdentifier = int;

// This class represents the daily board for quests in a game.
class CQuestsDailyBoard : public MultiworldIdentifiableStaticData< std::map< int, CQuestsDailyBoard > >
{
public:
	using ContainerDailyQuests = std::deque<CQuestDescription>;

private:
	QuestDailyBoardIdentifier m_ID {};
	char m_aName[32] {};
	vec2 m_Pos {};
	int m_WorldID {};

public:
	ContainerDailyQuests m_DailyQuestsInfoList {};

	// Default constructor for CQuestsDailyBoard class
	CQuestsDailyBoard() = default;

	// Constructor for CQuestsDailyBoard class with specified daily board identifier
	CQuestsDailyBoard(QuestDailyBoardIdentifier ID) : m_ID(ID) {}

	// Definite a function called Init with the following parameters:
	// - Name: a constant reference to a std::string object
	// - Pos: a vec2 object
	// - WorldID: an integer variable
	void Init(const std::string & Name, vec2 Pos, int WorldID)
	{
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		m_Pos = Pos;
		m_WorldID = WorldID;
		m_pData[m_ID] = *this;
	}

	// Function to retrieve the ID of the daily board
	QuestDailyBoardIdentifier GetID() const { return m_ID; }

	// Function to retrieve the name of the daily board
	const char* GetName() const { return m_aName; }

	// Function to retrieve the position of the daily board
	vec2 GetPos() const { return m_Pos; }

	// Function to retrieve the world ID of the daily board
	int GetWorldID() const { return m_WorldID; }
};

#endif