/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_data.h"
#include "account_db.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include <components/houses/house_data.h>
#include <components/achievements/achievement_data.h>
#include <components/inventory/inventory_manager.h>
#include <components/guilds/guild_manager.h>
#include <components/worlds/world_manager.h>
#include <tools/db_async_context.h>

std::map < int, CAccountData > CAccountData::ms_aData;
std::map < int, CAccountSharedData > CAccountSharedData::ms_aPlayerSharedData;

namespace
{
	int GetFreeModuleSlotsByType(int ClientID, int MaxSlots, bool WithAttributes)
	{
		int FreeSlots = MaxSlots;
		for(auto& [ID, Item] : CPlayerItem::Data()[ClientID])
		{
			if(Item.Info()->IsEquipmentModules() && Item.Info()->HasAttributes() == WithAttributes && Item.IsEquipped())
			{
				if(!FreeSlots)
					Item.UnEquip();
				else
					FreeSlots--;
			}
		}

		return FreeSlots;
	}
}

namespace
{
	struct SAchievementKey
	{
		int m_Type {};
		int m_Criteria {};

		bool operator<(const SAchievementKey& Other) const
		{
			return std::tie(m_Type, m_Criteria) < std::tie(Other.m_Type, Other.m_Criteria);
		}
	};

	struct SAchievementProgress
	{
		int m_Progress {};
		int m_CompletedLevel {};
	};
}

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
	auto* pNewProfession = GetProfession(Profession);
	if(!pNewProfession || pOldProfession == pNewProfession)
		return;

	m_pActiveProfession = pNewProfession;

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

void CAccountData::Init(int ID, int ClientID, const char* pLogin, std::string Language, std::string LoginDate, const Db::CResultSet& Result)
{
	// asserts
	dbg_assert(m_ID <= 0 || !Result.HasRows(), "Unique AccountID cannot change the value more than 1 time");

	// initialize
	IServer* pServer = Instance::Server();
	m_Login = pLogin;
	m_LastLoginDate = LoginDate;
	m_ID = ID;
	m_ClientID = ClientID;
	m_ReadyToPlay = false;
	m_CrimeScore = Result.GetInt("CrimeScore");
	m_aHistoryWorld.push_front(Result.GetInt("WorldID"));
	m_Bank = Result.GetString("Bank");
	m_Periods.m_DailyStamp = Result.GetInt64("DailyStamp");
	m_Periods.m_WeekStamp = Result.GetInt64("WeekStamp");
	m_Periods.m_MonthStamp = Result.GetInt64("MonthStamp");
	m_LastTickCrimeScoreChanges = pServer->Tick();
	pServer->SetClientLanguage(m_ClientID, Language.c_str());


	// update login ip, date, country (fire and forget: the login does not wait for it)
	char aAddrStr[NETADDR_MAXSTRSIZE]{};
	pServer->GetClientAddr(m_ClientID, aAddrStr, sizeof(aAddrStr));
	AccountDb::UpdateLoginStamp(ID, aAddrStr, Instance::Server()->ClientCountryIsoCode(m_ClientID));


	// initialize sub account data. What needs a statement is loaded afterwards.
	m_LoadPlan.Cancel();
	m_LoadPlan = CAccountLoadPlan {};
	m_SessionToken.clear();
	InitProfessions();
	InitSharedEquipments(Result.GetString("EquippedSlots"));
	InitEquipmentPresets(Result.GetString("EquipmentPresets"));
	InitAchievements();
	m_pActiveProfession = GetProfession((ProfessionIdentifier)Result.GetInt("ProfessionID"));
	m_BonusManager.Init(m_ClientID);
	m_PrisonManager.Init(m_ClientID);
	m_RatingSystem.Init(this);
	ReinitializeHouse();
	ReinitializeGuild();
}

void CAccountData::InitProfessions()
{
	m_vProfessions.clear();

	// initialize base professions
	m_vProfessions.push_back(CTankProfession());
	m_vProfessions.push_back(CDPSProfession());
	m_vProfessions.push_back(CHealerProfession());
	m_vProfessions.push_back(CFarmerProfession());
	m_vProfessions.push_back(CMinerProfession());
	m_vProfessions.push_back(CFishermanProfession());
	m_vProfessions.push_back(CLoaderProfession());
}

void CAccountData::RegisterLoadStages()
{
	LoadProfessionsData();
	LoadAchievementsProgress();
	m_BonusManager.RegisterLoadStage();
	m_PrisonManager.RegisterLoadStage();
	m_RatingSystem.RegisterLoadStage();
}

