/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountCore.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Dungeons/DungeonCore.h>
#include <game/server/mmocore/Components/Mails/MailBoxCore.h>
#include <game/server/mmocore/Components/Quests/QuestCore.h>
#include <game/server/mmocore/Components/Worlds/WorldData.h>

#include <base/hash_ctxt.h>

int CAccountCore::GetHistoryLatestCorrectWorldID(CPlayer* pPlayer) const
{
	const auto pWorldIterator = std::find_if(pPlayer->Acc().m_aHistoryWorld.begin(), pPlayer->Acc().m_aHistoryWorld.end(), [=](int WorldID)
	{
		if(GS()->GetWorldData(WorldID))
		{
			CQuestDataInfo* pQuestInfo = GS()->GetWorldData(WorldID)->GetRequiredQuest();
			return !Job()->Dungeon()->IsDungeonWorld(WorldID) && ((pQuestInfo && pPlayer->GetQuest(pQuestInfo->m_QuestID).IsComplected()) || !pQuestInfo);
		}
		return false;
	});
	return pWorldIterator != pPlayer->Acc().m_aHistoryWorld.end() ? *pWorldIterator : MAIN_WORLD_ID;
}

AccountCodeResult CAccountCore::RegisterAccount(int ClientID, const char *Login, const char *Password)
{
	if(str_length(Login) > 12 || str_length(Login) < 4 || str_length(Password) > 12 || str_length(Password) < 4)
	{
		GS()->Chat(ClientID, "Username / Password must contain 4-12 characters");
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS;
	}
	
	const CSqlString<32> cClearNick = CSqlString<32>(Server()->ClientName(ClientID));
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '%s'", cClearNick.cstr());
	if(pRes->next())
	{
		GS()->Chat(ClientID, "- - - - [Your nickname is already registered!] - - - -");
		GS()->Chat(ClientID, "Your game nick is a unique identifier, and it has already been used.");
		GS()->Chat(ClientID, "You can restore access by contacting support, or change nick.");
		GS()->Chat(ClientID, "Discord group \"{STR}\".", g_Config.m_SvDiscordInviteLink);
		return AccountCodeResult::AOP_NICKNAME_ALREADY_EXIST;
	}

	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_accounts", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID")+1 : 1; // thread save ? hm need for table all time auto increment = 1; NEED FIX IT

	const CSqlString<32> cClearLogin = CSqlString<32>(Login);
	const CSqlString<32> cClearPass = CSqlString<32>(Password);

	char aAddrStr[64];
	Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));

	char aSalt[32] = { 0 };
	secure_random_password(aSalt, sizeof(aSalt), 24);

	Database->Execute<DB::INSERT>("tw_accounts", "(ID, Username, Password, PasswordSalt, RegisterDate, RegisteredIP) VALUES ('%d', '%s', '%s', '%s', UTC_TIMESTAMP(), '%s')", InitID, cClearLogin.cstr(), HashPassword(cClearPass.cstr(), aSalt).c_str(), aSalt, aAddrStr);
	Database->Execute<DB::INSERT, 100>("tw_accounts_data", "(ID, Nick) VALUES ('%d', '%s')", InitID, cClearNick.cstr());

	GS()->Chat(ClientID, "- - - - - - - [Successful registered!] - - - - - - -");
	GS()->Chat(ClientID, "Don't forget your data, have a nice game!");
	GS()->Chat(ClientID, "# Your nickname is a unique identifier.");
	GS()->Chat(ClientID, "# Log in: \"/login {STR} {STR}\"", cClearLogin.cstr(), cClearPass.cstr());
	return AccountCodeResult::AOP_REGISTER_OK;
}

