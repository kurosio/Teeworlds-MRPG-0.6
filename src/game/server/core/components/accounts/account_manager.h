/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#define GAME_SERVER_COMPONENT_ACCOUNT_MAIN_CORE_H
#include <game/server/core/mmo_component.h>
#include <base/timeperiod.h>

#include <functional>
#include <string>
#include <vector>

#include "account_data.h"
#include "account_session.h"

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

	// Starts a login whose claim on the account has been written.
	void BeginSessionTakeover(CPlayer* pPlayer, const AccountSession::SClaim& Claim, bool RecommendPin);
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
	// A session logged in on this server.
	struct SLocalSession
	{
		int m_ClientID {};
		AccountSession::SKey m_Key {};
	};

	struct SPendingLogin
	{
		int m_ClientID {};
		AccountSession::SClaim m_Claim {};
		bool m_RecommendPin {};
		int m_EventID {}; // takeover request to the previous owner's server, 0 if none
		bool m_RelayFailed {};
		bool m_RemoteAck {};
		int64_t m_StartTime {};
	};

	enum class EPendingState
	{
		Wait, // keep waiting
		Ready, // complete the login
		Failed, // reject the login, keep the previous owner
		Abandoned, // the player is gone without OnClientReset
	};

	EPendingState EvaluatePendingLogin(const SPendingLogin& Pending, int64_t Now) const;
	void ProcessPendingLogins();
	bool CompleteLogin(int ClientID, const AccountSession::SClaim& Claim, bool RecommendPin);
	void FailPendingLogin(const SPendingLogin& Pending);

	std::vector<SLocalSession> CollectLocalSessions(int AccountID = 0) const;
	bool HasOtherLocalSession(int AccountID, const std::string& Token) const;
	void KickLocalSession(const SLocalSession& Session);
	void KickOtherLocalSessions(int AccountID, const std::string& Token);
	void VerifySessions(std::vector<SLocalSession> vSessions, std::function<void()> OnVerified = {});

	void AddMenuProfessionUpgrades(CPlayer* pPlayer, CProfession* pProf) const;

	std::vector<SPendingLogin> m_vPendingLogins {};
	int64_t m_NextSessionCheck = 0;
};

#endif
