/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildData.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

CGS* CGuildData::GS() const { return (CGS*)Instance::GetServer()->GameServer(m_pHouse != nullptr ? m_pHouse->GetWorldID() : MAIN_WORLD_ID); }

CGuildData::~CGuildData()
{
	delete m_pMembers;
	delete m_pLogger;
	delete m_pRanks;
	delete m_pBank;
}

bool CGuildData::Upgrade(int Type)
{
	// Check if the Type is within the valid range
	if(Type < UPGRADE_AVAILABLE_SLOTS || Type >= NUM_GUILD_UPGRADES)
		return false;

	// Check if the type is UPGRADE_AVAILABLE_SLOTS and the value of the first upgrade data is greater than or equal to MAX_GUILD_SLOTS
	auto* pUpgradeData = &m_UpgradesData(Type, 0);
	if(Type == UPGRADE_AVAILABLE_SLOTS && m_UpgradesData(Type, 0).m_Value >= MAX_GUILD_SLOTS)
		return false;

	// Get a pointer to the upgrade data and price for the specified Type
	int Price = GetUpgradePrice(Type);

	// Check if the guild has enough money to spend on the upgrade
	if(m_pBank->Spend(Price))
	{
		// Increase the value of the upgrade by 1
		pUpgradeData->m_Value += 1;
		Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "%s = '%d' WHERE ID = '%d'", pUpgradeData->getFieldName(), pUpgradeData->m_Value, m_ID);

		// Add and send a history entry for the upgrade
		m_pLogger->Add(LOGFLAG_UPGRADES_CHANGES, "'%s' upgraded to %d level", pUpgradeData->getDescription(), pUpgradeData->m_Value);
		GS()->ChatGuild(m_ID, "'{STR}' upgraded to {VAL} level", pUpgradeData->getDescription(), pUpgradeData->m_Value);
		return true;
	}

	return false;
}

void CGuildData::AddExperience(int Experience)
{
	// Increase the guild's experience by the given amount
	m_Experience += Experience;

	// Variable to track if the guild's table needs to be updated
	bool UpdateTable = false;

	// Calculate the experience needed to level up
	int ExperienceNeed = (int)computeExperience(m_Level);

	// Check if the guild's experience is enough to level up
	while(m_Experience >= ExperienceNeed)
	{
		// Increase the guild's level & subtract the experience needed to level up from the guild's experience
		m_Experience -= ExperienceNeed;
		m_Level++;

		// Calculate the new experience needed to level up
		ExperienceNeed = (int)computeExperience(m_Level);

		// Add a history and send a chat message to the server indicating the guild's level up
		GS()->Chat(-1, "Guild {STR} raised the level up to {INT}", GetName(), m_Level);
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Guild raised level to '%d'.", m_Level);
		UpdateTable = true;
	}

	// Check if a random chance or the need to update the table is met
	if(rand() % 10 == 2 || UpdateTable)
	{
		Database->Execute<DB::UPDATE>("tw_guilds", "Level = '%d', Experience = '%d' WHERE ID = '%d'", m_Level, m_Experience, m_ID);
	}
}

GUILD_RESULT CGuildData::SetNewLeader(int AccountID)
{
	// Check if the given AccountID is already the leader of the guild
	if(AccountID == m_LeaderUID)
	{
		return GUILD_RESULT::SET_LEADER_PLAYER_ALREADY_LEADER;
	}

	// Check if the given AccountID is a member of the guild
	if(!m_pMembers->Get(AccountID))
	{
		return GUILD_RESULT::SET_LEADER_NON_GUILD_PLAYER;
	}

	// Update data
	m_LeaderUID = AccountID;
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "LeaderUID = '%d' WHERE ID = '%d'", m_LeaderUID, m_ID);

	// Get the nickname of the new guild leader
	const char* pNickNewLeader = Instance::GetServer()->GetAccountNickname(m_LeaderUID);

	// Add a new entry and send chat message to the guild history indicating the change of leader
	m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "New guild leader '%s'", pNickNewLeader);
	GS()->ChatGuild(m_ID, "New guild leader '{STR}'", pNickNewLeader);

	// Return a success code
	return GUILD_RESULT::SUCCESSFUL;
}

GUILD_RESULT CGuildData::BuyHouse(int HouseID)
{
	// Check if the guild already has a house
	if(m_pHouse != nullptr)
	{
		return GUILD_RESULT::BUY_HOUSE_ALREADY_HAVE;
	}

	// Find the house data with the specified HouseID
	auto IterHouse = std::find_if(CGuildHouseData::Data().begin(), CGuildHouseData::Data().end(), [&HouseID](const CGuildHouseData* p)
	{
		return p->GetID() == HouseID;
	});

	// Check if the house data was found
	if(IterHouse == CGuildHouseData::Data().end())
	{
		return GUILD_RESULT::BUY_HOUSE_UNAVAILABLE;
	}

	// Retrieve the price of the house from the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_HOUSES, "WHERE ID = '%d'", HouseID);
	if(pRes->next())
	{
		const int Price = pRes->getInt("Price");

		// Check if the guild has enough gold to purchase the house
		if(!GetBank()->Spend(Price))
		{
			return GUILD_RESULT::BUY_HOUSE_NOT_ENOUGH_GOLD;
		}

		// Assign the house to the guild
		m_pHouse = *IterHouse;
		m_pHouse->UpdateGuild(this);

		// Add a history entry and send a chat message for getting a guild house
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Your guild has purchased a house!");
		GS()->ChatGuild(m_ID, "Your guild has purchased a house!");

		// Update the GuildID of the house in the database
		Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "GuildID = '%d' WHERE ID = '%d'", m_ID, HouseID);
		return GUILD_RESULT::SUCCESSFUL; // Return success code
	}

	return GUILD_RESULT::BUY_HOUSE_ALREADY_PURCHASED;
}

bool CGuildData::SellHouse()
{
	// Check if the guild house is not null
	if(m_pHouse != nullptr)
	{
		// Send an inbox message to the guild leader
		const int ReturnedGold = m_pHouse->GetPrice();
		GS()->SendInbox("System", m_LeaderUID, "Your guild house sold.", "We returned some gold from your guild.", itGold, ReturnedGold);

		// Add a history entry and send a chat message for losing a guild house
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Lost a house on '%s'.", Server()->GetWorldName(m_pHouse->GetWorldID()));
		GS()->ChatGuild(m_ID, "House sold, {VAL}gold returned to leader", ReturnedGold);

		// Update the database to remove the guild ID from the guild house
		Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "GuildID = NULL WHERE ID = '%d'", m_pHouse->GetID());

		// Reset house pointers
		m_pHouse->GetDoors()->CloseAll();
		m_pHouse->UpdateGuild(nullptr);
		m_pHouse = nullptr;
		return true;
	}

	return false;
}

int CGuildData::GetUpgradePrice(int Type)
{
	// Check if the Type is within the valid range
	if(Type < UPGRADE_AVAILABLE_SLOTS || Type >= NUM_GUILD_UPGRADES)
		return 0;

	// Return the calculated price
	return m_UpgradesData(Type, 0).m_Value * (Type == UPGRADE_AVAILABLE_SLOTS ? g_Config.m_SvPriceUpgradeGuildSlot : g_Config.m_SvPriceUpgradeGuildAnother);
}

bool CGuildData::IsAccountMemberGuild(int AccountID)
{
	return std::any_of(CGuildData::Data().begin(), CGuildData::Data().end(), [&AccountID](const CGuildData* p)
	{
		return p->GetMembers()->Get(AccountID) != nullptr;
	});
}