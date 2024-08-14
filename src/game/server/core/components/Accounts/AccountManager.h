/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#define GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#include <game/server/core/mmo_component.h>
#include <game/server/core/tools/timeperiod_data.h>

#include "AccountData.h"

class CAccountManager : public MmoComponent
{
	~CAccountManager() override
	{
		// free data
		mrpgstd::free_container(CAccountData::ms_aData, CAccountTempData::ms_aPlayerTempData);
	}

	void OnClientReset(int ClientID) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;
	void OnPlayerTimePeriod(CPlayer* pPlayer, ETimePeriod Period) override;
	void OnCharacterTile(CCharacter* pChr) override;

	bool OnSendMenuMotd(CPlayer* pPlayer, int Menulist) override;

    struct AccBan
    {
        int id;
        std::string until;
        std::string nickname;
        std::string reason;
    };

public:
	AccountCodeResult RegisterAccount(int ClientID, const char *Login, const char *Password);
	AccountCodeResult LoginAccount(int ClientID, const char *pLogin, const char *pPassword);
	void LoadAccount(CPlayer *pPlayer, bool FirstInitilize = false);
	void DiscordConnect(int ClientID, const char *pDID) const;
	bool ChangeNickname(const std::string& newNickname, int ClientID) const;
    bool BanAccount(CPlayer* pPlayer, TimePeriodData Time, const std::string& Reason);
    bool UnBanAccount(int BanId) const;
    std::vector<AccBan> BansAccount() const;

	int GetLastVisitedWorldID(CPlayer* pPlayer) const;
	
	static int GetRank(int AccountID);
	static bool IsActive(int ClientID)
	{
		return CAccountData::ms_aData.contains(ClientID);
	}

	static std::string HashPassword(const std::string& Password, const std::string& Salt);
	void UseVoucher(int ClientID, const char* pVoucher) const;
};

#endif