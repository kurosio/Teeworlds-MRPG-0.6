/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_manager.h"

#include <base/hash_ctxt.h>
#include <game/server/gamecontext.h>
#include <generated/server_data.h>


#include <game/server/core/components/inventory/inventory_manager.h>
#include <game/server/core/components/mails/mailbox_manager.h>
#include <game/server/core/components/worlds/world_data.h>
#include <game/server/core/tools/db_async_context.h>

#include <teeother/components/localization.h>

constexpr int LENGTH_LOGPASS_MAX = 12;
constexpr int LENGTH_LOGPASS_MIN = 4;
constexpr int LENGTH_PIN_MAX = 6;
constexpr int LENGTH_PIN_MIN = 4;
constexpr int AUTH_FIELD_LOGIN = 0;
constexpr int AUTH_FIELD_PASSWORD = 1;

namespace
{
	std::string BuildGuestCredential(const char* pNickname)
	{
		return std::string("Guest-") + pNickname;
	}

	bool IsGuestCredentialForNickname(const std::string& Login, const char* pNickname)
	{
		return Login == BuildGuestCredential(pNickname);
	}

	bool CheckLoginPasswordLength(CGS* pGS, int ClientID, const char* pLogin, const char* pPassword)
	{
		const int LengthLogin = str_length(pLogin);
		const int LengthPassword = str_length(pPassword);
		if(LengthLogin <= LENGTH_LOGPASS_MAX && LengthLogin >= LENGTH_LOGPASS_MIN
			&& LengthPassword <= LENGTH_LOGPASS_MAX && LengthPassword >= LENGTH_LOGPASS_MIN)
			return true;

		pGS->Chat(ClientID, "The username and password must each contain '{} - {} characters'.", LENGTH_LOGPASS_MIN, LENGTH_LOGPASS_MAX);
		return false;
	}
}

class DbAuthorization
{
	struct CAuthorizationPayload
	{
		CAccountManager* m_pAccountManager{};
		std::string m_Login{};
		std::string m_Password{};
		int m_AccountID{};
		std::string m_Language{};
		std::string m_LoginDate{};
		std::string m_PinCode{};
	};
	using CAuthorizationContextPtr = std::shared_ptr<DbAsync::CContext<CAuthorizationPayload>>;

	static void OnCheckNickname(const CAuthorizationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer)
			return;

		if(!pRes->next())
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Sorry, we couldn't locate your username in our system.");
			return;
		}

		auto& Data = pContext->Data();
		Data.m_AccountID = pRes->getInt("ID");
		auto pCheck = Database->Prepare<DB::SELECT>("ID, LoginDate, Language, PinCode, Password, PasswordSalt",
			"tw_accounts", "WHERE Username = '{}' AND ID = '{}'", Data.m_Login, Data.m_AccountID);
		pCheck->AtExecute([pContext](ResultPtr pCheckRes) { OnCheckCredentials(pContext, pCheckRes); });
	}

	static void OnCheckCredentials(const CAuthorizationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer)
			return;

		if(!pRes->next() || (pRes->getString("Password") != CAccountManager::HashPassword(pContext->Data().m_Password, pRes->getString("PasswordSalt"))))
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Oops, that doesn't seem to be the right login or password");
			return;
		}

		auto& Data = pContext->Data();
		Data.m_Language = pRes->getString("Language");
		Data.m_LoginDate = pRes->getString("LoginDate");
		Data.m_PinCode = pRes->getString("PinCode");
		auto pBan = Database->Prepare<DB::SELECT>("BannedUntil, Reason", "tw_accounts_bans", "WHERE AccountId = '{}' AND current_timestamp() < `BannedUntil`", Data.m_AccountID);
		pBan->AtExecute([pContext](ResultPtr pBanRes) { OnCheckBans(pContext, pBanRes); });
	}

	static void OnCheckBans(const CAuthorizationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer)
			return;

		if(pRes->next())
		{
			const auto BannedUntil = pRes->getString("BannedUntil");
			const auto Reason = pRes->getString("Reason");
			pContext->GS()->Chat(pContext->GetClientID(), "You account was suspended until '{}' with the reason of '{}'.", BannedUntil, Reason);
			return;
		}

		if(pContext->GS()->GetPlayerByUserID(pContext->Data().m_AccountID) != nullptr)
		{
			pContext->GS()->Chat(pContext->GetClientID(), "The account is already in the game.");
			return;
		}

		auto pData = Database->Prepare<DB::SELECT>("*", "tw_accounts_data", "WHERE ID = '{}'", pContext->Data().m_AccountID);
		pData->AtExecute([pContext](ResultPtr pDataRes) { OnLoadAccountData(pContext, pDataRes); });
	}


	static void OnLoadAccountData(const CAuthorizationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer)
			return;

		if(!pRes->next())
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Authorization failed due to account data loading issue.");
			return;
		}

		const auto& Data = pContext->Data();
		pPlayer->Account()->Init(Data.m_AccountID, pContext->GetClientID(), Data.m_Login.c_str(), Data.m_Language, Data.m_LoginDate, std::move(pRes));
		pContext->GS()->Chat(pContext->GetClientID(), "- Welcome! You've successfully logged in!");
		if(Data.m_PinCode.empty())
			pContext->GS()->Chat(pContext->GetClientID(), "PIN is not set, set it using '/set_pin'.");
		if(pPlayer->m_pMotdMenu)
			pPlayer->CloseMotdMenu();
		pContext->GS()->m_pController->DoTeamChange(pPlayer);
		Data.m_pAccountManager->LoadAccount(pPlayer, true);
		g_EventListenerManager.Notify<IEventListener::PlayerLogin>(pPlayer, pPlayer->Account());
		pPlayer->KillCharacter();
	}

