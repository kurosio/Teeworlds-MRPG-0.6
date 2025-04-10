/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_manager.h"

#include <base/hash_ctxt.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>
#include <game/server/core/components/mails/mailbox_manager.h>
#include <game/server/core/components/worlds/world_data.h>

#include <teeother/components/localization.h>

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
	return pWorldIterator != pPlayer->Account()->m_aHistoryWorld.end() ? *pWorldIterator : MAIN_WORLD_ID;
}

// Register an account for a client with the given parameters
AccountCodeResult CAccountManager::RegisterAccount(int ClientID, const char* Login, const char* Password)
{
	// Check if the length of the login and password is between 4 and 12 characters
	if(str_length(Login) > 12 || str_length(Login) < 4 || str_length(Password) > 12 || str_length(Password) < 4)
	{
		GS()->Chat(ClientID, "The username and password must each contain '4 - 12 characters'.");
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS; // Return mismatch length symbols error
	}

	// Get the client's clear nickname
	const CSqlString<32> cClearNick = CSqlString<32>(Server()->ClientName(ClientID));

	// Check if the client's nickname is already registered
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '{}'", cClearNick.cstr());
	if(pRes->next())
	{
		GS()->Chat(ClientID, "Sorry, but that game nickname is already taken by another player. To regain access, reach out to the support team or alter your nickname.");
		GS()->Chat(ClientID, "Discord: \"{}\".", g_Config.m_SvDiscordInviteLink);
		return AccountCodeResult::AOP_NICKNAME_ALREADY_EXIST; // Return nickname already exists error
	}

	// Get the highest ID from the tw_accounts table
	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_accounts", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1; // Get the next available ID

	// Convert the login and password to CSqlString objects
	const CSqlString<32> cClearLogin = CSqlString<32>(Login);
	const CSqlString<32> cClearPass = CSqlString<32>(Password);

	// Get and store the client's IP address
	char aAddrStr[64];
	Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));

	// Generate a random password salt
	char aSalt[32] = { 0 };
	secure_random_password(aSalt, sizeof(aSalt), 24);

	// Insert the account into the tw_accounts table with the values
	Database->Execute<DB::INSERT>("tw_accounts", "(ID, Username, Password, PasswordSalt, RegisterDate, RegisteredIP) VALUES ('{}', '{}', '{}', '{}', UTC_TIMESTAMP(), '{}')", InitID, cClearLogin.cstr(), HashPassword(cClearPass.cstr(), aSalt).c_str(), aSalt, aAddrStr);
	// Insert the account into the tw_accounts_data table with the ID and nickname values
	Database->Execute<DB::INSERT, 100>("tw_accounts_data", "(ID, Nick) VALUES ('{}', '{}')", InitID, cClearNick.cstr());

	Server()->UpdateAccountBase(InitID, cClearNick.cstr(), g_Config.m_SvMinRating);
	GS()->Chat(ClientID, "- Registration complete! Don't forget to save your data.");
	GS()->Chat(ClientID, "# Your nickname is a unique identifier.");
	GS()->Chat(ClientID, "# Log in: \"/login {} {}\"", cClearLogin.cstr(), cClearPass.cstr());
	return AccountCodeResult::AOP_REGISTER_OK; // Return registration success
}

