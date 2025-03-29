#ifndef GAME_SERVER_CORE_COMPONENTS_QUESTS_ENTITIES_MOVE_ACTION_H
#define GAME_SERVER_CORE_COMPONENTS_QUESTS_ENTITIES_MOVE_ACTION_H

#include <game/server/entity.h>

class CPlayer;
class CPlayerBot;
class CQuestStep;
class CPlayerQuest;
class CEntityDirNavigator;

class CEntityQuestAction : public CEntity
{
	enum
	{
		VISUAL_GROUP = 0,
		VISTUAL_IDS_NUM = 4
	};

	int m_MoveToIndex;
	CQuestStep* m_pStep {};
	CEntityDirNavigator* m_pEntDirNavigator {};
	std::optional<int> m_DefeatBotCID {};

public:
	CEntityQuestAction(CGameWorld* pGameWorld, int ClientID, int MoveToIndex, CQuestStep* pStep);
	~CEntityQuestAction() override;

	void Initialize();

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	CPlayerBot* GetDefeatPlayerBot() const;
	CPlayerQuest* GetPlayerQuest() const;
	QuestBotInfo::TaskAction* GetTaskMoveTo() const;

	bool PressedFire() const;
	void Handler(const std::function<bool()>& pCallbackSuccesful);
	void TryFinish();

	void HandleTaskType(const QuestBotInfo::TaskAction* pTaskData);
	void HandleBroadcastInformation(const QuestBotInfo::TaskAction* pTaskData) const;
};

#endif
