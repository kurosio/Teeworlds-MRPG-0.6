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
	int m_CollectItemID;
	class CPlayer* m_pPlayer;
	std::deque < CEntityMoveTo* >* m_apCollection;

	CEntityMoveTo(CGameWorld* pGameWorld, vec2 Pos, int ClientID, int QuestID, int CollectItemID, bool *pComplete, std::deque < CEntityMoveTo* >* apCollection);

	void Destroy() override;
	void Tick() override;
	void Snap(int SnappingClient) override;

	bool PickItem() const;
	int GetClientID() const { return m_ClientID; }
};

#endif
