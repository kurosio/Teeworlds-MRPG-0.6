/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountData.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include "../houses/house_data.h"
#include "../achievements/achievement_data.h"
#include "../guilds/guild_manager.h"
#include "../worlds/world_manager.h"

std::map < int, CAccountData > CAccountData::ms_aData;
std::map < int, CAccountTempData > CAccountTempData::ms_aPlayerTempData;

CGS* CAccountData::GS() const
{
	return (CGS*)Instance::GameServerPlayer(m_ClientID);
}

CPlayer* CAccountData::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

int CAccountData::GetGoldCapacity() const
{
	return DEFAULT_MAX_PLAYER_BAG_GOLD + GetPlayer()->GetTotalAttributeValue(AttributeIdentifier::GoldCapacity);
}

// Set the ID of the account
void CAccountData::Init(int ID, int ClientID, const char* pLogin, std::string Language, std::string LoginDate, ResultPtr pResult)
{
	// Check if the ID has already been set
	m_ClientID = ClientID;
	dbg_assert(m_ID <= 0 || !pResult, "Unique AccountID cannot change the value more than 1 time");

	// initialize
	IServer* pServer = Instance::Server();
	str_copy(m_aLogin, pLogin, sizeof(m_aLogin));
	str_copy(m_aLastLogin, LoginDate.c_str(), sizeof(m_aLastLogin));

	// base data
	m_ID = ID;
	m_CrimeScore = pResult->getInt("CrimeScore");
	m_aHistoryWorld.push_front(pResult->getInt("WorldID"));
	m_Bank = pResult->getString("Bank");

	// time periods
	m_Periods.m_DailyStamp = pResult->getInt64("DailyStamp");
	m_Periods.m_WeekStamp = pResult->getInt64("WeekStamp");
	m_Periods.m_MonthStamp = pResult->getInt64("MonthStamp");

	// initialize data
	InitProfessions();
	InitAchievements(pResult->getString("Achievements"));
	pServer->SetClientLanguage(m_ClientID, Language.c_str());

	// Execute a database update query to update the "tw_accounts" table
	// Set the LoginDate to the current timestamp and LoginIP to the client address
	// The update query is executed on the row with the ID equal to the given UserID
	char aAddrStr[64];
	pServer->GetClientAddr(m_ClientID, aAddrStr, sizeof(aAddrStr));
	Database->Execute<DB::UPDATE>("tw_accounts", "LoginDate = CURRENT_TIMESTAMP, LoginIP = '{}', CountryISO = '{}' WHERE ID = '{}'", 
		aAddrStr, Instance::Server()->ClientCountryIsoCode(m_ClientID), ID);

	/*
		Initialize sub account data.
	*/
	ReinitializeHouse();
	ReinitializeGuild();
	m_Class.Init(m_ClientID, (Professions)pResult->getInt("Profession"));
	m_BonusManager.Init(m_ClientID);
	m_PrisonManager.Init(m_ClientID);
}

void CAccountData::InitProfessions()
{
	// initialize base professions
	m_vProfessions.push_back(CTankProfession());
	m_vProfessions.push_back(CDPSProfession());
	m_vProfessions.push_back(CHealerProfession());
	m_vProfessions.push_back(CFarmerProfession());
	m_vProfessions.push_back(CMinerProfession());

	// load professions data
	std::map<Professions, std::string> vmProfessionsData {};
	const auto pResult = Database->Execute<DB::SELECT>("*", "tw_accounts_professions", "WHERE UserID = '{}'", m_ID);
	while(pResult->next())
	{
		const auto ProfessionID = (Professions)pResult->getInt("ProfessionID");
		vmProfessionsData[ProfessionID] = pResult->getString("Data");
	}

	for(auto& Profession : m_vProfessions)
	{
		const auto& ProfessionID = Profession.GetProfessionID();
		const auto it = vmProfessionsData.find(ProfessionID);
		const auto optData = it != vmProfessionsData.end() ? std::make_optional<std::string>(it->second) : std::nullopt;
		Profession.Init(m_ClientID, optData);
	}
}

