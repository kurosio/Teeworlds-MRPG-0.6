#ifndef GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENTS_ACHIEVEMENT_LISTENER_H
#define GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENTS_ACHIEVEMENT_LISTENER_H

#include <game/server/core/tools/event_listener.h>

#include "achievement_data.h"

class CGS;
class CPlayer;
class CPlayerItem;
class CProfession;
class CQuestDescription;

class CAchievementListener : public IEventListener
{
public:
	void Initialize();

protected:
	void OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage) override;
	void OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon) override;
	void OnPlayerGotItem(CPlayer* pPlayer, CPlayerItem* pItem, int Got) override;
	void OnPlayerCraftItem(CPlayer* pPlayer, CCraftItem* pCraft, int Amount, int Extra) override;
	void OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem) override;
	void OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel) override;
	void OnPlayerQuestChangeState(CPlayer* pPlayer, CPlayerQuest* pQuest, QuestState NewState) override;

private:
	void UpdateAchievement(CPlayer* pPlayer, AchievementType Type, int Criteria, int Progress, EAchievementProgress Mode);

	// (type, criteria) -> achievement IDs. Built once from the static descriptions.
	std::map<std::pair<int, int>, std::vector<int>> m_Index;
	bool m_IndexBuilt {};
};

extern CAchievementListener g_AchievementListener;

#endif
