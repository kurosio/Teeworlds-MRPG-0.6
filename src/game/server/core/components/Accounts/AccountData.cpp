/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountData.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/houses/house_data.h>
#include <game/server/core/components/groups/group_data.h>

#include <game/server/core/components/guilds/guild_manager.h>
#include <game/server/core/components/worlds/world_manager.h>

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
	IServer* pServer = Instance::Server();

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
	m_Upgrade = pResult->getInt("Upgrade");
	m_PrisonSeconds = pResult->getInt("PrisonSeconds");
	m_DailyChairGolds = pResult->getInt("DailyChairGolds");
	m_CrimeScore = pResult->getInt("CrimeScore");
	m_aHistoryWorld.push_front(pResult->getInt("WorldID"));
	m_ClassGroup = (ClassGroup)pResult->getInt("Class");

	// achievements data
	InitAchievements(pResult->getString("Achievements").c_str());

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
	ReinitializeGuild();
	m_BonusManager.Init(m_ClientID, m_pPlayer);
}

void CAccountData::InitAchievements(const std::string& Data)
{
	// initialize player base
	std::map<int, CAchievement*> m_apReferenceMap {};
	for(const auto& pAchievement : CAchievementInfo::Data())
	{
		const int AchievementID = pAchievement->GetID();
		m_apReferenceMap[AchievementID] = CAchievement::CreateElement(pAchievement, m_pPlayer->GetCID());
	}

	// initialize player achievements
	Utils::Json::parseFromString(Data, [&m_apReferenceMap, this](nlohmann::json& pJson)
	{
		for(auto& p : pJson)
		{
			int AchievementID = p.value("aid", -1);
			int Progress = p.value("progress", 0);
			bool Completed = p.value("completed", false);

			if(m_apReferenceMap.contains(AchievementID))
				m_apReferenceMap[AchievementID]->Init(Progress, Completed);
		}
		m_AchivementsData = std::move(pJson);
	});

	// clear reference map
	m_apReferenceMap.clear();
}

void CAccountData::UpdatePointer(CPlayer* pPlayer)
{
	dbg_assert(m_pPlayer != nullptr, "AccountManager pointer must always exist");

	m_pPlayer = pPlayer;
	m_ClientID = pPlayer->GetCID();

	// update class data
	m_pPlayer->GetClass()->Init(m_ClassGroup);
	m_pPlayer->GetClass()->SetClassSkin(m_TeeInfos, m_pPlayer->GetItem(itCustomizer)->IsEquipped());
}

