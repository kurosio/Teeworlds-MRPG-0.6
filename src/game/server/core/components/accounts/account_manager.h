/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_MANAGER_H
#include <game/server/core/mmo_component.h>

#include <any>
#include <cstdint>
#include <functional>
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

	// menus, implemented in account_menus.cpp
	void OnCharacterTile(CCharacter* pChr) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason) override;
	bool OnSendMenuMotd(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd) override;

public:
	// Starts the account load. It is staged and asynchronous: everything the
	// account needs is requested at once, OnLoaded runs on the game thread when
	// the last answer is in and the account is playable.
	//
	// Returns false when the load could not be started at all - the player is
	// gone, not authenticated, or not in a world - because then OnLoaded will
	// never run and the caller has to end the login instead of waiting for it.
	bool LoadAccount(CPlayer* pPlayer, bool FirstInitialize = false, int64_t ClaimDeadline = 0, std::function<void(bool)> OnLoaded = {});

	int GetLastVisitedWorldID(CPlayer* pPlayer) const;

	static bool IsActive(int ClientID)
	{
		return CAccountData::ms_aData.contains(ClientID);
	}

private:
	// Runs the part of the login that needs every loaded account field.
	void FinishAccountLoad(CPlayer* pPlayer, int64_t ClaimDeadline);

	// menu helpers, implemented in account_menus.cpp
	void AddMenuProfessionUpgrades(CPlayer* pPlayer, CProfession* pProf) const;
	void ShowPlayerAttributesByGroup(CPlayer* pPlayer, std::string_view Title, AttributeGroup Group) const;
};

#endif
