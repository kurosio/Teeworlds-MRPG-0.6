#ifndef GAME_SERVER_ENTITIES_AI_QUEST_MOB_AI_H
#define GAME_SERVER_ENTITIES_AI_QUEST_MOB_AI_H

#include "base_ai.h"

class CQuestBotMobInfo;

class CQuestMobAI final : public CBaseAI
{
	CQuestBotMobInfo* m_pQuestMobInfo {};

public:
	CQuestMobAI(CQuestBotMobInfo* pQuestMobInfo, CPlayerBot* pPlayer, CCharacterBotAI* pCharacter);

	void OnRewardPlayer(CPlayer* pPlayer, vec2 Force) const override;
	void OnTargetRules(float Radius) override;
	void Process() override;

	void OnSnapDDNetCharacter(int SnappingClient, CNetObj_DDNetCharacter* pDDNetCharacter) override;
};

#endif
