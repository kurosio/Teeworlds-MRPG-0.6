/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountManager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Dungeons/DungeonManager.h>
#include <game/server/mmocore/Components/Mails/MailBoxManager.h>
#include <game/server/mmocore/Components/Quests/QuestManager.h>
#include <game/server/mmocore/Components/Worlds/WorldData.h>

#include <game/server/mmocore/Components/Houses/HouseManager.h>

#include <base/hash_ctxt.h>

// This function returns the latest correct world ID from the player's history world list
// The function takes a pointer to a CPlayer object as an argument
int CAccountManager::GetHistoryLatestCorrectWorldID(CPlayer* pPlayer) const
{
	// Find the first correct world ID in the player's history world list
	// The correct world ID is the one that meets the following conditions:
	// - The world data exists
	// - The world is not a dungeon world
	// - The required quest for the world is completed or the world has no required quest
	const auto pWorldIterator = std::find_if(pPlayer->Acc().m_aHistoryWorld.begin(), pPlayer->Acc().m_aHistoryWorld.end(), [=](int WorldID)
	{
		if(GS()->GetWorldData(WorldID))
		{
			// Get the required quest for the world
			CQuestDescription* pQuestInfo = GS()->GetWorldData(WorldID)->GetRequiredQuest();

			// Check if the world is not a dungeon world and either the required quest is completed or the world has no required quest
			return !Job()->Dungeon()->IsDungeonWorld(WorldID) && ((pQuestInfo && pPlayer->GetQuest(pQuestInfo->GetID())->IsComplected()) || !pQuestInfo);
		}
		return false;
	});

	// Return the world ID if a correct world ID is found, otherwise return the main world ID
	return pWorldIterator != pPlayer->Acc().m_aHistoryWorld.end() ? *pWorldIterator : MAIN_WORLD_ID;
}

