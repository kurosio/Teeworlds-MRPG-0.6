/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>
#include <generated/server_data.h>

#include <components/accounts/account_data.h>
#include <components/inventory/item_data.h>

#include "achievement_data.h"
#include "achievement_db.h"

namespace
{
	std::string FormatRewardText(const nlohmann::json& RewardData, int Points)
	{
		std::string Text;
		auto AddEntry = [&](std::string Entry)
		{
			if(!Text.empty())
				Text += ", ";
			Text += std::move(Entry);
		};

		if(!RewardData.empty())
		{
			if(const int Experience = RewardData.value("exp", 0); Experience > 0)
				AddEntry("Exp " + std::to_string(Experience));

			const CItemsContainer Items = RewardData.value("items", CItemsContainer {});
			for(const auto& Item : Items)
			{
				if(Item.IsValid())
					AddEntry(std::string(Item.Info()->GetName()) + " x" + std::to_string(Item.GetValue()));
			}
		}

		if(Points > 0)
			AddEntry("Points " + std::to_string(Points) + "p");

		return Text;
	}
}

CGS* CAchievement::GS() const { return (CGS*)Instance::GameServerPlayer(m_ClientID); }

CPlayer* CAchievement::GetPlayer() const
{
	auto* pGS = GS();
	return pGS ? pGS->GetPlayer(m_ClientID) : nullptr;
}

void CAchievementInfo::Init(const std::string& Name, AchievementType Type, int Criteria, int Required,
	const std::string& RewardData, int Point)
{
	m_Name = Name;
	m_Type = Type;
	m_Criteria = Criteria;
	// Required == 0 is used by the data for "complete once" quests. Normalizing
	// it here keeps one completion rule for every achievement type.
	m_Required = maximum(1, Required);
	m_Point = Point;

	mystd::json::parse(RewardData, [this](nlohmann::json& Json)
	{
		m_RewardData = std::move(Json);
	});
}

void CAchievementInfo::BuildRewardText()
{
	m_RewardText = FormatRewardText(m_RewardData, m_Point);
}

bool CAchievement::UpdateProgress(int Criteria, int Progress, EAchievementProgress Mode)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer || m_Completed || !m_pInfo->MatchesCriteria(Criteria))
		return false;

	const int PreviousProgress = m_Progress;
	switch(Mode)
	{
		case EAchievementProgress::Absolute: m_Progress = Progress; break;
		case EAchievementProgress::Accumulate: m_Progress += Progress; break;
	}
	m_Progress = clamp(m_Progress, 0, m_pInfo->GetRequired());
	if(m_Progress == PreviousProgress)
		return true;

	bool CompletedNow = false;
	if(m_pInfo->IsProgressEnough(m_Progress))
	{
		GS()->CreateHammerHit(pPlayer->m_ViewPos);
		GS()->Chat(-1, "'{~}' has completed the achievement '{}'!", Server()->ClientName(m_ClientID), m_pInfo->GetName());
		GS()->CreatePlayerSound(m_ClientID, SOUND_GAME_DONE);

		m_Completed = true;
		CompletedNow = true;
		RewardPlayer(pPlayer);
	}
	else
	{
		NotifyPlayerProgress();
	}

	auto* pAccount = pPlayer->Account();
	if(!pAccount)
		return true;

	// The whole chain shares one database row. Only its last level writes
	// progress, so a lower level cannot overwrite it with a smaller clamped
	// value. Finishing a lower level is stored separately: absolute progress can
	// legitimately decrease, but its reward must not be granted twice.
	if(CompletedNow && !m_pInfo->IsChainTail())
	{
		AchievementDb::SaveCompletedLevel(pAccount->GetID(), (int)m_pInfo->GetType(),
			m_pInfo->GetCriteria(), m_pInfo->GetID());
	}

	const int CurrentTick = Server()->Tick();
	const int SaveIntervalTicks = Server()->TickSpeed() * 5;
	const bool ShouldPersist = m_pInfo->IsChainTail()
		&& (CompletedNow || m_LastSaveTick == 0 || CurrentTick - m_LastSaveTick >= SaveIntervalTicks);
	if(ShouldPersist)
	{
		const int CompletedLevel = m_Completed ? m_pInfo->GetID() : 0;
		AchievementDb::SaveProgress(pAccount->GetID(), (int)m_pInfo->GetType(),
			m_pInfo->GetCriteria(), m_Progress, CompletedLevel);
		m_LastSaveTick = CurrentTick;
	}
	return true;
}

void CAchievement::RewardPlayer(CPlayer* pPlayer) const
{
	const auto& RewardData = m_pInfo->GetRewardData();
	if(!RewardData.empty())
	{
		if(const int Experience = RewardData.value("exp", 0); Experience > 0)
			pPlayer->Account()->AddExperience(Experience);

		const CItemsContainer Items = RewardData.value("items", CItemsContainer {});
		for(const auto& Item : Items)
		{
			if(Item.IsValid())
				pPlayer->GetItem(Item)->Add(Item.GetValue());
		}
	}

	if(const int Points = m_pInfo->GetPoint(); Points > 0)
		pPlayer->GetItem(itAchievementPoint)->Add(Points);
}

void CAchievement::NotifyPlayerProgress()
{
	const int Percent = translate_to_percent(m_pInfo->GetRequired(), m_Progress);
	if(Percent > 80 && !m_NotifiedSoonComplete)
	{
		m_NotifiedSoonComplete = true;
		GS()->Chat(m_ClientID, "Achievement '{}' will be soon completed!", m_pInfo->GetName());
	}
}