AccountCodeResult CAccountCore::LoginAccount(int ClientID, const char *Login, const char *Password)
{
	CPlayer *pPlayer = GS()->GetPlayer(ClientID, false);
	if(!pPlayer)
		return AccountCodeResult::AOP_UNKNOWN;

	const int LengthLogin = str_length(Login);
	const int LengthPassword = str_length(Password);
	if(LengthLogin > 12 || LengthLogin < 4 || LengthPassword > 12 || LengthPassword < 4)
	{
		GS()->Chat(ClientID, "Username / Password must contain 4-12 characters");
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS;
	}

	const CSqlString<32> cClearLogin = CSqlString<32>(Login);
	const CSqlString<32> cClearPass = CSqlString<32>(Password);
	const CSqlString<32> cClearNick = CSqlString<32>(Server()->ClientName(ClientID));
	ResultPtr pResAccount = Database->Execute<DB::SELECT>("*", "tw_accounts_data", "WHERE Nick = '%s'", cClearNick.cstr());
	if(pResAccount->next())
	{
		const int UserID = pResAccount->getInt("ID");
		ResultPtr pResCheck = Database->Execute<DB::SELECT>("ID, LoginDate, Language, Password, PasswordSalt", "tw_accounts", "WHERE Username = '%s' AND ID = '%d'", cClearLogin.cstr(), UserID);

		bool LoginSuccess = false;
		if(pResCheck->next())
		{
			if(!str_comp(pResCheck->getString("Password").c_str(), HashPassword(cClearPass.cstr(), pResCheck->getString("PasswordSalt").c_str()).c_str()))
				LoginSuccess = true;
		}

		if(!LoginSuccess)
		{
			GS()->Chat(ClientID, "Wrong login or password.");
			return AccountCodeResult::AOP_LOGIN_WRONG;
		}

		if (GS()->GetPlayerFromUserID(UserID) != nullptr)
		{
			GS()->Chat(ClientID, "The account is already in the game.");
			return AccountCodeResult::AOP_ALREADY_IN_GAME;
		}

		Server()->SetClientLanguage(ClientID, pResCheck->getString("Language").c_str());
		str_copy(pPlayer->Acc().m_aLogin, cClearLogin.cstr(), sizeof(pPlayer->Acc().m_aLogin));
		str_copy(pPlayer->Acc().m_aLastLogin, pResCheck->getString("LoginDate").c_str(), sizeof(pPlayer->Acc().m_aLastLogin));

		pPlayer->Acc().m_UserID = UserID;
		pPlayer->Acc().m_Level = pResAccount->getInt("Level");
		pPlayer->Acc().m_Exp = pResAccount->getInt("Exp");
		pPlayer->Acc().m_GuildID = pResAccount->getInt("GuildID");
		pPlayer->Acc().m_Upgrade = pResAccount->getInt("Upgrade");
		pPlayer->Acc().m_GuildRank = pResAccount->getInt("GuildRank");
		pPlayer->Acc().m_aHistoryWorld.push_front(pResAccount->getInt("WorldID"));
		for (const auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			if(pAttribute->HasField())
				pPlayer->Acc().m_aStats[ID] = pResAccount->getInt(pAttribute->GetFieldName());
		}

		GS()->Chat(ClientID, "- - - - - - - [Successful login!] - - - - - - -");
		GS()->Chat(ClientID, "Don't forget that cl_motd_time must be set!");
		GS()->Chat(ClientID, "Menu is available in call-votes!");
		GS()->m_pController->DoTeamChange(pPlayer, false);
		LoadAccount(pPlayer, true);

		char aAddrStr[64];
		Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
		Database->Execute<DB::UPDATE>("tw_accounts", "LoginDate = CURRENT_TIMESTAMP, LoginIP = '%s' WHERE ID = '%d'", aAddrStr, UserID);
		return AccountCodeResult::AOP_LOGIN_OK;
	}

	GS()->Chat(ClientID, "Your nickname was not found in the Database.");
	return AccountCodeResult::AOP_NICKNAME_NOT_EXIST;
}

void CAccountCore::LoadAccount(CPlayer *pPlayer, bool FirstInitilize)
{
	if(!pPlayer || !pPlayer->IsAuthed() || !GS()->IsPlayerEqualWorld(pPlayer->GetCID()))
		return;

	const int ClientID = pPlayer->GetCID();
	GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 200, "You are located {STR} ({STR})",
		Server()->GetWorldName(GS()->GetWorldID()), (GS()->IsAllowedPVP() ? "Zone PVP" : "Safe zone"));

	if(!FirstInitilize)
	{
		if (const int Letters = Job()->Inbox()->GetMailLettersSize(pPlayer->Acc().m_UserID); Letters > 0)
			GS()->Chat(ClientID, "You have {INT} unread letters!", Letters);

		GS()->UpdateVotes(ClientID, MenuList::MENU_MAIN);
		return;
	}

	Job()->OnInitAccount(ClientID);
	const int Rank = GetRank(pPlayer->Acc().m_UserID);
	GS()->Chat(-1, "{STR} logged to account. Rank #{INT}", Server()->ClientName(ClientID), Rank);
