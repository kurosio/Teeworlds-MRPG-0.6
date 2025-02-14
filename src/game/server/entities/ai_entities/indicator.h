#ifndef GAME_SERVER_ENTITIES_AI_ENTITIES_INDICATOR_H
#define GAME_SERVER_ENTITIES_AI_ENTITIES_INDICATOR_H
#include <game/server/entity.h>

class CEntityBotIndicator : public CEntity
{
	int m_Type;
	int m_SubType;

public:
	CEntityBotIndicator(CGameWorld *pGameWorld, int ClientID, int Type, int SubType);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
