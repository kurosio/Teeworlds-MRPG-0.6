/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_DATA_H
#include <game/server/mmocore/Utils/FieldData.h>

#include "Manager/GuildBankManager.h"
#include "Manager/GuildHistoryLogManager.h"
#include "Manager/Members/GuildMembersManager.h"
#include "Manager/Ranks/GuildRanksManager.h"
#include "Houses/GuildHouseData.h"

#define TW_GUILDS_TABLE "tw_guilds"
#define TW_GUILDS_RANKS_TABLE "tw_guilds_ranks"
#define TW_GUILDS_HISTORY_TABLE "tw_guilds_history"
#define TW_GUILDS_INVITES_TABLE "tw_guilds_invites"

using GuildIdentifier = int;

// Enum for guild member results
enum class GUILD_RESULT : int
{
	BUY_HOUSE_ALREADY_HAVE,
	BUY_HOUSE_UNAVAILABLE,
	BUY_HOUSE_ALREADY_PURCHASED,
	BUY_HOUSE_NOT_ENOUGH_GOLD,
	SET_LEADER_PLAYER_ALREADY_LEADER,
	SET_LEADER_NON_GUILD_PLAYER,
	SUCCESSFUL
};

class CGuildData : public MultiworldIdentifiableStaticData< std::deque < CGuildData* > >
{
	friend class CGuildHouseData;
	friend class CGuildMemberData;
	friend class CGuildMembersManager;
	friend class CGuildRequestsManager;
	friend class CGuildRankData;
	friend class CGuildRanksManager;
	friend class CGuildBankManager;
	
	CGS* GS() const;

	CFieldContainer m_UpgradeData
	{
		CFieldData<int>{AVAILABLE_SLOTS, "AvailableSlots", "Available slots"},
		CFieldData<int>{CHAIR_EXPERIENCE, "ChairExperience", "Chair experience"},
	};

	GuildIdentifier m_ID {};
	std::string m_Name {};
	int m_LeaderUID {};
	int m_Level {};
	int m_Experience {};
	int m_Score {};

	CGuildBankManager* m_pBank {};
	CGuildHistoryController* m_pHistory {};
	CGuildRanksManager* m_pRanks {};
	CGuildMembersManager* m_pMembers {};
	CGuildHouseData* m_pHouse{};

public:
	enum
	{
		AVAILABLE_SLOTS = 0,
		CHAIR_EXPERIENCE = 1,
		NUM_GUILD_UPGRADES,
	};

	CGuildData() = default;
	~CGuildData();

	static CGuildData* CreateElement(GuildIdentifier ID)
	{
		auto pData = new CGuildData;
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(const std::string& Name, std::string&& MembersData, GuildRankIdentifier DefaultRankID, int Level, int Experience, int Score, int LeaderUID, int Bank)
	{
		m_Name = Name;
		m_LeaderUID = LeaderUID;
		m_Level = Level;
		m_Experience = Experience;
		m_Score = Score;

			// components init
		m_pHistory = new CGuildHistoryController(this);
		m_pBank = new CGuildBankManager(Bank, this);
		m_pRanks = new CGuildRanksManager(this, DefaultRankID);
		m_pMembers = new CGuildMembersManager(this, std::move(MembersData));

		m_pRanks->UpdateDefaultRank();
	}

	GuildIdentifier GetID() const { return m_ID; }
	CGuildBankManager* GetBank() const { return m_pBank; }
	CGuildHistoryController* GetHistory() const { return m_pHistory; }
	CGuildRanksManager* GetRanks() const { return m_pRanks; }
	CGuildHouseData* GetHouse() const { return m_pHouse; }
	CGuildMembersManager* GetMembers() const { return m_pMembers; }
	CFieldContainer& GetUpgrades() { return m_UpgradeData; }

	const char* GetName() const { return m_Name.c_str(); }
	int GetLeaderUID() const { return m_LeaderUID; }
	int GetLevel() const { return m_Level; }
	int GetExperience() const { return m_Experience;}
	int GetScore() const { return m_Score; }
	bool HasHouse() const { return m_pHouse != nullptr; }

	GUILD_RESULT BuyHouse(int HouseID);
	bool SellHouse();

	GUILD_RESULT SetNewLeader(int AccountID);

	void AddExperience(int Experience);

	static bool IsAccountMemberGuild(int AccountID);
};

#endif