// This function initializes the house data for the account
void CAccountData::ReinitializeHouse(bool SetNull)
{
	if(!SetNull)
	{
		// Iterate through all the group data objects
		for(auto pHouse : CHouse::Data())
		{
			// Check if the account ID of the group data object matches the account ID of the current account
			if(pHouse->GetAccountID() == m_ID)
			{
				m_pHouseData = pHouse;
				return;
			}
		}
	}

	// If no matching group data object is found, set the group data pointer of the account to nullptr
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

void CAccountData::ReinitializeGuild(bool SetNull)
{
	if(!SetNull)
	{
		// Iterate through all the group data objects
		for(auto pGuild : CGuild::Data())
		{
			// Check if the account ID of the group data object matches the account ID of the current account
			auto& pMembers = pGuild->GetMembers()->GetContainer();
			if(pMembers.find(m_ID) != pMembers.end() && pMembers.at(m_ID) != nullptr)
			{
				m_pGuildData = pGuild;
				return;
			}
		}
	}

	// If no matching group data object is found, set the group data pointer of the account to nullptr
	m_pGuildData = nullptr;
}

CGuild::CMember* CAccountData::GetGuildMember() const
{
	return m_pGuildData ? m_pGuildData->GetMembers()->Get(m_ID) : nullptr;
}

bool CAccountData::IsClientSameGuild(int ClientID) const
{
	if(!m_pGuildData)
		return false;

	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	return pPlayer && pPlayer->Account()->GetGuild() && pPlayer->Account()->GetGuild()->GetID() == m_pGuildData->GetID();
}

bool CAccountData::IsSameGuild(int GuildID) const
{
	return m_pGuildData && m_pGuildData->GetID() == GuildID;
}

// This function returns the daily limit of gold that a player can obtain from chairs
int CAccountData::GetLimitDailyChairGolds() const
{
	// Check if the player exists
	if(m_pPlayer)
	{
		// Calculate the daily limit based on the player's item value
		// The limit is 300 gold plus either 50 times the value of the player's AlliedSeals item or by config, whichever is higher
		return 300 + minimum(m_pPlayer->GetItem(itPermissionExceedLimits)->GetValue(), g_Config.m_SvMaxIncreasedChairGolds);
	}
	else
	{
		// If the player does not exist, return 0 as the daily limit
		return 0;
	}
}

void CAccountData::IncreaseCrimeScore(int Score)
{
	if(!m_pPlayer || IsCrimeScoreMaxedOut())
		return;

	m_CrimeScore = minimum(m_CrimeScore + Score, 100);
	GS()->Chat(m_ClientID, "Your Crime Score has increased to {}%!", m_CrimeScore);

	if(m_CrimeScore >= 100)
		GS()->Chat(m_ClientID, "You have reached the maximum Crime Score and are now a wanted criminal! Be cautious, as law enforcers are actively searching for you.");

	GS()->Core()->SaveAccount(m_pPlayer, SAVE_SOCIAL_STATUS);
}

void CAccountData::Imprison(int Seconds)
{
	if(!m_pPlayer)
		return;

	// kill character
	if(m_pPlayer->GetCharacter())
		m_pPlayer->GetCharacter()->Die(m_pPlayer->GetCID(), WEAPON_WORLD);

	vec2 SpawnPos;
	if(GS()->m_pController->CanSpawn(SPAWN_HUMAN_PRISON, &SpawnPos))
	{
		// Set the prison seconds and send a chat message to all players indicating that the player has been imprisoned
		m_PrisonSeconds = Seconds;
		GS()->Chat(-1, "{}, has been imprisoned for {} seconds.", Instance::Server()->ClientName(m_pPlayer->GetCID()), Seconds);
		GS()->Core()->SaveAccount(m_pPlayer, SAVE_SOCIAL_STATUS);
	}
}

void CAccountData::FreeFromPrison()
{
	if(!m_pPlayer)
		return;

	// kill character
	if(m_pPlayer->GetCharacter())
		m_pPlayer->GetCharacter()->Die(m_pPlayer->GetCID(), WEAPON_WORLD);

	m_PrisonSeconds = -1;
	GS()->Chat(-1, "{} has been released from prison.", Instance::Server()->ClientName(m_pPlayer->GetCID()));
	GS()->Core()->SaveAccount(m_pPlayer, SAVE_SOCIAL_STATUS);
}

void CAccountData::AddExperience(int Value)
{
	if(!m_pPlayer)
		return;

	// Increase the experience value
	m_BonusManager.ApplyBonuses(BONUS_TYPE_EXPERIENCE, &Value);
	m_Exp += Value;

	// Check if the experience is enough to level up
	while(m_Exp >= (int)computeExperience(m_Level))
	{
		// Reduce the experience and increase the level
		m_Exp -= (int)computeExperience(m_Level);
		m_Level++;
		m_Upgrade += 1;

		// increase skill points
		if(g_Config.m_SvSPEachLevel > 0)
		{
			auto pPlayerItemSP = m_pPlayer->GetItem(itSkillPoint);
			pPlayerItemSP->Add(g_Config.m_SvSPEachLevel);
			GS()->Chat(m_ClientID, "You have earned {} Skill Points! You now have {} SP!", g_Config.m_SvSPEachLevel, pPlayerItemSP->GetValue());
		}

		// effects
		if(CCharacter* pChar = m_pPlayer->GetCharacter())
		{
			GS()->CreateDeath(pChar->m_Core.m_Pos, m_ClientID);
			GS()->CreateSound(pChar->m_Core.m_Pos, 4);
			GS()->EntityManager()->Text(pChar->GetPos() + vec2(0, -40), 30, "level up");
		}

		GS()->Chat(m_ClientID, "Congratulations. You attain level {}!", m_Level);
		GS()->Core()->WorldManager()->NotifyUnlockedZonesByLeveling(m_pPlayer, m_ID);

		// post leveling
		if(m_Exp < (int)computeExperience(m_Level))
		{
			// Update votes, save stats, and save upgrades
			m_pPlayer->m_VotesData.UpdateVotesIf(MENU_MAIN);
			GS()->Core()->SaveAccount(m_pPlayer, SAVE_STATS);
			GS()->Core()->SaveAccount(m_pPlayer, SAVE_UPGRADES);
			m_pPlayer->UpdateAchievement(ACHIEVEMENT_LEVELING, NOPE, m_Level, PROGRESS_SET);
		}
	}

	// update the progress bar
	m_pPlayer->ProgressBar("Account", m_Level, m_Exp, (int)computeExperience(m_Level), Value);

	// randomly save the account stats
	if(rand() % 5 == 0)
	{
		GS()->Core()->SaveAccount(m_pPlayer, SAVE_STATS);
	}

	// add experience to the guild member
	if(HasGuild())
	{
		m_pGuildData->AddExperience(1);
	}
}

void CAccountData::AddGold(int Value) const
{
	if(m_pPlayer)
	{
		m_BonusManager.ApplyBonuses(BONUS_TYPE_GOLD, &Value);
		m_pPlayer->GetItem(itGold)->Add(Value);
	}
}

// This function checks if the player has enough currency to spend and deducts the price from their currency item
bool CAccountData::SpendCurrency(int Price, int CurrencyItemID) const
{
	// Check if the player is valid
	if(!m_pPlayer)
		return false;

	// Check if the price is zero or negative, in which case the player can spend it for free
	if(Price <= 0)
		return true;

	// Get the currency item from the player's inventory
	CPlayerItem* pCurrencyItem = m_pPlayer->GetItem(CurrencyItemID);

	// Check if the player has enough currency to spend
	if(pCurrencyItem->GetValue() < Price)
	{
		// Display a message to the player indicating that they don't have enough currency
		GS()->Chat(m_ClientID, "Required {}, but you only have {} {}!", Price, pCurrencyItem->GetValue(), pCurrencyItem->Info()->GetName());
		return false;
	}

	// Deduct the price from the player's currency item
	return pCurrencyItem->Remove(Price);
}

// Reset daily chair golds
void CAccountData::ResetDailyChairGolds()
{
	// Check if the player is valid
	if(!m_pPlayer)
		return;

	// Reset the daily chair golds to 0
	m_DailyChairGolds = 0;
	GS()->Core()->SaveAccount(m_pPlayer, SAVE_SOCIAL_STATUS);
}

void CAccountData::ResetCrimeScore()
{
	if(!m_pPlayer)
		return;

	m_CrimeScore = 0;
	GS()->Core()->SaveAccount(m_pPlayer, SAVE_SOCIAL_STATUS);
}

void CAccountData::HandleChair()
{
	// Check if the player is valid
	if(!m_pPlayer)
		return;

	// Check if the current tick is not a multiple of the tick speed multiplied by 5
	IServer* pServer = Instance::Server();
	if(pServer->Tick() % (pServer->TickSpeed() * 5) != 0)
		return;

	int ExpValue = 1;
	int GoldValue = clamp(1, 0, GetLimitDailyChairGolds() - GetCurrentDailyChairGolds());

	// TODO: Add special upgrades item's

	// Add the experience value to the player's experience
	AddExperience(ExpValue);

	// If gold was gained
	if(GoldValue > 0)
	{
		AddGold(GoldValue); // Add the gold value to the player's gold
		m_DailyChairGolds += GoldValue; // Increase the daily gold count
		GS()->Core()->SaveAccount(m_pPlayer, SAVE_SOCIAL_STATUS); // Save the player's account
	}

	// Broadcast the information about the gold and experience gained, as well as the current limits and counts
	std::string aExpBuf = "+" + std::to_string(ExpValue);
	std::string aGoldBuf = (GoldValue > 0) ? "+" + std::to_string(GoldValue) : "limit";
	GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 250,
		"Gold {} : {} (daily limit {} of {})\nExp {}/{} : {}\nThe limit and count is increased with special items!",
		m_pPlayer->GetItem(itGold)->GetValue(), aGoldBuf.c_str(), GetCurrentDailyChairGolds(), GetLimitDailyChairGolds(), m_Exp, computeExperience(m_Level), aExpBuf.c_str());
}

void CAccountData::SetAchieventProgress(int AchievementID, int Progress, bool Completed)
{
	// find achievement data by ID
	for(auto& pObj : m_AchivementsData)
	{
		if(pObj.value("aid", -1) == AchievementID)
		{
			pObj["progress"] = Progress;
			pObj["completed"] = Completed;
			return;
		}
	}

	// append new achievement data
	nlohmann::json Obj;
	Obj["aid"] = AchievementID;
	Obj["progress"] = Progress;
	Obj["completed"] = Completed;
	m_AchivementsData.push_back(Obj);
}