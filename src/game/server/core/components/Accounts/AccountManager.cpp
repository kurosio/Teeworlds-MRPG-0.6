/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountManager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Dungeons/DungeonManager.h>
#include <game/server/core/components/Mails/MailBoxManager.h>
#include <game/server/core/components/Worlds/WorldData.h>

#include <base/hash_ctxt.h>

// This function returns the latest correct world ID from the player's history world list
// The function takes a pointer to a CPlayer object as an argument
int CAccountManager::GetLastVisitedWorldID(CPlayer* pPlayer) const
{
	// Find the first element in the range [pPlayer->Account()->m_aHistoryWorld.begin(), pPlayer->Account()->m_aHistoryWorld.end()]
	// that satisfies the condition specified by the lambda function
	const auto pWorldIterator = std::find_if(pPlayer->Account()->m_aHistoryWorld.begin(), pPlayer->Account()->m_aHistoryWorld.end(), [=](int WorldID)
	{
		// Return true if the world is not a dungeon world and the player's level is greater than or equal to the required level
		if(GS()->GetWorldData(WorldID))
		{
			int RequiredLevel = GS()->GetWorldData(WorldID)->GetRequiredLevel();
			return !Server()->IsWorldType(WorldID, WorldType::Dungeon) && pPlayer->Account()->GetLevel() >= RequiredLevel;
		}

		// Return false if the world data for the given WorldID does not exist
		return false;
	});

	// Return the world ID if a correct world ID is found, otherwise return the main world ID
	return pWorldIterator != pPlayer->Account()->m_aHistoryWorld.end() ? *pWorldIterator : MAIN_WORLD_ID;
}

