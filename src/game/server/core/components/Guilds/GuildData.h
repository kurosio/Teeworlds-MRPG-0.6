/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_DATA_H
#include <game/server/core/tools/dbfield.h>

#include "Manager/GuildBankManager.h"
#include "Manager/GuildLoggerManager.h"
#include "Manager/Members/GuildMembersManager.h"
#include "Manager/Ranks/GuildRanksManager.h"
#include "Houses/GuildHouseData.h"

#define TW_GUILDS_TABLE "tw_guilds"
#define TW_GUILDS_RANKS_TABLE "tw_guilds_ranks"
#define TW_GUILDS_HISTORY_TABLE "tw_guilds_history"
#define TW_GUILDS_INVITES_TABLE "tw_guilds_invites"

// Forward declaration and alias
using GuildIdentifier = int;

// This enum class represents the possible results of guild operations
enum class GUILD_RESULT : int
{
	BUY_HOUSE_ALREADY_HAVE,                // The guild already owns a house and cannot buy another one
	BUY_HOUSE_UNAVAILABLE,                 // The house is not available for purchase
	BUY_HOUSE_ALREADY_PURCHASED,           // The house has already been purchased by another player
	BUY_HOUSE_NOT_ENOUGH_GOLD,             // The guild does not have enough gold to buy the house
	SET_LEADER_PLAYER_ALREADY_LEADER,      // The player is already the leader of the guild
	SET_LEADER_NON_GUILD_PLAYER,           // The player is not a member of the guild
	SUCCESSFUL                             // The guild operation was successful
};

class CGuildData : public MultiworldIdentifiableStaticData< std::deque < CGuildData* > >
{
public:
	CGS* GS() const;

private:
	friend class CGuildHouseData;

	GuildIdentifier m_ID {};
	std::string m_Name {};
	int m_LeaderUID {};
	int m_Level {};
	int m_Experience {};
	int m_Score {};

	DBFieldContainer m_UpgradesData
	{
		DBField<int> { UPGRADE_AVAILABLE_SLOTS, "AvailableSlots", "Available slots", DEFAULT_GUILD_AVAILABLE_SLOTS },
		DBField<int> { UPGRADE_CHAIR_EXPERIENCE, "ChairExperience", "Chair experience", DEFAULT_GUILD_CHAIR },
	};

	CGuildBankManager* m_pBank {};
	CGuildLoggerManager* m_pLogger {};
	CGuildRanksManager* m_pRanks {};
	CGuildMembersManager* m_pMembers {};
	CGuildHouseData* m_pHouse {};

public:
	enum
	{
		UPGRADE_AVAILABLE_SLOTS = 0,
		UPGRADE_CHAIR_EXPERIENCE = 1,
		NUM_GUILD_UPGRADES,
	};

	CGuildData() = default;
	~CGuildData();

	static CGuildData* CreateElement(const GuildIdentifier& ID)
	{
		auto pData = new CGuildData;
		pData->m_ID = ID;
		return m_pData.emplace_back(pData);
	}

	void Init(const std::string& Name, std::string&& MembersData, GuildRankIdentifier DefaultRankID, int Level, int Experience, int Score, int LeaderUID, int Bank, int64_t Logflag, ResultPtr* pRes)
	{
		m_Name = Name;
		m_LeaderUID = LeaderUID;
		m_Level = Level;
		m_Experience = Experience;
		m_Score = Score;
		m_UpgradesData.initFields(pRes);

		// components init
		m_pLogger = new CGuildLoggerManager(this, Logflag);
		m_pBank = new CGuildBankManager(Bank, this);
		m_pRanks = new CGuildRanksManager(this, DefaultRankID);
		m_pMembers = new CGuildMembersManager(this, std::move(MembersData));
		m_pRanks->UpdateDefaultRank();
	}

	// getters
	GuildIdentifier GetID() const { return m_ID; }
	CGuildBankManager* GetBank() const { return m_pBank; }
	CGuildLoggerManager* GetLogger() const { return m_pLogger; }
	CGuildRanksManager* GetRanks() const { return m_pRanks; }
	CGuildHouseData* GetHouse() const { return m_pHouse; }
	CGuildMembersManager* GetMembers() const { return m_pMembers; }
	DBField<int>* GetUpgrades(int Type) { return &m_UpgradesData(Type, 0); }
	const char* GetName() const { return m_Name.c_str(); }
	int GetLeaderUID() const { return m_LeaderUID; }
	int GetLevel() const { return m_Level; }
	int GetExperience() const { return m_Experience; }
	int GetScore() const { return m_Score; }
	bool HasHouse() const { return m_pHouse != nullptr; }
	int GetUpgradePrice(int Type);

	// functions
	void AddExperience(int Experience);
	[[nodiscard]] bool Upgrade(int Type);
	[[nodiscard]] GUILD_RESULT SetNewLeader(int AccountID);
	[[nodiscard]] GUILD_RESULT BuyHouse(int HouseID);
	[[nodiscard]] bool SellHouse();

	// global functions
	static bool IsAccountMemberGuild(int AccountID);
};

#endif