void CAccountData::InitAchievements(const std::string& Data)
{
	// initialize player base
	const auto* pPlayer = GetPlayer();
	std::map<int, CAchievement*> m_apReferenceMap {};

	for(const auto& pAchievement : CAchievementInfo::Data())
	{
		const int AchievementID = pAchievement->GetID();
		m_apReferenceMap[AchievementID] = CAchievement::CreateElement(pAchievement, pPlayer->GetCID());
	}

	// initialize player achievements
	mystd::json::parse(Data, [&m_apReferenceMap, this](nlohmann::json& pJson)
	{
		for(auto& p : pJson)
		{
			int AchievementID = p.value("aid", -1);
			int Progress = p.value("progress", 0);
			bool Completed = p.value("completed", false);

			if(m_apReferenceMap.contains(AchievementID))
				m_apReferenceMap[AchievementID]->Init(Progress, Completed);
		}
		m_AchievementsData = std::move(pJson);
	});

	// clear reference map
	m_apReferenceMap.clear();
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

void CAccountData::IncreaseCrimeScore(int Score)
{
	if(IsCrimeScoreMaxedOut())
		return;

	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	m_CrimeScore = minimum(m_CrimeScore + Score, 100);
	GS()->Chat(m_ClientID, "Your 'Crime Score' has increased to {}%!", m_CrimeScore);

	if(m_CrimeScore >= 100)
	{
		GS()->Chat(m_ClientID, "You have reached the maximum 'Crime Score' and are now a wanted criminal! Be cautious, as law enforcers are actively searching for you.");
	}

	GS()->Core()->SaveAccount(pPlayer, SAVE_SOCIAL_STATUS);
}

int CAccountData::GetGold() const
{
	CPlayer* pPlayer = GetPlayer();
	return pPlayer ? pPlayer->GetItem(itGold)->GetValue() : 0;
}

BigInt CAccountData::GetTotalGold() const
{
	CPlayer* pPlayer = GetPlayer();
	return pPlayer ? m_Bank + pPlayer->GetItem(itGold)->GetValue() : 0;
}

void CAccountData::AddExperience(uint64_t Value) const
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// check valid active profession
	const auto pClassProfession = GetClassProfession();
	if(!pClassProfession)
	{
		GS()->Chat(m_ClientID, "You don't have an active profession to gain experience!");
		return;
	}

	// increase exp value
	const auto OldLevel = pClassProfession->GetLevel();
	m_BonusManager.ApplyBonuses(BONUS_TYPE_EXPERIENCE, &Value);
	pClassProfession->AddExperience(Value);
	if(pClassProfession->GetLevel() > OldLevel)
	{
		// increase skill points
		if(g_Config.m_SvSkillPointsPerLevel > 0)
		{
			auto* pSkillPoint = pPlayer->GetItem(itSkillPoint);
			pSkillPoint->Add(g_Config.m_SvSkillPointsPerLevel);
			GS()->Chat(m_ClientID, "You have earned {} Skill Points! You now have {} SP!", g_Config.m_SvSkillPointsPerLevel, pSkillPoint->GetValue());
		}

		// notify about new zones
		GS()->Core()->WorldManager()->NotifyUnlockedZonesByLeveling(pPlayer);
	}

	// add experience to the guild
	if(HasGuild())
	{
		m_pGuildData->AddExperience(1);
	}
}

void CAccountData::AddGold(int Value, bool ToBank, bool ApplyBonuses)
{
	// check valid player
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// apply bonuses
	if(ApplyBonuses)
	{
		m_BonusManager.ApplyBonuses(BONUS_TYPE_GOLD, &Value);
	}

	// to bank
	if(ToBank)
	{
		m_Bank += Value;
		GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
		return;
	}

	// initialize variables
	CPlayerItem* pGoldItem = pPlayer->GetItem(itGold);
	const int CurrentGold = pGoldItem->GetValue();
	const int FreeSpace = GetGoldCapacity() - CurrentGold;

	// add golds
	if(Value > FreeSpace)
	{
		pGoldItem->Add(FreeSpace);
		m_Bank += (Value - FreeSpace);
		GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
	}
	else
	{
		pGoldItem->Add(Value);
	}
}

bool CAccountData::SpendCurrency(int Price, int CurrencyItemID)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return false;

	// check is free
	if(Price <= 0)
		return true;

	// initialize variables
	CPlayerItem* pCurrencyItem = pPlayer->GetItem(CurrencyItemID);
	const int PlayerCurrency = pCurrencyItem->GetValue();

	// gold with bank
	if(CurrencyItemID == itGold)
	{
		BigInt TotalCurrency = m_Bank + pCurrencyItem->GetValue();
		if(PlayerCurrency < Price)
		{
			GS()->Chat(m_ClientID, "Required {}, but you only have {} {} (including bank)!", Price, PlayerCurrency, pCurrencyItem->Info()->GetName());
			return false;
		}

		// first, spend currency from player's hand
		int RemainingPrice = Price;
		if(PlayerCurrency > 0)
		{
			int ToSpendFromHands = minimum(PlayerCurrency, RemainingPrice);
			pCurrencyItem->Remove(ToSpendFromHands);
			RemainingPrice -= ToSpendFromHands;
		}

		// second, spend the remaining currency from the bank
		if(RemainingPrice > 0)
		{
			m_Bank -= RemainingPrice;
			GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
		}

		return true;
	}

	// other currency
	if(PlayerCurrency < Price)
	{
		GS()->Chat(m_ClientID, "Required {}, but you only have {} {} (including bank)!", Price, PlayerCurrency, pCurrencyItem->Info()->GetName());
		return false;
	}

	return pCurrencyItem->Remove(Price);
}