// Register an account for a client with the given parameters
AccountCodeResult CAccountManager::RegisterAccount(int ClientID, const char* Login, const char* Password)
{
	// Check if the length of the login and password is between 4 and 12 characters
	if(str_length(Login) > 12 || str_length(Login) < 4 || str_length(Password) > 12 || str_length(Password) < 4)
	{
		GS()->Chat(ClientID, "Username / Password must contain 4-12 characters");
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS; // Return mismatch length symbols error
	}

	// Get the client's clear nickname
	const CSqlString<32> cClearNick = CSqlString<32>(Server()->ClientName(ClientID));

	// Check if the client's nickname is already registered
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '%s'", cClearNick.cstr());
	if(pRes->next())
	{
		GS()->Chat(ClientID, "- - - - [Your nickname is already registered!] - - - -");
		GS()->Chat(ClientID, "Your game nick is a unique identifier, and it has already been used.");
		GS()->Chat(ClientID, "You can restore access by contacting support, or change nick.");
		GS()->Chat(ClientID, "Discord group \"{STR}\".", g_Config.m_SvDiscordInviteLink);
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

	GS()->Chat(ClientID, "- - - - - - - [Successful registered!] - - - - - - -");
	GS()->Chat(ClientID, "Don't forget your data, have a nice game!");
	GS()->Chat(ClientID, "# Your nickname is a unique identifier.");
	GS()->Chat(ClientID, "# Log in: \"/login {STR} {STR}\"", cClearLogin.cstr(), cClearPass.cstr());
	return AccountCodeResult::AOP_REGISTER_OK; // Return registration success
}

// Function to log in to an account
AccountCodeResult CAccountManager::LoginAccount(int ClientID, const char* Login, const char* Password)
{
	// Get the player associated with the client ID
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, false);
	if(!pPlayer)
		return AccountCodeResult::AOP_UNKNOWN; // If player does not exist, return unknown error

	const int LengthLogin = str_length(Login);
	const int LengthPassword = str_length(Password);
	// Check if login and password lengths are valid
	if(LengthLogin > 12 || LengthLogin < 4 || LengthPassword > 12 || LengthPassword < 4)
	{
		// Send error message to the client
		GS()->Chat(ClientID, "Username / Password must contain 4-12 characters");
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS; // Return mismatch length symbols error
	}

	// Convert login, password, and client name to normalized SQL strings
	const CSqlString<32> cClearLogin = CSqlString<32>(Login);
	const CSqlString<32> cClearPass = CSqlString<32>(Password);
	const CSqlString<32> cClearNick = CSqlString<32>(Server()->ClientName(ClientID));

	// Check if the nickname exists in the database
	ResultPtr pResAccount = Database->Execute<DB::SELECT>("*", "tw_accounts_data", "WHERE Nick = '%s'", cClearNick.cstr());
	if(pResAccount->next())
	{
		const int UserID = pResAccount->getInt("ID");
		// Check if the given login and ID match in the database
		ResultPtr pResCheck = Database->Execute<DB::SELECT>("ID, LoginDate, Language, Password, PasswordSalt", "tw_accounts", "WHERE Username = '%s' AND ID = '%d'", cClearLogin.cstr(), UserID);

		bool LoginSuccess = false;
		if(pResCheck->next())
		{
			// Check if the given password matches the hashed password in the database
			if(!str_comp(pResCheck->getString("Password").c_str(), HashPassword(cClearPass.cstr(), pResCheck->getString("PasswordSalt").c_str()).c_str()))
				LoginSuccess = true;
		}

		if(!LoginSuccess)
		{
			// Send error message to the client
			GS()->Chat(ClientID, "Wrong login or password.");
			return AccountCodeResult::AOP_LOGIN_WRONG; // Return wrong login or password error
		}

		if(GS()->GetPlayerByUserID(UserID) != nullptr)
		{
			// Send error message to the client
			GS()->Chat(ClientID, "The account is already in the game.");
			return AccountCodeResult::AOP_ALREADY_IN_GAME; // Return already in game error
		}

		// Check if the account is banned
		ResultPtr pResBan = Database->Execute<DB::SELECT>("BannedUntil, Reason", "tw_accounts_bans", "WHERE AccountId = '%d' AND current_timestamp() < `BannedUntil`", UserID);
		if(pResBan->next())
		{
			// Send error message to the client with the ban information
			GS()->Chat(ClientID, "You account was suspended until \"{STR}\" with the reason of \"{STR}\"", pResBan->getString("BannedUntil").c_str(), pResBan->getString("Reason").c_str());
			return AccountCodeResult::AOP_ACCOUNT_BANNED; // Return account banned error
		}

		// Set the client language and copy login and last login information to the player account
		Server()->SetClientLanguage(ClientID, pResCheck->getString("Language").c_str());
		str_copy(pPlayer->Acc().m_aLogin, cClearLogin.cstr(), sizeof(pPlayer->Acc().m_aLogin));
		str_copy(pPlayer->Acc().m_aLastLogin, pResCheck->getString("LoginDate").c_str(), sizeof(pPlayer->Acc().m_aLastLogin));

		// Update player account information from the database
		pPlayer->Acc().m_ID = UserID;
		pPlayer->Acc().m_Level = pResAccount->getInt("Level");
		pPlayer->Acc().m_Exp = pResAccount->getInt("Exp");
		pPlayer->Acc().m_GuildID = pResAccount->getInt("GuildID");
		pPlayer->Acc().m_Upgrade = pResAccount->getInt("Upgrade");
		pPlayer->Acc().m_GuildRank = pResAccount->getInt("GuildRank");
		pPlayer->Acc().m_aHistoryWorld.push_front(pResAccount->getInt("WorldID"));
		Server()->SetClientScore(ClientID, pPlayer->Acc().m_Level);

		// Load player account upgrades data
		for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			if(pAttribute->HasDatabaseField())
				pPlayer->Acc().m_aStats[ID] = pResAccount->getInt(pAttribute->GetFieldName());
		}

		// Send success messages to the client
		GS()->Chat(ClientID, "- - - - - - - [Successful login!] - - - - - - -");
		GS()->Chat(ClientID, "Don't forget that cl_motd_time must be set!");
		GS()->Chat(ClientID, "Menu is available in call-votes!");
		GS()->m_pController->DoTeamChange(pPlayer, false);
		LoadAccount(pPlayer, true);

		char aAddrStr[64];
		Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
		// Update login date and IP address in the database
		Database->Execute<DB::UPDATE>("tw_accounts", "LoginDate = CURRENT_TIMESTAMP, LoginIP = '%s' WHERE ID = '%d'", aAddrStr, UserID);
		return AccountCodeResult::AOP_LOGIN_OK; // Return login success
	}

	// Send error message to the client
	GS()->Chat(ClientID, "Your nickname was not found in the Database.");
	return AccountCodeResult::AOP_NICKNAME_NOT_EXIST; // Return nickname not found error
}

