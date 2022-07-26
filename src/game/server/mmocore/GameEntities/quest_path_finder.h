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
	class CPlayer* m_pPlayer;

	CQuestPathFinder(CGameWorld* pGameWorld, vec2 Pos, int ClientID, QuestBotInfo QuestBot);

	void Reset() override;
	void Tick() override;
	void PathSnap(vec2 CorePos);
	
	int GetClientID() const { return m_ClientID; }
};

#endif
