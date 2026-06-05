#include "balance.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

// tools
namespace
{
	static float DiminishingReturns(float RawValue, float Knee)
	{
		if(RawValue <= 0.0f || Knee <= 0.0f)
			return 0.0f;
		return Knee * std::log1pf(RawValue / Knee);
	}
};

// implement
Balance& Balance::Get()
{
	static Balance Instance;
	return Instance;
}

void Balance::Init()
{
	static std::once_flag s_OnceFlag;
	std::call_once(s_OnceFlag, []
	{
		Balance& Instance = Get();
		Instance.Initialize();
	});
}

size_t Balance::ToIndex(AttributeIdentifier ID)
{
	const auto Index = static_cast<int>(ID);
	if(Index < 0 || Index >= static_cast<int>(kAttributeCount))
		return 0;

	return static_cast<size_t>(Index);
}

float Balance::GetAttributeCap(AttributeIdentifier ID) const
{
	return m_AttributeCaps[ToIndex(ID)];
}

int Balance::GetAttributeBase(AttributeIdentifier ID) const
{
	return m_AttributeBase[ToIndex(ID)];
}

float Balance::GetBotGroupPercent(AttributeGroup Group) const
{
	switch(Group)
	{
		case AttributeGroup::DamageType:
			return m_BotGroupScaling.DamagePercent;
		case AttributeGroup::Dps:
			return m_BotGroupScaling.DpsPercent;
		case AttributeGroup::Healer:
			return m_BotGroupScaling.HealerPercent;
		default:
			return 100.0f;
	}
}

float Balance::GetBotBossDownscaleDivider() const
{
	return m_BotGroupScaling.BossDownscaleDivider;
}

bool Balance::IsAttributeRelevantForMobPower(AttributeIdentifier ID) const
{
	switch(ID)
	{
		case AttributeIdentifier::Crit:
		case AttributeIdentifier::Vampirism:
		case AttributeIdentifier::Lucky:
		case AttributeIdentifier::LuckyDropItem:
		case AttributeIdentifier::AttackSPD:
		case AttributeIdentifier::AmmoRegen:
			return false;
		default:
			return true;
	}
}

int Balance::CalculateScenarioMobPower(const std::vector<CPlayer*>& vpPlayers, int BasePower, bool IsGroupScenario) const
{
	const auto& MobScalingData = m_ScenarioMobPowerScaling;
	const int ClampedBasePower = std::clamp(BasePower, MobScalingData.MinPower, MobScalingData.MaxPower);

	if(vpPlayers.empty())
		return ClampedBasePower;

	static thread_local std::vector<AttributeIdentifier> RelevantAttrs;
	RelevantAttrs.clear();

	float TotalBudgetSum = 0.0f;
	float MaxPlayerBudget = 0.0f;
	int ValidPlayersNum = 0;
	for(const auto* pPlayer : vpPlayers)
	{
		if(!pPlayer)
			continue;

		// only once initialize
		if(RelevantAttrs.empty())
		{
			RelevantAttrs.reserve(static_cast<size_t>(AttributeIdentifier::ATTRIBUTES_NUM));
			for(int AttID = (int)AttributeIdentifier::DMG; AttID < (int)AttributeIdentifier::ATTRIBUTES_NUM; ++AttID)
			{
				const auto AttrID = (AttributeIdentifier)AttID;
				if(!IsAttributeRelevantForMobPower(AttrID))
					continue;

				const auto* pInfo = pPlayer->GS()->GetAttributeInfo(AttrID);
				if(pInfo && (pInfo->IsGroup(AttributeGroup::Tank) || pInfo->IsGroup(AttributeGroup::Healer) || pInfo->IsGroup(AttributeGroup::Dps)))
					RelevantAttrs.push_back(AttrID);
			}
		}

		// calculate player stat
		int64_t PlayerBudget = 0;
		for(const auto& AttrID : RelevantAttrs)
			PlayerBudget += pPlayer->GetTotalRawAttributeValue(AttrID);

		// getting best player stats
		const auto CurrentPlayerBudget = (float)PlayerBudget;
		TotalBudgetSum += CurrentPlayerBudget;
		if(CurrentPlayerBudget > MaxPlayerBudget)
			MaxPlayerBudget = CurrentPlayerBudget;

		// player count
		ValidPlayersNum++;
	}

	// is no valid players
	if(ValidPlayersNum <= 0)
		return ClampedBasePower;

	// anti-carry formula:
	const float AverageBudget = TotalBudgetSum / (float)ValidPlayersNum;
	const float BaseBudgetForCalculation = (AverageBudget * 0.7f) + (MaxPlayerBudget * 0.3f);
	const float EffectiveBudget = DiminishingReturns(BaseBudgetForCalculation, MobScalingData.DiminishingKnee);
	const float Weight = IsGroupScenario ? MobScalingData.GroupStatBaseWeight : MobScalingData.SoloStatBaseWeight;
	const float GroupMultiplier = IsGroupScenario ? std::sqrt((float)ValidPlayersNum) : 1.0f;
	const float BonusPercent = (EffectiveBudget / MobScalingData.DiminishingKnee) * Weight * GroupMultiplier;
	const float ClampedBonusPercent = std::clamp(BonusPercent, 0.0f, 3.0f);
	const float FinalPowerFloat = (float)ClampedBasePower * (1.0f + ClampedBonusPercent);
	const int FinalPower = round_to_int(FinalPowerFloat);
	return std::clamp(FinalPower, ClampedBasePower, MobScalingData.MaxPower);
}


void Balance::Initialize()
{
	// cap level
	m_AttributeCaps.fill(0.0f);
	m_AttributeCaps[ToIndex(AttributeIdentifier::AttackSPD)] = 600.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::AmmoRegen)] = 800.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::Vampirism)] = 30.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::Crit)] = 30.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::Lucky)] = 20.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::LuckyDropItem)] = 30.0f;

	// default level
	m_AttributeBase.fill(0);
	m_AttributeBase[ToIndex(AttributeIdentifier::HP)] = 10;
	m_AttributeBase[ToIndex(AttributeIdentifier::MP)] = 10;

	// mob scaling
	m_BotGroupScaling = {
		2.5f,   // DamagePercent
		10.0f,  // DpsPercent
		20.0f,  // HealerPercent
		30.0f,  // BossDownscaleDivider
	};

	// scenario mob scaling
	m_ScenarioMobPowerScaling = {
		1,      // MinPower
		10000,   // MaxPower
		0.35f,   // SoloStatBaseWeight
		0.25f,   // GroupStatBaseWeight
		1500.0f,// DiminishingKnee
	};
}