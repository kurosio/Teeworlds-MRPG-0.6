/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_MMOCORE_COMPONENTS_QUESTS_ENTITIES_MOVE_TO_H
#define GAME_SERVER_MMOCORE_COMPONENTS_QUESTS_ENTITIES_MOVE_TO_H

#include <game/server/entity.h>

class CEntityQuestAction : public CEntity
{
	class CPlayer* m_pPlayer;
	class CPlayerBot* m_pDefeatMobPlayer;

	int m_QuestID;
	bool* m_pComplete;
	bool m_AutoCompletesQuestStep;
	float m_Radius;
	array < int > m_IDs;
	const QuestBotInfo::TaskAction* m_pTaskMoveTo;

public:
	CEntityQuestAction(CGameWorld* pGameWorld, const QuestBotInfo::TaskAction& TaskMoveTo, int ClientID, int QuestID, bool *pComplete, 
		bool AutoCompletesQuestStep, class CPlayerBot* pDefeatMobPlayer = nullptr);
	~CEntityQuestAction() override;

	void Tick() override;
	void Snap(int SnappingClient) override;

	void HandleBroadcastInformation() const;
	bool PressedFire() const;
	int GetQuestID() const { return m_QuestID; }

	void Handler(const std::function<bool()>& pCallbackSuccesful);
	void TryFinish();
};

#endif
