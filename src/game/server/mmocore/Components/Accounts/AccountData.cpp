/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountData.h"

#include "game/server/gamecontext.h"
#include "game/server/mmocore/Components/Houses/HouseData.h"
#include <game/server/mmocore/Components/Groups/GroupData.h>

#include <game/server/mmocore/Components/Guilds/GuildManager.h>

std::map < int, CAccountData > CAccountData::ms_aData;
std::map < int, CAccountTempData > CAccountTempData::ms_aPlayerTempData;

CGS* CAccountData::GS() const
{
	return m_pPlayer ? m_pPlayer->GS() : nullptr;
}

// Set the ID of the account
void CAccountData::Init(int ID, CPlayer* pPlayer, const char* pLogin, std::string Language, std::string LoginDate, ResultPtr pResult)
{
	// Check if the ID has already been set
	dbg_assert(m_ID <= 0 || !pResult, "Unique AccountID cannot change the value more than 1 time");

	// Get the server instance
	int ClientID = pPlayer->GetCID();
	IServer* pServer = Instance::GetServer();

	/*
		Initialize object
	*/
	m_ID = ID;
	m_pPlayer = pPlayer;
	str_copy(m_aLogin, pLogin, sizeof(m_aLogin));
	str_copy(m_aLastLogin, LoginDate.c_str(), sizeof(m_aLastLogin));

	// base data
	m_Level = pResult->getInt("Level");
	m_Exp = pResult->getInt("Exp");
	m_GuildID = pResult->getInt("GuildID");
	m_Upgrade = pResult->getInt("Upgrade");
	m_GuildRank = pResult->getInt("GuildRank");
	m_PrisonSeconds = pResult->getInt("PrisonSeconds");
	m_aHistoryWorld.push_front(pResult->getInt("WorldID"));

	// time periods
	{
		m_Periods.m_DailyStamp = pResult->getInt64("DailyStamp");
		m_Periods.m_WeekStamp = pResult->getInt64("WeekStamp");
		m_Periods.m_MonthStamp = pResult->getInt64("MonthStamp");
	}

	// upgrades data
	for(const auto& [AttrbiteID, pAttribute] : CAttributeDescription::Data())
	{
		if(pAttribute->HasDatabaseField())
			m_aStats[AttrbiteID] = pResult->getInt(pAttribute->GetFieldName());
	}

	pServer->SetClientLanguage(ClientID, Language.c_str());
	pServer->SetClientScore(ClientID, m_Level);

	// Execute a database update query to update the "tw_accounts" table
	// Set the LoginDate to the current timestamp and LoginIP to the client address
	// The update query is executed on the row with the ID equal to the given UserID
	char aAddrStr[64];
	pServer->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
	Database->Execute<DB::UPDATE>("tw_accounts", "LoginDate = CURRENT_TIMESTAMP, LoginIP = '%s' WHERE ID = '%d'", aAddrStr, ID);

	/*
		Initialize sub account data.
	*/
	ReinitializeHouse();
	ReinitializeGroup();
}

void CAccountData::UpdatePointer(CPlayer* pPlayer)
{
	dbg_assert(m_pPlayer != nullptr, "Account pointer must always exist");

	m_pPlayer = pPlayer;
	m_ClientID = pPlayer->GetCID();
}

// This function initializes the house data for the account
void CAccountData::ReinitializeHouse()
{
	// Iterate through all the house data objects
	for(const auto& p : CHouseData::Data())
	{
		// Check if the account ID of the house data object matches the account ID of the current account
		if(p->GetAccountID() == m_ID)
		{
			// Set the house data pointer of the account to the current house data object
			m_pHouseData = p.get();
			return; // Exit the function
		}
	}

	// If no matching house data object is found, set the house data pointer of the account to nullptr
	m_pHouseData = nullptr;
}

// 
void CAccountData::ReinitializeGroup()
{
	// Iterate through all the group data objects
	for(auto& p : GroupData::Data())
	{
		// Check if the account ID of the group data object matches the account ID of the current account
		auto& Accounts = p.second.GetAccounts();
		if(Accounts.find(m_ID) != Accounts.end())
		{
			// Set the group data pointer of the account to the current group data object
			m_pGroupData = &p.second;
			return; // Exit the function
		}
	}

	// If no matching group data object is found, set the group data pointer of the account to nullptr
	m_pGroupData = nullptr;
}

void CAccountData::Prison(int Seconds)
{
	if(!m_pPlayer)
		return;

}

void CAccountData::AddExperience(int Value)
{
	if(!m_pPlayer)
		return;

	m_Exp += Value;
	while(m_Exp >= (int)computeExperience(m_Level))
	{
		m_Exp -= (int)computeExperience(m_Level);
		m_Level++;
		m_Upgrade += 1;

		if(CCharacter* pChar = m_pPlayer->GetCharacter())
		{
			GS()->CreateDeath(pChar->m_Core.m_Pos, m_ClientID);
			GS()->CreateSound(pChar->m_Core.m_Pos, 4);
			GS()->CreateText(pChar, false, vec2(0, -40), vec2(0, -1), 30, "level up");
		}

		GS()->Chat(m_ClientID, "Congratulations. You attain level {INT}!", m_Level);
		if(m_Exp < (int)computeExperience(m_Level))
		{
			GS()->StrongUpdateVotes(m_ClientID, MENU_MAIN);
			GS()->Mmo()->SaveAccount(m_pPlayer, SAVE_STATS);
			GS()->Mmo()->SaveAccount(m_pPlayer, SAVE_UPGRADES);
		}
	}
	m_pPlayer->ProgressBar("Account", m_Level, m_Exp, (int)computeExperience(m_Level), Value);

	if(rand() % 5 == 0)
	{
		GS()->Mmo()->SaveAccount(m_pPlayer, SAVE_STATS);
	}

	if(IsGuild())
		GS()->Mmo()->Member()->AddExperience(m_GuildID);
}

void CAccountData::AddGold(int Value) const
{
	if(m_pPlayer)
		m_pPlayer->GetItem(itGold)->Add(Value);
}
