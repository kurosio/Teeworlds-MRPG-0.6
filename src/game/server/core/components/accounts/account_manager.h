/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#define GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#include <game/server/core/mmo_component.h>
#include <base/timeperiod.h>

#include "account_data.h"

class CAccountManager : public MmoComponent
{
	~CAccountManager() override
	{
		// free data
		mystd::freeContainer(CAccountData::ms_aData, CAccountSharedData::ms_aPlayerSharedData);
	}

	void OnPlayerLogin(CPlayer* pPlayer) override;
	void OnClientReset(int ClientID) override;
	void OnCharacterTile(CCharacter* pChr) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason) override;
	bool OnSendMenuMotd(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd) override;

	struct AccBan
	{
		int id;
		std::string until;
		std::string nickname;
		std::string reason;
	};

public:
	AccountCodeResult RegisterAccount(int ClientID, const char *pLogin, const char *pPassword);
	AccountCodeResult LoginAccount(int ClientID, const char *pLogin, const char *pPassword);
	void LoadAccount(CPlayer *pPlayer, bool FirstInitilize = false);
	void SetPinCode(int ClientID, const char* pCurrentPasswordOrPin, const char* pNewPin, bool IsChangingPin);
	void ChangePassword(int ClientID, const char* pOldPassword, const char* pNewPassword, const char* pPinCode);
	bool ChangeNickname(const std::string& newNickname, int ClientID) const;
	bool BanAccount(CPlayer* pPlayer, CTimePeriod Time, const std::string& Reason);
	bool UnBanAccount(int BanId) const;

	int GetLastVisitedWorldID(CPlayer* pPlayer) const;
	bool GetAccountPin(int AccountID, std::string& OutPinCode) const;
	std::vector<AccBan> BansAccount() const;

	static bool IsActive(int ClientID)
	{
		return CAccountData::ms_aData.contains(ClientID);
	}

	static std::string HashPassword(const std::string& Password, const std::string& Salt);
	void UseVoucher(int ClientID, const char* pVoucher) const;
	void ShowPlayerAttributesByGroup(CPlayer* pPlayer, std::string_view Title, AttributeGroup Group) const;

private:
	void AddMenuProfessionUpgrades(CPlayer* pPlayer, CProfession* pProf) const;
};

#endif