bool CAccountData::DepositGoldToBank(int Amount)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return false;

	// initialize variables
	CPlayerItem* pItemGold = pPlayer->GetItem(itGold);
	int CurrentGold = pItemGold->GetValue();

	// check amount
	if(CurrentGold < Amount)
	{
		GS()->Chat(m_ClientID, "You don't have enough gold in your inventory. You only have {$} gold.", CurrentGold);
		return false;
	}

	// remove gold from player
	if(pItemGold->Remove(Amount))
	{
		const int Commission = translate_to_percent_rest(Amount, g_Config.m_SvBankCommissionRate);
		const int FinalAmount = Amount - Commission;

		m_Bank += FinalAmount;
		GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);

		GS()->Chat(m_ClientID, "You have deposited {$} gold into your bank (Commission: {$}).", FinalAmount, Commission);
		return true;
	}

	return false;
}

bool CAccountData::WithdrawGoldFromBank(int Amount)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return false;

	// initialize variables
	CPlayerItem* pItemGold = pPlayer->GetItem(itGold);
	int CurrentGold = pItemGold->GetValue();
	int AvailableSpace = GetGoldCapacity() - CurrentGold;

	// check bank amount
	if(m_Bank < Amount)
	{
		GS()->Chat(m_ClientID, "You only have {$} gold in your bank, but you tried to withdraw {}!", m_Bank, Amount);
		return false;
	}

	// calculate gold for withdraw
	int GoldToWithdraw = minimum(Amount, AvailableSpace);

	if(GoldToWithdraw > 0)
	{
		pItemGold->Add(GoldToWithdraw);
		m_Bank -= GoldToWithdraw;
		GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
		GS()->Chat(m_ClientID, "You have withdrawn {$} gold from your bank to your inventory.", GoldToWithdraw);
	}

	if(GoldToWithdraw < Amount)
	{
		GS()->Chat(m_ClientID, "Your inventory is full. You could only withdraw {$} gold.", GoldToWithdraw);
	}

	return true;
}

void CAccountData::ResetCrimeScore()
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	m_CrimeScore = 0;
	GS()->Core()->SaveAccount(pPlayer, SAVE_SOCIAL_STATUS);
}

void CAccountData::HandleChair(uint64_t Exp, int Gold)
{
	IServer* pServer = Instance::Server();
	if(pServer->Tick() % pServer->TickSpeed() != 0)
		return;

	// check active profession
	const auto* pClassProfession = GetClassProfession();
	if(!pClassProfession)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::GameWarning, 100, "You don't have an active profession to gain experience!");
		return;
	}

	// initialize variables
	const int Level = pClassProfession->GetLevel();
	const int maxGoldCapacity = GetGoldCapacity();
	const bool isGoldBagFull = (GetGold() >= maxGoldCapacity);

	const auto expGain = std::max<uint64_t>(Exp, calculate_exp_gain(g_Config.m_SvChairExpFactor, Level, Exp + Level));
	const int goldGain = isGoldBagFull ? 0 : maximum(Gold, (int)calculate_gold_gain(g_Config.m_SvChairGoldFactor, Level, Gold + Level));

	// total percent bonuses
	const int totalPercentBonusGold = round_to_int(m_BonusManager.GetTotalBonusPercentage(BONUS_TYPE_GOLD));
	const int totalPercentBonusExp = round_to_int(m_BonusManager.GetTotalBonusPercentage(BONUS_TYPE_EXPERIENCE));

	// add exp & gold
	AddExperience(expGain);
	if(!isGoldBagFull)
	{
		AddGold(goldGain, false, true);
	}

	// format
	std::string expStr = "+" + std::to_string(expGain);
	std::string goldStr = goldGain > 0 ? "+" + std::to_string(goldGain) : "Bag Full";

	// add bonus information
	if(totalPercentBonusExp > 0)
	{
		expStr += " (+" + std::to_string(totalPercentBonusExp) + "% bonus)";
	}
	if(totalPercentBonusGold > 0 && goldGain > 0)
	{
		goldStr += " (+" + std::to_string(totalPercentBonusGold) + "% bonus)";
	}

	// send broadcast
	GS()->Broadcast(m_ClientID, BroadcastPriority::MainInformation, 250, "Gold {$} of {$} (Total: {$}) : {}\nExp {}/{} : {}",
		GetGold(), maxGoldCapacity, GetTotalGold(), goldStr.c_str(), pClassProfession->GetExperience(),
		pClassProfession->GetExpForNextLevel(), expStr.c_str());
}

void CAccountData::UpdateAchievementProgress(int AchievementID, int Progress, bool Completed)
{
	const auto it = std::ranges::find_if(m_AchievementsData, [AchievementID](const nlohmann::json& obj)
	{
		return obj.value("aid", -1) == AchievementID;
	});

	// update
	if(it != m_AchievementsData.end())
	{
		(*it)["progress"] = Progress;
		(*it)["completed"] = Completed;
	}
	else
	{
		nlohmann::json newAchievement;
		newAchievement["aid"] = AchievementID;
		newAchievement["progress"] = Progress;
		newAchievement["completed"] = Completed;
		m_AchievementsData.push_back(newAchievement);
	}
}