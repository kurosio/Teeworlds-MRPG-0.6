/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include "../houses/house_data.h"
#include "../achievements/achievement_data.h"
#include "../inventory/inventory_manager.h"
#include "../guilds/guild_manager.h"
#include "../worlds/world_manager.h"

std::map < int, CAccountData > CAccountData::ms_aData;
std::map < int, CAccountSharedData > CAccountSharedData::ms_aPlayerSharedData;

CGS* CAccountData::GS() const
{
	return (CGS*)Instance::GameServerPlayer(m_ClientID);
}

CPlayer* CAccountData::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

void CAccountData::ChangeProfession(ProfessionIdentifier Profession)
{
	auto* pOldProfession = m_pActiveProfession;
	m_pActiveProfession = GetProfession(Profession);

	// notify event listener
	g_EventListenerManager.Notify<IEventListener::PlayerProfessionChange>(GetPlayer(), pOldProfession, m_pActiveProfession);
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
	InitSharedEquipments(pResult->getString("EquippedSlots"));
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

void CAccountData::InitSharedEquipments(const std::string& EquippedSlots)
{
	// initialize default equipment slots
	m_EquippedSlots.initSlot(ItemType::EquipPotionHeal);
	m_EquippedSlots.initSlot(ItemType::EquipPotionMana);
	m_EquippedSlots.initSlot(ItemType::EquipEidolon);
	m_EquippedSlots.initSlot(ItemType::EquipTitle);

	// load equipped data
	if(!EquippedSlots.empty())
		m_EquippedSlots.load(EquippedSlots);
	else
		SaveSharedEquipments();
}

void CAccountData::SaveSharedEquipments()
{
	Database->Execute<DB::UPDATE>("tw_accounts_data", "EquippedSlots = '{}' WHERE ID = '{}'", m_EquippedSlots.dumpJson().dump(), m_ID);
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
	if(!pPlayer || !GS()->HasWorldFlag(WORLD_FLAG_CRIME_SCORE))
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
	if(!pPlayer || !GS()->HasWorldFlag(WORLD_FLAG_CRIME_SCORE))
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

void CAccountData::AddExperience(uint64_t Value, bool ApplyBonuses) const
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
	if(ApplyBonuses)
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

void CAccountData::HandleChair(int ChairLevel)
{
	// per every sec
	const auto* pServer = Instance::Server();
	if(pServer->Tick() % pServer->TickSpeed() != 0)
		return;

	// check active profession
	const auto* pClassProfession = GetActiveProfession();
	if(!pClassProfession)
		return;

	// initialize variables
	const int ProfessionLevel = pClassProfession->GetLevel();
	const int MaxGoldCapacity = GetGoldCapacity();
	const bool IsGoldBagFull = (GetGold() >= MaxGoldCapacity);
	const int TotalPercentBonusGold = round_to_int(m_BonusManager.GetTotalBonusPercentage(BONUS_TYPE_GOLD));
	const int TotalPercentBonusExp = round_to_int(m_BonusManager.GetTotalBonusPercentage(BONUS_TYPE_EXPERIENCE));

	// format
	auto gainExp = std::max<uint64_t>(1, calculate_exp_gain(ProfessionLevel, ChairLevel));
	int gainGold = IsGoldBagFull ? 0 : std::max(1, (int)calculate_loot_gain(ChairLevel, 2));
	std::string expStr = fmt_default("+{}", gainExp);
	std::string goldStr = gainGold > 0 ? fmt_default("+{}", gainGold) : "Bag Full";

	// apply bonuses and add info
	if(TotalPercentBonusExp > 0 && gainExp > 0)
	{
		uint64_t bonusExp = 0;
		m_BonusManager.ApplyBonuses(BONUS_TYPE_EXPERIENCE, &gainExp, &bonusExp);
		expStr += fmt_default("+{} (+{}% bonus)", bonusExp, TotalPercentBonusExp);
	}

	if(TotalPercentBonusGold > 0 && gainGold > 0)
	{
		int bonusGold = 0;
		m_BonusManager.ApplyBonuses(BONUS_TYPE_GOLD, &gainGold, &bonusGold);
		goldStr += fmt_default("+{} (+{}% bonus)", bonusGold, TotalPercentBonusGold);
	}

	// add exp & gold
	AddExperience(gainExp, false);
	if(!IsGoldBagFull)
		AddGold(gainGold, false);

	// send broadcast
	GS()->Broadcast(m_ClientID, BroadcastPriority::MainInformation, 50, "Gold {$} of {$} (Total: {$}) : {}\nExp {}/{} : {}",
		GetGold(), MaxGoldCapacity, GetTotalGold(), goldStr.c_str(), pClassProfession->GetExperience(),
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


void CAccountData::AutoEquipSlots(bool OnlyEmptySlots)
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	static constexpr std::array<ItemType, 5> OnlyEmptySlotTypes =
	{
		ItemType::EquipHammer,
		ItemType::EquipGun,
		ItemType::EquipShotgun,
		ItemType::EquipGrenade,
		ItemType::EquipLaser
	};

	// lambda tool
	auto autoEquipSlotsImpl = [&](const auto& equippedSlots)
	{
		for(const auto& [Type, EquippedItemIdOpt] : equippedSlots.getSlots())
		{
			// is type potions group disable auto equip
			if(Type == ItemType::EquipPotionHeal || Type == ItemType::EquipPotionMana)
				continue;

			// is only empty slot and weapons always empty
			if(EquippedItemIdOpt && (OnlyEmptySlots || std::ranges::find(OnlyEmptySlotTypes, Type) != OnlyEmptySlotTypes.end()))
				continue;

			// try equip best item
			auto* pBestItem = GS()->Core()->InventoryManager()->GetBestEquipmentSlotItem(pPlayer, Type);
			if(pBestItem && pBestItem->Equip())
				GS()->Chat(pPlayer->GetCID(), "Auto equip '{} - {}'.", pBestItem->Info()->GetName(), pBestItem->GetStringAttributesInfo(pPlayer));
		}
	};

	// process all relevant equipment slots
	autoEquipSlotsImpl(m_EquippedSlots);

	if(m_pActiveProfession)
		autoEquipSlotsImpl(m_pActiveProfession->GetEquippedSlots());

	for(auto& Prof : GetProfessions())
	{
		if(Prof.IsProfessionType(PROFESSION_TYPE_OTHER))
			autoEquipSlotsImpl(Prof.GetEquippedSlots());
	}
}


bool CAccountData::EquipItem(int ItemID)
{
	bool Successful = false;
	if(m_EquippedSlots.equipItem(ItemID))
	{
		SaveSharedEquipments();
		Successful = true;
	}
	if(m_pActiveProfession && m_pActiveProfession->GetEquippedSlots().equipItem(ItemID))
	{
		m_pActiveProfession->Save();
		Successful = true;
	}
	for(auto& Prof : GetProfessions())
	{
		if(Prof.IsProfessionType(PROFESSION_TYPE_OTHER) && Prof.GetEquippedSlots().equipItem(ItemID))
		{
			Prof.Save();
			Successful = true;
		}
	}

	return Successful;
}


bool CAccountData::UnequipItem(int ItemID)
{
	bool Successful = false;
	if(m_EquippedSlots.unequipItem(ItemID))
	{
		SaveSharedEquipments();
		Successful = true;
	}
	if(m_pActiveProfession && m_pActiveProfession->GetEquippedSlots().unequipItem(ItemID))
	{
		m_pActiveProfession->Save();
		Successful = true;
	}
	for(auto& Prof : GetProfessions())
	{
		if(Prof.IsProfessionType(PROFESSION_TYPE_OTHER) && Prof.GetEquippedSlots().unequipItem(ItemID))
		{
			Prof.Save();
			Successful = true;
		}
	}

	return Successful;
}


bool CAccountData::IsAvailableEquipmentSlot(ItemType Type)
{
	bool Has = false;

	// shared slots always available
	if(m_EquippedSlots.hasSlot(Type))
		Has = true;

	// active profession always available
	if(m_pActiveProfession && m_pActiveProfession->GetEquippedSlots().hasSlot(Type))
		Has = true;

	// other professions always availables
	for(auto& Prof : GetProfessions())
	{
		if(Prof.IsProfessionType(PROFESSION_TYPE_OTHER) && Prof.GetEquippedSlots().hasSlot(Type))
			Has = true;
	}

	return Has;
}


std::optional<int> CAccountData::GetEquippedSlotItemID(ItemType Type) const
{
	// search from shared
	if(m_EquippedSlots.hasSlot(Type))
		return m_EquippedSlots.getEquippedItemID(Type);

	// search from active profession
	if(m_pActiveProfession && m_pActiveProfession->GetEquippedSlots().hasSlot(Type))
		return m_pActiveProfession->GetEquippedSlots().getEquippedItemID(Type);

	// search from other professions
	for(auto& Prof : GetProfessions())
	{
		if(Prof.IsProfessionType(PROFESSION_TYPE_OTHER) && Prof.GetEquippedSlots().hasSlot(Type))
			return Prof.GetEquippedSlots().getEquippedItemID(Type);
	}

	return std::nullopt;
}

int CAccountData::GetFreeSlotsAttributedModules() const
{
	int FreeSlots = g_Config.m_SvAttributedModulesSlots;

	for(auto& [ID, Item] : CPlayerItem::Data()[m_ClientID])
	{
		if(Item.Info()->IsEquipmentModules() && Item.Info()->HasAttributes() && Item.IsEquipped())
		{
			if(!FreeSlots)
				Item.UnEquip();
			else
				FreeSlots--;
		}
	}

	return FreeSlots;
}

int CAccountData::GetFreeSlotsFunctionalModules() const
{
	int FreeSlots = g_Config.m_SvNonAttributedModulesSlots;

	for(auto& [ID, Item] : CPlayerItem::Data()[m_ClientID])
	{
		if(Item.Info()->IsEquipmentModules() && !Item.Info()->HasAttributes() && Item.IsEquipped())
		{
			if(!FreeSlots)
				Item.UnEquip();
			else
				FreeSlots--;
		}
	}

	return FreeSlots;
}
