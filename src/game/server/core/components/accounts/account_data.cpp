/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_data.h"

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
	const auto TotalByAttribute = GetPlayer()->GetTotalAttributeValue(AttributeIdentifier::GoldCapacity);
	return DEFAULT_MAX_PLAYER_BAG_GOLD + TotalByAttribute;
}

const CTeeInfo& CAccountData::GetTeeInfo() const
{
	auto* pPlayer = GetPlayer();
	auto* pProfession = m_pActiveProfession;
	if(pPlayer && pProfession && !pPlayer->GetItem(itCustomizer)->IsEquipped())
		return pProfession->GetTeeInfo();
	return m_TeeInfos;
}

void CAccountData::Init(int ID, int ClientID, const char* pLogin, std::string Language, std::string LoginDate, ResultPtr pResult)
{
	// asserts
	dbg_assert(m_ID <= 0 || !pResult, "Unique AccountID cannot change the value more than 1 time");

	// initialize
	IServer* pServer = Instance::Server();
	m_Login = pLogin;
	m_LastLoginDate = LoginDate;
	m_ID = ID;
	m_ClientID = ClientID;
	m_CrimeScore = pResult->getInt("CrimeScore");
	m_aHistoryWorld.push_front(pResult->getInt("WorldID"));
	m_Bank = pResult->getString("Bank");
	m_Periods.m_DailyStamp = pResult->getInt64("DailyStamp");
	m_Periods.m_WeekStamp = pResult->getInt64("WeekStamp");
	m_Periods.m_MonthStamp = pResult->getInt64("MonthStamp");
	m_LastTickCrimeScoreChanges = pServer->Tick();
	pServer->SetClientLanguage(m_ClientID, Language.c_str());


	// update login ip, date, country
	char aAddrStr[NETADDR_MAXSTRSIZE]{};
	pServer->GetClientAddr(m_ClientID, aAddrStr, sizeof(aAddrStr));
	Database->Execute<DB::UPDATE>("tw_accounts",
		"LoginDate = CURRENT_TIMESTAMP, LoginIP = '{}', CountryISO = '{}' WHERE ID = '{}'",
		aAddrStr, Instance::Server()->ClientCountryIsoCode(m_ClientID), ID);


	// initialize sub account data.
	InitProfessions();
	InitAchievements(pResult->getString("Achievements"));
	m_pActiveProfession = GetProfession((ProfessionIdentifier)pResult->getInt("ProfessionID"));
	m_BonusManager.Init(m_ClientID);
	m_PrisonManager.Init(m_ClientID);
	m_RatingSystem.Init(this);
	ReinitializeHouse();
	ReinitializeGuild();


	// update account base
	pServer->UpdateAccountBase(m_ID, pServer->ClientName(m_ClientID), m_RatingSystem.GetRating());
}

void CAccountData::InitProfessions()
{
	// initialize base professions
	m_vProfessions.push_back(CTankProfession());
	m_vProfessions.push_back(CDPSProfession());
	m_vProfessions.push_back(CHealerProfession());
	m_vProfessions.push_back(CFarmerProfession());
	m_vProfessions.push_back(CMinerProfession());
	m_vProfessions.push_back(CFishermanProfession());
	m_vProfessions.push_back(CLoaderProfession());

	// load professions data
	std::map<ProfessionIdentifier, std::string> vmProfessionsData {};
	const auto pResult = Database->Execute<DB::SELECT>("*", "tw_accounts_professions", "WHERE UserID = '{}'", m_ID);
	while(pResult->next())
	{
		const auto ProfessionID = (ProfessionIdentifier)pResult->getInt("ProfessionID");
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
	if(!m_pGuildData)
		return nullptr;

	return m_pGuildData->GetMembers()->Get(m_ID);
}

bool CAccountData::IsClientSameGuild(int ClientID) const
{
	if(!m_pGuildData)
		return false;

	// check valid from player
	const auto* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return false;

	// check valid guild data
	const auto pGuildData = pPlayer->Account()->GetGuild();
	if(!pGuildData)
		return false;

	return pGuildData->GetID() == m_pGuildData->GetID();
}

bool CAccountData::IsSameGuild(int GuildID) const
{
	if(!m_pGuildData)
		return false;

	return m_pGuildData->GetID() == GuildID;
}

void CAccountData::IncreaseCrime(int Score)
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	const auto OldCrimeScore = m_CrimeScore;
	const auto NewCrimeScore = minimum(m_CrimeScore + Score, 100);

	if(OldCrimeScore != NewCrimeScore)
	{
		m_CrimeScore = NewCrimeScore;
		m_LastTickCrimeScoreChanges = GS()->Server()->Tick();

		if(NewCrimeScore >= 100)
			GS()->Chat(-1, "'{}' added to wanted list, reward for elimination.", GS()->Server()->ClientName(pPlayer->GetCID()));

		GS()->Chat(m_ClientID, "Your crime level has been increased to '{} points'.", m_CrimeScore);
		GS()->Core()->SaveAccount(pPlayer, SAVE_SOCIAL);
	}
}