// Register an account for a client with the given parameters
AccountCodeResult CAccountManager::RegisterAccount(int ClientID, const char* Login, const char* Password)
{
	// Check if the length of the login and password is between 4 and 12 characters
	if(str_length(Login) > 12 || str_length(Login) < 4 || str_length(Password) > 12 || str_length(Password) < 4)
	{
		GS()->Chat(ClientID, "The username and password must each contain 4 - 12 characters.");
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS; // Return mismatch length symbols error
	}

	// Get the client's clear nickname
	const CSqlString<32> cClearNick = CSqlString<32>(Server()->ClientName(ClientID));

	// Check if the client's nickname is already registered
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '%s'", cClearNick.cstr());
	if(pRes->next())
	{
		GS()->Chat(ClientID, "Sorry, but that game nickname is already taken by another player. To regain access, reach out to the support team or alter your nickname.");
		GS()->Chat(ClientID, "Discord: \"{STR}\".", g_Config.m_SvDiscordInviteLink);
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
	Database->Execute<DB::INSERT>("tw_accounts", "(ID, Username, Password, PasswordSalt, RegisterDate, RegisteredIP) VALUES ('%d', '%s', '%s', '%s', UTC_TIMESTAMP(), '%s')", InitID, cClearLogin.cstr(), HashPassword(cClearPass.cstr(), aSalt).c_str(), aSalt, aAddrStr);
	// Insert the account into the tw_accounts_data table with the ID and nickname values
	Database->Execute<DB::INSERT, 100>("tw_accounts_data", "(ID, Nick) VALUES ('%d', '%s')", InitID, cClearNick.cstr());

	Server()->AddAccountNickname(InitID, cClearNick.cstr());
	GS()->Chat(ClientID, "- Registration complete! Don't forget to save your data.");
	GS()->Chat(ClientID, "# Your nickname is a unique identifier.");
	GS()->Chat(ClientID, "# Log in: \"/login {STR} {STR}\"", cClearLogin.cstr(), cClearPass.cstr());
	return AccountCodeResult::AOP_REGISTER_OK; // Return registration success
}

// Function to log in to an account
AccountCodeResult CAccountManager::LoginAccount(int ClientID, const char* Login, const char* Password)
{
	// Get the player associated with the client ID
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, false);

	// Check if the player exists
	if(!pPlayer)
	{
		// If player does not exist, return unknown error
		return AccountCodeResult::AOP_UNKNOWN;
	}

	// Check if the length of the login is less than 4 or greater than 12, or if the length of the password is less than 4 or greater than 12
	const int LengthLogin = str_length(Login);
	const int LengthPassword = str_length(Password);
	if(LengthLogin < 4 || LengthLogin > 12 || LengthPassword < 4 || LengthPassword > 12)
	{
		// Send error message to the client
		GS()->Chat(ClientID, "The username and password must each contain 4 - 12 characters.");
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS; // Return mismatch length symbols error
	}

	// Create a SQL strings
	const auto sqlStrLogin = CSqlString<32>(Login);
	const auto sqlStrPass = CSqlString<32>(Password);
	const auto sqlStrNick = CSqlString<32>(Server()->ClientName(ClientID));

	// Check if the nickname exists in the database
	ResultPtr pResAccount = Database->Execute<DB::SELECT>("*", "tw_accounts_data", "WHERE Nick = '%s'", sqlStrNick.cstr());
	if(!pResAccount->next())
	{
		// Send error message to the client
		GS()->Chat(ClientID, "Sorry, we couldn't locate your username in our system.");
		return AccountCodeResult::AOP_NICKNAME_NOT_EXIST; // Return nickname not found error
	}

	// Check if the wrong login or password error
	int UserID = pResAccount->getInt("ID"); // Get the ID from the account result
	ResultPtr pResCheck = Database->Execute<DB::SELECT>("ID, LoginDate, Language, Password, PasswordSalt", "tw_accounts", "WHERE Username = '%s' AND ID = '%d'", sqlStrLogin.cstr(), UserID);
	if(!pResCheck->next() || str_comp(pResCheck->getString("Password").c_str(), HashPassword(sqlStrPass.cstr(), pResCheck->getString("PasswordSalt").c_str()).c_str()) != 0)
	{
		// Send error message to the client
		GS()->Chat(ClientID, "Oops, that doesn't seem to be the right login or password");
		return AccountCodeResult::AOP_LOGIN_WRONG; // Return wrong login or password error
	}

	// Check if the account is banned
	ResultPtr pResBan = Database->Execute<DB::SELECT>("BannedUntil, Reason", "tw_accounts_bans", "WHERE AccountId = '%d' AND current_timestamp() < `BannedUntil`", UserID);
	if(pResBan->next())
	{
		// Send error message to the client with the ban information
		GS()->Chat(ClientID, "You account was suspended until \"{STR}\" with the reason of \"{STR}\"", pResBan->getString("BannedUntil").c_str(), pResBan->getString("Reason").c_str());
		return AccountCodeResult::AOP_ACCOUNT_BANNED; // Return account banned error
	}

	// Check if a player with the given UserID exists in the game state
	if(GS()->GetPlayerByUserID(UserID) != nullptr)
	{
		// Send error message to the client
		GS()->Chat(ClientID, "The account is already in the game.");
		return AccountCodeResult::AOP_ALREADY_IN_GAME; // Return already in game error
	}

	// Update player account information from the database
	std::string Language = pResCheck->getString("Language").c_str();
	std::string LoginDate = pResCheck->getString("LoginDate").c_str();
	pPlayer->Account()->Init(UserID, pPlayer, sqlStrLogin.cstr(), Language, LoginDate, std::move(pResAccount));

	// Send success messages to the client
	GS()->Chat(ClientID, "- Welcome! You've successfully logged in!");
	GS()->m_pController->DoTeamChange(pPlayer, false);
	LoadAccount(pPlayer, true);
	return AccountCodeResult::AOP_LOGIN_OK;
}