public:
	static void StartAuthorization(CAccountManager* pMgr, int ClientID, std::string Login, std::string Pass, std::string Nick)
	{
		auto pContext = DbAsync::MakeContext<DbAuthorization::CAuthorizationPayload>(ClientID,
			DbAuthorization::CAuthorizationPayload { pMgr, Login, Pass, 0, {}, {}, {} });
		auto pAuth = Database->Prepare<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '{}'", Nick);
		pAuth->AtExecute([pContext](ResultPtr pRes) { DbAuthorization::OnCheckNickname(pContext, pRes); });
	}
};

class DbRegistration
{
	struct CRegistrationPayload
	{
		CAccountManager* m_pAccountManager{};
		int m_InitID{};
		std::string m_Login{};
		std::string m_Password{};
		std::string m_PasswordHash{};
		std::string m_PasswordSalt{};
		std::string m_Nickname{};
		std::string m_RegisteredIP{};
	};
	using CRegistrationContextPtr = std::shared_ptr<DbAsync::CContext<CRegistrationPayload>>;

	static void CleanupAccountOnError(const CRegistrationContextPtr& pContext)
	{
		const auto& Data = pContext->Data();
		if(Data.m_InitID > 0)
			Database->Execute<DB::REMOVE>("tw_accounts", "WHERE ID = '{}'", Data.m_InitID);
		else
			Database->Execute<DB::REMOVE>("tw_accounts", "WHERE Username = '{}' AND Password = '{}' AND PasswordSalt = '{}' ORDER BY ID DESC LIMIT 1",
				Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt);

		pContext->GS()->Chat(pContext->GetClientID(), "Registration failed. Please try again later.");
	}

	static void OnLookup(const CRegistrationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pGS = pContext->GS();
		const auto& Data = pContext->Data();
		const int ClientID = pContext->GetClientID();

		if(pRes->next())
		{
			const auto AccountID = pRes->getInt("ID");
			const auto CurrentLogin = pRes->getString("Username");
			if(!IsGuestCredentialForNickname(CurrentLogin, Data.m_Nickname.c_str()))
			{
				pGS->Chat(ClientID, "Sorry, but that game nickname is already taken by another player. To regain access, reach out to the support team or alter your nickname.");
				pGS->Chat(ClientID, "Discord: \"{}\".", g_Config.m_SvDiscordInviteLink);
				return;
			}

			Database->Execute<DB::UPDATE>([pContext](bool Success)
			{
				if(!Success)
				{
					pContext->GS()->Chat(pContext->GetClientID(), "Registration failed. Please try again later.");
					return;
				}

				const auto& Data = pContext->Data();
				pContext->GS()->Chat(pContext->GetClientID(), "- Registration complete! Don't forget to save your data.");
				pContext->GS()->Chat(pContext->GetClientID(), "# Your nickname is a unique identifier.");
				Data.m_pAccountManager->LoginAccountRaw(pContext->GetClientID(), Data.m_Login.c_str(), Data.m_Password.c_str());
			}, "tw_accounts", "Username = '{}', Password = '{}', PasswordSalt = '{}' WHERE ID = '{}'", Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt, AccountID);
			return;
		}

		auto pLookup = Database->Prepare<DB::SELECT>("a.ID, a.Username", "tw_accounts_data d JOIN tw_accounts a ON a.ID = d.ID",
			"WHERE d.Nick = '{}' LIMIT 1", Data.m_Nickname);
		pLookup->AtExecute([pContext](ResultPtr pRes) { OnLookup(pContext, pRes); });
	}

	static void OnCheckNickname(const CRegistrationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pGS = pContext->GS();
		if(pRes->next())
		{
			pGS->Chat(pContext->GetClientID(), "Sorry, but that game nickname is already taken by another player. To regain access, reach out to the support team or alter your nickname.");
			pGS->Chat(pContext->GetClientID(), "Discord: \"{}\".", g_Config.m_SvDiscordInviteLink);
			return;
		}

		const auto& Data = pContext->Data();
		Database->Execute<DB::INSERT>([pContext](bool Success) { OnInsertAccount(pContext, Success); },
			"tw_accounts", "(Username, Password, PasswordSalt, RegisterDate, RegisteredIP) VALUES ('{}', '{}', '{}', UTC_TIMESTAMP(), '{}')",
			Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt, Data.m_RegisteredIP);
	}

	static void OnInsertAccount(const CRegistrationContextPtr& pContext, bool Success)
	{
		if(!Success)
		{
			CleanupAccountOnError(pContext);
			return;
		}

		const auto& Data = pContext->Data();
		auto pInitID = Database->Prepare<DB::SELECT>("ID", "tw_accounts", "ORDER BY ID DESC LIMIT 1", Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt);
		pInitID->AtExecute([pContext](ResultPtr pRes) { OnResolveAccountID(pContext, pRes); });
	}

