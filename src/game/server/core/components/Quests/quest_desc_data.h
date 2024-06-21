/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_DATA_INFO_H
#define GAME_SERVER_COMPONENT_QUEST_DATA_INFO_H

#include "QuestStepDataInfo.h"

using QuestIdentifier = int;

enum
{
	QF_NONE = 0,
	QF_DAILY = 1 << 0,
};

class CQuestDescription : public MultiworldIdentifiableData < std::map< int, CQuestDescription* > >
{
	QuestIdentifier m_ID{};
	char m_aName[24]{};
	char m_aStoryLine[24]{};
	int m_Gold{};
	int m_Exp{};
	int m_Flags{};

public:
	CQuestDescription(QuestIdentifier ID) : m_ID(ID) {}

	/*
	 * Create a new instance of the quest description
	 */
	static CQuestDescription* CreateElement(QuestIdentifier ID)
	{
		const auto pData = new CQuestDescription(ID);
		return m_pData[ID] = pData;
	}

	/*
	 * Initialize the quest description
	 */
	void Init(const std::string& Name, const std::string& Story, int Gold, int Exp)
	{
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		str_copy(m_aStoryLine, Story.c_str(), sizeof(m_aStoryLine));
		m_Gold = Gold;
		m_Exp = Exp;
		m_Flags = QF_NONE;
	}

	/*
	 * Mark quest as daily
	 *
	 */
	void MarkDaily() { m_Flags |= QF_DAILY; }

	/*
	 * Get ID of the quest
	 *
	 */
	QuestIdentifier GetID() const { return m_ID; }

	/*
	 * Get name of the quest
	 *	 
	 */
	const char* GetName() const { return m_aName; }

	/*
	 * Get storyline of the quest
	 *
	 */
	const char* GetStory() const { return m_aStoryLine; }

	/*
	 * Get position of quests storyline
	 *
	 */
	int GetStoryQuestPosition() const;

	/*
	 * Get size of quests storyline
	 *	 
	 */
	int GetStoryQuestsNum() const;

	/*
	 * Get reward of the quest Gold
	 *
	 */
	int GetRewardGold() const { return m_Gold; }

	/*
	 * Get reward of the quest Exp
	 *	 
	 */
	int GetRewardExp() const { return m_Exp; }

	/*
	 * Prepare player steps
	 *
	 */
	void PreparePlayerSteps(int StepPos, int ClientID, std::deque<CQuestStep>* pElem);

	/*
	 * Is has flag
	 *	 
	 */
	bool IsHasFlag(int Flag) const { return (m_Flags & Flag) != 0; }

	/*
	 * Is daily quest
	 *
	 */
	bool IsDaily() const { return IsHasFlag(QF_DAILY); }

	// steps with array bot data on active step
	std::map < int, std::deque<CQuestStepBase> > m_vSteps;
};

#endif