#ifdef CONF_DISCORD
	char aLoginBuf[64];
	str_format(aLoginBuf, sizeof(aLoginBuf), "%s logged in Account ID %d", Server()->ClientName(ClientID), pPlayer->Acc().m_UserID);
	Server()->SendDiscordGenerateMessage(aLoginBuf, pPlayer->Acc().m_UserID);
#endif

	if (!pPlayer->GetItem(itHammer)->GetValue())
	{
		pPlayer->GetItem(itHammer)->Add(1);
		GS()->Chat(ClientID, "Quest NPCs are marked with an aura Heart and Shield.");
		GS()->Chat(ClientID, "Shield around you indicates location of active quest.");
	}

	// settings
	auto InitializeSettings = [&pPlayer](std::unordered_map<int, int> pItems)
	{
		for(auto& [id, defaultvalue] : pItems)
		{
			if(!pPlayer->GetItem(id)->GetValue())
				pPlayer->GetItem(id)->Add(1, defaultvalue);
			
		}
	};
	InitializeSettings
	({
		{ itModePVP, 1},
		{ itShowEquipmentDescription, 0 },
		{ itShowCriticalDamage, 1 }
	});
	
	// correcting world swapper
	pPlayer->GetTempData().m_TempSafeSpawn = true;
	
	const int LatestCorrectWorldID = GetHistoryLatestCorrectWorldID(pPlayer);
	if(LatestCorrectWorldID != GS()->GetWorldID())
	{
		pPlayer->ChangeWorld(LatestCorrectWorldID);
		return;
	}
}

void CAccountCore::DiscordConnect(int ClientID, const char *pDID) const
{
#ifdef CONF_DISCORD
	CPlayer *pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	const CSqlString<64> cDiscordID = CSqlString<64>(pDID);

	// disable another account if it is connected to this discord
	Database->Execute<DB::UPDATE>("tw_accounts_data", "DiscordID = 'null' WHERE DiscordID = '%s'", cDiscordID.cstr());

	// connect the player discord id
	Database->Execute<DB::UPDATE, 1000>("tw_accounts_data", "DiscordID = '%s' WHERE ID = '%d'", cDiscordID.cstr(), pPlayer->Acc().m_UserID);

	GS()->Chat(ClientID, "Your Discord ID has been updated.");
	GS()->Chat(ClientID, "Check the connection status in discord \"/connect\".");
#endif
}

bool CAccountCore::ChangeNickname(int ClientID)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer || !pPlayer->m_RequestChangeNickname)
		return false;

	// check newnickname
	const CSqlString<32> cClearNick = CSqlString<32>(Server()->GetClientNameChangeRequest(ClientID));
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '%s'", cClearNick.cstr());
	if(pRes->next())
		return false;

	Database->Execute<DB::UPDATE>("tw_accounts_data", "Nick = '%s' WHERE ID = '%d'", cClearNick.cstr(), pPlayer->Acc().m_UserID);
	Server()->SetClientName(ClientID, Server()->GetClientNameChangeRequest(ClientID));
	return true;
}

int CAccountCore::GetRank(int AccountID)
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

bool CAccountCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if (ReplaceMenu)
	{
		return false;
	}

	// settings
	if (Menulist == MENU_SETTINGS)
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
		for (const auto& it : CPlayerItem::Data()[ClientID])
		{
			const CPlayerItem ItemData = it.second;
			if (ItemData.Info()->IsType(ItemType::TYPE_MODULE) && ItemData.GetValue() > 0)
			{
				char aAttributes[128];
				ItemData.StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes));
				GS()->AVM(ClientID, "ISETTINGS", it.first, TAB_SETTINGS_MODULES, "{STR}{STR} * {STR}", (ItemData.GetSettings() ? "✔" : "\0"), ItemData.Info()->GetName(), aAttributes);
				IsFoundModules = true;
			}
		}

		// if no modules are found
		if (!IsFoundModules)
			GS()->AVM(ClientID, "null", NOPE, TAB_SETTINGS_MODULES, "The list of modules equipment is empty.");

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	// language selection
	if (Menulist == MENU_SELECT_LANGUAGE)
	{
		pPlayer->m_LastVoteMenu = MENU_SETTINGS;
		GS()->AVH(ClientID, TAB_INFO_LANGUAGES, "Languages Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_LANGUAGES, "Here you can choose the language.");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_LANGUAGES, "Note: translation is not complete.");
		GS()->AV(ClientID, "null");


		const char* pPlayerLanguage = pPlayer->GetLanguage();
		GS()->AVH(ClientID, TAB_LANGUAGES, "Active language: [{STR}]", pPlayerLanguage);
		for(int i = 0; i < Server()->Localization()->m_pLanguages.size(); i++)
		{
			// do not show the language already selected by the player in the selection lists
			if(str_comp(pPlayerLanguage, Server()->Localization()->m_pLanguages[i]->GetFilename()) == 0)
				continue;

			// add language selection
			const char *pLanguageName = Server()->Localization()->m_pLanguages[i]->GetName();
			GS()->AVM(ClientID, "SELECTLANGUAGE", i, TAB_LANGUAGES, "SELECT language \"{STR}\"", pLanguageName);
		}
		GS()->AddVotesBackpage(ClientID);
		return true;
	}
	return false;
}