	static void OnResolveAccountID(const CRegistrationContextPtr& pContext, ResultPtr pRes)
	{
		if(!pRes->next())
		{
			CleanupAccountOnError(pContext);
			return;
		}

		pContext->Data().m_InitID = pRes->getInt("ID");
		Database->Execute<DB::INSERT>([pContext](bool Success) { OnInsertAccountData(pContext, Success); },
			"tw_accounts_data", "(ID, Nick) VALUES ('{}', '{}')", pContext->Data().m_InitID, pContext->Data().m_Nickname);
	}

	static void OnInsertAccountData(const CRegistrationContextPtr& pContext, bool Success)
	{
		if(!Success)
		{
			CleanupAccountOnError(pContext);
			return;
		}

		AuthorizeAfterSuccess(pContext);
	}

	static void AuthorizeAfterSuccess(const CRegistrationContextPtr& pContext)
	{
		auto* pGS = pContext->GS();
		const auto& Data = pContext->Data();
		pContext->Server()->UpdateAccountBase(Data.m_InitID, Data.m_Nickname, g_Config.m_SvMinRating);
		pGS->Chat(pContext->GetClientID(), "- Registration complete! Don't forget to save your data.");
		pGS->Chat(pContext->GetClientID(), "# Your nickname is a unique identifier.");

		if(auto* pPlayer = pContext->GetPlayer(false))
		{
			pPlayer->m_AuthMenuAllowRegister = false;
			if(pPlayer->IsSameMotdMenu(MOTD_MENU_AUTH))
				pGS->SendMenuMotd(pPlayer, MOTD_MENU_AUTH);
		}

		DbAuthorization::StartAuthorization(Data.m_pAccountManager, pContext->GetClientID(), Data.m_Login, Data.m_Password, Data.m_Nickname);
	}

public:
	static void StartRegistration(CAccountManager* pMgr, int ClientID, std::string Login, std::string Pass, std::string Nick, std::string RegisterIP)
	{
		char aSalt[32] = { 0 };
		secure_random_password(aSalt, sizeof(aSalt), 24);
		const auto AccountPasswordHash = CAccountManager::HashPassword(Pass, aSalt);
		auto pContext = DbAsync::MakeContext<DbRegistration::CRegistrationPayload>(ClientID,
			DbRegistration::CRegistrationPayload { pMgr, 0, Login, Pass, AccountPasswordHash, aSalt, Nick, RegisterIP });
		auto pReg = Database->Prepare<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '{}'", Nick);
		pReg->AtExecute([pContext](ResultPtr pRes) { DbRegistration::OnCheckNickname(pContext, pRes); });
	}
};


void CAccountManager::OnPlayerLogin(CPlayer* pPlayer)
{
	if(!pPlayer || !pPlayer->Account())
		return;

	// select first random profession by system
	auto* pAccount = pPlayer->Account();
	if(pAccount->GetActiveProfessionID() == ProfessionIdentifier::None)
	{
		ChanceProcessor<ProfessionIdentifier> process;
		process.addElement(ProfessionIdentifier::Tank, 100.f);
		process.addElement(ProfessionIdentifier::Healer, 100.f);
		process.addElement(ProfessionIdentifier::Dps, 100.f);
		process.setEqualChance(100.f);
		process.normalizeChances();
		pAccount->ChangeProfession(process.getRandomElement());
		GS()->Chat(pPlayer->GetCID(), "Your profession has been chosen by the system. You can change it at any time in the voting menu.");
	}
}

