#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_BONUS_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_BONUS_MANAGER_H

class CPlayer;

enum
{
	BONUS_TYPE_EXPERIENCE = 1,
	BONUS_TYPE_GOLD,
	BONUS_TYPE_HP,
	BONUS_TYPE_MP,
	END_BONUS_TYPE,
};

struct TemporaryBonus
{
	int Type{};
	float Amount{};
	time_t StartTime{};
	int Duration{};

	void SetDuration(int days, int hours, int minutes, int seconds)
	{
		Duration = (days * 24 * 3600) + (hours * 3600) + (minutes * 60) + seconds;
	}

	void GetRemainingTimeFormatted(int* days, int* hours, int* minutes, int* seconds) const
	{
		int remaining = RemainingTime();
		if(days)
		{
			*days = remaining / (24 * 3600);
			remaining %= (24 * 3600);
		}

		if(hours)
		{
			*hours = remaining / 3600;
			remaining %= 3600;
		}

		if(minutes)
		{
			*minutes = remaining / 60;
			remaining %= 60;
		}

		if(seconds)
		{
			*seconds = remaining % 60;
		}
	}

	bool IsActive() const { return difftime(time(nullptr), StartTime) < Duration; }
	int RemainingTime() const { return maximum(0, Duration - static_cast<int>(difftime(time(nullptr), StartTime))); }
};

class BonusManager
{
	int m_ClientID{};
	std::vector<TemporaryBonus> m_vTemporaryBonuses{};

public:
	void Init(int ClientID)
	{
		m_ClientID = ClientID;
		Load();
	}

	const char* GetStringBonusType(int bonusType) const;
	void SendInfoAboutActiveBonuses() const;
	void AddBonus(const TemporaryBonus& bonus);
	void PostTick();

	template <typename T> requires std::is_integral_v<T>
	void ApplyBonuses(int bonusType, T* pValue, T* pBonusValue = nullptr) const
	{
		auto Result = (T)0;

		for(const TemporaryBonus& bonus : m_vTemporaryBonuses)
		{
			if(bonus.Type == bonusType)
			{
				if(pValue)
				{
					Result += maximum((T)1, (T)translate_to_percent_rest(*pValue, bonus.Amount));
					*pValue += Result;
				}
			}
		}

		if(pBonusValue)
		{
			*pBonusValue = Result;
		}
	}
	float GetTotalBonusPercentage(int bonusType) const;
	std::string GetBonusActivitiesString() const;
	std::vector<TemporaryBonus>& GetTemporaryBonuses() { return m_vTemporaryBonuses; }

private:
	void Load();
	void Save() const;
};

#endif