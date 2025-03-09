#ifndef GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENTS_ACHIEVEMENT_LISTENER_H
#define GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENTS_ACHIEVEMENT_LISTENER_H

#include <game/server/core/tools/event_listener.h>

class CGS;
class CPlayer;
class CPlayerItem;
class CProfession;
enum class AchievementType;

class CAchievementListener : public IEventListener
{
public:
	void Initialize()
	{
		g_EventListenerManager.RegisterListener(IEventListener::CharacterDamage, this);
		g_EventListenerManager.RegisterListener(IEventListener::CharacterDeath, this);
		g_EventListenerManager.RegisterListener(IEventListener::PlayerGotItem, this);
		g_EventListenerManager.RegisterListener(IEventListener::PlayerCraftItem, this);
		g_EventListenerManager.RegisterListener(IEventListener::PlayerEquipItem, this);
		g_EventListenerManager.RegisterListener(IEventListener::PlayerProfessionLeveling, this);
		g_EventListenerManager.RegisterListener(IEventListener::PlayerProfessionUnlockedZone, this);
	}

protected:
	void OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage) override;
	void OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon) override;
	void OnPlayerGotItem(CPlayer* pPlayer, CPlayerItem* pItem, int Got) override;
	void OnPlayerCraftItem(CPlayer* pPlayer, CCraftItem* pCraft) override;
	void OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem) override;
	void OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel) override;
	void OnPlayerProfessionUnlockedZone(CPlayer* pPlayer, CProfession* pProfession, int WorldID) override;

private:
	void UpdateAchievement(CPlayer* pPlayer, AchievementType Type, int Criteria, int Progress, int ProgressType) const;
};

extern CAchievementListener g_AchievementListener;

#endif