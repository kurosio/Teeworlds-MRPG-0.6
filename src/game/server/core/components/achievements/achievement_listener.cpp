#include "achievement_listener.h"
#include "achievement_data.h"

#include "../crafting/craft_data.h"
#include <game/server/gamecontext.h>

CAchievementListener g_AchievementListener;

void CAchievementListener::OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage)
{
	if(pFrom != pTo && !pFrom->IsBot())
		pFrom->UpdateAchievement(AchievementType::TotalDamage, NOPE, Damage, PROGRESS_ACCUMULATE);
}

void CAchievementListener::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	// achievement death
	if(!pVictim->IsBot())
		pVictim->UpdateAchievement(AchievementType::Death, NOPE, 1, PROGRESS_ACCUMULATE);

	if(pVictim != pKiller)
	{
		// achievement defeat mob & pve
		if(pVictim->IsBot() && !pKiller->IsBot())
		{
			const auto VictimBotID = dynamic_cast<CPlayerBot*>(pVictim)->GetBotID();
			pKiller->UpdateAchievement(AchievementType::DefeatMob, VictimBotID, 1, PROGRESS_ACCUMULATE);
			pKiller->UpdateAchievement(AchievementType::DefeatPVE, NOPE, 1, PROGRESS_ACCUMULATE);
		}

		// achievement defeat pvp
		if(!pVictim->IsBot() && !pKiller->IsBot())
		{
			pKiller->UpdateAchievement(AchievementType::DefeatPVP, NOPE, 1, PROGRESS_ACCUMULATE);
		}
	}
}

void CAchievementListener::OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
	pPlayer->UpdateAchievement(AchievementType::Equip, pItem->GetID(), true, PROGRESS_ABSOLUTE);
}

void CAchievementListener::OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel)
{
	pPlayer->UpdateAchievement(AchievementType::Leveling, (int)pProfession->GetProfessionID(), NewLevel, PROGRESS_ABSOLUTE);
}

void CAchievementListener::OnPlayerProfessionUnlockedZone(CPlayer* pPlayer, CProfession* pProfession, int WorldID)
{
	pPlayer->UpdateAchievement(AchievementType::UnlockWorld, WorldID, 1, PROGRESS_ABSOLUTE);
}

void CAchievementListener::OnPlayerGotItem(CPlayer* pPlayer, CPlayerItem* pItem, int Got)
{
	pPlayer->UpdateAchievement(AchievementType::ReceiveItem, pItem->GetID(), Got, PROGRESS_ACCUMULATE);
	pPlayer->UpdateAchievement(AchievementType::HaveItem, pItem->GetID(), pItem->GetValue(), PROGRESS_ABSOLUTE);
}

void CAchievementListener::OnPlayerCraftItem(CPlayer* pPlayer, CCraftItem* pCraft)
{
	pPlayer->UpdateAchievement(AchievementType::CraftItem, pCraft->GetID(), pCraft->GetItem()->GetValue(), PROGRESS_ACCUMULATE);
}
