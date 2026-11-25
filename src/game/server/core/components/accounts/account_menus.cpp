/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_manager.h"

#include <base/system.h>
#include <game/server/gamecontext.h>
#include <generated/server_data.h>
#include <game/server/worldmodes/default.h>

#include <casino/dice_duel_game.h>
#include <casino/roulette_game.h>

#include <game/server/core/components/inventory/item_data.h>
#include <game/server/core/components/mails/mailbox_manager.h>
#include <game/server/core/tools/db_async_context.h>

#include <teeother/components/localization.h>

namespace
{
	constexpr int BET_FIELD_AMOUNT = 0;
}

void CAccountManager::AddMenuProfessionUpgrades(CPlayer* pPlayer, CProfession* pProf) const
{
	if(!pProf || !pPlayer)
		return;

	const char* pProfName = GetProfessionName(pProf->GetProfessionID());
	const auto ClientID = pPlayer->GetCID();
	const auto ProfID = pProf->GetProfessionID();

	VoteWrapper VUpgrades(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "{} upgrades", pProfName);
	{
		for(const auto& ID : pProf->GetAttributes() | std::views::keys)
		{
			const auto* pAttribute = GS()->GetAttributeInfo(ID);
			const int AttributeSize = pPlayer->GetTotalAttributeValue(ID);
			const auto PercentOpt = pPlayer->GetTotalAttributeChance(ID);

			char aBuf[64] {};
			if(PercentOpt)
			{
				str_format(aBuf, sizeof(aBuf), "(%0.2f%%)", *PercentOpt);
			}
			VUpgrades.Add("Total {} - {}{~}", pAttribute->GetName(), AttributeSize, aBuf);
		}
	}
	VUpgrades.AddLine();
	{
		for(auto& [ID, Value] : pProf->GetAttributes())
		{
			const auto* pAttribute = GS()->GetAttributeInfo(ID);
			VUpgrades.AddOption("UPGRADE", MakeAnyList(ProfID, ID), "Upgrade {} - {} ({} point)", pAttribute->GetName(), Value, pAttribute->GetUpgradePrice());
		}
	}
}

