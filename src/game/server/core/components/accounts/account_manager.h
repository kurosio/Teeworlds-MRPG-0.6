/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#define GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#include <game/server/core/mmo_component.h>
#include <base/timeperiod.h>

#include <mutex>
#include <string>
#include <vector>

#include "account_data.h"

class CAccountManager : public MmoComponent
{
	~CAccountManager() override
	{
		// free data
		mystd::freeContainer(CAccountData::ms_aData, CAccountSharedData::ms_aPlayerSharedData);
	}

	void OnPlayerLogin(CPlayer* pPlayer) override;
	void OnTick() override;
	void OnSyncEvent(std::string EventName, const CServerSync::SRequest& Req) override;
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
	AccountCodeResult RegisterAccountRaw(int ClientID, const char* pLogin, const char* pPassword);
	AccountCodeResult RegisterGuestAccount(int ClientID, const char* pNickname);
	AccountCodeResult LoginAccount(int ClientID, const char *pLogin, const char *pPassword);
	AccountCodeResult LoginAccountRaw(int ClientID, const char* pLogin, const char* pPassword);

	void ValidateLocalSessions(int AccountID, int AckEventID = 0, int AckWorldID = 0, int AckTargetServerID = 0);
	void BeginSessionTakeover(CPlayer* pPlayer, const std::string& Token, int PreviousServerID, int PreviousClientID, const std::string& PreviousToken, int PreviousHeartbeat, int PreviousHeartbeatAge, bool RecommendPin, int64_t ClaimDeadline);
	void LoadAccount(CPlayer *pPlayer, bool FirstInitilize = false, int64_t ClaimDeadline = 0);
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

	static std::string BuildGuestCredential(const char* pNickname);
	static bool IsGuestCredentialForNickname(std::string_view Login, const char* pNickname);
	static std::string HashPassword(const std::string& Password, const std::string& Salt);

	void UseVoucher(int ClientID, const char* pVoucher) const;
	void ShowPlayerAttributesByGroup(CPlayer* pPlayer, std::string_view Title, AttributeGroup Group) const;

private:
	struct SPendingKick
	{
		int m_ClientID {};
		int m_AccountID {};
		std::string m_Token {};
	};

	struct SSessionValidation
	{
		int m_AccountID {};
		std::string m_Token {};
		int m_AckEventID {};
		int m_AckWorldID {};
		int m_AckTargetServerID {};
	};

	struct SPendingLogin
	{
		int m_ClientID {};
		int m_AccountID {};
		std::string m_Token {};
		int m_PreviousServerID {};
		int m_PreviousClientID {};
		std::string m_PreviousToken {};
		int m_PreviousHeartbeat {};
		int m_PreviousHeartbeatAge{ -1 };
		int m_EventID {};
		int m_StartTick {};
		int64_t m_ClaimDeadline {};
		bool m_RemoteAck {};
		bool m_RelayFailed {};
		bool m_RecommendPin {};
	};

	void ProcessPendingKicks();
	void UpdateLocalSessionHeartbeats();
	void SendSessionEvents();
	void ProcessPendingLogins();
	bool CompleteLogin(int ClientID, int AccountID, const std::string& Token, bool RecommendPin, int PreviousServerID, int64_t ClaimDeadline);
	void RestorePreviousSession(const SPendingLogin& Pending);
	void QueueLocalSessionKicks(int AccountID, const std::string& Token);
	bool HasAnyLocalSession(int AccountID) const;
	bool HasOtherLocalSession(int AccountID, const std::string& Token) const;
	void AddMenuProfessionUpgrades(CPlayer* pPlayer, CProfession* pProf) const;

	std::vector<SPendingKick> m_vPendingKicks {};
	std::vector<SSessionValidation> m_vSessionValidations {};
	std::vector<SPendingLogin> m_vPendingLogins {};
	std::vector<int> m_vSessionEvents {};
	std::mutex m_SessionMutex {};
	int m_LastSessionCheck = 0;
};

#endif