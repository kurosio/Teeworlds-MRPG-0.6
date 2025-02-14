#ifndef GAME_SERVER_ENTITIES_AI_CORE_QUEST_NPC_AI_H
#define GAME_SERVER_ENTITIES_AI_CORE_QUEST_NPC_AI_H

#include "base_ai.h"

class QuestBotInfo;

class CQuestNpcAI final : public CBaseAI
{
	QuestBotInfo* m_pQuestNpcInfo {};

public:
	CQuestNpcAI(QuestBotInfo* pQuestNpcInfo, CPlayerBot* pPlayer, CCharacterBotAI* pCharacter);

	bool CanDamage(CPlayer* pFrom) override;

	void OnSpawn() override;
	void Process() override;

	bool IsConversational() override;
};

#endif