bool CAccountManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	// account information
	if(Menulist == MENU_ACCOUNT_DETAIL_INFO)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		auto* pAccount = pPlayer->Account();
		const char* pLastLoginDate = pAccount->GetLastLoginDate();

		// Account information
		VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "Account Information");
		VInfo.Add("Last login: {}", pLastLoginDate);
		VInfo.Add("Account ID: {}", pAccount->GetID());
		VInfo.Add("Login: {}", pAccount->GetLogin());
		VInfo.Add("Class: {}", GetProfessionName(pPlayer->Account()->GetActiveProfessionID()));
		VInfo.Add("Crime score: {}", pAccount->GetCrime());
		VInfo.Add("Gold capacity: {}", pAccount->GetGoldCapacity());
		VInfo.Add("Has house: {}", pAccount->HasHouse() ? "yes" : "no");
		VInfo.Add("Has guild: {}", pAccount->HasGuild() ? "yes" : "no");
		VInfo.Add("In group: {}", pAccount->HasGroup() ? "yes" : "no");
		VoteWrapper::AddEmptyline(ClientID);

		// Ranking information
		VoteWrapper VRating(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "Rating information");
		VRating.Add("Rating: {}({~})", pAccount->GetRatingSystem().GetRating(), pAccount->GetRatingSystem().GetRankName());
		VRating.Add("Progress to {~}: {}/{}", pAccount->GetRatingSystem().GetNextRankName(),
			pAccount->GetRatingSystem().GetRankPointsProgress(), pAccount->GetRatingSystem().GetRankPointsRequired());
		VRating.Add("Played: {}", pAccount->GetRatingSystem().GetPlayed());
		VRating.Add("Win: {} / Losses: {}", pAccount->GetRatingSystem().GetWins(), pAccount->GetRatingSystem().GetLosses());
		VRating.Add("Win rate: {~.2}%", pAccount->GetRatingSystem().GetWinRate());
		VoteWrapper::AddEmptyline(ClientID);

		auto addLevelingInfo = [&](VoteWrapper& Wrapper, const CProfession* pProfession, const std::string& name)
		{
			const auto expNeed = pProfession->GetExpForNextLevel();
			const auto progress = translate_to_percent(expNeed, pProfession->GetExperience());
			const auto progressBar = mystd::string::progressBar(100, progress, 5, "\u25B0", "\u25B1");
			Wrapper.MarkList().Add("{} [Lv{} {}] - {~.2}%", name, pProfession->GetLevel(), progressBar, progress);
		};

		// Leveling information (war)
		VoteWrapper VLevelingWar(ClientID, VWF_ALIGN_TITLE, "Leveling (War professions)");
		for(auto& Profession : pPlayer->Account()->GetProfessions())
		{
			if(Profession.IsProfessionType(PROFESSION_TYPE_WAR))
			{
				const auto AppendActiveStatus = pPlayer->Account()->GetActiveProfessionID() == Profession.GetProfessionID() ? "(A)" : "";
				const auto pProfessionName = std::string(GetProfessionName(Profession.GetProfessionID()));
				const auto Title = AppendActiveStatus + pProfessionName;
				addLevelingInfo(VLevelingWar , &Profession, Title);
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

		// Leveling information (work)
		VoteWrapper VLevelingOther(ClientID, VWF_ALIGN_TITLE, "Leveling (Other professions)");
		for(auto& Profession : pPlayer->Account()->GetProfessions())
		{
			if(Profession.IsProfessionType(PROFESSION_TYPE_OTHER))
			{
				const auto pProfessionName = std::string(GetProfessionName(Profession.GetProfessionID()));
				addLevelingInfo(VLevelingOther, &Profession, pProfessionName);
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

		// Currency information
		const auto currencyItemIDs = CItemDescription::CollectIDs(ItemGroup::Currency, std::nullopt);
		VoteWrapper VCurrency(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "Account Currency");
		VCurrency.Add("Bank: {}", pAccount->GetBankManager());
		for(int itemID : currencyItemIDs)
			VCurrency.Add("{}: {}", pPlayer->GetItem(itemID)->Info()->GetName(), pPlayer->GetItem(itemID)->GetValue());

		// Add backpage
		VoteWrapper::AddBackpage(ClientID);
	}

	// menu upgrades
	if(Menulist == MENU_UPGRADES)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		const auto ActiveProfID = pPlayer->Account()->GetActiveProfessionID();

		// select war profession
		VoteWrapper VClassSelector(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "\u2694 Change class profession");
		VClassSelector.AddMenu(MENU_EQUIPMENT_PRESETS, MENU_UPGRADES, "Manage equipment presets");
		VClassSelector.AddMenu(MENU_UPGRADES_ATTRIBUTES_DETAIL, "Player Attributes: Details");
		if(const auto* pActiveProf = pPlayer->Account()->GetActiveProfession())
		{
			for(const auto& [ID, Percent] : pActiveProf->GetAmplifiers())
			{
				const auto pAttributeName = GS()->GetAttributeInfo(ID)->GetName();
				VClassSelector.Add("Class amplifier +{~.2}% to {}", Percent, pAttributeName);
			}
		}
		VClassSelector.AddLine();
		for(const auto& Prof : pPlayer->Account()->GetProfessions())
		{
			if(Prof.IsProfessionType(PROFESSION_TYPE_WAR))
			{
				const char* StrActiveFlag = (ActiveProfID == Prof.GetProfessionID()) ? "\u2713" : "\u2715";
				const char* pProfName = GetProfessionName(Prof.GetProfessionID());
				const auto expNeed = Prof.GetExpForNextLevel();
				const int progress = round_to_int(translate_to_percent(expNeed, Prof.GetExperience()));
				const auto progressBar = mystd::string::progressBar(100, progress, 5, "\u25B0", "\u25B1");

				VClassSelector.AddOption("SELECT_CLASS", MakeAnyList(Prof.GetProfessionID()), "({}) {} [Lv{} {} {~.1}%] ({}P)",
					StrActiveFlag, pProfName, Prof.GetLevel(), progressBar, progress, Prof.GetUpgradePoint());
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

		// professions
		VoteWrapper VProfessions(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT, "\u2696 Upgrade professions");
		for(const auto& Prof : pPlayer->Account()->GetProfessions())
		{
			const auto ProfessionID = Prof.GetProfessionID();
			VProfessions.AddMenu(MENU_UPGRADES, (int)ProfessionID, "Profession {} ({}P)", GetProfessionName(ProfessionID), Prof.GetUpgradePoint());
		}

		// list upgrades by profession
		if(const auto ProfID = pPlayer->m_VotesData.GetExtraID())
		{
			// check valid profession
			auto* pProf = pPlayer->Account()->GetProfession((ProfessionIdentifier)ProfID.value());
			if(!pProf)
			{
				VoteWrapper::AddBackpage(ClientID);
				return true;
			}

			// add profession upgrades
			VoteWrapper::AddEmptyline(ClientID);
			AddMenuProfessionUpgrades(pPlayer, pProf);
		}

		// Add back page
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_SKILLS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		const auto ActiveProfID = pPlayer->Account()->GetActiveProfessionID();
		const auto spAmount = pPlayer->GetItem(itSkillPoint)->GetValue();
		const auto GetSkillStateIcon = [spAmount](const CSkillDescription* pSkillDesc, const CSkill* pSkill) -> const char*
		{
			if(pSkill->IsLearned())
				return "\u2714";
			if(spAmount >= pSkillDesc->GetPriceSP())
				return "\u2726";
			return "\u2022";
		};
		const auto AddSkillSection = [&](VoteWrapper& VSection, ProfessionIdentifier ProfID)
		{
			int Total = 0;
			int Learned = 0;
			int AvailableToLearn = 0;
			for(const auto& [ID, pSkillDesc] : CSkillDescription::Data())
			{
				if(pSkillDesc->GetProfessionID() != ProfID)
					continue;

				const auto* pSkill = pPlayer->GetSkill(ID);
				Total++;
				if(pSkill->IsLearned())
					Learned++;
				else if(spAmount >= pSkillDesc->GetPriceSP())
					AvailableToLearn++;
			}

			VSection.Add("Learned: {}/{}  |  Ready to learn: {}", Learned, Total, AvailableToLearn);
			VSection.AddLine();

			for(const auto& [ID, pSkillDesc] : CSkillDescription::Data())
			{
				if(pSkillDesc->GetProfessionID() != ProfID)
					continue;

				const auto* pSkill = pPlayer->GetSkill(ID);
				const char* pIcon = GetSkillStateIcon(pSkillDesc, pSkill);
				VSection.AddMenu(MENU_SKILL_SELECT, ID, "{} {} - {}SP | {~}",
					pIcon, pSkillDesc->GetName(), pSkillDesc->GetPriceSP(), pSkill->GetStringLevelStatus());
			}
		};

		VoteWrapper VSkillsInfo(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "\u2654 Skills");
		VSkillsInfo.Add("Active class: {}", GetProfessionName(ActiveProfID));
		VSkillsInfo.Add("Available SP: {}", spAmount);
		VoteWrapper::AddEmptyline(ClientID);

		// class skills
		VoteWrapper VActiveSkills(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "\u2694 Class skills");
		AddSkillSection(VActiveSkills, ActiveProfID);
		VoteWrapper::AddEmptyline(ClientID);

		// universal skills
		VoteWrapper VUniversalSkills(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "\u2694 Universal skills");
		AddSkillSection(VUniversalSkills, ProfessionIdentifier::None);
		VoteWrapper::AddEmptyline(ClientID);

		// Add back page
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_UPGRADES_ATTRIBUTES_DETAIL)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_UPGRADES);

		const std::map<AttributeGroup, std::string> vMapNames =
		{
			{ AttributeGroup::Tank, "Tank-related" },
			{ AttributeGroup::Dps, "Dps-related" },
			{ AttributeGroup::Healer, "Healer-related" },
			{ AttributeGroup::Weapon, "Weapons-related" },
			{ AttributeGroup::DamageType, "Damage-related" },
			{ AttributeGroup::Job, "Job-related" },
			{ AttributeGroup::Other, "Other" }
		};


		VoteWrapper VSelector(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "Select an Attribute Group");
		for(auto& [group, title] : vMapNames)
		{
			const auto groupOpt = pPlayer->m_VotesData.GetExtraID();
			const char* pSelector = GetSelectorStringByCondition(groupOpt && (AttributeGroup)(*groupOpt) == group);
			VSelector.AddMenu(MENU_UPGRADES_ATTRIBUTES_DETAIL, (int)group, "{}{SELECTOR}", title, pSelector);
		}
		VoteWrapper::AddEmptyline(ClientID);

		if(const auto groupOpt = pPlayer->m_VotesData.GetExtraID())
		{
			const auto group = (AttributeGroup)(*groupOpt);
			ShowPlayerAttributesByGroup(pPlayer, vMapNames.at(group), group);
		}

		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
	}

	// settings
	if(Menulist == MENU_SETTINGS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// information
		VoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "Settings Information");
		VInfo.Add("Here you can change the settings of your account.");
		VoteWrapper::AddEmptyline(ClientID);

		// game settings
		VoteWrapper VMain(ClientID, VWF_OPEN, "\u2699 Main settings");
		VMain.AddMenu(MENU_SETTINGS_LANGUAGE, "Settings language");
		VMain.AddMenu(MENU_SETTINGS_ACCOUNT, "Settings account");

		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// settings account
	if(Menulist == MENU_SETTINGS_ACCOUNT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_SETTINGS);

		// information
		VoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Account settings Information");
		VInfo.Add("Some of the settings become valid after death or reconnect.");
		VoteWrapper::AddEmptyline(ClientID);

		// account settings
		VoteWrapper VAccount(ClientID, VWF_OPEN, "\u2699 Account settings");
		const auto& PlayerItems = CPlayerItem::Data()[ClientID];
		for(const auto& [ItemID, ItemData] : PlayerItems)
		{
			if(ItemData.Info()->IsGroup(ItemGroup::Settings) && ItemData.HasItem())
			{
				const char* Status = ItemData.GetSettings() ? "Enabled" : "Disabled";
				VAccount.AddOption("TOGGLE_SETTING", ItemID, "[{}] {}", Status, ItemData.Info()->GetName());
			}
		}

		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// language selection
	if(Menulist == MENU_SETTINGS_LANGUAGE)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_SETTINGS);

		// language information
		const char* pPlayerLanguage = pPlayer->GetLanguage();
		VoteWrapper VLanguageInfo(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "Languages Information");
		VLanguageInfo.Add("Here you can choose the language.");
		VLanguageInfo.Add("Active language: [{~}]", pPlayerLanguage);
		VLanguageInfo.AddLine();
		VLanguageInfo.Add("Playing with the selected language");
		VLanguageInfo.Add("will automatically collect strings");
		VLanguageInfo.Add("that are missing translation.");
		VLanguageInfo.AddLine();
		VLanguageInfo.Add("These strings will later");
		VLanguageInfo.Add("be available on the website.");
		VLanguageInfo.Add("{~}", g_Config.m_SvWebTranslateLink);
		VLanguageInfo.AddLine();
		VLanguageInfo.Add("Any user will be able");
		VLanguageInfo.Add("to translate them there.");
		VoteWrapper::AddEmptyline(ClientID);

		// languages
		VoteWrapper VLanguages(ClientID, VWF_OPEN|VWF_STYLE_STRICT, "Available languages");
		for(int i = 0; i < Server()->Localization()->m_pLanguages.size(); i++)
		{
			// do not show the language that is already selected by the player in the selection lists
			if(str_comp(pPlayerLanguage, Server()->Localization()->m_pLanguages[i]->GetFilename()) == 0)
				continue;

			// add language selection
			auto* pLanguage = Server()->Localization()->m_pLanguages[i];
			const char* pLanguageName = pLanguage->GetName();
			const bool NonDefault = str_comp(pLanguage->GetFilename(), "en") != 0;
			VLanguages.AddOption("SELECT_LANGUAGE", i, "Select language \"{~}\"", pLanguageName);
			if(NonDefault)
				VLanguages.Add("- Number of lines collected: {} / Completed: {}%", pLanguage->GetCollectedLines(), pLanguage->GetCompletionPercent());
			VLanguages.AddLine();
		}

		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

void CAccountManager::ShowPlayerAttributesByGroup(CPlayer* pPlayer, std::string_view Title, AttributeGroup Group) const
{
	const auto ClientID = pPlayer->GetCID();
	VoteWrapper VGroup(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, Title.data());
	for(auto& [ID, pInfo] : CAttributeDescription::Data())
	{
		if(pInfo->IsGroup(Group))
		{
			if(const auto PercentOpt = pPlayer->GetTotalAttributeChance(ID))
			{
				VGroup.Add("{}: {~.2}%", pInfo->GetName(), (*PercentOpt));
				continue;
			}

			const int AttributeSize = pPlayer->GetTotalAttributeValue(ID);
			VGroup.Add("{}: {}", pInfo->GetName(), AttributeSize);
		}
	}
}

bool CAccountManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	// select language command
	if(PPSTR(pCmd, "SELECT_LANGUAGE") == 0)
	{
        const int LanguageID = GetIfExists<int>(Extras, 0, NOPE);
		const char* pSelectedLanguage = Server()->Localization()->m_pLanguages[LanguageID]->GetFilename();
		Server()->SetClientLanguage(ClientID, pSelectedLanguage);
		GS()->Chat(ClientID, "You have chosen a language \"{}\".", pSelectedLanguage);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_SETTINGS_LANGUAGE);
		Core()->SaveAccount(pPlayer, SAVE_LANGUAGE);
		return true;
	}

	// upgrade command
	if(PPSTR(pCmd, "UPGRADE") == 0)
	{
        const auto ProfessionID = GetIfExists<ProfessionIdentifier>(Extras, 0, ProfessionIdentifier::None);
        const auto AttributeID = GetIfExists<AttributeIdentifier>(Extras, 1, AttributeIdentifier::Unknown);

		// check valid profession
		auto* pProfession = pPlayer->Account()->GetProfession(ProfessionID);
		if(!pProfession)
			return false;

		// check valid attribute
		const auto* pAttributeInfo = GS()->GetAttributeInfo(AttributeID);
		if(!pAttributeInfo)
			return false;

		// upgrade
		if(pProfession->Upgrade(AttributeID, ReasonNumber, pAttributeInfo->GetUpgradePrice()))
		{
			const auto nowValue = pProfession->GetAttributeValue(AttributeID);
			const auto pProfessionName = GetProfessionName(ProfessionID);

			g_EventListenerManager.Notify<IEventListener::PlayerProfessionUpgrade>(pPlayer, (int)AttributeID);
			GS()->Chat(ClientID, "[{}] Attribute '{}' enhanced to '{}p'!", pProfessionName, pAttributeInfo->GetName(), nowValue);
			GS()->CreatePlayerSound(ClientID, SOUND_SFX_UPGRADE);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// select class
	if(PPSTR(pCmd, "SELECT_CLASS") == 0)
	{
        const auto ProfessionID = GetIfExists<ProfessionIdentifier>(Extras, 0, ProfessionIdentifier::None);

		// can't change profession in dungeon
		if(GS()->IsWorldType(WorldType::Dungeon))
		{
			GS()->Chat(ClientID, "You can't change profession in dungeons");
			return true;
		}

		// check spam
		if((pPlayer->m_aPlayerTick[LastDamage] + Server()->TickSpeed() * 5) > Server()->Tick())
		{
			GS()->Chat(ClientID, "Wait a couple of seconds, your player is currently in combat or taking damage");
			return true;
		}

		// change class
		pPlayer->Account()->ChangeProfession(ProfessionID);
		pPlayer->GetCharacter()->UpdateEquippedStats();
		pPlayer->m_VotesData.ResetExtraID();
		pPlayer->m_VotesData.UpdateCurrentVotes();
		Core()->SaveAccount(pPlayer, SAVE_PROFESSION);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_SFX_CHANGE_PROFESSION);
		return true;
	}

	return false;
}

void CAccountManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	int ClientID = pPlayer->GetCID();

	HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_BANK_MANAGER, MOTD_MENU_BANK_MANAGER)
	HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_CASINO_ACTION, MOTD_MENU_CASINO)

	// selector language by action tile
	const auto TrySelectLanguageInActionZone = [&](const char* pActionZone, const char* pLanguageFile, const char* pLanguageName)
	{
		// select language
		auto SelectLanguage = [&]()
		{
			Server()->SetClientLanguage(ClientID, pLanguageFile);
			GS()->Chat(ClientID, "You have chosen a language \"{}\".", pLanguageName);
			Core()->SaveAccount(pPlayer, SAVE_LANGUAGE);
			pPlayer->m_VotesData.UpdateVotesIf(MENU_SETTINGS_LANGUAGE);
		};

		const bool InPlayerZone = pChr->GetTiles()->IsActiveActionZone(pActionZone, EActionZonePosition::PLAYER);
		const bool InMouseZone = pChr->GetTiles()->IsActiveActionZone(pActionZone, EActionZonePosition::MOUSE);
		const bool LanguageChanged = str_comp(pPlayer->GetLanguage(), pLanguageFile) != 0;
		if(LanguageChanged && (InPlayerZone || (InMouseZone && Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_FIRE))))
			SelectLanguage();
		if(InMouseZone)
		{
			GS()->Broadcast(ClientID, BroadcastPriority::VeryImportant, Server()->TickSpeed() / 2,
				"Language: {}\nPress fire (mouse) to select.", pLanguageName);
		}

		// leavce
		if(pChr->GetTiles()->IsLeaveActionZone(pActionZone, EActionZonePosition::MOUSE))
			GS()->Broadcast(ClientID, BroadcastPriority::VeryImportant, 1, "");
	};
	TrySelectLanguageInActionZone("LANG_EN", "en", "English");
	TrySelectLanguageInActionZone("LANG_RU", "ru", "Russian");
	TrySelectLanguageInActionZone("LANG_CN", "cn", "Chinese");
}

