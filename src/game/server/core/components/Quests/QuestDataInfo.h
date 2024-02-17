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

class CQuestDescription : public MultiworldIdentifiableStaticData < std::map< int, CQuestDescription* > >
{
	QuestIdentifier m_ID{};
	char m_aName[24]{};
	char m_aStoryLine[24]{};
	int m_Gold{};
	int m_Exp{};
	int m_Flags{};

public:
	CQuestDescription(QuestIdentifier ID) : m_ID(ID) {}

	static CQuestDescription* CreateElement(QuestIdentifier ID)
	{
		const auto pData = new CQuestDescription(ID);
		return m_pData[ID] = pData;
	}

	void Init(const std::string& Name, const std::string& Story, int Gold, int Exp)
	{
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		str_copy(m_aStoryLine, Story.c_str(), sizeof(m_aStoryLine));
		m_Gold = Gold;
		m_Exp = Exp;
		m_Flags = QF_NONE;
	}

	void MarkDaily()
	{
		m_Flags |= QF_DAILY;
	}

	QuestIdentifier GetID() const { return m_ID; }
	std::string GetDataFilename(int AccountID) const;
	const char* GetName() const { return m_aName; }
	const char* GetStory() const { return m_aStoryLine; }
	int GetQuestStoryPosition() const;
	int GetQuestStorySize() const;
	int GetRewardGold() const { return m_Gold; }
	int GetRewardExp() const { return m_Exp; }

	void PreparePlayerQuestSteps(int ClientID, std::map < int, CQuestStep >* pElem) const;
	bool IsHasFlag(int Flag) const { return (m_Flags & Flag) != 0; }
	bool IsDaily() const { return IsHasFlag(QF_DAILY); }

	// steps with array bot data on active step
	std::unordered_map < int, CQuestStepBase > m_StepsQuestBot;
};

#endif