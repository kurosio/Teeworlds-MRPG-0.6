/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_MMOCORE_COMPONENTS_QUESTS_ENTITIES_MOVE_TO_H
#define GAME_SERVER_MMOCORE_COMPONENTS_QUESTS_ENTITIES_MOVE_TO_H

#include <game/server/entity.h>

class CEntityMoveTo : public CEntity
{
	class CPlayer* m_pPlayer;
	class CPlayerBot* m_pDefeatMobPlayer;

	int m_QuestID;
	bool* m_pComplete;
	bool m_AutoCompletesQuestStep;
	float m_Radius;
	array < int > m_IDs;
	std::deque < CEntityMoveTo* >* m_apCollection;
	const QuestBotInfo::TaskRequiredMoveTo* m_pTaskMoveTo;

public:
	CEntityMoveTo(CGameWorld* pGameWorld, const QuestBotInfo::TaskRequiredMoveTo* pTaskMoveTo, int ClientID, int QuestID, bool *pComplete, std::deque < CEntityMoveTo* >* apCollection, 
		bool AutoCompletesQuestStep, class CPlayerBot* pDefeatMobPlayer = nullptr);
	~CEntityMoveTo() override;

	void Tick() override;
	void Snap(int SnappingClient) override;

	void HandleBroadcastInformation() const;
	bool PressedFire() const;

	int GetQuestID() const { return m_QuestID; }

	void ClearPointers();

	void Handler(const std::function<bool()> pCallbackSuccesful);
	void TryFinish(bool AutoCompleteQuestStep);
};

#endif