bool CAccountManager::OnSendMenuMotd(CPlayer* pPlayer, int Menulist)
{
	int ClientID = pPlayer->GetCID();

	// motd menu bank
	if(Menulist == MOTD_MENU_BANK_MANAGER)
	{
		// initialize variables
		const int CurrentGold = pPlayer->Account()->GetGold();
		const BigInt CurrentBankGold = pPlayer->Account()->GetBankManager();
		const BigInt TotalGold = pPlayer->Account()->GetTotalGold();

		MotdMenu MBonuses(ClientID,  "Here you can securely store your gold. Remember, gold in the Bank is protected and will never be lost, even in death.");
		MBonuses.AddText("Bank Management \u2697");
		MBonuses.AddSeparateLine();
		MBonuses.AddText("Gold: {$}", CurrentGold);
		MBonuses.AddText("Bank: {$}", CurrentBankGold);
		MBonuses.AddText("Total: {$}", TotalGold);
		MBonuses.AddText("Commision rate: {}%", g_Config.m_SvBankCommissionRate);
		MBonuses.AddSeparateLine();
		MBonuses.AddOption("BANK_DEPOSIT", "Deposit All").Pack(CurrentGold);
		MBonuses.Send(MOTD_MENU_BANK_MANAGER);
		return true;
	}

	// motd menu about bonuses
	if(Menulist == MOTD_MENU_BONUSES_INFO)
	{
		int Position = 1;
		const auto& rBonusManager = pPlayer->Account()->GetBonusManager();
		MotdMenu MBonuses(ClientID, "World rates, random events, bonus items, and potions add together; a positive bonus adds at least 1 point.");
		MBonuses.AddText("Active bonuses \u2696");
		MBonuses.AddSeparateLine();
		for(const auto& Bonus : rBonusManager.GetTemporaryBonuses())
		{
			int Days, Hours, Minutes, Seconds;
			Bonus.GetRemainingTimeFormatted(&Days, &Hours, &Minutes, &Seconds);

			MBonuses.AddText("{}. {} ({}) - {~.2}%", Position,
				CMultiplierManager::GetTypeName(Bonus.Type), CMultiplierManager::GetSourceName(Bonus.Source), Bonus.Amount);
			MBonuses.AddText("Time left: {}d {}h {}m {}s.", Days, Hours, Minutes, Seconds);
			MBonuses.AddSeparateLine();
			++Position;
		}

		if(Position == 1)
			MBonuses.AddText("No active personal bonuses.");

		const auto& vWorldMultipliers = GS()->m_Multipliers.GetContributions();
		if(!vWorldMultipliers.empty())
		{
			MBonuses.AddSeparateLine();
			MBonuses.AddText("World and random-event sources:");
			for(const auto& Multiplier : vWorldMultipliers)
				MBonuses.AddText("- {}: {} +{~.2}%", Multiplier.m_Name,
					CMultiplierManager::GetTypeName(Multiplier.m_Type), Multiplier.m_Percent);
		}

		bool HasCombinedRates = false;
		for(const auto Type : rBonusManager.GetActiveMultiplierTypes())
		{
			const float BonusPercent = rBonusManager.GetTotalBonusPercentage(Type);
			if(BonusPercent <= 0.0f)
				continue;

			if(!HasCombinedRates)
			{
				MBonuses.AddSeparateLine();
				MBonuses.AddText("Effective combined rates:");
				HasCombinedRates = true;
			}
			const auto Rate = 100.f + BonusPercent;
			MBonuses.AddText("- {}: {~.2}%", CMultiplierManager::GetTypeName(Type), Rate);
		}

		MBonuses.AddBackpage();
		MBonuses.AddSeparateLine();
		MBonuses.Send(MOTD_MENU_BONUSES_INFO);
		return true;
	}

	// motd menu about wanted
	if(Menulist == MOTD_MENU_WANTED_INFO)
	{
		bool hasWanted = false;
		MotdMenu MWanted(ClientID, "A list of wanted players for whom bounties have been assigned. To get the bounty you need to kill a player from the list.");
		MWanted.AddText("Wanted players \u2694");
		MWanted.AddSeparateLine();
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			CPlayer* pPl = GS()->GetPlayer(i, true);
			if(!pPl || !pPl->Account()->IsCrimeMaxedOut())
				continue;

			CPlayerItem* pItemGold = pPl->GetItem(itGold);
			const int Reward = minimum(translate_to_percent_rest(pItemGold->GetValue(), (float)g_Config.m_SvArrestGoldOnDeath), pItemGold->GetValue());
			MWanted.AddText("{~}", Server()->ClientName(i));
			MWanted.AddText("Reward: {$} gold", Reward);
			MWanted.AddText("Last seen in : {}", Server()->GetWorldName(pPl->GetCurrentWorldID()));
			MWanted.AddSeparateLine();
			hasWanted = true;
		}

		if(!hasWanted)
		{
			MWanted.AddText("No wanted players!");
			MWanted.AddSeparateLine();
		}

		MWanted.AddBackpage();
		MWanted.AddSeparateLine();
		MWanted.Send(MOTD_MENU_WANTED_INFO);
		return true;
	}

	// motd menu personal assistant
	if(Menulist == MOTD_MENU_PERSONAL_ASSISTANT)
	{
		// The assistant counts the letters, so the menu is built when the answer
		// is in instead of waiting for it here.
		auto pAccount = pPlayer->Account();
		auto pContext = DbAsync::MakeContext(ClientID);
		Core()->MailboxManager()->CountMails(pAccount->GetID()).Then([pContext](Db::CResultSet&& Result)
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer)
				return;

			auto* pAccount = pPlayer->Account();
			const auto gold = pAccount->GetGold();
			const auto goldCapacity = pAccount->GetGoldCapacity();
			const auto bankGold = pAccount->GetBankManager();
			const int mailCount = Result.HasRows() ? Result.GetInt("Letters") : 0;

			// personal assistant
			MotdMenu MAssistant(pContext->GetClientID(), MTFLAG_CLOSE_BUTTON, "Quick tips update as you progress.");
			MAssistant.AddText("Profession: {}", GetProfessionName(pAccount->GetActiveProfessionID()));
			MAssistant.AddText("Level {}.", pAccount->GetLevel());
			MAssistant.AddText("Gold: {$}/{$}", gold, goldCapacity);
			MAssistant.AddText("Bank: {$}", bankGold);
			MAssistant.AddText("World: {}", pContext->Server()->GetWorldName(pPlayer->GetCurrentWorldID()));
			MAssistant.AddSeparateLine();

			// recommendations
			MAssistant.AddText("Recommendations:");
			bool hasRecommendation = false;
			const auto crimeScore = pAccount->GetCrime();
			const auto totalProfUP = pAccount->GetTotalProfessionsUpgradePoints();

			if(mailCount > 0)
			{
				MAssistant.AddText("- You have {} mail waiting for you.", mailCount);
				hasRecommendation = true;
			}
			if(goldCapacity > 0 && gold >= (goldCapacity * 8) / 10)
			{
				MAssistant.AddText("- Consider depositing gold to avoid loss.");
				hasRecommendation = true;
			}
			if(crimeScore > 0)
			{
				MAssistant.AddText("- Your crime score attracts bounty hunters.");
				hasRecommendation = true;
			}
			if(totalProfUP > 0)
			{
				MAssistant.AddText("- Spend {} upgrade points to strengthen your profession.", totalProfUP);
				hasRecommendation = true;
			}
			if(!hasRecommendation)
				MAssistant.AddText("- Everything looks good. Keep it up!");
			MAssistant.AddSeparateLine();

			// quick actions
			MAssistant.AddText("Quick actions:");
			MAssistant.AddMenu(MOTD_MENU_BONUSES_INFO, NOPE, "\u2696 Active bonuses");
			MAssistant.AddMenu(MOTD_MENU_WANTED_INFO, NOPE, "\u2694 Wanted list");
			MAssistant.AddMenu(MOTD_MENU_WIKI_INFO, NOPE, "\u2605 Wiki guide");
			MAssistant.AddSeparateLine();
			MAssistant.Send(MOTD_MENU_PERSONAL_ASSISTANT);
		});
		return true;
	}

	// motd menu casino dice
	if (Menulist == MOTD_MENU_CASINO)
	{
		auto* pController = dynamic_cast<CGameControllerDefault*>(GS()->m_pController);
		if (!pController)
			return true;

		CCasinoGame* pGame = pController->GetCasinoGameInRange(pPlayer->m_ViewPos);
		if (!pGame)
			return false;

		CDiceDuelGame* pDiceGame = dynamic_cast<CDiceDuelGame*>(pGame);
		CRouletteGame* pRouletteGame = dynamic_cast<CRouletteGame*>(pGame);
		if (!pDiceGame && !pRouletteGame)
			return true;

		const auto* pCurrency = pPlayer->GetItem(pPlayer->m_CurrencyCasinoItemID);
		const int TimeLeft = (pGame->GetTicksLeft() / Server()->TickSpeed());
		const bool bIsJoined = pGame->IsPlayerJoined(pPlayer->GetCID());
		const char* pMenuDesc = pDiceGame
			? "Roll the dice and test your luck!\nThe more you bet, the higher your chances of winning."
			: "Spin the wheel of fortune!\nThe more you bet, the higher your chances of winning.";
		const char* pMenuTitle = pDiceGame ? "\u2696 Casino: Dice Duel" : "\u2696 Casino: Roulette";
		MotdMenu MCasino(ClientID, MTFLAG_ALWAYS_UPDATE, pMenuDesc);
		MCasino.AddText(pMenuTitle);
		MCasino.AddSeparateLine();

		MCasino.AddText("Enter the amount bet:");
		MCasino.AddMenu(MOTD_MENU_CASINO_CURRENCY_SELECT, NOPE, "Currency: {} ({$})", pCurrency->Info()->GetName(), 
			(pCurrency->GetID() == itGold) ? pPlayer->Account()->GetTotalGold() : pCurrency->GetValue());
		MCasino.AddField(BET_FIELD_AMOUNT, MTTEXTINPUTFLAG_ONLY_NUMERIC);
		MCasino.AddSeparateLine();

		if (pDiceGame)
		{
			if (TimeLeft > 0 && pDiceGame->GetState() == EDiceGameState::LOBBY)
				MCasino.AddText("Starting in: {} sec.", TimeLeft);
			else if (pDiceGame->GetState() == EDiceGameState::ROLLING)
				MCasino.AddText("Rolling the dice...");
			else if (pDiceGame->GetState() == EDiceGameState::SHOW_RESULT)
				MCasino.AddText("Waiting to start round...");
			else
				MCasino.AddText("Waiting for players...");
		}
		else if (pRouletteGame)
		{
			if (TimeLeft > 0 && pRouletteGame->GetState() == ERouletteState::LOBBY)
				MCasino.AddText("Starting in: {} sec.", TimeLeft);
			else if (pRouletteGame->GetState() == ERouletteState::SPINNING)
				MCasino.AddText("Spinning the wheel...");
			else if (pRouletteGame->GetState() == ERouletteState::SHOW_RESULT)
				MCasino.AddText("Waiting to start round...");
			else
				MCasino.AddText("Waiting for players...");
		}

		if (bIsJoined)
		{
			MCasino.AddOption("CASINO_BET_LEAVE", "Leave the game");
		}
		else
		{
			if (pDiceGame)
			{
				MCasino.AddOption("DICE_BET_ENTER", "Bet on High (8-12)").Pack(EDiceBetType::HIGH, 7);
				MCasino.AddOption("DICE_BET_ENTER", "Bet on Equal (7)").Pack(EDiceBetType::EQUAL, 7);
				MCasino.AddOption("DICE_BET_ENTER", "Bet on Low (2-6)").Pack(EDiceBetType::LOW, 7);
			}
			else if (pRouletteGame)
			{
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on range 1-12").Pack(ERouletteBetKind::RANGE, (int)ERouletteRange::LOW);
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on range 13-24").Pack(ERouletteBetKind::RANGE, (int)ERouletteRange::MID);
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on range 25-36").Pack(ERouletteBetKind::RANGE, (int)ERouletteRange::HIGH);
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on Red").Pack(ERouletteBetKind::COLOR, (int)ERouletteColor::RED);
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on Green").Pack(ERouletteBetKind::COLOR, (int)ERouletteColor::GREEN);
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on Black").Pack(ERouletteBetKind::COLOR, (int)ERouletteColor::BLACK);
			}
		}

		MCasino.Send(MOTD_MENU_CASINO);
		return true;
	}

	if (Menulist == MOTD_MENU_CASINO_CURRENCY_SELECT)
	{
		MotdMenu MCasino(ClientID, MTFLAG_ALWAYS_UPDATE);
		for(auto& [ItemID, ItemData] : CPlayerItem::Data()[ClientID])
		{
			if(ItemData.Info()->IsGroup(ItemGroup::Currency))
			{
				MCasino.AddOption("CASINO_CURRENCY_SELECT", "{} ({$})", ItemData.Info()->GetName(), 
					(ItemID == itGold ? pPlayer->Account()->GetTotalGold() : ItemData.GetValue())).Pack(ItemID);
			}
		}
		MCasino.AddBackpage();
		MCasino.Send(MOTD_MENU_CASINO_CURRENCY_SELECT);
		return true;
	}

	return false;
}

