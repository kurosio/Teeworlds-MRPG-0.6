/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_MANAGER_H
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

public:
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
	// Starts the account load. It is staged and asynchronous: everything the
	// account needs is requested at once, OnLoaded runs on the game thread when
	// the last answer is in and the account is playable.
	//
	// Returns false when the load could not be started at all - the player is
	// gone, not authenticated, or not in a world - because then OnLoaded will
	// never run and the caller has to end the login instead of waiting for it.
	bool LoadAccount(CPlayer* pPlayer, bool FirstInitilize = false, int64_t ClaimDeadline = 0, std::function<void(bool)> OnLoaded = {});
	void SetPinCode(int ClientID, const char* pCurrentPasswordOrPin, const char* pNewPin, bool IsChangingPin);
	void ChangePassword(int ClientID, const char* pOldPassword, const char* pNewPassword, const char* pPinCode);
	// Asynchronous commands: the answer (a chat message or an rcon line) is the
	// only feedback, so they return nothing and never wait for the database.
	void ChangeNickname(std::string NewNickname, int ClientID);
	void BanAccount(CPlayer* pPlayer, CTimePeriod Time, const std::string& Reason);
	void UnBanAccount(int BanId) const;
	void BansAccount(std::function<void(std::vector<AccBan>&&)> OnLoaded) const;

	// Runs the part of the login that needs every loaded account field.
	void FinishAccountLoad(CPlayer* pPlayer, int64_t ClaimDeadline);
	// Gives a claim back and ends the connection that waited for it.
	void AbortSessionClaim(int ClientID, const AccountSession::SClaim& Claim);

	int GetLastVisitedWorldID(CPlayer* pPlayer) const;

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

	// A login whose account load has been started: the answer of that load is the
	// last thing a login waits for, so it is watched like the pending stage.
	struct SLoadingLogin
	{
		int m_ClientID {};
		AccountSession::SClaim m_Claim {};
		int64_t m_StartedAt {};
	};

	// Forgets the load of a client and answers how long it took, 0 if it was not
	// watched (already finished, reset or given up).
	int64_t AckLoadingLogin(int ClientID);
	// Ends the logins whose account load never answered.
	void SweepLoadingLogins();

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
	std::vector<SLoadingLogin> m_vLoadingLogins {};
	int64_t m_NextSessionCheck = 0;
};

#endif
