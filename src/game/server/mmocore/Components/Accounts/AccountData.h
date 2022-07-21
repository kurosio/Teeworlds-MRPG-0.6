/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_DATA_H
#define GAME_SERVER_COMPONENT_ACCOUNT_DATA_H

#include <game/server/mmocore/Components/Shops/ShopData.h>
#include <game/server/mmocore/Utils/FieldData.h>

struct CAccountData
{
	// main
	char m_aLogin[64];
	char m_aLastLogin[64];
	char m_aLanguage[8];
	int m_UserID;
	int m_Level;
	int m_Exp;
	int m_GuildID;
	int m_GuildRank;
	std::list< int > m_aHistoryWorld;

	// upgrades
	int m_Upgrade;
	std::map< int, int > m_aStats;

	// skins
	char m_aaSkinPartNames[NUM_SKINPARTS][MAX_SKIN_ARRAY_SIZE];
	int m_aUseCustomColors[NUM_SKINPARTS];
	int m_aSkinPartColors[NUM_SKINPARTS];

	int m_Team;
	std::map < int, bool > m_aAetherLocation;
	bool IsGuild() const { return m_GuildID > 0; }

	CFieldData<int> m_aMining[NUM_JOB_ACCOUNTS_STATS];
	CFieldData<int> m_aFarming[NUM_JOB_ACCOUNTS_STATS];

	CAccountData()
	{
		m_UserID = -1;
		m_Level = 0;
		m_Team = TEAM_SPECTATORS;

		m_aMining[JOB_LEVEL].init("Level", "Miner level");
		m_aMining[JOB_EXPERIENCE].init("Exp", "Miner experience");
		m_aMining[JOB_UPGR_QUANTITY].init("Quantity", "Miner quantity");
		m_aMining[JOB_UPGRADES].init("Upgrade", "Miner upgrades");
		
		m_aFarming[JOB_LEVEL].init("Level", "Farmer level");
		m_aFarming[JOB_EXPERIENCE].init("Exp", "Farmer experience");
		m_aFarming[JOB_UPGR_QUANTITY].init("Quantity", "Farmer quantity");
		m_aFarming[JOB_UPGRADES].init("Upgrade", "Farmer upgrades");
	}

	static std::map < int, CAccountData > ms_aData;
};

struct CAccountTempData
{
	int m_TempDecoractionID;
	int m_TempDecorationType;
	int m_TempID3;

	CAuctionItem m_SellItem;

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