bool CAccountCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if (PPSTR(CMD, "SELECTLANGUAGE") == 0)
	{
		const char *pSelectedLanguage = Server()->Localization()->m_pLanguages[VoteID]->GetFilename();
		Server()->SetClientLanguage(ClientID, pSelectedLanguage);
		GS()->Chat(ClientID, "You chosen a language \"{STR}\".", pSelectedLanguage);
		GS()->StrongUpdateVotes(ClientID, MenuList::MENU_SELECT_LANGUAGE);
		Job()->SaveAccount(pPlayer, SaveType::SAVE_LANGUAGE);
		return true;
	}
	return false;
}

void CAccountCore::OnResetClient(int ClientID)
{
	CAccountTempData::ms_aPlayerTempData.erase(ClientID);
	CAccountData::ms_aData.erase(ClientID);
}

std::string CAccountCore::HashPassword(const char* pPassword, const char* pSalt)
{
	char aPlaintext[128] = { 0 };
	SHA256_CTX Sha256Ctx;
	sha256_init(&Sha256Ctx);
	str_format(aPlaintext, sizeof(aPlaintext), "%s%s%s", pSalt, pPassword, pSalt);
	sha256_update(&Sha256Ctx, aPlaintext, str_length(aPlaintext));
	const SHA256_DIGEST Digest = sha256_finish(&Sha256Ctx);

	char aHash[SHA256_MAXSTRSIZE];
	sha256_str(Digest, aHash, sizeof(aHash));
	return std::string(aHash);
}

void CAccountCore::UseVoucher(int ClientID, const char* pVoucher) const
{
	CPlayer* pPlayer = GS()->m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->IsAuthed())
		return;

	char aSelect[256];
	const CSqlString<32> cVoucherCode = CSqlString<32>(pVoucher);
	str_format(aSelect, sizeof(aSelect), "v.*, IF((SELECT r.ID FROM tw_voucher_redeemed r WHERE CASE v.Multiple WHEN 1 THEN r.VoucherID = v.ID AND r.UserID = %d ELSE r.VoucherID = v.ID END) IS NULL, FALSE, TRUE) AS used", pPlayer->Acc().m_UserID);

	ResultPtr pResVoucher = Database->Execute<DB::SELECT>(aSelect, "tw_voucher v", "WHERE v.Code = '%s'", cVoucherCode.cstr());
	if (pResVoucher->next())
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

					if(ItemID > NOPE && Value > 0)
					{
						if(CItemDescription::Data().find(ItemID) != CItemDescription::Data().end())
							pPlayer->GetItem(ItemID)->Add(Value);
					}
				}
			}

			GS()->Mmo()->SaveAccount(pPlayer, SaveType::SAVE_STATS);
			GS()->Mmo()->SaveAccount(pPlayer, SaveType::SAVE_UPGRADES);

			Database->Execute<DB::INSERT>("tw_voucher_redeemed", "(VoucherID, UserID, TimeCreated) VALUES (%d, %d, %d)", VoucherID, pPlayer->Acc().m_UserID, (int)time(0));
			GS()->Chat(ClientID, "You have successfully redeemed the voucher '{STR}'.", pVoucher);
		}

		return;
	}

	GS()->Chat(ClientID, "The voucher code '{STR}' does not exist.", pVoucher);
}