AccountCodeResult CAccountManager::LoginAccount(int ClientID, const char* pLogin, const char* pPassword)
{
	// check valid player
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, false);
	if(!pPlayer)
	{
		return AccountCodeResult::AOP_UNKNOWN;
	}

	// check valid login and password
	const int LengthLogin = str_length(pLogin);
	const int LengthPassword = str_length(pPassword);
	if(LengthLogin < 4 || LengthLogin > 12 || LengthPassword < 4 || LengthPassword > 12)
	{
		GS()->Chat(ClientID, "The username and password must each contain 4 - 12 characters.");
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS;
	}

	// initialize sql string
	const auto sqlStrLogin = CSqlString<32>(pLogin);
	const auto sqlStrPass = CSqlString<32>(pPassword);
	const auto sqlStrNick = CSqlString<32>(Server()->ClientName(ClientID));

	// check if the account exists
	ResultPtr pResAccount = Database->Execute<DB::SELECT>("*", "tw_accounts_data", "WHERE Nick = '{}'", sqlStrNick.cstr());
	if(!pResAccount->next())
	{
		GS()->Chat(ClientID, "Sorry, we couldn't locate your username in our system.");
		return AccountCodeResult::AOP_NICKNAME_NOT_EXIST;
	}
	int AccountID = pResAccount->getInt("ID");

	// check is login and password correct
	ResultPtr pResCheck = Database->Execute<DB::SELECT>("ID, LoginDate, Language, Password, PasswordSalt",
		"tw_accounts", "WHERE Username = '{}' AND ID = '{}'", sqlStrLogin.cstr(), AccountID);
	if(!pResCheck->next() || str_comp(pResCheck->getString("Password").c_str(), HashPassword(sqlStrPass.cstr(), pResCheck->getString("PasswordSalt").c_str()).c_str()) != 0)
	{
		GS()->Chat(ClientID, "Oops, that doesn't seem to be the right login or password");
		return AccountCodeResult::AOP_LOGIN_WRONG;
	}

	// check if the account is banned
	ResultPtr pResBan = Database->Execute<DB::SELECT>("BannedUntil, Reason", "tw_accounts_bans", "WHERE AccountId = '{}' AND current_timestamp() < `BannedUntil`", AccountID);
	if(pResBan->next())
	{
		const char* pBannedUntil = pResBan->getString("BannedUntil").c_str();
		const char* pReason = pResBan->getString("Reason").c_str();
		GS()->Chat(ClientID, "You account was suspended until '{}' with the reason of '{}'.", pBannedUntil, pReason);
		return AccountCodeResult::AOP_ACCOUNT_BANNED;
	}

	// check is player ingame
	if(GS()->GetPlayerByUserID(AccountID) != nullptr)
	{
		GS()->Chat(ClientID, "The account is already in the game.");
		return AccountCodeResult::AOP_ALREADY_IN_GAME;
	}

	// Update player account information from the database
	const auto Language = pResCheck->getString("Language");
	const auto LoginDate = pResCheck->getString("LoginDate");
	pPlayer->Account()->Init(AccountID, ClientID, sqlStrLogin.cstr(), Language, LoginDate, std::move(pResAccount));

	// Send success messages to the client
	GS()->Chat(ClientID, "- Welcome! You've successfully logged in!");
	GS()->m_pController->DoTeamChange(pPlayer);
	LoadAccount(pPlayer, true);
	return AccountCodeResult::AOP_LOGIN_OK;
}

