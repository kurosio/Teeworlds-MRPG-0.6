#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_BONUS_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_BONUS_MANAGER_H

#include <algorithm>
#include <ctime>
#include <string>
#include <vector>

#include <game/server/multipliers.h>

struct TemporaryBonus
{
	MultiplierType Type{ MultiplierType::Invalid };
	float Amount{};
	time_t StartTime{};
	int Duration{};
	MultiplierSource Source{ MultiplierSource::BonusItem };

	void SetDuration(int Days, int Hours, int Minutes, int Seconds)
	{
		Duration = Days * 86400 + Hours * 3600 + Minutes * 60 + Seconds;
	}

	void GetRemainingTimeFormatted(int* pDays, int* pHours, int* pMinutes, int* pSeconds) const
	{
		int Rem = RemainingTime();
		if (pDays) { *pDays = Rem / 86400; Rem %= 86400; }
		if (pHours) { *pHours = Rem / 3600; Rem %= 3600; }
		if (pMinutes) { *pMinutes = Rem / 60; Rem %= 60; }
		if (pSeconds) *pSeconds = Rem;
	}

	bool IsActive() const { return difftime(time(nullptr), StartTime) < Duration; }
	int RemainingTime() const { return std::max(0, Duration - static_cast<int>(difftime(time(nullptr), StartTime))); }
};

class BonusManager
{
	int m_ClientID{};
	std::vector<TemporaryBonus> m_vTemporaryBonuses{};
	CMultiplierManager m_TimedMultipliers{};

public:
	void Init(int ClientID)
	{
		m_ClientID = ClientID;
		Load();
	}

	void SendInfoAboutActiveBonuses() const;
	void AddBonus(const TemporaryBonus& Bonus);
	void PostTick();

	template<typename T> requires std::is_integral_v<T>
	void ApplyBonuses(MultiplierType Type, T* pValue, T* pBonusValue = nullptr) const
	{
		CMultiplierManager::ApplyPercent(GetTotalBonusPercentage(Type), pValue, pBonusValue);
	}

	float GetTotalBonusPercentage(MultiplierType Type) const;
	std::vector<MultiplierType> GetActiveMultiplierTypes() const;
	std::string GetBonusActivitiesString() const;
	const std::vector<TemporaryBonus>& GetTemporaryBonuses() const { return m_vTemporaryBonuses; }
	const CMultiplierManager& GetTimedMultipliers() const { return m_TimedMultipliers; }

private:
	void Load();
	void Save() const;
	void RebuildTimedMultipliers();
};

#endif