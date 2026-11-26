#include "achievement_listener.h"

#include <game/server/gamecontext.h>

#include <components/crafting/craft_data.h>

CAchievementListener g_AchievementListener;

void CAchievementListener::Initialize()
{
	g_EventListenerManager.RegisterListener(IEventListener::CharacterDamage, this);
	g_EventListenerManager.RegisterListener(IEventListener::CharacterDeath, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerGotItem, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerCraftItem, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerEquipItem, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerProfessionLeveling, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerQuestChangeState, this);
}

void CAchievementListener::OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage)
{
	if(pFrom && pFrom != pTo && !pFrom->IsBot())
		UpdateAchievement(pFrom, AchievementType::TotalDamage, NOPE, Damage, EAchievementProgress::Accumulate);
}

void CAchievementListener::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	if(pVictim && !pVictim->IsBot() && Weapon != WEAPON_WORLD)
		UpdateAchievement(pVictim, AchievementType::Death, NOPE, 1, EAchievementProgress::Accumulate);

	if(pKiller && pVictim && pVictim != pKiller)
	{
		if(pVictim->IsBot() && !pKiller->IsBot())
		{
			const auto VictimBotID = dynamic_cast<CPlayerBot*>(pVictim)->GetBotID();
			UpdateAchievement(pKiller, AchievementType::DefeatMob, VictimBotID, 1, EAchievementProgress::Accumulate);
			UpdateAchievement(pKiller, AchievementType::DefeatPVE, NOPE, 1, EAchievementProgress::Accumulate);
		}

		if(!pVictim->IsBot() && !pKiller->IsBot())
			UpdateAchievement(pKiller, AchievementType::DefeatPVP, NOPE, 1, EAchievementProgress::Accumulate);
	}
}

void CAchievementListener::OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
	UpdateAchievement(pPlayer, AchievementType::Equip, pItem->GetID(), 1, EAchievementProgress::Absolute);
}

void CAchievementListener::OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel)
{
	UpdateAchievement(pPlayer, AchievementType::Leveling, (int)pProfession->GetProfessionID(), NewLevel, EAchievementProgress::Absolute);
}

void CAchievementListener::OnPlayerQuestChangeState(CPlayer* pPlayer, CPlayerQuest* pQuest, QuestState NewState)
{
	if(pQuest->IsCompleted())
		UpdateAchievement(pPlayer, AchievementType::CompleteQuest, pQuest->GetID(), 1, EAchievementProgress::Absolute);
}

void CAchievementListener::OnPlayerGotItem(CPlayer* pPlayer, CPlayerItem* pItem, int Got)
{
	UpdateAchievement(pPlayer, AchievementType::ReceiveItem, pItem->GetID(), Got, EAchievementProgress::Accumulate);
	UpdateAchievement(pPlayer, AchievementType::HaveItem, pItem->GetID(), pItem->GetValue(), EAchievementProgress::Absolute);
}

void CAchievementListener::OnPlayerCraftItem(CPlayer* pPlayer, CCraftItem* pCraft, int Amount, int Extra)
{
	UpdateAchievement(pPlayer, AchievementType::CraftItem, pCraft->GetID(), Amount + Extra, EAchievementProgress::Accumulate);
}

void CAchievementListener::UpdateAchievement(CPlayer* pPlayer, AchievementType Type, int Criteria, int Progress, EAchievementProgress Mode)
{
	if(!pPlayer || pPlayer->IsBot() || !pPlayer->IsAuthed())
		return;

	// Listeners are registered before the descriptions are loaded in OnPreInit(),
	// so the lookup table is built on the first event instead.
	if(!m_IndexBuilt && !CAchievementInfo::Data().empty())
	{
		for(const auto* pInfo : CAchievementInfo::Data())
			m_Index[{ (int)pInfo->GetType(), pInfo->GetCriteria() }].push_back(pInfo->GetID());
		m_IndexBuilt = true;
	}

	const auto ClientIt = CAchievement::Data().find(pPlayer->GetCID());
	if(ClientIt == CAchievement::Data().end())
		return;

	const auto& vAchievements = ClientIt->second;
	const int KeyCriteria = Criteria > 0 ? Criteria : 0;

	auto UpdateByCriteria = [&](int Criterion)
	{
		const auto It = m_Index.find({ (int)Type, Criterion });
		if(It == m_Index.end())
			return;

		for(const int AchievementID : It->second)
		{
			const auto AchievementIt = vAchievements.find(AchievementID);
			if(AchievementIt != vAchievements.end() && !AchievementIt->second->IsCompleted())
				AchievementIt->second->UpdateProgress(Criteria, Progress, Mode);
		}
	};

	UpdateByCriteria(KeyCriteria);
	if(KeyCriteria != 0)
		UpdateByCriteria(0); // achievements without a concrete criteria match any event
}
