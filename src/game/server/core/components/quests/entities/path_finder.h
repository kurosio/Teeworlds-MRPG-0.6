/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_MMOCORE_COMPONENTS_QUESTS_ENTITIES_PATH_FINDER_H
#define GAME_SERVER_MMOCORE_COMPONENTS_QUESTS_ENTITIES_PATH_FINDER_H

#include <game/server/entity.h>

class CPlayer;
class CQuestStep;

class CEntityPathArrow : public CEntity, public std::enable_shared_from_this<CEntityPathArrow>
{
	float m_AreaClipped{};
	int m_ConditionType{};
	int m_ConditionIndex{};
	std::weak_ptr<CQuestStep> m_pStep{};

public:
	enum
	{
		CONDITION_MOVE_TO = 1,
		CONDITION_DEFEAT_BOT,
	};

	CEntityPathArrow(CGameWorld* pGameWorld, int ClientID, float AreaClipped, vec2 SearchPos, int WorldID,
		const std::weak_ptr<CQuestStep>& pStep, int ConditionType, int ConditionIndex);

	void Tick() override;
	void Snap(int SnappingClient) override;
	void Destroy() override;

private:
	CPlayer* GetPlayer() const;
	CQuestStep* GetQuestStep() const;
};

#endif