void CAccountManager::LoadAccount(CPlayer* pPlayer, bool FirstInitilize)
{
	// Check if pPlayer exists and is authenticated, and if the player is in the same world as the game server
	if(!pPlayer || !pPlayer->IsAuthed() || !GS()->IsPlayerEqualWorld(pPlayer->GetCID()))
		return;

	// Broadcast a message to the player with their current location
	const int ClientID = pPlayer->GetCID();
	GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 200, "You are located {STR} ({STR})", Server()->GetWorldName(GS()->GetWorldID()), (GS()->IsAllowedPVP() ? "Zone PVP" : "Safe zone"));

	// Check if it is not the first initialization
	if(!FirstInitilize)
	{
		// Get the number of unread letters in the player's inbox
		// Send a chat message to the player informing them about their unread letters
		if(const int Letters = Job()->Inbox()->GetMailLettersSize(pPlayer->Acc().m_ID); Letters > 0)
			GS()->Chat(ClientID, "You have {INT} unread letters!", Letters);

		// Update the player's votes and show the main menu
		GS()->UpdateVotes(ClientID, MENU_MAIN);
		return;
	}

	// If it is the first initialization, initialize the player's job account
	Job()->OnInitAccount(ClientID);

	// Send information about log in
	const int Rank = GetRank(pPlayer->Acc().m_ID);
	GS()->Chat(-1, "{STR} logged to account. Rank #{INT}", Server()->ClientName(ClientID), Rank);
#ifdef CONF_DISCORD
	char aLoginBuf[64];
	str_format(aLoginBuf, sizeof(aLoginBuf), "%s logged in Account ID %d", Server()->ClientName(ClientID), pPlayer->Acc().m_ID);
	Server()->SendDiscordGenerateMessage(aLoginBuf, pPlayer->Acc().m_ID);
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
				{ itModePVP, 1 },
				{ itShowEquipmentDescription, 0 },
				{ itShowCriticalDamage, 1 },
				{ itShowQuestNavigator, 1 }
			});
	}

	// Set temp safe spawn
	pPlayer->GetTempData().m_TempSafeSpawn = true;

	// Change player's world ID to the latest correct world ID
	const int LatestCorrectWorldID = GetHistoryLatestCorrectWorldID(pPlayer);
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
	Database->Execute<DB::UPDATE, 1000>("tw_accounts_data", "DiscordID = '%s' WHERE ID = '%d'", cDiscordID.cstr(), pPlayer->Acc().m_ID);

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

	Database->Execute<DB::UPDATE>("tw_accounts_data", "Nick = '%s' WHERE ID = '%d'", cClearNick.cstr(), pPlayer->Acc().m_ID);
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

