#ifndef GAME_SERVER_MULTIPLIERS_H
#define GAME_SERVER_MULTIPLIERS_H

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

enum class MultiplierType : int
{
	Invalid = 0,
	Experience = 1,
	Gold = 2,
	Health = 3,
	Mana = 4,
	MobDrop = 5,
	MiningDrop = 6,
	FarmingDrop = 7,
	FishingDrop = 8,
	SkillPointDrop = 9,
	Num,
};

enum class MultiplierSource : int
{
	World = 0,
	RandomEvent = 1,
	BonusItem = 2,
	Other = 3,
	Num,
};

struct MultiplierContribution
{
	MultiplierType m_Type{ MultiplierType::Invalid };
	MultiplierSource m_Source{ MultiplierSource::Other };
	float m_Percent{};
	std::string m_Name{};
};

class CMultiplierManager
{
	std::vector<MultiplierContribution> m_vContributions{};

	static std::string GetContributionName(MultiplierSource Source, std::string_view Name)
	{
		return Name.empty() ? std::string(GetSourceName(Source)) : std::string(Name);
	}

public:
	static constexpr bool IsValidType(MultiplierType Type)
	{
		return Type > MultiplierType::Invalid && Type < MultiplierType::Num;
	}

	static constexpr bool IsValidSource(MultiplierSource Source)
	{
		return Source == MultiplierSource::World || Source == MultiplierSource::RandomEvent
			|| Source == MultiplierSource::BonusItem || Source == MultiplierSource::Other;
	}

	static float ClampPercent(long double Percent)
	{
		constexpr auto Max = static_cast<long double>(std::numeric_limits<float>::max());
		return static_cast<float>(std::clamp(Percent, -Max, Max));
	}

	static std::optional<MultiplierType> FromId(int Value)
	{
		const auto Type = static_cast<MultiplierType>(Value);
		return IsValidType(Type) ? std::optional{ Type } : std::nullopt;
	}

	static const char* GetTypeName(MultiplierType Type)
	{
		switch (Type)
		{
		case MultiplierType::Experience: return "EXP";
		case MultiplierType::Gold: return "Gold";
		case MultiplierType::Health: return "HP";
		case MultiplierType::Mana: return "MP";
		case MultiplierType::MobDrop: return "Mob drop";
		case MultiplierType::MiningDrop: return "Mining drop";
		case MultiplierType::FarmingDrop: return "Farming drop";
		case MultiplierType::FishingDrop: return "Fishing drop";
		case MultiplierType::SkillPointDrop: return "Skill point drop";
		default: return "Unknown";
		}
	}

	static const char* GetSourceName(MultiplierSource Source)
	{
		switch (Source)
		{
		case MultiplierSource::World: return "World rate";
		case MultiplierSource::RandomEvent: return "Random event";
		case MultiplierSource::BonusItem: return "Bonus item";
		default: return "Other";
		}
	}

	static std::optional<MultiplierType> ParseType(std::string_view Name)
	{
		std::string Normalized;
		Normalized.reserve(Name.size());
		for (const unsigned char Ch : Name)
		{
			if (Ch == ' ' || Ch == '-')
				Normalized.push_back('_');
			else
				Normalized.push_back(static_cast<char>(std::tolower(Ch)));
		}

		if (Normalized == "experience") return MultiplierType::Experience;
		if (Normalized == "gold") return MultiplierType::Gold;
		if (Normalized == "health") return MultiplierType::Health;
		if (Normalized == "mana") return MultiplierType::Mana;
		if (Normalized == "mob_drop") return MultiplierType::MobDrop;
		if (Normalized == "mining_drop") return MultiplierType::MiningDrop;
		if (Normalized == "farming_drop") return MultiplierType::FarmingDrop;
		if (Normalized == "fishing_drop") return MultiplierType::FishingDrop;
		if (Normalized == "skill_point_drop") return MultiplierType::SkillPointDrop;
		return std::nullopt;
	}

	static std::optional<MultiplierSource> SourceFromId(int Value)
	{
		const auto Source = static_cast<MultiplierSource>(Value);
		return IsValidSource(Source) ? std::optional{ Source } : std::nullopt;
	}

	void Clear() { m_vContributions.clear(); }

	void AddMultiplier(MultiplierType Type, MultiplierSource Source, float Percent, std::string_view Name = {})
	{
		if (!IsValidType(Type) || !IsValidSource(Source) || !std::isfinite(Percent) || Percent == 0.0f)
			return;
		m_vContributions.push_back({ Type, Source, Percent, GetContributionName(Source, Name) });
	}

	void SetMultiplier(MultiplierType Type, MultiplierSource Source, float Percent, std::string_view Name = {})
	{
		if (!IsValidType(Type) || !IsValidSource(Source) || !std::isfinite(Percent))
			return;

		RemoveMultiplier(Type, Source);
		if (Percent != 0.0f)
			m_vContributions.push_back({ Type, Source, Percent, GetContributionName(Source, Name) });
	}

	void RemoveMultiplier(MultiplierType Type, MultiplierSource Source)
	{
		std::erase_if(m_vContributions, [Type, Source](const MultiplierContribution& E) {
			return E.m_Type == Type && E.m_Source == Source;
			});
	}

	void RemoveSource(MultiplierSource Source)
	{
		std::erase_if(m_vContributions, [Source](const MultiplierContribution& E) {
			return E.m_Source == Source;
			});
	}

	float GetTotalPercent(MultiplierType Type) const
	{
		long double Total{};
		for (const auto& E : m_vContributions)
			if (E.m_Type == Type)
				Total += E.m_Percent;
		return ClampPercent(Total);
	}

	float GetSourcePercent(MultiplierType Type, MultiplierSource Source) const
	{
		long double Total{};
		for (const auto& E : m_vContributions)
			if (E.m_Type == Type && E.m_Source == Source)
				Total += E.m_Percent;
		return ClampPercent(Total);
	}

	const std::vector<MultiplierContribution>& GetContributions() const { return m_vContributions; }

	template<typename T> requires std::is_integral_v<T>
	static void ApplyPercent(float Percent, T* pValue, T* pBonusValue = nullptr)
	{
		if (!pValue || !std::isfinite(Percent) || Percent <= 0.0f)
			return;
		if constexpr (std::is_signed_v<T>)
		{
			if (*pValue <= 0) return;
		}
		else if (*pValue == 0)
			return;

		const T MaxValue = std::numeric_limits<T>::max();
		if (*pValue >= MaxValue)
			return;

		const T MaxBonus = MaxValue - *pValue;
		const auto RawBonus = (static_cast<long double>(*pValue) * Percent) / 100.0L;
		const T Calculated = RawBonus >= static_cast<long double>(MaxBonus) ? MaxBonus : static_cast<T>(RawBonus);
		const T Bonus = std::min(MaxBonus, std::max(static_cast<T>(1), Calculated));
		*pValue += Bonus;
		if (pBonusValue)
			*pBonusValue = std::min(MaxValue, *pBonusValue + Bonus);
	}

	template<typename T> requires std::is_integral_v<T>
	void Apply(MultiplierType Type, T* pValue, T* pBonusValue = nullptr) const
	{
		ApplyPercent(GetTotalPercent(Type), pValue, pBonusValue);
	}
};

#endif