/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_DATA_H
#define GAME_SERVER_COMPONENT_ACCOUNT_DATA_H

// TODO: fully rework structures

#include <game/server/core/components/Auction/AuctionData.h>
#include <game/server/core/tools/dbfield.h>

class CGS;
class CPlayer;
class CHouseData;
class GroupData;
class CGuildData;
class CGuildMemberData;

class CAccountData
{
	ska::unordered_set< int > m_aAetherLocation {};

	int m_ID {};
	int m_ClientID {};
	char m_aLogin[64] {};
	char m_aLastLogin[64] {};
	int m_DailyChairGolds {};
	int m_Relations {};

	int m_Level {};
	int m_Exp {};
	CHouseData* m_pHouseData{};
	GroupData* m_pGroupData{};
	CGuildData* m_pGuildData{};

	CPlayer* m_pPlayer {};
	CGS* GS() const;
	CPlayer* GetPlayer() const { return m_pPlayer; };
public:
	/*
	 * Group functions: initialize or uniques from function
	 */
	void Init(int ID, CPlayer* pPlayer, const char* pLogin, std::string Language, std::string LoginDate, ResultPtr pResult); // Function to initialize
	void UpdatePointer(CPlayer* pPlayer);
	int GetID() const { return m_ID; } // Function to get the ID of an object

	/*
	 * Group functions: house system
	 */
	void ReinitializeHouse(); // This function re-initializes the house object
	CHouseData* GetHouse() const { return m_pHouseData; } // Get the house data for the current object
	bool HasHouse() const { return m_pHouseData != nullptr; } // Check if the current object has house data

	/*
	 * Group functions: group system
	 */
	void ReinitializeGroup(); // This function re-initializes the group object
	GroupData* GetGroup() const { return m_pGroupData; } // Get the group data for the current object
	bool HasGroup() const { return m_pGroupData != nullptr; } // Check if the current object has group data

	/*
	 * Group functions: guild system
	 */
	void ReinitializeGuild(bool SetNull = false);
	CGuildData* GetGuild() const { return m_pGuildData; }
	CGuildMemberData* GetGuildMemberData() const;
	bool HasGuild() const { return m_pGuildData != nullptr; }
	bool SameGuild(int ClientID) const;

	/*
	 * Group function: getters / setters
	 */
	int GetLevel() const { return m_Level; } // Returns the level of the player
	int GetExperience() const { return m_Exp; } // Returns the experience points of the player
	const char* GetLogin() const { return m_aLogin; } // Returns the login name of the player as a const char pointer
	const char* GetLastLoginDate() const { return m_aLastLogin; } // Returns the last login date of the player as a const char pointer
	int GetCurrentDailyChairGolds() const { return m_DailyChairGolds; } // Returns current daily chair golds
	int GetLimitDailyChairGolds() const; // Returns the daily limit of gold that a player can obtain from chairs
	int GetRelations() const { return m_Relations; } // Returns the relations

	bool IsPrisoned() const { return m_PrisonSeconds > 0; } // Checks if the player is currently in prison
	bool IsRelationshipsDeterioratedToMax() const { return m_Relations >= 100; } // Checks if the player's relationships have deteriorated to the maximum level

	void IncreaseRelations(int Relations); // This function increases the relations of an account by a given value
	void Imprison(int Seconds); // Puts the player in prison for the specified number of seconds
	void Unprison(); // Release the player from prison
	void AddExperience(int Value); // Adds the specified value to the player's experience points
	void AddGold(int Value) const; // Adds the specified value to the player's gold (currency)
	bool SpendCurrency(int Price, int CurrencyItemID = 1) const; // Returns a boolean value indicating whether the currency was successfully spent or not.
	void ResetDailyChairGolds(); // Reset daily getting chair golds
	void ResetRelations(); // Reset relations
	void HandleChair();

	// Aethers
	bool IsUnlockedAether(int AetherID) const { return m_aAetherLocation.find(AetherID) != m_aAetherLocation.end(); }
	void AddAether(int AetherID) { m_aAetherLocation.insert(AetherID); }
	void RemoveAether(int AetherID) { m_aAetherLocation.erase(AetherID); }
	ska::unordered_set< int >& GetAetherLocation() { return m_aAetherLocation; }

	struct TimePeriods
	{
		time_t m_DailyStamp { };
		time_t m_WeekStamp { };
		time_t m_MonthStamp { };
	};

	// main
	int m_GuildRank {};
	int m_PrisonSeconds {};
	TimePeriods m_Periods {};
	std::list< int > m_aHistoryWorld {};

	// upgrades
	int m_Upgrade {};
	std::map< AttributeIdentifier, int > m_aStats {};

	CTeeInfo m_TeeInfos {};
	int m_Team {};

	DBFieldContainer m_MiningData
	{
		{ DBField<int>(JOB_LEVEL, "Level", "Miner level") },
		{ DBField<int>(JOB_EXPERIENCE, "Exp", "Miner experience") },
		{ DBField<int>(JOB_UPGRADES, "Upgrade", "Miner upgrades") }
	};

	DBFieldContainer m_FarmingData
	{
		{ DBField<int>(JOB_LEVEL, "Level", "Farmer level") },
		{ DBField<int>(JOB_EXPERIENCE, "Exp", "Farmer experience") },
		{ DBField<int>(JOB_UPGRADES, "Upgrade", "Farmer upgrades") }
	};

	static std::map < int, CAccountData > ms_aData;
};

struct CAccountTempData
{
	int m_LastKilledByWeapon;
	int m_TempDecoractionID;
	int m_TempDecorationType;
	int m_TempID3;

	CAuctionSlot m_AuctionData;

	// temp rankname for guild rank settings
	char m_aRankGuildBuf[32];

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
	int m_TempTankVotingDungeon;
	bool m_TempAlreadyVotedDungeon;

	void SetTeleportPosition(vec2 Position) { m_TempTeleportPos = Position; }
	vec2 GetTeleportPosition() const { return m_TempTeleportPos; }
	void ClearTeleportPosition() { m_TempTeleportPos = { -1, -1 }; }

	static std::map < int, CAccountTempData > ms_aPlayerTempData;

private:
	vec2 m_TempTeleportPos{};
};

#endif