void CAccountManager::LoadAccount(CPlayer* pPlayer, bool FirstInitilize)
{
	// Check if pPlayer exists and is authenticated, and if the player is in the same world as the game server
	if(!pPlayer || !pPlayer->IsAuthed() || !GS()->IsPlayerEqualWorld(pPlayer->GetCID()))
		return;

	// Update account context pointer
	pPlayer->Account()->UpdatePointer(pPlayer);

	// Broadcast a message to the player with their current location
	const int ClientID = pPlayer->GetCID();
	GS()->Broadcast(ClientID, BroadcastPriority::VERY_IMPORTANT, 200, "You are currently positioned at {STR}({STR})!",
		Server()->GetWorldName(GS()->GetWorldID()), (GS()->IsAllowedPVP() ? "PVE/PVP" : "PVE"));

	// Check if it is not the first initialization
	if(!FirstInitilize)
	{
		// Get the number of unread letters in the player's inbox
		// Send a chat message to the player informing them about their unread letters
		if(const int Letters = Core()->MailboxManager()->GetMailLettersSize(pPlayer->Account()->GetID()); Letters > 0)
			GS()->Chat(ClientID, "You have {INT} unread letters!", Letters);

		// Update the player's votes and show the main menu
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
		return;
	}

	// If it is the first initialization, initialize the player's job account
	Core()->OnInitAccount(ClientID);

	// Send information about log in
	const int Rank = GetRank(pPlayer->Account()->GetID());
	GS()->Chat(-1, "{STR} logged to account. Rank #{INT}", Server()->ClientName(ClientID), Rank);
#ifdef CONF_DISCORD
	char aLoginBuf[64];
	str_format(aLoginBuf, sizeof(aLoginBuf), "%s logged in AccountManager ID %d", Server()->ClientName(ClientID), pPlayer->AccountManager()->GetID());
	Server()->SendDiscordGenerateMessage(aLoginBuf, pPlayer->AccountManager()->GetID());
#endif

	/* Initialize static settings items' data */
	{
		// Define a lambda function InitSettingsItem that takes in a player and a map of item IDs and default values
		auto InitSettingsItem = [pPlayer](const std::unordered_map<int, int>& pItems)
		{
			for(auto& [id, defaultValue] : pItems)
			{
				if(auto pItem = pPlayer->GetItem(id); !pItem->GetValue())
					pItem->Add(1, defaultValue);
			}
		};

		// Call the InitSettingsItem function with a map of item IDs and default values
		InitSettingsItem(
			{
				{ itHammer, 1 },
				{ itShowEquipmentDescription, 0 },
				{ itShowCriticalDamage, 1 },
				{ itShowQuestNavigator, 1 }
			});
	}

	// Hanlde time period
	Core()->HandlePlayerTimePeriod(pPlayer);

	// Change player's world ID to the latest correct world ID
	const int LatestCorrectWorldID = GetLastVisitedWorldID(pPlayer);
	if(LatestCorrectWorldID != GS()->GetWorldID())
	{
		pPlayer->ChangeWorld(LatestCorrectWorldID);
		return;
	}
}

void CAccountManager::DiscordConnect(int ClientID, const char* pDID) const
{
#ifdef CONF_DISCORD
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	const CSqlString<64> cDiscordID = CSqlString<64>(pDID);

	// disable another account if it is connected to this discord
	Database->Execute<DB::UPDATE>("tw_accounts_data", "DiscordID = 'null' WHERE DiscordID = '%s'", cDiscordID.cstr());

	// connect the player discord id
	Database->Execute<DB::UPDATE, 1000>("tw_accounts_data", "DiscordID = '%s' WHERE ID = '%d'", cDiscordID.cstr(), pPlayer->AccountManager()->GetID());

	GS()->Chat(ClientID, "Your Discord ID has been updated.");
	GS()->Chat(ClientID, "Check the connection status in discord \"/connect\".");
#endif
}

bool CAccountManager::ChangeNickname(int ClientID)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer || !pPlayer->m_RequestChangeNickname)
		return false;

	// check newnickname
	const CSqlString<32> cClearNick = CSqlString<32>(Server()->GetClientNameChangeRequest(ClientID));
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '%s'", cClearNick.cstr());
	if(pRes->next())
		return false;

	Database->Execute<DB::UPDATE>("tw_accounts_data", "Nick = '%s' WHERE ID = '%d'", cClearNick.cstr(), pPlayer->Account()->GetID());
	Server()->SetClientName(ClientID, Server()->GetClientNameChangeRequest(ClientID));
	return true;
}

