#include "balance.h"

#include <mutex>

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

Balance::DungeonFactor Balance::GetDungeonFactor(AttributeIdentifier ID, bool IsBoss) const
{
	DungeonFactor Result {};
	if(IsDamageAttributeIdentifier(ID))
	{
		Result.BaseFactor = m_DungeonFactors.DamagePercent / 100.0f;
	}
	else if(ID == AttributeIdentifier::Crit)
	{
		Result.BaseFactor = m_DungeonFactors.CritPercent / 100.0f;
	}
	else if(IsBoss && ID == AttributeIdentifier::HP)
	{
		Result.BaseFactor = m_DungeonFactors.BossHpPercent / 100.0f;
		Result.MinValue = m_DungeonFactors.BossMinValue;
		return Result;
	}
	else
	{
		Result.BaseFactor = m_DungeonFactors.OtherPercent / 100.0f;
	}

	Result.MinValue = m_DungeonFactors.MinValue;
	return Result;
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

	m_DungeonFactors = {
		15.0f,  // DamagePercent
		7.0f,   // CritPercent
		50.0f,  // OtherPercent
		300.0f, // BossHpPercent
		5,      // MinValue
		5,      // BossMinValue
	};

	m_BotGroupScaling = {
		5.0f,  // DamagePercent
		15.0f, // DpsPercent
		20.0f, // HealerPercent
		10.0f, // BossDownscaleDivider
	};
}
