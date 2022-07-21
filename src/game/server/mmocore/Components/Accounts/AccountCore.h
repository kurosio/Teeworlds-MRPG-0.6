/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#define GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#include <game/server/mmocore/MmoComponent.h>
#include <generated/protocol.h>

#include "AccountData.h"

class CAccountCore : public MmoComponent
{
	~CAccountCore() override
	{
		CAccountData::ms_aData.clear();
		CAccountTempData::ms_aPlayerTempData.clear();
	};

	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	void OnResetClient(int ClientID) override;
	void OnMessage(int MsgID, void* pRawMsg, int ClientID) override;

public:
	void SendAccountCodeResult(int ClientID, AccountCodeResult Code) const;
	AccountCodeResult RegisterAccount(int ClientID, const char *Login, const char *Password);
	AccountCodeResult LoginAccount(int ClientID, const char *Login, const char *Password);
	void LoadAccount(CPlayer *pPlayer, bool FirstInitilize = false);
	void DiscordConnect(int ClientID, const char *pDID) const;

	int GetHistoryLatestCorrectWorldID(CPlayer* pPlayer) const;
	
	static int GetRank(int AccountID);
	static bool IsActive(int ClientID)
	{
		return CAccountData::ms_aData.find(ClientID) != CAccountData::ms_aData.end();
	}

	static std::string HashPassword(const char* pPassword, const char* pSalt);
	void UseVoucher(int ClientID, const char* pVoucher) const;
};

#endif