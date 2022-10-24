/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_DATA_H
#define GAME_SERVER_COMPONENT_ACCOUNT_DATA_H

#include <game/server/mmocore/Components/Auction/AuctionData.h>
#include <game/server/mmocore/Utils/FieldData.h>

// TODO: reconstruct
class CAccountData
{
	class CHouseData* m_pHouse{};

public:
	// main
	char m_aLogin[64]{};
	char m_aLastLogin[64]{};
	char m_aLanguage[8]{};
	int m_UserID{};
	int m_Level{};
	int m_Exp{};
	int m_GuildID{};
	int m_GuildRank{};
	std::list< int > m_aHistoryWorld{};

	void SetHouse(CHouseData* pHouse)
	{
		m_pHouse = pHouse;
	};
	class CHouseData* GetHouse() const { return m_pHouse; }
	bool HasHouse() const { return m_pHouse; }

	// upgrades
	int m_Upgrade{};
	std::map< AttributeIdentifier, int > m_aStats{};

	CTeeInfo m_TeeInfos{};
	int m_Team{};
	std::map < int, bool > m_aAetherLocation{};
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

	// temp guild name for searching
	char m_aGuildSearchBuf[32];

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