/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_QUESTAI_H
#define GAME_SERVER_ENTITIES_QUESTAI_H

#include <game/server/entity.h>

class CStepPathFinder : public CEntity
{
	enum { STEP_PATH_FINDER_IDS = 2, };
	bool m_MainScenario;

public:
	array < int > m_IDs;
	int m_ClientID;
	int m_SubBotID;
	class CPlayer* m_pPlayer;

	CStepPathFinder(CGameWorld* pGameWorld, vec2 Pos, int ClientID, QuestBotInfo QuestBot);
	~CStepPathFinder() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
	
	int GetClientID() const { return m_ClientID; }
};

#endif
