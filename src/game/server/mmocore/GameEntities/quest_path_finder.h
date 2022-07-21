/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_QUESTAI_H
#define GAME_SERVER_ENTITIES_QUESTAI_H
#include <game/server/entity.h>

class CQuestPathFinder : public CEntity
{
	bool m_MainScenario;

public:
	int m_ClientID;
	int m_SubBotID;

	CQuestPathFinder(CGameWorld* pGameWorld, vec2 Pos, int ClientID, QuestBotInfo QuestBot);

	void Tick() override;
	void Snap(int SnappingClient) override;

	vec2 m_TargetPos;
	int GetClientID() const { return m_ClientID; }
};

#endif
