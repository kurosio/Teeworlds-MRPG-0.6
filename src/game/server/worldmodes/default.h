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

	void OnCharacterDamage(class CPlayer* pFrom, class CPlayer* pTo, int Damage) override;
	void OnCharacterDeath(class CPlayer* pVictim, class CPlayer* pKiller, int Weapon) override;
	bool OnCharacterSpawn(class CCharacter* pChr) override;
	bool OnCharacterBotSpawn(class CCharacterBotAI* pChr) override;
	void OnEntity(int Index, vec2 Pos, int Flags) override;
	void OnEntitySwitch(int Index, vec2 Pos, int Flags, int Number) override;
	void OnPlayerConnect(class CPlayer* pPlayer) override;
	void OnPlayerDisconnect(class CPlayer* pPlayer) override;

	void Tick() override;

	void TryGenerateMoneyBag();
};
#endif