int CAccountManager::GetRank(int AccountID)
{
	int Rank = 0;
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "ORDER BY Level DESC, Exp DESC");
	while(pRes->next())
	{
		Rank++;
		const int ID = pRes->getInt("ID");
		if(AccountID == ID)
			return Rank;
	}
	return -1;
}

bool CAccountManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	// settings
	if(Menulist == MENU_SETTINGS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// information
		CVoteWrapper VSettingsInfo(ClientID, VWF_SEPARATE_CLOSED, "Settings Information");
		VSettingsInfo.Add("Some of the settings become valid after death.");
		VSettingsInfo.Add("Here you can change the settings of your account.");
		CVoteWrapper::AddLine(ClientID);

		// game settings
		CVoteWrapper VMainSettings(ClientID, VWF_SEPARATE_OPEN, "\u2699 Main settings");
		VMainSettings.AddMenu(MENU_SETTINGS_LANGUAGE_SELECT, "Settings language");
		for(const auto& [ItemID, ItemData] : CPlayerItem::Data()[ClientID])
		{
			if(ItemData.Info()->IsType(ItemType::TYPE_SETTINGS) && ItemData.HasItem())
				VMainSettings.AddOption("ISETTINGS", ItemID, "[{STR}] {STR}", (ItemData.GetSettings() ? "Enabled" : "Disabled"), ItemData.Info()->GetName());
		}
		CVoteWrapper::AddLine(ClientID);

		// equipment modules
		CVoteWrapper VModulesSettings(ClientID, VWF_SEPARATE_OPEN, "\u2694 Modules settings");
		for(auto& iter : CPlayerItem::Data()[ClientID])
		{
			CPlayerItem* pPlayerItem = &iter.second;
			CItemDescription* pItemInfo = pPlayerItem->Info();
			if(!pItemInfo->IsType(ItemType::TYPE_MODULE) || !pPlayerItem->HasItem())
				continue;

			char aAttributesInfo[128];
			if(pItemInfo->HasAttributes())
				pPlayerItem->StrFormatAttributes(pPlayer, aAttributesInfo, sizeof(aAttributesInfo));
			else
				str_copy(aAttributesInfo, pItemInfo->GetDescription(), sizeof(aAttributesInfo));

			VModulesSettings.AddOption("ISETTINGS", pItemInfo->GetID(), "{STR}{STR} * {STR}", 
				pPlayerItem->IsEquipped() ? "✔" : "\0", pItemInfo->GetName(), aAttributesInfo);
		}
		VModulesSettings.AddIf(VModulesSettings.IsEmpty(), "The list of equipment modules is empty.");

		CVoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// Language selection
	if(Menulist == MENU_SETTINGS_LANGUAGE_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_SETTINGS);

		// language information
		CVoteWrapper VLanguageInfo(ClientID, VWF_SEPARATE_CLOSED, "Languages Information");
		VLanguageInfo.Add("Here you can choose the language.");
		VLanguageInfo.Add("Note: translation is not complete.");
		CVoteWrapper::AddLine(ClientID);

		// active language
		const char* pPlayerLanguage = pPlayer->GetLanguage();
		CVoteWrapper VLanguage(ClientID, VWF_STYLE_STRICT_BOLD);
		VLanguage.Add("Active language: [{STR}]", pPlayerLanguage);
		VLanguage.AddLine();

		// languages
		CVoteWrapper VLanguages(ClientID, VWF_SEPARATE_OPEN, "Available languages");
		for(int i = 0; i < Server()->Localization()->m_pLanguages.size(); i++)
		{
			// Do not show the language that is already selected by the player in the selection lists
			if(str_comp(pPlayerLanguage, Server()->Localization()->m_pLanguages[i]->GetFilename()) == 0)
				continue;

			// Add language selection
			const char* pLanguageName = Server()->Localization()->m_pLanguages[i]->GetName();
			VLanguages.AddOption("SELECT_LANGUAGE", i, "Select language \"{STR}\"", pLanguageName);
		}

		CVoteWrapper::AddBackpage(ClientID);
		return true;
	}
	return false;
}

