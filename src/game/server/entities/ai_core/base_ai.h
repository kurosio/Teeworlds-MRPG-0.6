#ifndef GAME_SERVER_ENTITIES_AI_CORE_BASE_AI_H
#define GAME_SERVER_ENTITIES_AI_CORE_BASE_AI_H

#include "target_ai.h"

class CGS;
class CPlayer;
class CPlayerBot;
class CCharacterBotAI;
class CEntityBotIndicator;

class CBaseAI
{
public:
	CBaseAI(CPlayerBot* pPlayer, CCharacterBotAI* pCharacter);
	virtual ~CBaseAI() {}

	virtual bool IsConversational() { return false; }
	virtual void Process() = 0;
	virtual bool CanDamage(CPlayer* pFrom) = 0;

	virtual void OnSpawn() {}
	virtual void OnTakeDamage(int Dmg, int From, int Weapon) {}
	virtual void OnDie(int Killer, int Weapon) {}
	virtual void OnRewardPlayer(CPlayer* pForPlayer, vec2 Force) const {}
	virtual void OnHandleTunning(CTuningParams* pTuning) {}
	virtual void OnGiveRandomEffect(int ClientID) {}
	virtual void OnTargetRules(float Radius) {}
	virtual void OnSnapDDNetCharacter(int SnappingClient, CNetObj_DDNetCharacter* pDDNetCharacter) {};

	int GetEmotionStyle() const { return m_EmotionStyle; }
	CTargetAI* GetTarget() { return &m_Target; }
	void EnableBotIndicator(int Type, int SubType);
	void DisableBotIndicator();

protected:
	int m_ClientID{};
	int m_EmotionStyle{};
	vec2 m_SpawnPoint{};
	CPlayerBot* m_pPlayer{};
	CCharacterBotAI* m_pCharacter{};
	CTargetAI m_Target{};

	IServer* Server() const;
	CGS* GS() const;

	CPlayer* SearchPlayerCondition(float Distance, const std::function<bool(CPlayer*)>& Condition);
	CPlayerBot* SearchPlayerBotCondition(float Distance, const std::function<bool(CPlayerBot*)>& Condition);

private:
	CEntityBotIndicator* m_pEntBotIndicator {};
};

#endif
