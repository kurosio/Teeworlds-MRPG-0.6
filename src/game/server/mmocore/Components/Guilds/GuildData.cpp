/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildData.h"

#include <game/server/gamecontext.h>

CGS* CGuildData::GS() const { return (CGS*)Instance::GetServer()->GameServer(m_pHouse != nullptr ? m_pHouse->GetWorldID() : MAIN_WORLD_ID); }

CGuildData::~CGuildData()
{
	delete m_pMembers;
	delete m_pHistory;
	delete m_pRanks;
	delete m_pBank;
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

		// Update the GuildID of the house in the database
		Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "GuildID = '%d' WHERE ID = '%d'", m_ID, HouseID);
		return GUILD_RESULT::SUCCESSFUL; // Return success code
	}

	return GUILD_RESULT::BUY_HOUSE_ALREADY_PURCHASED;
}

bool CGuildData::SellHouse()
{
	if(m_pHouse == nullptr)
	{
		return false;
	}

	const int ReturnedGold = m_pHouse->GetPrice();
	GS()->SendInbox("System", m_LeaderUID, "Your guild house sold.", "We returned some gold from your guild.", itGold, ReturnedGold);
	GS()->ChatGuild(m_ID, "House sold, {VAL}gold returned to leader", ReturnedGold);
	m_pHistory->Add("Lost a house on '%s'.", Server()->GetWorldName(m_pHouse->GetWorldID()));

	Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "GuildID = NULL WHERE ID = '%d'", m_pHouse->GetID());
	m_pHouse->GetDoors()->CloseAll();
	m_pHouse->UpdateGuild(nullptr);
	m_pHouse = nullptr;
	return true;
}

GUILD_RESULT CGuildData::SetNewLeader(int AccountID)
{
	if(AccountID == m_LeaderUID)
	{
		return GUILD_RESULT::SET_LEADER_PLAYER_ALREADY_LEADER;
	}

	if(!m_pMembers->Get(AccountID))
	{
		return GUILD_RESULT::SET_LEADER_NON_GUILD_PLAYER;
	}

	m_LeaderUID = AccountID;
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "LeaderUID = '%d' WHERE ID = '%d'", m_LeaderUID, m_ID);

	const char* pNickNewLeader = Instance::GetServer()->GetAccountNickname(m_LeaderUID);
	m_pHistory->Add("New guild leader '%s'", pNickNewLeader);
	GS()->ChatGuild(m_ID, "New guild leader '{STR}'", pNickNewLeader);
	return GUILD_RESULT::SUCCESSFUL;
}

void CGuildData::AddExperience(int Experience)
{
	m_Experience += Experience;

	bool UpdateTable = false;
	int ExperienceNeed = (int)computeExperience(m_Level);
	while(m_Experience >= ExperienceNeed)
	{
		m_Experience -= ExperienceNeed;
		m_Level++;

		ExperienceNeed = (int)computeExperience(m_Level);
		if(m_Experience < ExperienceNeed)
			UpdateTable = true;

		GS()->Chat(-1, "Guild {STR} raised the level up to {INT}", GetName(), m_Level);
		m_pHistory->Add("Guild raised level to '%d'.", m_Level);
	}

	if(rand() % 10 == 2 || UpdateTable)
	{
		Database->Execute<DB::UPDATE>("tw_guilds", "Level = '%d', Experience = '%d' WHERE ID = '%d'", m_Level, m_Experience, m_ID);
	}
}

bool CGuildData::IsAccountMemberGuild(int AccountID)
{
	return std::any_of(CGuildData::Data().begin(), CGuildData::Data().end(), [&AccountID](const CGuildData* p)
	{
		return p->GetMembers()->Get(AccountID) != nullptr;
	});
}