bool CAccountManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if(ReplaceMenu)
	{
		return false;
	}

	// settings
	if(Menulist == MENU_SETTINGS)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;

		// game settings
		GS()->AVH(ClientID, TAB_SETTINGS, "Some of the settings becomes valid after death");
		GS()->AVM(ClientID, "MENU", MENU_SELECT_LANGUAGE, TAB_SETTINGS, "Settings language");
		for(const auto& [ItemID, ItemData] : CPlayerItem::Data()[ClientID])
		{
			if(ItemData.Info()->IsType(ItemType::TYPE_SETTINGS) && ItemData.HasItem())
				GS()->AVM(ClientID, "ISETTINGS", ItemID, TAB_SETTINGS, "[{STR}] {STR}", (ItemData.GetSettings() ? "Enabled" : "Disabled"), ItemData.Info()->GetName());
		}

		// equipment modules
		bool IsFoundModules = false;
		GS()->AV(ClientID, "null");
		GS()->AVH(ClientID, TAB_SETTINGS_MODULES, "Modules settings");
		for(const auto& it : CPlayerItem::Data()[ClientID])
		{
			const CPlayerItem ItemData = it.second;
			if(ItemData.Info()->IsType(ItemType::TYPE_MODULE) && ItemData.GetValue() > 0)
			{
				char aAttributes[128];
				ItemData.StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes));
				GS()->AVM(ClientID, "ISETTINGS", it.first, TAB_SETTINGS_MODULES, "{STR}{STR} * {STR}", (ItemData.GetSettings() ? "✔" : "\0"), ItemData.Info()->GetName(), aAttributes);
				IsFoundModules = true;
			}
		}

		// if no modules are found
		if(!IsFoundModules)
			GS()->AVM(ClientID, "null", NOPE, TAB_SETTINGS_MODULES, "The list of modules equipment is empty.");

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	// Language selection
	if(Menulist == MENU_SELECT_LANGUAGE)
	{
		// Save the last vote menu as SETTINGS
		pPlayer->m_LastVoteMenu = MENU_SETTINGS;

		// Display the languages information
		GS()->AVH(ClientID, TAB_INFO_LANGUAGES, "Languages Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_LANGUAGES, "Here you can choose the language.");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_LANGUAGES, "Note: translation is not complete.");
		GS()->AV(ClientID, "null");

		// Get the player's current language
		const char* pPlayerLanguage = pPlayer->GetLanguage();

		// Display the active language
		GS()->AVH(ClientID, TAB_LANGUAGES, "Active language: [{STR}]", pPlayerLanguage);

		// Iterate through each language
		for(int i = 0; i < Server()->Localization()->m_pLanguages.size(); i++)
		{
			// Do not show the language that is already selected by the player in the selection lists
			if(str_comp(pPlayerLanguage, Server()->Localization()->m_pLanguages[i]->GetFilename()) == 0)
				continue;

			// Add language selection
			const char* pLanguageName = Server()->Localization()->m_pLanguages[i]->GetName();
			GS()->AVM(ClientID, "SELECTLANGUAGE", i, TAB_LANGUAGES, "SELECT language \"{STR}\"", pLanguageName);
		}

		// Add the votes backpage
		GS()->AddVotesBackpage(ClientID);

		// Return true to indicate that the code execution is successful
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
	if(PPSTR(CMD, "SELECTLANGUAGE") == 0)
	{
		// Set the client's language to the selected language from the localization object
		const char* pSelectedLanguage = Server()->Localization()->m_pLanguages[VoteID]->GetFilename();
		Server()->SetClientLanguage(ClientID, pSelectedLanguage);

		// Inform the client about the selected language
		GS()->Chat(ClientID, "You have chosen a language \"{STR}\".", pSelectedLanguage);

		// Update the votes menu for the client
		GS()->StrongUpdateVotes(ClientID, MENU_SELECT_LANGUAGE);

		// Save the account's language
		Job()->SaveAccount(pPlayer, SAVE_LANGUAGE);
		return true;
	}

	return false;
}

void CAccountManager::OnResetClient(int ClientID)
{
	CAccountTempData::ms_aPlayerTempData.erase(ClientID);
	CAccountData::ms_aData.erase(ClientID);
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
	str_format(aSelect, sizeof(aSelect), "v.*, IF((SELECT r.ID FROM tw_voucher_redeemed r WHERE CASE v.Multiple WHEN 1 THEN r.VoucherID = v.ID AND r.UserID = %d ELSE r.VoucherID = v.ID END) IS NULL, FALSE, TRUE) AS used", pPlayer->Acc().m_ID);

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
				pPlayer->AddExp(Exp);
			if(Money > 0)
				pPlayer->AddMoney(Money);
			if(Upgrade > 0)
				pPlayer->Acc().m_Upgrade += Upgrade;

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

			GS()->Mmo()->SaveAccount(pPlayer, SAVE_STATS);
			GS()->Mmo()->SaveAccount(pPlayer, SAVE_UPGRADES);

			Database->Execute<DB::INSERT>("tw_voucher_redeemed", "(VoucherID, UserID, TimeCreated) VALUES (%d, %d, %d)", VoucherID, pPlayer->Acc().m_ID, (int)time(0));
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
	ResultPtr pResBan = Database->Execute<DB::SELECT>("BannedUntil", "tw_accounts_bans", "WHERE AccountId = '%d' AND current_timestamp() < `BannedUntil`", pPlayer->Acc().m_ID);
	if(pResBan->next())
	{
		// Print message and return false if the account is already banned
		m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "This account is already banned");
		return false;
	}

	// Ban the account
	Database->Execute<DB::INSERT>("tw_accounts_bans", "(AccountId, BannedUntil, Reason) VALUES (%d, %s, '%s')",
		pPlayer->Acc().m_ID, std::string("current_timestamp + " + Time.asSqlInterval()).c_str(), Reason.c_str());
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
		std::string PlayerNickname = MmoController::PlayerName(AccountID); // Get the player nickname using the AccountID
		std::string Reason = pResBan->getString("Reason").c_str(); // Get the value of the "Reason" column from the current row

		// Create an instance of the AccBan struct with the retrieved values and add it to the vector "out"
		out.push_back({ ID, BannedUntil, std::move(PlayerNickname), std::move(Reason) });
	}

	return out; // Return the vector "out" containing the AccBan structs
}