#ifndef GAME_SERVER_ENTITIES_AI_CORE_NPC_AI_H
#define GAME_SERVER_ENTITIES_AI_CORE_NPC_AI_H

#include "base_ai.h"

class NpcBotInfo;

class CNpcAI final : public CBaseAI
{
	NpcBotInfo* m_pNpcInfo {};

public:
	CNpcAI(NpcBotInfo* pNpcInfo, CPlayerBot* pPlayer, CCharacterBotAI* pCharacter);

	bool CanDamage(CPlayer* pFrom) override;

	void OnSpawn() override;
	void OnTakeDamage(int Dmg, int From, int Weapon) override;
	void OnHandleTunning(CTuningParams* pTuning) override;

	void OnTargetRules(float Radius) override;
	void Process() override;

	bool IsConversational() override;

private:
	void ProcessGuardianNPC() const;
	void ProcessDefaultNPC();
};

#endif