// Function to handle vote commands for an account
bool CAccountManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	// Get the client ID of the player
	const int ClientID = pPlayer->GetCID();

	// Check if the command is "SELECTLANGUAGE"
	if(PPSTR(CMD, "SELECT_LANGUAGE") == 0)
	{
		// Set the client's language to the selected language from the localization object
		const char* pSelectedLanguage = Server()->Localization()->m_pLanguages[VoteID]->GetFilename();
		Server()->SetClientLanguage(ClientID, pSelectedLanguage);

		// Inform the client about the selected language
		GS()->Chat(ClientID, "You have chosen a language \"{STR}\".", pSelectedLanguage);

		// Update the votes menu for the client
		pPlayer->m_VotesData.UpdateVotesIf(MENU_SETTINGS_LANGUAGE_SELECT);

		// Save the account's language
		Core()->SaveAccount(pPlayer, SAVE_LANGUAGE);
		return true;
	}

	if(PPSTR(CMD, "UPGRADE") == 0)
	{
		if(pPlayer->Upgrade(Get, &pPlayer->Account()->m_aStats[(AttributeIdentifier)VoteID], &pPlayer->Account()->m_Upgrade, VoteID2, 1000))
		{
			GS()->Core()->SaveAccount(pPlayer, SAVE_UPGRADES);
			pPlayer->m_VotesData.UpdateVotes(MENU_UPGRADES);
		}
		return true;
	}

	return false;
}

void CAccountManager::OnResetClient(int ClientID)
{
	CAccountTempData::ms_aPlayerTempData.erase(ClientID);
	CAccountData::ms_aData.erase(ClientID);
}

void CAccountManager::OnPlayerHandleTimePeriod(CPlayer* pPlayer, TIME_PERIOD Period)
{
	// Get the client ID of the player
	int ClientID = pPlayer->GetCID();

	// If the time period is set to DAILY_STAMP
	if(Period == TIME_PERIOD::DAILY_STAMP)
	{
		pPlayer->Account()->ResetDailyChairGolds();
		GS()->Chat(ClientID, "The gold limit in the chair has been updated.");
	}
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
	CPlayer* pPlayer = GS()->m_apPlayers[ClientID];
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	char aSelect[256];
	const CSqlString<32> cVoucherCode = CSqlString<32>(pVoucher);
	str_format(aSelect, sizeof(aSelect), "v.*, IF((SELECT r.ID FROM tw_voucher_redeemed r WHERE CASE v.Multiple WHEN 1 THEN r.VoucherID = v.ID AND r.UserID = %d ELSE r.VoucherID = v.ID END) IS NULL, FALSE, TRUE) AS used", pPlayer->Account()->GetID());

	ResultPtr pResVoucher = Database->Execute<DB::SELECT>(aSelect, "tw_voucher v", "WHERE v.Code = '%s'", cVoucherCode.cstr());
	if(pResVoucher->next())
	{
		const int VoucherID = pResVoucher->getInt("ID");
		const int ValidUntil = pResVoucher->getInt("ValidUntil");
		nlohmann::json JsonData = nlohmann::json::parse(pResVoucher->getString("Data").c_str());

		if(ValidUntil > 0 && ValidUntil < time(0))
			GS()->Chat(ClientID, "The voucher code '{STR}' has expired.", pVoucher);
		else if(pResVoucher->getBoolean("used"))
			GS()->Chat(ClientID, "This voucher has already been redeemed.");
		else
		{
			const int Exp = JsonData.value("exp", 0);
			const int Money = JsonData.value("money", 0);
			const int Upgrade = JsonData.value("upgrade", 0);

			if(Exp > 0)
				pPlayer->Account()->AddExperience(Exp);
			if(Money > 0)
				pPlayer->Account()->AddGold(Money);
			if(Upgrade > 0)
				pPlayer->Account()->m_Upgrade += Upgrade;

			if(JsonData.find("items") != JsonData.end() && JsonData["items"].is_array())
			{
				for(const nlohmann::json& Item : JsonData["items"])
				{
					const int ItemID = Item.value("id", -1);
					const int Value = Item.value("value", 0);
					if(Value > 0 && CItemDescription::Data().find(ItemID) != CItemDescription::Data().end())
						pPlayer->GetItem(ItemID)->Add(Value);
				}
			}

			GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
			GS()->Core()->SaveAccount(pPlayer, SAVE_UPGRADES);

			Database->Execute<DB::INSERT>("tw_voucher_redeemed", "(VoucherID, UserID, TimeCreated) VALUES (%d, %d, %d)", VoucherID, pPlayer->Account()->GetID(), (int)time(0));
			GS()->Chat(ClientID, "You have successfully redeemed the voucher '{STR}'.", pVoucher);
		}

		return;
	}

	GS()->Chat(ClientID, "The voucher code '{STR}' does not exist.", pVoucher);
}