bool CAccountManager::OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd)
{
	// casino bet currency select
	if (strcmp(pCmd, "CASINO_CURRENCY_SELECT") == 0)
	{
		const auto& [ItemID] = pMotdData->GetCurrent()->Unpack<int>();
		pPlayer->m_CurrencyCasinoItemID = ItemID;
		pPlayer->CloseMotdMenu();
		GS()->SendMenuMotd(pPlayer, MOTD_MENU_CASINO);
		return true;
	}

	// casino bet leave
	if (strcmp(pCmd, "CASINO_BET_LEAVE") == 0)
	{
		auto* pController = dynamic_cast<CGameControllerDefault*>(GS()->m_pController);
		if (!pController)
			return true;

		CCasinoGame* pGame = pController->GetCasinoGameInRange(pPlayer->m_ViewPos);
		if (pGame && pGame->Leave(pPlayer->GetCID()))
			GS()->Chat(pPlayer->GetCID(), "You have left the dice game.");
		return true;
	}

	// roulette bet currency select
	if (strcmp(pCmd, "ROULETTE_BET_ENTER") == 0)
	{
		const auto& [Kind, Value] = pMotdData->GetCurrent()->Unpack<ERouletteBetKind, int>();
		const auto Bet = pMotdData->GetFieldStr(BET_FIELD_AMOUNT);
		if (!Bet.has_value())
		{
			GS()->Chat(pPlayer->GetCID(), "Please enter a bet amount.");
			return true;
		}

		const auto BetAmount = clamp(str_toint(Bet.value().c_str()), 0, 1000000000);
		auto* pController = dynamic_cast<CGameControllerDefault*>(GS()->m_pController);
		if (pController)
		{
			CCasinoGame* pGame = pController->GetCasinoGameInRange(pPlayer->m_ViewPos);
			CRouletteGame* pTable = dynamic_cast<CRouletteGame*>(pGame);
			if (!pTable || !pTable->Join(pPlayer->GetCID(), BetAmount, Kind, Value))
				GS()->Chat(pPlayer->GetCID(), "Failed to join the roulette game. Please check your gold and try again.");
		}
		return true;
	}

	// dice bet high
	if (strcmp(pCmd, "DICE_BET_ENTER") == 0)
	{
		const auto Bet = pMotdData->GetFieldStr(BET_FIELD_AMOUNT);
		if(!Bet.has_value())
		{
			GS()->Chat(pPlayer->GetCID(), "Please enter a bet amount.");
			return true;
		}

		const auto& [BetType, Threshold] = pMotdData->GetCurrent()->Unpack<EDiceBetType, int>();
		const auto BetAmount = clamp(str_toint(Bet.value().c_str()), 0, 1000000000);
		auto* pController = dynamic_cast<CGameControllerDefault*>(GS()->m_pController);
		if (pController)
		{
			CCasinoGame* pGame = pController->GetCasinoGameInRange(pPlayer->m_ViewPos);
			CDiceDuelGame* pTable = dynamic_cast<CDiceDuelGame*>(pGame);
			if (!pTable || !pTable->Join(pPlayer->GetCID(), BetAmount, BetType, Threshold))
				GS()->Chat(pPlayer->GetCID(), "Failed to join the dice game. Please check your gold and try again.");
		}
		return true;
	}

	// deposit bank gold
	if(strcmp(pCmd, "BANK_DEPOSIT") == 0)
	{
		auto* pGold = pPlayer->GetItem(itGold);
		if(pGold->GetValue() <= 100)
		{
			GS()->Chat(pPlayer->GetCID(), "You need at least '100 gold' to deposit.");
			return true;
		}

		const auto& [Value] = pMotdData->GetCurrent()->Unpack<int>();
		if(pGold->GetValue() < Value)
		{
			GS()->Chat(pPlayer->GetCID(), "You can max deposit '{} gold'.", pGold->GetValue());
			return true;
		}

		const auto CommisionValue = translate_to_percent_rest(Value, g_Config.m_SvBankCommissionRate);
		const auto FinalValue = Value - CommisionValue;

		pGold->Remove(Value);
		pPlayer->Account()->AddGoldToBank(FinalValue);
		GS()->Chat(pPlayer->GetCID(), "You have successfully deposited '{} gold'. Commision '{} gold'.", FinalValue, CommisionValue);
		GS()->Chat(pPlayer->GetCID(), "Commision '{} gold'.", CommisionValue);
		return true;
	}

	return false;
}