void CAccountManager::LoadAccount(CPlayer* pPlayer, bool FirstInitilize)
{
	if(!pPlayer || !pPlayer->IsAuthed() || !GS()->IsPlayerInWorld(pPlayer->GetCID()))
		return;

	// Update account context pointer
	auto* pAccount = pPlayer->Account();

	// Broadcast a message to the player with their current location
	const int ClientID = pPlayer->GetCID();
	GS()->Broadcast(ClientID, BroadcastPriority::VeryImportant, 200, "You are currently positioned at {}({})!",
		Server()->GetWorldName(GS()->GetWorldID()), (GS()->IsAllowedPVP() ? "PVE/PVP" : "PVE"));

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

bool CAccountManager::ChangeNickname(const std::string& newNickname, int ClientID) const
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return false;

	// check newnickname
	const CSqlString<32> cClearNick = CSqlString<32>(newNickname.c_str());
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '{}'", cClearNick.cstr());
	if(pRes->next())
		return false;

	Database->Execute<DB::UPDATE>("tw_accounts_data", "Nick = '{}' WHERE ID = '{}'", cClearNick.cstr(), pPlayer->Account()->GetID());
	Server()->SetClientName(ClientID, newNickname.c_str());
	return true;
}

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
	CAccountTempData::ms_aPlayerTempData.erase(ClientID);
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
			const float Percent = pPlayer->GetAttributeChance(ID);

			char aBuf[64] {};
			if(Percent)
			{
				str_format(aBuf, sizeof(aBuf), "(%0.4f%%)", Percent);
			}
			VUpgrades.Add("Total {} - {}{}", pAttribute->GetName(), AttributeSize, aBuf);
		}
	}
	VUpgrades.AddLine();
	{
		for(auto& [ID, Value] : pProf->GetAttributes())
		{
			const auto* pAttribute = GS()->GetAttributeInfo(ID);
			VUpgrades.AddOption("UPGRADE", (int)ProfID, (int)ID, "Upgrade {} - {} (1 point)", pAttribute->GetName(), Value);
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
			const auto progressBar = mystd::string::progressBar(100, progress, 20, "\u25B0", "\u25B1");
			Wrapper.MarkList().Add("{} [Lv{} {}] - {~.2}%", name.c_str(), pProfession->GetLevel(), progressBar, progress);
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
		VoteWrapper VClassSelector(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2694 Change class profession");
		for(const auto& Prof : pPlayer->Account()->GetProfessions())
		{
			if(Prof.IsProfessionType(PROFESSION_TYPE_WAR))
			{
				const char* StrActiveFlag = (ActiveProfID == Prof.GetProfessionID()) ? "\u2713" : "\u2715";
				const char* pProfName = GetProfessionName(Prof.GetProfessionID());
				const auto expNeed = Prof.GetExpForNextLevel();
				const int progress = round_to_int(translate_to_percent(expNeed, Prof.GetExperience()));
				const auto progressBar = mystd::string::progressBar(100, progress, 20, "\u25B0", "\u25B1");

				VClassSelector.AddOption("SELECT_CLASS", static_cast<int>(Prof.GetProfessionID()), "({}) {} [Lv{} {} {~.1}%] ({}P)",
					StrActiveFlag, pProfName, Prof.GetLevel(), progressBar, progress, Prof.GetUpgradePoint());
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

		// professions
		VoteWrapper VProfessions(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2696 Upgrade professions");
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
		VMain.AddMenu(MENU_SETTINGS_TITLE, "Select personal title");
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
				VAccount.AddOption("EQUIP_ITEM", ItemID, "[{}] {}", Status, ItemData.Info()->GetName());
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

		// languages TODO fix language selection
		VoteWrapper VLanguages(ClientID, VWF_OPEN, "Available languages");
		for(int i = 0; i < Server()->Localization()->m_pLanguages.size(); i++)
		{
			// Do not show the language that is already selected by the player in the selection lists
			if(str_comp(pPlayerLanguage, Server()->Localization()->m_pLanguages[i]->GetFilename()) == 0)
				continue;

			// Add language selection
			const char* pLanguageName = Server()->Localization()->m_pLanguages[i]->GetName();
			VLanguages.AddOption("SELECT_LANGUAGE", i, "Select language \"{}\"", pLanguageName);
		}

		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// title selection
	if(Menulist == MENU_SETTINGS_TITLE)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_SETTINGS);

		// initialize variables
		const auto EquippedTitleItemID = pPlayer->GetEquippedItemID(ItemType::EquipTitle);
		const char* pCurrentTitle = EquippedTitleItemID.has_value() ? pPlayer->GetItem(EquippedTitleItemID.value())->Info()->GetName() : "title is not used";

		// title information
		VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "Title Information");
		VInfo.Add("Here you can set the title.");
		VInfo.Add("Current title: {}", pCurrentTitle);
		VoteWrapper::AddEmptyline(ClientID);

		// title list
		bool IsEmpty = true;
		for(auto& pairItem : CPlayerItem::Data()[ClientID])
		{
			CPlayerItem* pPlayerItem = &pairItem.second;
			if(pPlayerItem->Info()->IsType(ItemType::EquipTitle) && pPlayerItem->HasItem())
			{
				// initialize variables
				bool IsEquipped = pPlayerItem->IsEquipped();

				// add to list
				VoteWrapper VList(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE,"Title: {}", pPlayerItem->Info()->GetName());
				VList.Add("{}", pPlayerItem->Info()->GetDescription());
				if(pPlayerItem->Info()->HasAttributes())
				{
					VList.Add("{}", pPlayerItem->GetStringAttributesInfo(pPlayer));
				}
				VList.AddOption("EQUIP_ITEM", pPlayerItem->GetID(), "{} {}", IsEquipped ? "Unset" : "Set", pPlayerItem->Info()->GetName());
				IsEmpty = false;
			}
		}
		if(IsEmpty)
		{
			VoteWrapper(ClientID).Add("Is empty list");
		}

		// add backpage
		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
	}

	return false;
}

bool CAccountManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	// select language command
	if(PPSTR(pCmd, "SELECT_LANGUAGE") == 0)
	{
		const char* pSelectedLanguage = Server()->Localization()->m_pLanguages[Extra1]->GetFilename();
		Server()->SetClientLanguage(ClientID, pSelectedLanguage);
		GS()->Chat(ClientID, "You have chosen a language \"{}\".", pSelectedLanguage);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_SETTINGS_LANGUAGE);
		Core()->SaveAccount(pPlayer, SAVE_LANGUAGE);
		return true;
	}

	// upgrade command
	if(PPSTR(pCmd, "UPGRADE") == 0)
	{
		const auto ProfessionID = (ProfessionIdentifier)Extra1;
		const auto AttributeID = (AttributeIdentifier)Extra2;

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
			GS()->CreatePlayerSound(ClientID, SOUND_VOTE_UPGRADE);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// select class
	if(PPSTR(pCmd, "SELECT_CLASS") == 0)
	{
		const auto ProfessionID = (ProfessionIdentifier)Extra1;

		if((pPlayer->m_aPlayerTick[LastDamage] + Server()->TickSpeed() * 5) > Server()->Tick())
		{
			GS()->Chat(ClientID, "Wait a couple of seconds, your player is currently in combat or taking damage");
			return true;
		}

		pPlayer->Account()->ChangeProfession(ProfessionID);
		pPlayer->GetCharacter()->UpdateEquippedStats();
		pPlayer->m_VotesData.ResetExtraID();
		pPlayer->m_VotesData.UpdateCurrentVotes();
		Core()->SaveAccount(pPlayer, SAVE_PROFESSION);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_CHANGE_CLASS);
		return true;
	}

	return false;
}

void CAccountManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	int ClientID = pPlayer->GetCID();

	HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_INFO_BONUSES, MOTD_MENU_BONUSES_INFO)
	HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_INFO_WANTED, MOTD_MENU_WANTED_INFO)
	HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_BANK_MANAGER, MOTD_MENU_BANK_MANAGER)
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
		int position = 1;
		MotdMenu MBonuses(ClientID, MTFLAG_CLOSE_BUTTON, "All bonuses overlap, the minimum increase cannot be lower than 1 point.");
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

		MWanted.Send(MOTD_MENU_WANTED_INFO);
		return true;
	}

	return false;
}

bool CAccountManager::OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd)
{
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

void CAccountManager::UseVoucher(int ClientID, const char* pVoucher) const
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	char aSelect[256];
	const CSqlString<32> cVoucherCode = CSqlString<32>(pVoucher);
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
		pPlayer->Account()->GetID(), std::string("current_timestamp + " + Time.asSqlInterval()).c_str(), Reason.c_str());
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
		std::string BannedUntil = pResBan->getString("BannedUntil").c_str();
		std::string PlayerNickname = Server()->GetAccountNickname(AccountID);
		std::string Reason = pResBan->getString("Reason").c_str();
		out.push_back({ ID, BannedUntil, std::move(PlayerNickname), std::move(Reason) });
	}

	return out;
}
