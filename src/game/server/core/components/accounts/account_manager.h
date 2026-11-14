/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#define GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#include <game/server/core/mmo_component.h>
#include <base/timeperiod.h>

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
	// Reservation of an account session row plus the owner it replaced.
	// The row (tw_account_sessions) is the single source of truth: the session
	// whose token is stored there is the only one allowed to play.
	struct SSessionClaim
	{
		int m_AccountID {};
		std::string m_Token {};
		int m_PreviousServerID {}; // 0: there was no previous owner
		int m_PreviousClientID {};
		std::string m_PreviousToken {};
		int m_PreviousHeartbeat {}; // 0: the previous login never finished
		int m_PreviousHeartbeatAge { -1 }; // seconds since its last heartbeat, -1 if unknown
		int64_t m_ClaimDeadline {}; // time_get() limit for the whole login
	};

	AccountCodeResult RegisterAccount(int ClientID, const char *pLogin, const char *pPassword);
	AccountCodeResult RegisterAccountRaw(int ClientID, const char* pLogin, const char* pPassword);
	AccountCodeResult RegisterGuestAccount(int ClientID, const char* pNickname);
	AccountCodeResult LoginAccount(int ClientID, const char *pLogin, const char *pPassword);
	AccountCodeResult LoginAccountRaw(int ClientID, const char* pLogin, const char* pPassword);

	void BeginSessionTakeover(CPlayer* pPlayer, const SSessionClaim& Claim, bool RecommendPin);
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
	// A session of this server's client slots, captured when a check is issued.
	struct SLocalSession
	{
		int m_ClientID {};
		int m_AccountID {};
		std::string m_Token {};
	};

	// Acknowledgement of a takeover: sent once the local sessions were checked.
	struct SSessionAck
	{
		int m_EventID {};
		int m_WorldID {};
		int m_TargetServerID {};
	};

	struct SPendingLogin
	{
		int m_ClientID {};
		SSessionClaim m_Claim {};
		bool m_RecommendPin {};
		int m_EventID {}; // takeover request sent to the previous owner's server, 0 if none
		bool m_RelayFailed {};
		bool m_RemoteAck {};
		int64_t m_StartTime {};
		int64_t m_TakeoverDeadline {};
	};

	void ProcessPendingLogins();
	bool CompleteLogin(int ClientID, const SSessionClaim& Claim, bool RecommendPin);
	void FailPendingLogin(const SPendingLogin& Pending, bool RelayFailed);
	void UpdateLocalSessionHeartbeats();

	std::vector<SLocalSession> CollectLocalSessions(int AccountID = 0) const;
	void KickLocalSession(const SLocalSession& Session);
	void KickOtherLocalSessions(int AccountID, const std::string& Token);
	void VerifyLocalSessions(std::vector<SLocalSession> vSessions, const SSessionAck* pAck = nullptr);
	bool HasOtherLocalSession(int AccountID, const std::string& Token) const;
	void AddMenuProfessionUpgrades(CPlayer* pPlayer, CProfession* pProf) const;

	std::vector<SPendingLogin> m_vPendingLogins {};
	int64_t m_NextSessionCheck = 0;
};

#endif
