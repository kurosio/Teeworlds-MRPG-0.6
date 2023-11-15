/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_DATA_H
#define GAME_SERVER_COMPONENT_ACCOUNT_DATA_H

// TODO: fully rework structures

#include <game/server/mmocore/Components/Auction/AuctionData.h>
#include <game/server/mmocore/Utils/FieldData.h>

class CHouseData;
class GroupData;

class CAccountData
{
	int m_ID {};
	char m_aLogin[64] {};
	char m_aLastLogin[64] {};

	class CHouseData* m_pHouseData {};
	class GroupData* m_pGroupData {};

public:
	/*
	 * Group functions: initialize or uniques from function
	 */
	void Init(int ID, int ClientID, const char* pLogin, std::string Language, std::string LoginDate, ResultPtr pResult); // Function to initialize
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
	GroupData* GetGroup() const { return m_pGroupData; }; // Get the group data for the current object
	bool HasGroup() const { return m_pGroupData != nullptr; } // Check if the current object has group data

	/*
	 * Group function: getters / setters
	 */
	const char* GetLogin() const { return m_aLogin; } // Get the login name as a const char pointer
	const char* GetLastLoginDate() const { return m_aLastLogin; } // Get the last login date as a const char pointer

	struct TimePeriods
	{
		struct tm m_DailyStamp { };
		struct tm m_WeekStamp { };
		struct tm m_MonthStamp { };
	};

	// main
	int m_Level {};
	int m_Exp {};
	int m_Relations {};
	int m_GuildID {};
	int m_GuildRank {};
	TimePeriods m_Periods {};
	std::list< int > m_aHistoryWorld {};


	bool IsRelationshipsDeterioratedToMax() const { return m_Relations >= 100; }

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