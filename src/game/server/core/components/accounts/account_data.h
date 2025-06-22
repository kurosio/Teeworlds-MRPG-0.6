/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_DATA_H
#define GAME_SERVER_COMPONENT_ACCOUNT_DATA_H

#include "bonus_manager.h"
#include "prison_manager.h"
#include "profession.h"
#include "rating_system.h"

#include <game/server/core/components/guilds/guild_data.h>
#include <game/server/core/components/auction/auction_data.h>

class CGS;
class CPlayer;
class CHouse;
class GroupData;
class CGuild;
class CGuildMemberData;

class CAccountData
{
	mutable ska::unordered_set< int > m_aAetherLocation {};
	mutable std::vector<CProfession> m_vProfessions {};
	int m_ClientID {};

	int m_ID {};
	std::string m_Login{};
	std::string m_LastLoginDate{};
	int m_CrimeScore {};
	int m_LastTickCrimeScoreChanges {};

	CHouse* m_pHouseData{};
	std::weak_ptr<GroupData> m_pGroupData;
	CGuild* m_pGuildData{};
	CProfession* m_pActiveProfession {};
	nlohmann::json m_AchievementsData { };
	BonusManager m_BonusManager{};
	PrisonManager m_PrisonManager{};
	BigInt m_Bank {};
	RatingSystem m_RatingSystem{};
	EquippedSlots m_EquippedSlots {};

	CGS* GS() const;
	CPlayer* GetPlayer() const;

public:
	// main
	struct TimePeriods
	{
		time_t m_DailyStamp { };
		time_t m_WeekStamp { };
		time_t m_MonthStamp { };
	};
	TimePeriods m_Periods {};
	CTeeInfo m_TeeInfos {};
	std::list< int > m_aHistoryWorld {};
	static std::map < int, CAccountData > ms_aData;

	PrisonManager& GetPrisonManager() { return m_PrisonManager; }
	const PrisonManager& GetPrisonManager() const { return m_PrisonManager; }

	BonusManager& GetBonusManager() { return m_BonusManager; }
	const BonusManager& GetBonusManager() const { return m_BonusManager; }

	RatingSystem& GetRatingSystem() { return m_RatingSystem; }
	const RatingSystem& GetRatingSystem() const { return m_RatingSystem; }

	EquippedSlots& GetEquippedSlots() { return m_EquippedSlots; }
	const EquippedSlots& GetEquippedSlots() const { return m_EquippedSlots; }

	void ChangeProfession(ProfessionIdentifier Profession);

	CProfession* GetActiveProfession() const
	{
		return m_pActiveProfession;
	}

	ProfessionIdentifier GetActiveProfessionID() const
	{
		return m_pActiveProfession ? m_pActiveProfession->GetProfessionID() : ProfessionIdentifier::None;
	}

	CProfession* GetProfession(ProfessionIdentifier Profession) const
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

	const CTeeInfo& GetTeeInfo() const;

	/*
	 * Group functions: initialize or uniques from function
	 */
	void Init(int ID, int ClientID, const char* pLogin, std::string Language, std::string LoginDate, ResultPtr pResult);
	void InitProfessions();
	void InitSharedEquipments(const std::string& EquippedSlots);
	void SaveSharedEquipments();
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
		const auto* pClassProfession = GetActiveProfession();
		return pClassProfession ? pClassProfession->GetLevel() : 1;
	}

	uint64_t GetExperience() const
	{
		const auto* pClassProfession = GetActiveProfession();
		return pClassProfession ? pClassProfession->GetExperience() : 0;
	}

	int GetTotalProfessionsUpgradePoints() const
	{
		return std::accumulate(m_vProfessions.begin(), m_vProfessions.end(), 0, [](int Total, const CProfession& Prof)
		{
			return Total + Prof.GetUpgradePoint();
		});
	}

	int GetClientID() const { return m_ClientID; }
	const char* GetLogin() const { return m_Login.c_str(); }
	const char* GetLastLoginDate() const { return m_LastLoginDate.c_str(); }

	void IncreaseCrime(int Score);
	void DecreaseCrime(int Score);
	bool IsCrimeDecreaseTime() const;
	bool IsCrimeMaxedOut() const { return m_CrimeScore >= 100; }
	int GetCrime() const { return m_CrimeScore; }
	void ResetCrimeScore();

	BigInt GetBankManager() const { return m_Bank; }
	int GetGold() const;
	BigInt GetTotalGold() const;
	int GetGoldCapacity() const;

	void AddExperience(uint64_t Value, bool ApplyBonuses = true) const;
	void AddGold(int Value, bool ApplyBonuses = false);
	void AddGoldToBank(int Amount);
	bool RemoveGoldFromBank(int Amount);
	bool SpendCurrency(int Price, int CurrencyItemID = 1); // Returns a boolean value indicating whether the currency was successfully spent or not.
	void HandleChair(int Level);

	// Achievements
	void InitAchievements(const std::string& Data);
	void UpdateAchievementProgress(int AchievementID, int Progress, bool Completed);
	nlohmann::json& GetAchievementsData() { return m_AchievementsData; }

	// Aethers
	bool IsUnlockedAether(int AetherID) const { return m_aAetherLocation.find(AetherID) != m_aAetherLocation.end(); }
	void AddAether(int AetherID) { m_aAetherLocation.insert(AetherID); }
	void RemoveAether(int AetherID) { m_aAetherLocation.erase(AetherID); }
	ska::unordered_set< int >& GetAethers() { return m_aAetherLocation; }

	// Equipments
	void AutoEquipSlots(bool OnlyEmptySlots);
	bool EquipItem(int ItemID);
	bool UnequipItem(int ItemID);
	bool IsAvailableEquipmentSlot(ItemType Type);
	std::optional<int> GetEquippedSlotItemID(ItemType Type) const;

	//
	int GetFreeSlotsAttributedModules() const;
	int GetFreeSlotsFunctionalModules() const;
	int GetUsedSlotsAttributedModules() const { return g_Config.m_SvAttributedModulesSlots - GetFreeSlotsAttributedModules(); }
	int GetUsedSlotsFunctionalModules() const { return g_Config.m_SvNonAttributedModulesSlots - GetFreeSlotsFunctionalModules(); }
};

struct CAccountSharedData
{
	int m_LastKilledByWeapon;
	CAuctionSlot m_TempAuctionSlot;

	// temp for searching
	char m_aGuildSearchBuf[32];
	char m_aPlayerSearchBuf[32];

	// player stats
	int m_Health;
	int m_Mana;
	int m_Ping;

	// dungeon
	int m_TempStartDungeonTick {};
	bool m_TempDungeonReady {};

	void SetSpawnPosition(vec2 Position)
	{
		m_TempSpawnPos = Position;
	}

	std::optional<vec2> GetSpawnPosition() const
	{
		return m_TempSpawnPos;
	}

	void ClearSpawnPosition()
	{
		m_TempSpawnPos = std::nullopt;
	}

	static std::map < int, CAccountSharedData > ms_aPlayerSharedData;

private:
	std::optional<vec2> m_TempSpawnPos{};
};

#endif