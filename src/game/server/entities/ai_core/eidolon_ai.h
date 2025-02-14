#ifndef GAME_SERVER_ENTITIES_AI_CORE_EIDOLON_AI_H
#define GAME_SERVER_ENTITIES_AI_CORE_EIDOLON_AI_H

#include "base_ai.h"

class CEidolonAI final : public CBaseAI
{
public:
	CEidolonAI(CPlayerBot* pPlayer, CCharacterBotAI* pCharacter);

	bool CanDamage(CPlayer* pFrom) override;

	void OnSpawn() override;
	void OnDie(int Killer, int Weapon) override;

	void OnTargetRules(float Radius) override;
	void Process() override;

	void OnSnapDDNetCharacter(int SnappingClient, CNetObj_DDNetCharacter* pDDNetCharacter) override;
};

#endif