void CAccountManager::OnClientReset(int ClientID)
{
	CAccountSharedData::ms_aPlayerSharedData.erase(ClientID);
	CAccountData::ms_aData.erase(ClientID);
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
			VUpgrades.Add("Total {} - {}{}", pAttribute->GetName(), AttributeSize, aBuf);
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
		VRating.Add("Rating: {}({})", pAccount->GetRatingSystem().GetRating(), pAccount->GetRatingSystem().GetRankName());
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
		const auto currencyItemIDs = CInventoryManager::GetItemsCollection(ItemGroup::Currency, std::nullopt);
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

		// skill upgrades
		VoteWrapper VActiveSkills(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "\u2694 Improving class skills");
		for(const auto& [ID, pSkillDesc] : CSkillDescription::Data())
		{
			if(pSkillDesc->GetProfessionID() == ActiveProfID)
			{
				const auto* pSkill = pPlayer->GetSkill(ID);
				VActiveSkills.AddMenu(MENU_SKILL_SELECT, ID, "{} - {}SP {}", pSkillDesc->GetName(), pSkillDesc->GetPriceSP(), pSkill->GetStringLevelStatus());
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

		// skill univirsal
		VoteWrapper VUniversalSkills(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "\u2694 Improving universal skills");
		for(const auto& [ID, pSkillDesc] : CSkillDescription::Data())
		{
			if(pSkillDesc->GetProfessionID() == ProfessionIdentifier::None)
			{
				const auto* pSkill = pPlayer->GetSkill(ID);
				VUniversalSkills.AddMenu(MENU_SKILL_SELECT, ID, "{} - {}SP {}", pSkillDesc->GetName(), pSkillDesc->GetPriceSP(), pSkill->GetStringLevelStatus());
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
		VLanguageInfo.Add("Note: translation is not complete.");
		VLanguageInfo.Add("Active language: [{}]", pPlayerLanguage);
		VoteWrapper::AddEmptyline(ClientID);

		// languages
		VoteWrapper VLanguages(ClientID, VWF_OPEN, "Available languages");
		for(int i = 0; i < Server()->Localization()->m_pLanguages.size(); i++)
		{
			// do not show the language that is already selected by the player in the selection lists
			if(str_comp(pPlayerLanguage, Server()->Localization()->m_pLanguages[i]->GetFilename()) == 0)
				continue;

			// add language selection
			const char* pLanguageName = Server()->Localization()->m_pLanguages[i]->GetName();
			VLanguages.AddOption("SELECT_LANGUAGE", i, "Select language \"{}\"", pLanguageName);
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

	//HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_INFO_BONUSES, MOTD_MENU_BONUSES_INFO)
	//HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_INFO_WANTED, MOTD_MENU_WANTED_INFO)
	HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_BANK_MANAGER, MOTD_MENU_BANK_MANAGER)
}

bool CAccountManager::OnSendMenuMotd(CPlayer* pPlayer, int Menulist)
{
	int ClientID = pPlayer->GetCID();

	// motd menu auth
	if(Menulist == MOTD_MENU_AUTH)
	{
		if(pPlayer->IsAuthed())
			return false;

		MotdMenu MAuth(ClientID, "Use chat message like '/<text>' to edit the fields.");
		MAuth.AddText("Account authorization");
		MAuth.AddSeparateLine();
		MAuth.AddLine();
		MAuth.AddText("Login:");
		MAuth.AddField(AUTH_FIELD_LOGIN);
		MAuth.AddLine();
		MAuth.AddText("Password:");
		MAuth.AddField(AUTH_FIELD_PASSWORD, MTTEXTINPUTFLAG_PASSWORD);
		MAuth.AddLine();
		MAuth.AddSeparateLine();

		if(pPlayer->m_AuthMenuAllowRegister)
			MAuth.AddOption("AUTH_REGISTER", "Register");
		else
			MAuth.AddOption("AUTH_LOGIN", "Log in");

		MAuth.Send(MOTD_MENU_AUTH);
		return true;
	}

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
		int position = 1;
		MotdMenu MBonuses(ClientID, "All bonuses overlap, the minimum increase cannot be lower than 1 point.");
		MBonuses.AddText("Active bonuses \u2696");
		MBonuses.AddSeparateLine();
		for(auto& bonus : pPlayer->Account()->GetBonusManager().GetTemporaryBonuses())
		{
			int days, hours, minutes, seconds;
			const char* pBonusType = pPlayer->Account()->GetBonusManager().GetStringBonusType(bonus.Type);
			bonus.GetRemainingTimeFormatted(&days, &hours, &minutes, &seconds);

			MBonuses.AddText("{}. {} - {~.2}%", position, pBonusType, bonus.Amount);
			MBonuses.AddText("Time left: {}d {}h {}m {}s.", days, hours, minutes, seconds);
			MBonuses.AddSeparateLine();
			position++;
		}

		if(position == 1)
		{
			MBonuses.AddText("No active bonuses!");
			MBonuses.AddSeparateLine();
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
			MWanted.AddText(Server()->ClientName(i));
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
		auto* pAccount = pPlayer->Account();
		const auto gold = pAccount->GetGold();
		const auto goldCapacity = pAccount->GetGoldCapacity();
		const auto bankGold = pAccount->GetBankManager();

		// personal assistant
		MotdMenu MAssistant(ClientID, MTFLAG_CLOSE_BUTTON, "Quick tips update as you progress.");
		MAssistant.AddText("Profession: {}", GetProfessionName(pAccount->GetActiveProfessionID()));
		MAssistant.AddText("Level {}.", pAccount->GetLevel());
		MAssistant.AddText("Gold: {$}/{$}", gold, goldCapacity);
		MAssistant.AddText("Bank: {$}", bankGold);
		MAssistant.AddText("World: {}", Server()->GetWorldName(pPlayer->GetCurrentWorldID()));
		MAssistant.AddSeparateLine();

		// recommendations
		MAssistant.AddText("Recommendations:");
		bool hasRecommendation = false;
		const int mailCount = Core()->MailboxManager()->GetMailCount(pAccount->GetID());
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
		return true;
	}

	return false;
}

bool CAccountManager::OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd)
{
	// register by motd menu ui
	if(strcmp(pCmd, "AUTH_LOGIN") == 0 || strcmp(pCmd, "AUTH_REGISTER") == 0)
	{
		const auto LoginOpt = pMotdData->GetFieldStr(AUTH_FIELD_LOGIN);
		const auto PassOpt = pMotdData->GetFieldStr(AUTH_FIELD_PASSWORD);
		if(!LoginOpt.has_value() || !PassOpt.has_value())
		{
			GS()->Chat(pPlayer->GetCID(), "Please fill in login and password.");
			return true;
		}

		if(strcmp(pCmd, "AUTH_LOGIN") == 0)
		{
			LoginAccount(pPlayer->GetCID(), LoginOpt->c_str(), PassOpt->c_str());
			return true;
		}

		const auto Result = RegisterAccount(pPlayer->GetCID(), LoginOpt->c_str(), PassOpt->c_str());
		if(Result == AccountCodeResult::AOP_REGISTER_OK)
		{
			pPlayer->m_AuthMenuAllowRegister = false;
			GS()->SendMenuMotd(pPlayer, MOTD_MENU_AUTH);
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

std::string CAccountManager::HashPassword(const std::string& Password, const std::string& Salt)
{
	std::string plaintext = Salt + Password + Salt;
	SHA256_CTX sha256Ctx;
	sha256_init(&sha256Ctx);
	sha256_update(&sha256Ctx, plaintext.c_str(), plaintext.length());
	const SHA256_DIGEST digest = sha256_finish(&sha256Ctx);

	char hash[SHA256_MAXSTRSIZE];
	sha256_str(digest, hash, sizeof(hash));
	return std::string(hash);
}

AccountCodeResult CAccountManager::RegisterAccount(int ClientID, const char* pLogin, const char* pPassword)
{
	if(!CheckLoginPasswordLength(GS(), ClientID, pLogin, pPassword))
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS;

	return RegisterAccountRaw(ClientID, pLogin, pPassword);
}

AccountCodeResult CAccountManager::RegisterAccountRaw(int ClientID, const char* pLogin, const char* pPassword)
{
	const auto cClearLogin = CSqlString<32>(pLogin);
	const auto cClearPass = CSqlString<32>(pPassword);
	const auto cClearNick = CSqlString<32>(Server()->ClientName(ClientID));

	char aAddrStr[64] {};
	Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
	DbRegistration::StartRegistration(this, ClientID, cClearLogin.cstr(), cClearPass.cstr(), cClearNick.cstr(), aAddrStr);
	return AccountCodeResult::AOP_REGISTER_OK;
}

AccountCodeResult CAccountManager::RegisterGuestAccount(int ClientID, const char* pNickname)
{
	const auto GuestCredential = BuildGuestCredential(pNickname);
	RegisterAccountRaw(ClientID, GuestCredential.c_str(), GuestCredential.c_str());
	return AccountCodeResult::AOP_REGISTER_OK;
}

AccountCodeResult CAccountManager::LoginAccount(int ClientID, const char* pLogin, const char* pPassword)
{
	if(!CheckLoginPasswordLength(GS(), ClientID, pLogin, pPassword))
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS;

	return LoginAccountRaw(ClientID, pLogin, pPassword);
}

AccountCodeResult CAccountManager::LoginAccountRaw(int ClientID, const char* pLogin, const char* pPassword)
{
	const auto sqlStrLogin = CSqlString<64>(pLogin);
	const auto sqlStrPass = CSqlString<64>(pPassword);
	const auto sqlStrNick = CSqlString<32>(Server()->ClientName(ClientID));
	DbAuthorization::StartAuthorization(this, ClientID, sqlStrLogin.cstr(), sqlStrPass.cstr(), sqlStrNick.cstr());
	return AccountCodeResult::AOP_LOGIN_OK;
}

void CAccountManager::SaveTimeoutCodeByNickname(const char* pNickname, const char* pCode) const
{
	const auto cNick = CSqlString<32>(pNickname);
	const auto cCode = CSqlString<64>(pCode);
	Database->Execute<DB::UPDATE>("tw_accounts a JOIN tw_accounts_data d ON d.ID = a.ID", "a.TimeoutCode = '{}' WHERE d.Nick = '{}'", cCode.cstr(), cNick.cstr());
}

void CAccountManager::TryLoginGuestByTimeoutCode(int ClientID, const char* pNickname, const char* pCode, const char* pGuestLogin)
{
	const auto cNick = CSqlString<32>(pNickname);
	const auto cCode = CSqlString<64>(pCode);
	const auto cGuest = CSqlString<64>(pGuestLogin);
	auto pCheck = Database->Prepare<DB::SELECT>("a.ID", "tw_accounts a JOIN tw_accounts_data d ON d.ID = a.ID",
		"WHERE d.Nick = '{}' AND a.Username = '{}' AND a.TimeoutCode = '{}' LIMIT 1", cNick.cstr(), cGuest.cstr(), cCode.cstr());
	pCheck->AtExecute([this, ClientID, GuestLogin = std::string(pGuestLogin)](ResultPtr pRes)
	{
		auto* pPlayer = GS()->GetPlayer(ClientID, false);
		if(!pRes->next() || !pPlayer || pPlayer->IsAuthed() || !pPlayer->m_WaitingGuestTimeoutAuth)
			return;

		pPlayer->m_WaitingGuestTimeoutAuth = false;
		LoginAccountRaw(ClientID, GuestLogin.c_str(), GuestLogin.c_str());
	});
}

void CAccountManager::LoadAccount(CPlayer* pPlayer, bool FirstInitilize)
{
	if(!pPlayer || !pPlayer->IsAuthed() || !GS()->IsPlayerInWorld(pPlayer->GetCID()))
		return;

	auto* pAccount = pPlayer->Account();

	// Broadcast a message to the player with their current location
	const int ClientID = pPlayer->GetCID();
	GS()->Broadcast(ClientID, BroadcastPriority::VeryImportant, 300, "You are currently positioned at {}({})!\n- Rates: Exp {}% | Gold {}%",
		Server()->GetWorldName(GS()->GetWorldID()), (GS()->IsAllowedPVP() ? "PVE/PVP" : "PVE"), GS()->m_Multipliers.Experience, GS()->m_Multipliers.Gold);

	// Check if it is not the first initialization
	if(!FirstInitilize)
	{
		const int Letters = Core()->MailboxManager()->GetMailCount(pAccount->GetID());
		if(Letters > 0)
		{
			GS()->Chat(ClientID, "You have '{} unread letters'.", Letters);
		}

		pAccount->GetBonusManager().SendInfoAboutActiveBonuses();
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
		return;
	}

	// on player login
	Core()->OnPlayerLogin(pPlayer);

	// initialize default item's & settings
	auto InitSettingsItem = [pPlayer](const std::unordered_map<int, int>& pItems)
	{
		for(auto& [id, defaultValue] : pItems)
		{
			if(auto pItem = pPlayer->GetItem(id))
			{
				if(!pItem->HasItem())
					pItem->Add(1, defaultValue);
			}
		}
	};

	InitSettingsItem(
		{
			{ itHammer, 1 },
			{ itShowEquipmentDescription, 0 },
			{ itShowCriticalDamage, 1 },
			{ itShowQuestStarNavigator, 1 },
			{ itShowDetailGainMessages, 0 },
			{ itShowOnlyFunctionModules, 1 },
		});

	// update player time periods
	Core()->OnHandlePlayerTimePeriod(pPlayer);

	// notify about rank
	const int Rank = Server()->GetAccountRank(pAccount->GetID());
	GS()->Chat(-1, "'{}' logged to account. Rank '#{}[{}]' ({})", Server()->ClientName(ClientID), Rank,
		Server()->ClientCountryIsoCode(ClientID), pPlayer->Account()->GetRatingSystem().GetRankName());

	// Change player's world ID to the latest correct world ID
	const int LatestCorrectWorldID = GetLastVisitedWorldID(pPlayer);
	if(LatestCorrectWorldID != GS()->GetWorldID())
	{
		pPlayer->ChangeWorld(LatestCorrectWorldID);
		return;
	}
}

void CAccountManager::SetPinCode(int ClientID, const char* pCurrentPasswordOrPin, const char* pNewPin, bool IsChangingPin)
{
	// check valid player
	auto* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer || !pPlayer->IsAuthed())
	{
		GS()->Chat(ClientID, "You must be logged in to set or change a PIN code.");
		return;
	}

	// check valid pin length
	const int LengthNewPin = str_length(pNewPin);
	if(LengthNewPin < LENGTH_PIN_MIN || LengthNewPin > LENGTH_PIN_MAX)
	{
		GS()->Chat(ClientID, "PIN code must be {} to {} digits long.", LENGTH_PIN_MIN, LENGTH_PIN_MAX);
		return;
	}
	for(int i = 0; i < LengthNewPin; ++i)
	{
		if(!isdigit(pNewPin[i]))
		{
			GS()->Chat(ClientID, "PIN code must consist of digits only.");
			return;
		}
	}

	// fetch current pin code
	const auto AccountID = pPlayer->Account()->GetID();
	std::string CurrentPinInDb;
	bool bPinCurrentlySet = GetAccountPin(AccountID, CurrentPinInDb);

	if(IsChangingPin)
	{
		// is not set need set pin
		if(!bPinCurrentlySet)
		{
			GS()->Chat(ClientID, "You don't have a PIN code set.");
			GS()->Chat(ClientID, "Use '/set_pin' to set your PIN.");
			return;
		}

		// check valid pin
		if(CurrentPinInDb != pCurrentPasswordOrPin)
		{
			GS()->Chat(ClientID, "The current PIN code you entered is incorrect.");
			return;
		}
		if(str_comp(pCurrentPasswordOrPin, pNewPin) == 0)
		{
			GS()->Chat(ClientID, "The new PIN cannot be the same as the old PIN.");
			return;
		}
	}
	else
	{
		// is set need change pin
		if(bPinCurrentlySet)
		{
			GS()->Chat(ClientID, "You already have a PIN code set.");
			GS()->Chat(ClientID, "Use '/change_pin' to update your PIN.");
			return;
		}

		// check valid password
		ResultPtr pResAccount = Database->Execute<DB::SELECT>("Password, PasswordSalt", "tw_accounts", "WHERE ID = '{}'", AccountID);
		if(!pResAccount->next())
		{
			GS()->Chat(ClientID, "Error retrieving account data.");
			return;
		}

		const auto DbPassword = pResAccount->getString("Password");
		const auto DbSalt = pResAccount->getString("PasswordSalt");
		const CSqlString<32> sqlCurrentPassword(pCurrentPasswordOrPin);
		if(DbPassword != HashPassword(sqlCurrentPassword.cstr(), DbSalt))
		{
			GS()->Chat(ClientID, "The account password you entered is incorrect.");
			return;
		}
	}

	// save new pin-code
	const CSqlString<16> sqlNewPin(pNewPin);
	Database->Execute<DB::UPDATE>("tw_accounts", "PinCode = '{}' WHERE ID = '{}'", sqlNewPin.cstr(), AccountID);
	GS()->Chat(ClientID, "Your PIN code has been successfully {}!", Instance::Localize(ClientID, IsChangingPin ? "changed" : "set"));
	return;
}

void CAccountManager::ChangePassword(int ClientID, const char* pOldPassword, const char* pNewPassword, const char* pPinCode)
{
	// check valid account
	auto* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer || !pPlayer->IsAuthed())
	{
		GS()->Chat(ClientID, "You must be logged in to change your password.");
		return;
	}

	// check valid length and password same
	const int LengthOldPass = str_length(pOldPassword);
	const int LengthNewPass = str_length(pNewPassword);
	if(LengthOldPass < LENGTH_LOGPASS_MIN || LengthOldPass > LENGTH_LOGPASS_MAX || LengthNewPass < LENGTH_LOGPASS_MIN || LengthNewPass > LENGTH_LOGPASS_MAX)
	{
		GS()->Chat(ClientID, "The old and new passwords must each contain '{} - {} characters'.", LENGTH_LOGPASS_MIN, LENGTH_LOGPASS_MAX);
		return;
	}
	if(str_comp(pOldPassword, pNewPassword) == 0)
	{
		GS()->Chat(ClientID, "The new password cannot be the same as the old password.");
		return;
	}

	// get pincode
	std::string PinFromDb;
	const int AccountID = pPlayer->Account()->GetID();
	bool bPinIsSet = GetAccountPin(AccountID, PinFromDb);

	// check is pincode set
	if(!bPinIsSet)
	{
		GS()->Chat(ClientID, "For security, you must first set up a PIN code.");
		GS()->Chat(ClientID, "Use '/set_pin' to set your PIN.");
		return;
	}

	// require pin code
	if(pPinCode == nullptr || *pPinCode == '\0')
	{
		GS()->Chat(ClientID, "PIN code is required to change your password.");
		return;
	}

	// check valid and length pin code
	const int LengthPin = str_length(pPinCode);
	if(LengthPin < LENGTH_PIN_MIN || LengthPin > LENGTH_PIN_MAX)
	{
		GS()->Chat(ClientID, "Invalid PIN code format.");
		return;
	}
	for(int i = 0; i < LengthPin; ++i)
	{
		if(!isdigit(pPinCode[i]))
		{
			GS()->Chat(ClientID, "PIN code must consist of digits only.");
			return;
		}
	}

	// check valid pincode
	if(PinFromDb != pPinCode)
	{
		GS()->Chat(ClientID, "The PIN code you entered is incorrect.");
		return;
	}

	// get current password
	ResultPtr pResAccount = Database->Execute<DB::SELECT>("Password, PasswordSalt", "tw_accounts", "WHERE ID = '{}'", AccountID);
	if(!pResAccount->next())
	{
		GS()->Chat(ClientID, "Error retrieving account data.");
		return;
	}

	// check valid password
	const auto CurrentDbPassword = pResAccount->getString("Password");
	const auto CurrentDbSalt = pResAccount->getString("PasswordSalt");
	const CSqlString<32> sqlOldPassword(pOldPassword);
	if(CurrentDbPassword != HashPassword(sqlOldPassword.cstr(), CurrentDbSalt))
	{
		GS()->Chat(ClientID, "The old password you entered is incorrect.");
		return;
	}

	// update new password
	char aNewSalt[32] = { 0 };
	secure_random_password(aNewSalt, sizeof(aNewSalt), 24);
	const CSqlString<32> sqlNewPassword(pNewPassword);
	const std::string HashedNewPassword = HashPassword(sqlNewPassword.cstr(), aNewSalt);
	Database->Execute<DB::UPDATE>("tw_accounts", "Password = '{}', PasswordSalt = '{}' WHERE ID = '{}'", HashedNewPassword, aNewSalt, AccountID);

	// information
	GS()->Chat(ClientID, "Your password has been successfully changed!");
	GS()->Chat(ClientID, "Please remember your new login details.");
	GS()->Chat(ClientID, "New password: '{}'.", sqlNewPassword.cstr());
	return;
}

bool CAccountManager::ChangeNickname(const std::string& newNickname, int ClientID) const
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return false;

	// check newnickname
	const auto cClearNick = CSqlString<32>(newNickname.c_str());
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '{}'", cClearNick.cstr());
	if(pRes->next())
		return false;

	Database->Execute<DB::UPDATE>("tw_accounts_data", "Nick = '{}' WHERE ID = '{}'", cClearNick.cstr(), pPlayer->Account()->GetID());
	Server()->SetClientName(ClientID, newNickname.c_str());
	return true;
}

void CAccountManager::UseVoucher(int ClientID, const char* pVoucher) const
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	char aSelect[256];
	const auto cVoucherCode = CSqlString<32>(pVoucher);
	str_format(aSelect, sizeof(aSelect), "v.*, IF((SELECT r.ID FROM tw_voucher_redeemed r WHERE CASE v.Multiple WHEN 1 THEN r.VoucherID = v.ID AND r.UserID = %d ELSE r.VoucherID = v.ID END) IS NULL, FALSE, TRUE) AS used", pPlayer->Account()->GetID());

	ResultPtr pResVoucher = Database->Execute<DB::SELECT>(aSelect, "tw_voucher v", "WHERE v.Code = '{}'", cVoucherCode.cstr());
	if(pResVoucher->next())
	{
		const int VoucherID = pResVoucher->getInt("ID");
		const int ValidUntil = pResVoucher->getInt("ValidUntil");
		nlohmann::json JsonData = nlohmann::json::parse(pResVoucher->getString("Data").c_str());

		if(ValidUntil > 0 && ValidUntil < time(0))
		{
			GS()->Chat(ClientID, "The voucher code '{}' has expired.", pVoucher);
			return;
		}

		if(pResVoucher->getBoolean("used"))
		{
			GS()->Chat(ClientID, "This voucher has already been redeemed.");
			return;
		}

		const int Exp = JsonData.value("exp", 0);
		const int Money = JsonData.value("money", 0);

		if(Exp > 0)
			pPlayer->Account()->AddExperience(Exp);
		if(Money > 0)
			pPlayer->Account()->AddGold(Money);

		if(JsonData.find("items") != JsonData.end() && JsonData["items"].is_array())
		{
			for(const nlohmann::json& Item : JsonData["items"])
			{
				const int ItemID = Item.value("id", -1);
				const int Value = Item.value("value", 0);
				if(Value > 0 && CItemDescription::Data().contains(ItemID))
					pPlayer->GetItem(ItemID)->Add(Value);
			}
		}

		GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
		GS()->Core()->SaveAccount(pPlayer, SAVE_UPGRADES);

		Database->Execute<DB::INSERT>("tw_voucher_redeemed", "(VoucherID, UserID, TimeCreated) VALUES ({}, {}, {})", VoucherID, pPlayer->Account()->GetID(), (int)time(0));
		GS()->Chat(ClientID, "You have successfully redeemed the voucher '{}'.", pVoucher);
		return;
	}

	GS()->Chat(ClientID, "The voucher code '{}' does not exist.", pVoucher);
}

bool CAccountManager::BanAccount(CPlayer* pPlayer, CTimePeriod Time, const std::string& Reason)
{
	// Check if the account is already banned
	ResultPtr pResBan = Database->Execute<DB::SELECT>("BannedUntil", "tw_accounts_bans", "WHERE AccountId = '{}' AND current_timestamp() < `BannedUntil`", pPlayer->Account()->GetID());
	if(pResBan->next())
	{
		// Print message and return false if the account is already banned
		m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "This account is already banned");
		return false;
	}

	// Ban the account
	Database->Execute<DB::INSERT>("tw_accounts_bans", "(AccountId, BannedUntil, Reason) VALUES ('{}', '{}', '{}')",
		pPlayer->Account()->GetID(), std::string("current_timestamp + " + Time.asSqlInterval()), Reason);
	GS()->Server()->Kick(pPlayer->GetCID(), "Your account was banned");
	m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Successfully banned!");
	return true;
}

bool CAccountManager::UnBanAccount(int BanId) const
{
	// Search for ban using the specified BanId
	ResultPtr pResBan = Database->Execute<DB::SELECT>("AccountId", "tw_accounts_bans", "WHERE Id = '{}' AND current_timestamp() < `BannedUntil`", BanId);
	if(pResBan->next())
	{
		// If the ban exists and the current timestamp is still less than the BannedUntil timestamp, unban the account
		Database->Execute<DB::UPDATE>("tw_accounts_bans", "BannedUntil = current_timestamp WHERE Id = '{}'", BanId);
		m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Successfully unbanned!");
		return true;
	}

	// If the ban does not exist or the current timestamp is already greater than the BannedUntil timestamp, print an error message
	m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Ban is not valid anymore or does not exist!");
	return false;
}

// Function: BansAccount
std::vector<CAccountManager::AccBan> CAccountManager::BansAccount() const
{
	constexpr std::size_t capacity = 100;
	std::vector<AccBan> out;
	out.reserve(capacity);

	/*
		Execute a SELECT query on the "tw_accounts_bans" table in the database,
		retrieving the columns "Id", "BannedUntil", "Reason", and "AccountId"
		where the current timestamp is less than the "BannedUntil" value
	*/
	ResultPtr pResBan = Database->Execute<DB::SELECT>("Id, BannedUntil, Reason, AccountId", "tw_accounts_bans", "WHERE current_timestamp() < `BannedUntil`");
	while(pResBan->next())
	{
		int ID = pResBan->getInt("Id");
		int AccountID = pResBan->getInt("AccountId");
		std::string BannedUntil = pResBan->getString("BannedUntil");
		std::string PlayerNickname = Server()->GetAccountNickname(AccountID);
		std::string Reason = pResBan->getString("Reason");
		out.emplace_back(ID, BannedUntil, std::move(PlayerNickname), std::move(Reason));
	}

	return out;
}

bool CAccountManager::GetAccountPin(int AccountID, std::string& OutPinCode) const
{
	ResultPtr pResPin = Database->Execute<DB::SELECT>("PinCode", "tw_accounts", "WHERE ID = '{}'", AccountID);
	if(pResPin->next())
	{
		OutPinCode = pResPin->getString("PinCode");
		return !OutPinCode.empty();
	}

	OutPinCode.clear();
	return false;
}

int CAccountManager::GetLastVisitedWorldID(CPlayer* pPlayer) const
{
	const auto pWorldIterator = std::ranges::find_if(pPlayer->Account()->m_aHistoryWorld, [&](int WorldID)
	{
		if(GS()->GetWorldData(WorldID))
		{
			int RequiredLevel = GS()->GetWorldData(WorldID)->GetRequiredLevel();
			return Server()->IsWorldType(WorldID, WorldType::Default) && pPlayer->Account()->GetLevel() >= RequiredLevel;
		}
		return false;
	});
	return pWorldIterator != pPlayer->Account()->m_aHistoryWorld.end() ? *pWorldIterator : BASE_GAME_WORLD_ID;
}
