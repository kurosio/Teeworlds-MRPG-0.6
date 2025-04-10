#include "achievement_listener.h"
#include "achievement_data.h"

#include "../crafting/craft_data.h"
#include <game/server/gamecontext.h>

CAchievementListener g_AchievementListener;

void CAchievementListener::OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage)
{
	if(pFrom && pFrom != pTo && !pFrom->IsBot())
	{
		UpdateAchievement(pFrom, AchievementType::TotalDamage, NOPE, Damage, PROGRESS_ACCUMULATE);
	}
}

void CAchievementListener::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	// achievement death
	if(pVictim && !pVictim->IsBot() && Weapon != WEAPON_WORLD)
	{
		UpdateAchievement(pVictim, AchievementType::Death, NOPE, 1, PROGRESS_ACCUMULATE);
	}

	if(pKiller && pVictim && pVictim != pKiller)
	{
		// achievement defeat mob & pve
		if(pVictim->IsBot() && !pKiller->IsBot())
		{
			const auto VictimBotID = dynamic_cast<CPlayerBot*>(pVictim)->GetBotID();
			UpdateAchievement(pKiller, AchievementType::DefeatMob, VictimBotID, 1, PROGRESS_ACCUMULATE);
			UpdateAchievement(pKiller, AchievementType::DefeatPVE, NOPE, 1, PROGRESS_ACCUMULATE);
		}

		// achievement defeat pvp
		if(!pVictim->IsBot() && !pKiller->IsBot())
		{
			UpdateAchievement(pKiller, AchievementType::DefeatPVP, NOPE, 1, PROGRESS_ACCUMULATE);
		}
	}
}

void CAchievementListener::OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
	UpdateAchievement(pPlayer, AchievementType::Equip, pItem->GetID(), true, PROGRESS_ABSOLUTE);
}

void CAchievementListener::OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel)
{
	UpdateAchievement(pPlayer, AchievementType::Leveling, (int)pProfession->GetProfessionID(), NewLevel, PROGRESS_ABSOLUTE);
}

void CAchievementListener::OnPlayerQuestChangeState(CPlayer* pPlayer, CPlayerQuest* pQuest, QuestState NewState)
{
	if(pQuest->IsCompleted())
	{
		UpdateAchievement(pPlayer, AchievementType::CompleteQuest, pQuest->GetID(), true, PROGRESS_ABSOLUTE);
	}
}

void CAchievementListener::OnPlayerGotItem(CPlayer* pPlayer, CPlayerItem* pItem, int Got)
{
	UpdateAchievement(pPlayer, AchievementType::ReceiveItem, pItem->GetID(), Got, PROGRESS_ACCUMULATE);
	UpdateAchievement(pPlayer, AchievementType::HaveItem, pItem->GetID(), pItem->GetValue(), PROGRESS_ABSOLUTE);
}

void CAchievementListener::OnPlayerCraftItem(CPlayer* pPlayer, CCraftItem* pCraft)
{
	UpdateAchievement(pPlayer, AchievementType::CraftItem, pCraft->GetID(), pCraft->GetItem()->GetValue(), PROGRESS_ACCUMULATE);
}

void CAchievementListener::UpdateAchievement(CPlayer* pPlayer, AchievementType Type, int Criteria, int Progress, int ProgressType) const
{
	if(!pPlayer || pPlayer->IsBot())
		return;

	// initialize variables
	bool Updated = false;
	auto& pAchievements = CAchievement::Data()[pPlayer->GetCID()];

	// search for the achievement
	for(const auto& pAchievement : pAchievements)
	{
		const auto achievementType = pAchievement->Info()->GetType();
		const auto achievementCriteria = pAchievement->Info()->GetCriteria();

		if(achievementType != Type || pAchievement->IsCompleted())
			continue;

		if(achievementCriteria <= 0 || achievementCriteria == Criteria)
		{
			if(pAchievement->UpdateProgress(Criteria, Progress, ProgressType))
				Updated = true;
		}
	}

	// update the achievement progress in the database
	if(Updated)
	{
		pPlayer->GS()->Core()->SaveAccount(pPlayer, SAVE_ACHIEVEMENTS);
	}
}
