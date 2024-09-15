#ifndef GAME_SERVER_CORE_COMPONENTS_QUESTS_ENTITIES_MOVE_ACTION_H
#define GAME_SERVER_CORE_COMPONENTS_QUESTS_ENTITIES_MOVE_ACTION_H

#include <game/server/entity.h>

class CPlayer;
class CPlayerBot;
class CPlayerQuest;
class CQuestStep;

class CEntityQuestAction : public CEntity, public std::enable_shared_from_this<CEntityQuestAction>
{
	bool m_AutoCompletesQuestStep;
	int m_MoveToIndex;
	std::optional<int> m_optDefeatBotCID {};
	std::weak_ptr<CQuestStep> m_pStep;
	array < int > m_IDs;

public:
	CEntityQuestAction(CGameWorld* pGameWorld, int ClientID, int MoveToIndex, const std::weak_ptr<CQuestStep>& pStep, 
		bool AutoCompletesQuestStep, std::optional<int> optDefeatBotCID = std::nullopt);
	~CEntityQuestAction() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetMoveToIndex() const { return m_MoveToIndex; }
	void Destroy() override;

private:
	CPlayer* GetPlayer() const;
	CPlayerBot* GetDefeatPlayerBot() const;
	CPlayerQuest* GetPlayerQuest() const;
	CQuestStep* GetQuestStep() const;
	QuestBotInfo::TaskAction* GetTaskMoveTo() const;

	bool PressedFire() const;
	void Handler(const std::function<bool()>& pCallbackSuccesful);
	void TryFinish();

	void HandleTaskType(const QuestBotInfo::TaskAction* pTaskData);
	void HandleBroadcastInformation(const QuestBotInfo::TaskAction* pTaskData) const;
};

#endif