void CAccountData::AddLoadStage(std::string Name, CAccountLoadPlan::CStage Start, std::vector<std::string> Dependencies)
{
	m_LoadPlan.Add(std::move(Name), std::move(Dependencies), std::move(Start));
}

void CAccountData::RunLoadStages(std::function<void(bool)> OnComplete)
{
	m_LoadPlan.Start(std::move(OnComplete));
}

void CAccountData::CancelLoad()
{
	m_LoadPlan.Cancel();
}

void CAccountData::LoadProfessionsData()
{
	const int ClientID = m_ClientID;
	const int AccountID = m_ID;
	AddLoadStage("professions", [ClientID, AccountID](CLoadDone Done)
	{
		auto pContext = DbAsync::MakeContext(ClientID);
		AccountDb::LoadProfessions(AccountID).Then([pContext, AccountID, Done = std::move(Done)](Db::CResultSet&& Rows) mutable
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer || !Rows.Ok())
			{
				Done(false);
				return;
			}

			auto* pAccount = pPlayer->Account();
			std::map<ProfessionIdentifier, std::string> vData {};
			while(Rows.Next())
				vData[(ProfessionIdentifier)Rows.GetInt("ProfessionID")] = Rows.GetString("Data");

			std::vector<AccountDb::SProfessionData> vMissing;
			for(auto& Profession : pAccount->m_vProfessions)
			{
				const auto it = vData.find(Profession.GetProfessionID());
				const auto optData = it != vData.end() ? std::make_optional<std::string>(it->second) : std::nullopt;
				Profession.Init(pAccount->m_ClientID, optData);
				if(!optData)
					vMissing.push_back({ (int)Profession.GetProfessionID(), Profession.GetPreparedJsonString() });
			}
			if(vMissing.empty())
			{
				Done(true);
				return;
			}

			AccountDb::CreateProfessions(AccountID, vMissing).Then([Done = std::move(Done)](Db::CResultSet&& Created) mutable
			{
				Done(Created.Ok());
			});
		});
	});
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
	AccountDb::SaveEquippedSlots(m_ID, m_EquippedSlots.dumpJson().dump());
}

void CAccountData::InitEquipmentPresets(const std::string& PresetsJson)
{
	if(!PresetsJson.empty())
		m_EquipmentPresets.initialize(m_ClientID, PresetsJson);
}

void CAccountData::SaveEquipmentPresets()
{
	AccountDb::SaveEquipmentPresets(m_ID, m_EquipmentPresets.dumpJson().dump());
}

void CAccountData::InitAchievements()
{
	// The elements exist as soon as the client is there; their progress is the
	// statement of LoadAchievementsProgress().
	m_apAchievements.clear();
	auto* pPlayer = GetPlayer();
	for(const auto& pAchievement : CAchievementInfo::Data())
	{
		const int AchievementID = pAchievement->GetID();
		m_apAchievements[AchievementID] = CAchievement::CreateElement(pAchievement, pPlayer->GetCID());
	}
}

void CAccountData::LoadAchievementsProgress()
{
	const int ClientID = m_ClientID;
	const int AccountID = m_ID;
	AddLoadStage("achievements", [ClientID, AccountID](CLoadDone Done)
	{
		auto pContext = DbAsync::MakeContext(ClientID);
		AccountDb::LoadAchievements(AccountID).Then([pContext, Done = std::move(Done)](Db::CResultSet&& Rows) mutable
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer || !Rows.Ok())
			{
				Done(false);
				return;
			}

			auto* pAccount = pPlayer->Account();
			std::map<SAchievementKey, SAchievementProgress> vProgress {};
			while(Rows.Next())
			{
				const SAchievementKey Key { Rows.GetInt("AchievementType"), Rows.GetInt("Criteria") };
				const SAchievementProgress Progress { Rows.GetInt("Progress"), Rows.GetInt("CompletedLevel") };
				vProgress[Key] = Progress;
			}

			for(const auto& pAchievement : CAchievementInfo::Data())
			{
				const int AchievementID = pAchievement->GetID();
				const SAchievementKey Key { (int)pAchievement->GetType(), pAchievement->GetCriteria() };
				const auto it = vProgress.find(Key);
				if(it == vProgress.end())
					continue;

				const SAchievementProgress& ProgressData = it->second;
				const int Required = pAchievement->GetRequired();
				const int ClampedProgress = clamp(ProgressData.m_Progress, 0, Required);
				const bool Completed = ProgressData.m_CompletedLevel >= AchievementID || ClampedProgress >= Required;
				if(pAccount->m_apAchievements.contains(AchievementID))
					pAccount->m_apAchievements[AchievementID]->Init(ClampedProgress, Completed);
			}

			pAccount->m_apAchievements.clear();
			Done(true);
		});
	});
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
			GS()->Chat(-1, "'{~}' added to wanted list, reward for elimination.", GS()->Server()->ClientName(pPlayer->GetCID()));

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
			GS()->Chat(-1, "'{~}' removed from wanted list.", GS()->Server()->ClientName(pPlayer->GetCID()));

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

