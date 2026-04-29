#include "balance.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>
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

	const float StatWeight = IsGroupScenario ? Scaling.GroupStatBaseWeight : Scaling.SoloStatBaseWeight;
	const int ValidPlayersNum = static_cast<int>(std::count_if(vpPlayers.begin(), vpPlayers.end(), [](const CPlayer* pPlayer)
	{
		return pPlayer != nullptr;
	}));
	if(ValidPlayersNum <= 0)
		return std::clamp(BasePower, Scaling.MinPower, Scaling.MaxPower);

	float AttributePowerPart = 0.f;
	for(const auto* pPlayer : vpPlayers)
	{
		if(!pPlayer)
			continue;

		int64_t TotalAttributeBudget = 0;
		for(int ID = (int)AttributeIdentifier::DMG; ID < (int)AttributeIdentifier::ATTRIBUTES_NUM; ID++)
		{
			const auto AttributeID = (AttributeIdentifier)ID;
			const auto* pGS = pPlayer->GS();
			const auto* pAttInfo = pGS->GetAttributeInfo(AttributeID);
			if(pAttInfo->IsGroup(AttributeGroup::Tank) || pAttInfo->IsGroup(AttributeGroup::Healer) || pAttInfo->IsGroup(AttributeGroup::Dps))
				TotalAttributeBudget += pPlayer->GetTotalRawAttributeValue(AttributeID);
		}
		AttributePowerPart += TotalAttributeBudget * (StatWeight * (float)BasePower);
	}

	const int FinalPower = static_cast<int>(AttributePowerPart);
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
		4.0f,  // DamagePercent
		15.0f, // DpsPercent
		20.0f, // HealerPercent
		30.0f, // BossDownscaleDivider
	};

	m_ScenarioMobPowerScaling = {
		1,      // MinPower
		10000,  // MaxPower
		0.0055f,  // SoloStatBaseWeight
		0.0055f,  // GroupStatBaseWeight
	};
}
