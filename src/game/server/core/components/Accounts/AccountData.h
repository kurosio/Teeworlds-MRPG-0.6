/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_DATA_H
#define GAME_SERVER_COMPONENT_ACCOUNT_DATA_H

// TODO: fully rework structures
#include "bonus_manager.h"
#include "prison_manager.h"
#include "profession.h"
#include <game/server/core/components/guilds/guild_data.h>
#include <game/server/core/components/auction/auction_data.h>
#include <game/server/class_data.h>

class CGS;
class CPlayer;
class CHouse;
class GroupData;
class CGuild;
class CGuildMemberData;

class CAccountData
{
	ska::unordered_set< int > m_aAetherLocation {};
	mutable std::vector<CProfession> m_vProfessions {};

	int m_ID {};
	int m_ClientID {};
	char m_aLogin[64] {};
	char m_aLastLogin[64] {};
	int m_CrimeScore {};

	CHouse* m_pHouseData{};
	std::weak_ptr<GroupData> m_pGroupData;
	CGuild* m_pGuildData{};
	CClassData m_Class {};
	nlohmann::json m_AchievementsData { };
	CBonusManager m_BonusManager{};
	CPrisonManager m_PrisonManager{};
	BigInt m_Bank {};

	CGS* GS() const;
	CPlayer* GetPlayer() const;

public:
	CPrisonManager& GetPrisonManager() { return m_PrisonManager; }
	const CPrisonManager& GetPrisonManager() const { return m_PrisonManager; }
	CBonusManager& GetBonusManager() { return m_BonusManager; }
	const CBonusManager& GetBonusManager() const { return m_BonusManager; }
	CClassData& GetClass() { return m_Class; }
	const CClassData& GetClass() const { return m_Class; }

	CProfession* GetClassProfession() const
	{
		return GetProfession(m_Class.GetProfessionID());
	}

	CProfession* GetProfession(Professions Profession) const
	{
		const auto it = std::ranges::find_if(m_vProfessions, [Profession](const CProfession& Prof)
		{
			return Prof.GetProfessionID() == Profession;
		});

		return it != m_vProfessions.end() ? &(*it) : nullptr;
	}

	std::vector<CProfession>& GetProfessions() const
	{
		return m_vProfessions;
	}

	/*
	 * Group functions: initialize or uniques from function
	 */
	void Init(int ID, int ClientID, const char* pLogin, std::string Language, std::string LoginDate, ResultPtr pResult);
	void InitProfessions();
	int GetID() const { return m_ID; }

	/*
	 * Group functions: house system
	 */
	void ReinitializeHouse(bool SetNull = false); // This function re-initializes the house object
	CHouse* GetHouse() const { return m_pHouseData; } // Get the house data for the current object
	bool HasHouse() const { return m_pHouseData != nullptr; } // Check if the current object has house data

	/*
	 * Group functions: group system
	 */
	void SetGroup(const std::shared_ptr<GroupData>& pGroupPtr) { m_pGroupData = pGroupPtr; }
	GroupData* GetGroup() const { return m_pGroupData.lock().get(); }
	bool HasGroup() const { return !m_pGroupData.expired(); }

	/*
	 * Group functions: guild system
	 */
	void ReinitializeGuild(bool SetNull = false);
	CGuild* GetGuild() const { return m_pGuildData; }
	CGuild::CMember* GetGuildMember() const;
	bool HasGuild() const { return m_pGuildData != nullptr; }
	bool IsClientSameGuild(int ClientID) const;
	bool IsSameGuild(int GuildID) const;

	/*
	 * Group function: getters / setters
	 */
	int GetLevel() const
	{
		const auto* pClassProfession = GetClassProfession();
		return pClassProfession ? pClassProfession->GetLevel() : 1;
	}

	uint64_t GetExperience() const
	{
		const auto* pClassProfession = GetClassProfession();
		return pClassProfession ? pClassProfession->GetExperience() : 0;
	}

	int GetTotalProfessionsUpgradePoints() const
	{
		return std::accumulate(m_vProfessions.begin(), m_vProfessions.end(), 0, [](int Total, const CProfession& Prof)
		{
			return Total + Prof.GetUpgradePoint();
		});
	}

	const char* GetLogin() const
	{
		return m_aLogin;
	}

	const char* GetLastLoginDate() const
	{
		return m_aLastLogin;
	}

	void IncreaseCrimeScore(int Score);
	bool IsCrimeScoreMaxedOut() const { return m_CrimeScore >= 100; }
	int GetCrimeScore() const { return m_CrimeScore; }
	void ResetCrimeScore();

	BigInt GetBank() const { return m_Bank; }
	int GetGold() const;
	BigInt GetTotalGold() const;
	int GetGoldCapacity() const;

	void AddExperience(uint64_t Value) const;
	void AddGold(int Value, bool ToBank = true, bool ApplyBonuses = false); // Adds the specified value to the player's gold (currency)
	bool DepositGoldToBank(int Amount);
	bool WithdrawGoldFromBank(int Amount);
	bool SpendCurrency(int Price, int CurrencyItemID = 1); // Returns a boolean value indicating whether the currency was successfully spent or not.
	void HandleChair(uint64_t Exp, int Gold);

	// Achievements
	void InitAchievements(const std::string& Data);
	void UpdateAchievementProgress(int AchievementID, int Progress, bool Completed);
	nlohmann::json& GetAchievementsData() { return m_AchievementsData; }

	// Aethers
	bool IsUnlockedAether(int AetherID) const { return m_aAetherLocation.find(AetherID) != m_aAetherLocation.end(); }
	void AddAether(int AetherID) { m_aAetherLocation.insert(AetherID); }
	void RemoveAether(int AetherID) { m_aAetherLocation.erase(AetherID); }
	ska::unordered_set< int >& GetAethers() { return m_aAetherLocation; }

	struct TimePeriods
	{
		time_t m_DailyStamp { };
		time_t m_WeekStamp { };
		time_t m_MonthStamp { };
	};

	// main
	TimePeriods m_Periods {};
	std::list< int > m_aHistoryWorld {};

	CTeeInfo m_TeeInfos {};
	static std::map < int, CAccountData > ms_aData;
};

struct CAccountTempData
{
	int m_LastKilledByWeapon;
	CAuctionSlot m_TempAuctionSlot;

	// temp for searching
	char m_aGuildSearchBuf[32];
	char m_aPlayerSearchBuf[32];

	// player stats
	int m_TempHealth;
	int m_TempMana;
	int m_TempPing;

	// dungeon
	int m_TempTimeDungeon;
	bool m_TempDungeonReady;

	void SetTeleportPosition(vec2 Position) { m_TempTeleportPos = Position; }
	vec2 GetTeleportPosition() const { return m_TempTeleportPos; }
	void ClearTeleportPosition() { m_TempTeleportPos = { -1, -1 }; }

	static std::map < int, CAccountTempData > ms_aPlayerTempData;

private:
	vec2 m_TempTeleportPos{};
};

#endif