void CAccountData::AddExperience(uint64_t Value, bool ApplyMultipliers) const
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
	if(ApplyMultipliers)
	{
		m_BonusManager.ApplyBonuses(MultiplierType::Experience, &Value);
	}

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

void CAccountData::AddGold(int Value, bool ApplyMultipliers)
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// apply bonuses
	if(ApplyMultipliers)
	{
		m_BonusManager.ApplyBonuses(MultiplierType::Gold, &Value);
	}

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
	const int TotalPercentBonusGold = round_to_int(m_BonusManager.GetTotalBonusPercentage(MultiplierType::Gold));
	const int TotalPercentBonusExp = round_to_int(m_BonusManager.GetTotalBonusPercentage(MultiplierType::Experience));

	// format
	auto gainExp = std::max<uint64_t>(1, calculate_exp_gain(ProfessionLevel, ChairLevel));
	int gainGold = IsGoldBagFull ? 0 : std::max(1, (int)calculate_loot_gain(ChairLevel, 2));
	std::string expStr = fmt_default("+{}", gainExp);
	std::string goldStr = gainGold > 0 ? fmt_default("+{}", gainGold) : "Bag Full";

	// apply bonuses and add info
	if(TotalPercentBonusExp > 0 && gainExp > 0)
	{
		uint64_t bonusExp = 0;
		m_BonusManager.ApplyBonuses(MultiplierType::Experience, &gainExp, &bonusExp);
		expStr += fmt_default(" (+{} bonus, +{}%)", bonusExp, TotalPercentBonusExp);
	}

	if(TotalPercentBonusGold > 0 && gainGold > 0)
	{
		int bonusGold = 0;
		m_BonusManager.ApplyBonuses(MultiplierType::Gold, &gainGold, &bonusGold);
		goldStr += fmt_default(" (+{} bonus, +{}%)", bonusGold, TotalPercentBonusGold);
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

void CAccountData::UpdateAchievementProgress(const CAchievementInfo* pInfo, int Progress, bool Completed)
{
	if(!pInfo)
		return;

	const int AchievementType = (int)pInfo->GetType();
	const int Criteria = pInfo->GetCriteria();
	const int CompletedLevel = Completed ? pInfo->GetID() : 0;

	AccountDb::SaveAchievement(m_ID, AchievementType, Criteria, Progress, CompletedLevel);
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

	static constexpr std::array<ItemType, 4> DisabledAutoEquipTypes =
	{
		ItemType::EquipPotionHeal,
		ItemType::EquipPotionMana,
		ItemType::EquipPotionMiner,
		ItemType::EquipEidolon,
	};

	// the items of this account, as they were loaded
	const auto& PlayerItems = CPlayerItem::Data()[pPlayer->GetCID()];

	auto autoEquipSlotsImpl = [&](const auto& equippedSlots)
	{
		for(const auto& [Type, EquippedItemIdOpt] : equippedSlots.getSlots())
		{
			if(std::ranges::find(DisabledAutoEquipTypes, Type) != DisabledAutoEquipTypes.end())
				continue;

			const auto ItemIt = EquippedItemIdOpt ? PlayerItems.find(*EquippedItemIdOpt) : PlayerItems.end();
			const bool HasOwnedItem = ItemIt != PlayerItems.end() && ItemIt->second.HasItem();

			// A slot that names an item the account does not own equips nothing and
			// would stay busy for good, so it is treated like an empty one.
			if(HasOwnedItem && (OnlyEmptySlots || std::ranges::find(OnlyEmptySlotTypes, Type) != OnlyEmptySlotTypes.end()))
				continue;

			auto* pBestItem = GS()->Core()->InventoryManager()->GetBestEquipmentSlotItem(pPlayer, Type);
			if(pBestItem && pBestItem->Equip())
			{
				if(pBestItem->Info()->HasAttributes())
					GS()->Chat(pPlayer->GetCID(), "Auto equip '{} - {~}'.", pBestItem->Info()->GetName(), pBestItem->GetStringAttributesInfo(pPlayer));
				else
					GS()->Chat(pPlayer->GetCID(), "Auto equip '{}'.", pBestItem->Info()->GetName());
			}
		}
	};

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

int CAccountData::GetMaxSlotsPassiveModules() const
{
	return g_Config.m_SvAttributedModulesSlots + GetPlayer()->GetSkill(SKILL_MASTER_ADVENTURER)->GetMod(SkillMod::MasterAdventurerExtraSlotPassiveModule);
}

int CAccountData::GetMaxSlotsActiveModules() const
{
	return g_Config.m_SvNonAttributedModulesSlots + GetPlayer()->GetSkill(SKILL_MASTER_ADVENTURER)->GetMod(SkillMod::MasterAdventurerExtraSlotActiveModule);
}

int CAccountData::GetFreeSlotsPassiveModules() const
{
	return GetFreeModuleSlotsByType(m_ClientID, GetMaxSlotsPassiveModules(), true);
}

int CAccountData::GetFreeSlotsActiveModules() const
{
	return GetFreeModuleSlotsByType(m_ClientID, GetMaxSlotsActiveModules(), false);
}

void CAccountData::UpdateAuthTimeoutCodeIfNeeded()
{
	auto* pPlayer = GetPlayer();
	if(pPlayer && pPlayer->IsAuthed() && !pPlayer->GetSharedData().m_TimeoutCode.empty())
	{
		const std::string TimeoutCode = pPlayer->GetSharedData().m_TimeoutCode;
		auto pContext = DbAsync::MakeContext(pPlayer->GetCID());
		AccountDb::SaveTimeoutCode(TimeoutCode, Instance::Server()->ClientName(pPlayer->GetCID()))
			.Then([pContext](Db::CResultSet&& Result)
		{
			if(Result.Changed() && pContext->GetPlayer())
			{
				pContext->GS()->Chat(pContext->GetClientID(), "Timeout code has been updated!");
				pContext->GS()->Chat(pContext->GetClientID(), "You will now use it to authorize your account.");
			}
		});
		pPlayer->GetSharedData().m_TimeoutCode.clear();
	}
}

bool CAccountSharedData::TryUpdateSkinColors(CNetObj_ClientInfo* pClientInfo)
{
	// enabled rainbow
	if(m_RainbowMode != RAINBOW_MODE_DISABLED)
	{
		// rainbow mode
		const int Tick = Instance::Server()->Tick();
		const int TickSpeed = Instance::Server()->TickSpeed();
		const float Speed = 0.002f;
		m_RainbowHue = fmodf(m_RainbowHue + Speed, 1.0f);
		float BodyHue = m_RainbowHue;
		float FeetHue = m_RainbowHue;
		float Saturation = 1.0f;
		float Lightness = 0.5f;

		if(m_RainbowMode == RAINBOW_MODE_OPPOSITE)
		{
			FeetHue = fmodf(BodyHue + 0.5f, 1.0f);
		}
		else if(m_RainbowMode == RAINBOW_MODE_RANDOM)
		{
			const int Time = Tick / 25;
			BodyHue = fmodf(Time * 0.173f, 1.0f);
			FeetHue = fmodf(Time * 0.317f, 1.0f);
		}
		else if(m_RainbowMode == RAINBOW_MODE_BOO)
		{
			const float Time = Tick / (float)TickSpeed;
			BodyHue = 0.0f;
			FeetHue = 0.0f;
			Saturation = 0.0f;
			Lightness = (std::sin((pi * Time) / 3) + 1.0f) / 2.0f;
		}

		const ColorRGBA BodyColor = color_cast<ColorRGBA>(
			ColorHSLA(BodyHue, Saturation, Lightness, 1.0f)
		);

		const ColorRGBA FeetColor = color_cast<ColorRGBA>(
			ColorHSLA(FeetHue, Saturation, Lightness, 1.0f)
		);

		pClientInfo->m_UseCustomColor = 1;
		pClientInfo->m_ColorBody = BodyColor.Pack(false);
		pClientInfo->m_ColorFeet = FeetColor.Pack(false);
		return true;
	}

	return false;
}