// Function BanAccount
bool CAccountManager::BanAccount(CPlayer* pPlayer, TimePeriodData Time, const std::string& Reason)
{
	// Check if the account is already banned
	ResultPtr pResBan = Database->Execute<DB::SELECT>("BannedUntil", "tw_accounts_bans", "WHERE AccountId = '%d' AND current_timestamp() < `BannedUntil`", pPlayer->Account()->GetID());
	if(pResBan->next())
	{
		// Print message and return false if the account is already banned
		m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "This account is already banned");
		return false;
	}

	// Ban the account
	Database->Execute<DB::INSERT>("tw_accounts_bans", "(AccountId, BannedUntil, Reason) VALUES (%d, %s, '%s')",
		pPlayer->Account()->GetID(), std::string("current_timestamp + " + Time.asSqlInterval()).c_str(), Reason.c_str());
	GS()->Server()->Kick(pPlayer->GetCID(), "Your account was banned");

	// Print success message and return true
	m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Successfully banned!");
	return true;
}

// Function: UnBanAccount by specified BanId
bool CAccountManager::UnBanAccount(int BanId)
{
	// Search for ban using the specified BanId
	ResultPtr pResBan = Database->Execute<DB::SELECT>("AccountId", "tw_accounts_bans", "WHERE Id = '%d' AND current_timestamp() < `BannedUntil`", BanId);
	if(pResBan->next())
	{
		// If the ban exists and the current timestamp is still less than the BannedUntil timestamp, unban the account
		Database->Execute<DB::UPDATE>("tw_accounts_bans", "BannedUntil = current_timestamp WHERE Id = '%d'", BanId);
		m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Successfully unbanned!");
		return true;
	}

	// If the ban does not exist or the current timestamp is already greater than the BannedUntil timestamp, print an error message
	m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Ban is not valid anymore or does not exist!");
	return false;
}

// Function: BansAccount
std::vector<CAccountManager::AccBan> CAccountManager::BansAccount()
{
	std::vector<AccBan> out; // Create a vector called "out" to store instances of the AccBan struct
	std::size_t capacity = 100; // Set the initial capacity of the vector to 100
	out.reserve(capacity); // Reserve memory for the vector based on the capacity

	/*
		Execute a SELECT query on the "tw_accounts_bans" table in the database,
		retrieving the columns "Id", "BannedUntil", "Reason", and "AccountId"
		where the current timestamp is less than the "BannedUntil" value
	*/
	ResultPtr pResBan = Database->Execute<DB::SELECT>("Id, BannedUntil, Reason, AccountId", "tw_accounts_bans", "WHERE current_timestamp() < `BannedUntil`");
	while(pResBan->next())
	{
		int ID = pResBan->getInt("Id"); // Get the value of the "Id" column from the current row
		int AccountID = pResBan->getInt("AccountId"); // Get the value of the "AccountId" column from the current row
		std::string BannedUntil = pResBan->getString("BannedUntil").c_str(); // Get the value of the "BannedUntil" column from the current row
		std::string PlayerNickname = Server()->GetAccountNickname(AccountID); // Get the player nickname using the AccountID
		std::string Reason = pResBan->getString("Reason").c_str(); // Get the value of the "Reason" column from the current row

		// Create an instance of the AccBan struct with the retrieved values and add it to the vector "out"
		out.push_back({ ID, BannedUntil, std::move(PlayerNickname), std::move(Reason) });
	}

	return out; // Return the vector "out" containing the AccBan structs
}