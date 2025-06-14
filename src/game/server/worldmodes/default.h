/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_MOD_H
#define GAME_SERVER_GAMEMODES_MOD_H

#include <game/server/gamecontroller.h>

class CEntityMoneyBag;
class CGameControllerDefault : public IGameController
{
	int m_MoneyBagTick{};
	PathRequestHandle m_PathMoneyBag{};
	std::vector<CEntityMoneyBag*> m_vMoneyBags{};

public:
	CGameControllerDefault(class CGS *pGameServer);

	void Tick() override;

	void TryGenerateMoneyBag();
};
#endif