void CAccountData::DecreaseCrime(int Score)
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	const auto OldCrimeScore = m_CrimeScore;
	const auto NewCrimeScore = maximum(m_CrimeScore - Score, 0);

	if(OldCrimeScore != NewCrimeScore)
	{
		m_CrimeScore = NewCrimeScore;
		m_LastTickCrimeScoreChanges = GS()->Server()->Tick();

		if(OldCrimeScore >= 100)
			GS()->Chat(-1, "'{}' removed from wanted list.", GS()->Server()->ClientName(pPlayer->GetCID()));

		GS()->Chat(m_ClientID, "Your crime level has been decreased to '{} points'.", m_CrimeScore);
		GS()->Core()->SaveAccount(pPlayer, SAVE_SOCIAL);
	}
}

bool CAccountData::IsCrimeDecreaseTime() const
{
	return m_CrimeScore > 0 &&
		GS()->Server()->Tick() > m_LastTickCrimeScoreChanges + ((SERVER_TICK_SPEED * 60) * g_Config.m_SvCrimeIntervalDecrease);
}

void CAccountData::ResetCrimeScore()
{
	auto* pPlayer = GetPlayer();
	if(pPlayer)
	{
		m_CrimeScore = 0;
		GS()->Core()->SaveAccount(pPlayer, SAVE_SOCIAL);
	}
}

int CAccountData::GetGold() const
{
	auto* pPlayer = GetPlayer();
	return pPlayer ? pPlayer->GetItem(itGold)->GetValue() : 0;
}

BigInt CAccountData::GetTotalGold() const
{
	auto* pPlayer = GetPlayer();
	return pPlayer ? m_Bank + pPlayer->GetItem(itGold)->GetValue() : 0;
}

void CAccountData::AddExperience(uint64_t Value) const
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// check valid active profession
	const auto pClassProfession = GetActiveProfession();
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
			GS()->Chat(m_ClientID, "You have earned '{} Skill Points'! You now have '{} SP'!", g_Config.m_SvSkillPointsPerLevel, pSkillPoint->GetValue());
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

void CAccountData::AddGold(int Value, bool ApplyBonuses)
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// apply bonuses
	if(ApplyBonuses)
		m_BonusManager.ApplyBonuses(BONUS_TYPE_GOLD, &Value);

	// add gold
	pPlayer->GetItem(itGold)->Add(Value);
}

bool CAccountData::SpendCurrency(int Price, int CurrencyItemID)
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return false;

	// check is free
	if(Price <= 0)
		return true;

	// initialize variables
	auto* pCurrencyItem = pPlayer->GetItem(CurrencyItemID);
	const auto CurrencyValue = pCurrencyItem->GetValue();

	// gold with bank
	if(CurrencyItemID == itGold)
	{
		const auto TotalCurrency = m_Bank + pCurrencyItem->GetValue();

		// check amount
		if(TotalCurrency < Price)
		{
			GS()->Chat(m_ClientID, "Required '{}', but you only have '{} {} (including bank)'!", Price, TotalCurrency, pCurrencyItem->Info()->GetName());
			return false;
		}

		// first, spend currency from player's hand
		int RemainingPrice = Price;
		if(CurrencyValue > 0)
		{
			const auto ToSpendFromHands = minimum(CurrencyValue, RemainingPrice);
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
	if(CurrencyValue < Price)
	{
		GS()->Chat(m_ClientID, "Required '{}', but you only have '{} {} (including bank)'!", Price, CurrencyValue, pCurrencyItem->Info()->GetName());
		return false;
	}

	return pCurrencyItem->Remove(Price);
}

void CAccountData::AddGoldToBank(int Amount)
{
	auto* pPlayer = GetPlayer();
	if(pPlayer)
	{
		m_Bank += Amount;
		GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
	}
}

bool CAccountData::RemoveGoldFromBank(int Amount)
{
	auto* pPlayer = GetPlayer();
	if(pPlayer && m_Bank >= Amount)
	{
		m_Bank -= Amount;
		GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
		return true;
	}

	return false;
}

void CAccountData::HandleChair(uint64_t Exp, int Gold)
{
	// per every sec
	const auto* pServer = Instance::Server();
	if(pServer->Tick() % pServer->TickSpeed() != 0)
		return;

	// check active profession
	const auto* pClassProfession = GetActiveProfession();
	if(!pClassProfession)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::GameWarning, 100, "You don't have an active profession to gain experience!");
		return;
	}

	// initialize variables
	const int level = pClassProfession->GetLevel();
	const int maxGoldCapacity = GetGoldCapacity();
	const bool isGoldBagFull = (GetGold() >= maxGoldCapacity);
	const auto expGain = std::max<uint64_t>(Exp, calculate_exp_gain(g_Config.m_SvChairExpFactor, level, Exp + level));
	const int goldGain = isGoldBagFull ? 0 : maximum(Gold, (int)calculate_gold_gain(g_Config.m_SvChairGoldFactor, level, Gold + level));

	// total percent bonuses
	const int totalPercentBonusGold = round_to_int(m_BonusManager.GetTotalBonusPercentage(BONUS_TYPE_GOLD));
	const int totalPercentBonusExp = round_to_int(m_BonusManager.GetTotalBonusPercentage(BONUS_TYPE_EXPERIENCE));

	// add exp & gold
	AddExperience(expGain);
	if(!isGoldBagFull)
	{
		AddGold(goldGain, true);
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
	GS()->Broadcast(m_ClientID, BroadcastPriority::MainInformation, 50, "Gold {$} of {$} (Total: {$}) : {}\nExp {}/{} : {}",
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