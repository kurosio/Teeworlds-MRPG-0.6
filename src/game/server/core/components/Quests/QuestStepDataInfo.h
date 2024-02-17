/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_STEP_DATA_INFO_H
#define GAME_SERVER_COMPONENT_QUEST_STEP_DATA_INFO_H

#include <game/server/core/components/Bots/BotData.h>

class CGS;
class CPlayer;

/*
 * Quest step
 */
class CQuestStepBase
{
public:
	QuestBotInfo m_Bot{};
	virtual void UpdateBot() const;

private:
	bool IsActiveStep() const;
};

// ##############################################################
// ################# PLAYER STEP STRUCTURE ######################
class CQuestStep : public CQuestStepBase
{
	class CGS* GS() const;
	class CPlayer* GetPlayer() const;

	struct MobProgressStatus
	{
		int m_Count;
		bool m_Complete;
	};

public:
	std::unordered_map < int /*BotID*/, MobProgressStatus/*MobProgressStatus*/ > m_aMobProgress { };
	std::deque < bool /* State */ > m_aMoveToProgress { };

	int m_ClientID {};
	bool m_StepComplete{};
	bool m_ClientQuitting{};
	bool m_TaskListReceived{};

	void Clear();
	int GetNumberBlockedItem(int ItemID) const;
	bool IsComplete();
	bool Finish();
	void PostFinish();

	void AppendDefeatProgress(int DefeatedBotID);
	void CreateVarietyTypesRequiredItems();
	void FormatStringTasks(char* aBufQuestTask, int Size);

	void UpdatePathNavigator();
	void UpdateTaskMoveTo();
	void Update();

	int GetMoveToNum() const;
	int GetMoveToCurrentStepPos() const;
	int GetCountMoveToComplected();

	// steps path finder tools
	std::deque < class CEntityMoveTo* > m_apEntitiesMoveTo {};
	std::deque < class CEntityPathFinder* > m_apEntitiesNavigator {};

	CEntityMoveTo* FoundEntityMoveTo(vec2 Position) const;
	CEntityPathFinder* FoundEntityNavigator(vec2 Position) const;
	CEntityMoveTo* AddEntityMoveTo(const QuestBotInfo::TaskRequiredMoveTo* pTaskMoveTo, bool* pComplete, class CPlayerBot* pDefeatMobPlayer = nullptr);
	CEntityPathFinder* AddEntityNavigator(vec2 Position, int WorldID, float AreaClipped, bool* pComplete);
};

#endif
