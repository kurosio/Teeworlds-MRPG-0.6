/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_DATA_H
#define GAME_SERVER_COMPONENT_ACCOUNT_DATA_H

// TODO: fully rework structures

#include <game/server/mmocore/Components/Auction/AuctionData.h>
#include <game/server/mmocore/Utils/FieldData.h>

class CGS;
class CPlayer;
class CHouseData;
class GroupData;

class CAccountData
{
	int m_ID {};
	int m_ClientID {};
	char m_aLogin[64] {};
	char m_aLastLogin[64] {};

	int m_Level {};
	int m_Exp {};
	CHouseData* m_pHouseData {};
	GroupData* m_pGroupData {};

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
	 * Group function: getters / setters
	 */
	int GetLevel() const { return m_Level; } // Returns the level of the player
	int GetExperience() const { return m_Exp; } // Returns the experience points of the player
	const char* GetLogin() const { return m_aLogin; } // Returns the login name of the player as a const char pointer
	const char* GetLastLoginDate() const { return m_aLastLogin; } // Returns the last login date of the player as a const char pointer

	bool IsPrisoned() const { return m_PrisonSeconds > 0; } // Checks if the player is currently in prison
	bool IsRelationshipsDeterioratedToMax() const { return m_Relations >= 100; } // Checks if the player's relationships have deteriorated to the maximum level

	void Prison(int Seconds); // Puts the player in prison for the specified number of seconds
	void AddExperience(int Value); // Adds the specified value to the player's experience points
	void AddGold(int Value) const; // Adds the specified value to the player's gold (currency)

	struct TimePeriods
	{
		time_t m_DailyStamp { };
		time_t m_WeekStamp { };
		time_t m_MonthStamp { };
	};

	// main
	int m_Relations {};
	int m_GuildID {};
	int m_GuildRank {};
	int m_PrisonSeconds {};
	TimePeriods m_Periods {};
	std::list< int > m_aHistoryWorld {};

	// upgrades
	int m_Upgrade {};
	std::map< AttributeIdentifier, int > m_aStats {};

	CTeeInfo m_TeeInfos {};
	int m_Team {};
	std::map < int, bool > m_aAetherLocation {};
	bool IsGuild() const { return m_GuildID > 0; }

	CFieldContainer m_MiningData
	{
		{ CFieldData<int>(JOB_LEVEL, "Level", "Miner level") },
		{ CFieldData<int>(JOB_EXPERIENCE, "Exp", "Miner experience") },
		{ CFieldData<int>(JOB_UPGRADES, "Upgrade", "Miner upgrades") }
	};

	CFieldContainer m_FarmingData
	{
		{ CFieldData<int>(JOB_LEVEL, "Level", "Farmer level") },
		{ CFieldData<int>(JOB_EXPERIENCE, "Exp", "Farmer experience") },
		{ CFieldData<int>(JOB_UPGRADES, "Upgrade", "Farmer upgrades") }
	};

	static std::map < int, CAccountData > ms_aData;
};

struct CAccountTempData
{
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

	// save pos teleport
	bool m_TempSafeSpawn;
	vec2 m_TempTeleportPos;

	// dungeon
	int m_TempTimeDungeon;
	bool m_TempDungeonReady;
	int m_TempTankVotingDungeon;
	bool m_TempAlreadyVotedDungeon;

	static std::map < int, CAccountTempData > ms_aPlayerTempData;
};

#endif