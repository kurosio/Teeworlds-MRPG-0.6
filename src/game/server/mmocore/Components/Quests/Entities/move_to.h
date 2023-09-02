/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_MMOCORE_COMPONENTS_QUESTS_ENTITIES_MOVE_TO_H
#define GAME_SERVER_MMOCORE_COMPONENTS_QUESTS_ENTITIES_MOVE_TO_H

#include <game/server/entity.h>

class CEntityMoveTo : public CEntity
{
public:
	bool* m_pComplete;
	int m_ClientID;
	int m_QuestID;
	class CPlayer* m_pPlayer;

	CEntityMoveTo(CGameWorld* pGameWorld, vec2 Pos, int ClientID, int QuestID, bool *pComplete);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	
	int GetClientID() const { return m_ClientID; }
};

#endif
