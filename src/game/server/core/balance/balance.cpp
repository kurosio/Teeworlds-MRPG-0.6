#include "balance.h"

#include <game/server/player.h>

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

int Balance::CalculateScenarioMobPower(const std::vector<CPlayer*>& vpPlayers, int BasePower, bool IsGroupScenario) const
{
	const auto& Scaling = m_ScenarioMobPowerScaling;
	if(vpPlayers.empty())
		return std::clamp(BasePower, Scaling.MinPower, Scaling.MaxPower);

	const float StatWeight = IsGroupScenario ? Scaling.GroupStatWeight : Scaling.SoloStatWeight;
	const int ValidPlayersNum = static_cast<int>(std::count_if(vpPlayers.begin(), vpPlayers.end(), [](const CPlayer* pPlayer)
	{
		return pPlayer != nullptr;
	}));
	if(ValidPlayersNum <= 0)
		return std::clamp(BasePower, Scaling.MinPower, Scaling.MaxPower);

	int64_t TotalAttributeBudget = 0;
	for(const auto* pPlayer : vpPlayers)
	{
		if(!pPlayer)
			continue;

		for(int ID = (int)AttributeIdentifier::DMG; ID < (int)AttributeIdentifier::ATTRIBUTES_NUM; ID++)
		{
			const auto AttributeID = (AttributeIdentifier)ID;
			TotalAttributeBudget += pPlayer->GetTotalRawAttributeValue(AttributeID);
		}
	}

	const float AverageAttributeBudget = static_cast<float>(TotalAttributeBudget) / static_cast<float>(ValidPlayersNum);
	const float AttributePowerPart = (AverageAttributeBudget / std::max(1.0f, Scaling.AvgAttributeDivider)) * StatWeight;
	const int GroupPlayersBonus = IsGroupScenario ? std::max(0, ValidPlayersNum - 1) * Scaling.GroupPerPlayerBonus : 0;
	const int FinalPower = BasePower + static_cast<int>(AttributePowerPart) + GroupPlayersBonus;
	return std::clamp(FinalPower, Scaling.MinPower, Scaling.MaxPower);
}

void Balance::Initialize()
{
	m_AttributeCaps.fill(0.0f);
	m_AttributeCaps[ToIndex(AttributeIdentifier::AttackSPD)] = 600.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::AmmoRegen)] = 800.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::Vampirism)] = 30.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::Crit)] = 30.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::Lucky)] = 20.0f;
	m_AttributeCaps[ToIndex(AttributeIdentifier::LuckyDropItem)] = 30.0f;

	m_AttributeBase.fill(0);
	m_AttributeBase[ToIndex(AttributeIdentifier::HP)] = 10;
	m_AttributeBase[ToIndex(AttributeIdentifier::MP)] = 10;

	m_BotGroupScaling = {
		5.0f,  // DamagePercent
		15.0f, // DpsPercent
		20.0f, // HealerPercent
		10.0f, // BossDownscaleDivider
	};

	m_ScenarioMobPowerScaling = {
		1,      // MinPower
		10000,  // MaxPower
		0.50f,  // SoloStatWeight
		0.75f,  // GroupStatWeight
		1,      // GroupPerPlayerBonus
		85.0f,  // AvgAttributeDivider
	};
}
