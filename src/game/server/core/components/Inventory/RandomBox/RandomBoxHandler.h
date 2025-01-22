/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_INVENTORY_RANDOM_BOX_HANDLER_H
#define GAME_SERVER_INVENTORY_RANDOM_BOX_HANDLER_H

#include <game/server/entity.h>

class CPlayerItem;
class CPlayer;

class CEntityRandomBoxRandomizer : public CEntity
{
	int m_Used;
	int m_Lifetime;
	int m_AccountID;
	CPlayerItem* m_pPlayerUsesItem;

	ChanceProcessor<CRandomItem> m_ChanceProcessor;
	CRandomItem m_Current {};

public:
	CEntityRandomBoxRandomizer(CGameWorld* pGameWorld, int AccountID, int LifeTime,
		const ChanceProcessor<CRandomItem>& chanceProcessor, CPlayerItem* pPlayerUsesItem, int UseValue);

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	void Finish();
};

#endif