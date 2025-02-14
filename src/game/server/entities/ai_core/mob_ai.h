#ifndef GAME_SERVER_ENTITIES_AI_CORE_MOB_AI_H
#define GAME_SERVER_ENTITIES_AI_CORE_MOB_AI_H

#include "base_ai.h"

class MobBotInfo;

class CMobAI final : public CBaseAI
{
	MobBotInfo* m_pMobInfo {};

public:
	CMobAI(MobBotInfo* pMobInfo, CPlayerBot* pPlayer, CCharacterBotAI* pCharacter);

	bool CanDamage(CPlayer* pFrom) override;

	void OnSpawn() override;
	void OnGiveRandomEffect(int ClientID) override;
	void OnRewardPlayer(CPlayer* pPlayer, vec2 Force) const override;
	void OnTargetRules(float Radius) override;
	void Process() override;

private:
	void ShowHealth() const;
};

#endif
