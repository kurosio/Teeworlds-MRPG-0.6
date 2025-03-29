#ifndef GAME_SERVER_CORE_COMPONENTS_QUESTS_ENTITIES_DIR_NAVIGATOR_H
#define GAME_SERVER_CORE_COMPONENTS_QUESTS_ENTITIES_DIR_NAVIGATOR_H

#include <game/server/entity.h>

class CPlayer;
class CQuestStep;
class CEntityPathNavigator;
class CEntityDirNavigator : public CEntity
{
	int m_Type {};
	int m_Subtype {};
	float m_Clipped {};
	CEntityPathNavigator* m_pEntNavigator {};

public:
	CEntityDirNavigator(CGameWorld* pGameWorld, int Type, int Subtype, bool StarNavigator, int ClientID, float Clipped, vec2 Pos, int WorldID);
	~CEntityDirNavigator() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
