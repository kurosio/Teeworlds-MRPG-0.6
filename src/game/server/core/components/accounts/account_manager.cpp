/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_manager.h"
#include "account_session.h"

#include <base/system.h>
#include <game/server/gamecontext.h>
#include <generated/server_data.h>

#include <game/server/core/components/inventory/inventory_manager.h>
#include <game/server/core/components/mails/mailbox_manager.h>
#include <game/server/core/components/worlds/world_data.h>
#include <game/server/core/tools/db_async_context.h>

#include <algorithm>

void CAccountManager::OnPlayerLogin(CPlayer* pPlayer)
{
	if(!pPlayer || !pPlayer->Account())
		return;

	// select first random profession by system
	auto* pAccount = pPlayer->Account();
	if(pAccount->GetActiveProfessionID() == ProfessionIdentifier::None)
	{
		ChanceProcessor<ProfessionIdentifier> process;
		process.addElement(ProfessionIdentifier::Tank, 100.f);
		process.addElement(ProfessionIdentifier::Healer, 100.f);
		process.addElement(ProfessionIdentifier::Dps, 100.f);
		process.setEqualChance(100.f);
		process.normalizeChances();
		pAccount->ChangeProfession(process.getRandomElement());
		GS()->Chat(pPlayer->GetCID(), "Your profession has been chosen by the system. You can change it at any time in the voting menu.");
	}
}

void CAccountManager::OnTick()
{
	// Server-wide session work runs only in the initializer world.
	if(GS()->GetWorldID() != INITIALIZER_WORLD_ID)
		return;

	AccountSession::Tick();
}

void CAccountManager::OnSyncEvent(std::string EventName, const CServerSync::SRequest& Req)
{
	AccountSession::HandleSyncEvent(EventName, Req);
}

void CAccountManager::OnClientReset(int ClientID)
{
	if(const auto It = CAccountData::ms_aData.find(ClientID); It != CAccountData::ms_aData.end())
		It->second.CancelLoad();

	// Unfinished logins give their reservations back; the active session row is
	// removed when it still carries this client's token.
	AccountSession::OnClientReset(ClientID);

	CAccountSharedData::ms_aPlayerSharedData.erase(ClientID);
	CAccountData::ms_aData.erase(ClientID);
}

bool CAccountManager::LoadAccount(CPlayer* pPlayer, bool FirstInitialize, int64_t ClaimDeadline, std::function<void(bool)> OnLoaded)
{
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	// The client may already be in another world: the manager of that world owns
	// the player object the answers have to be applied to.
	if(!GS()->IsPlayerInWorld(pPlayer->GetCID()))
	{
		auto* pActiveGS = static_cast<CGS*>(Instance::GameServerPlayer(pPlayer->GetCID()));
		auto* pActivePlayer = pActiveGS ? pActiveGS->GetPlayer(pPlayer->GetCID(), false) : nullptr;
		if(!pActivePlayer)
			return false;

		return pActiveGS->Core()->AccountManager()->LoadAccount(pActivePlayer, FirstInitialize, ClaimDeadline, std::move(OnLoaded));
	}

	// Broadcast a message to the player with their current location
	auto* pAccount = pPlayer->Account();
	const int ClientID = pPlayer->GetCID();
	const auto RateExp = 100 + round_to_int(GS()->m_Multipliers.GetSourcePercent(MultiplierType::Experience, MultiplierSource::World));
	const auto RateGold = 100 + round_to_int(GS()->m_Multipliers.GetSourcePercent(MultiplierType::Gold, MultiplierSource::World));
	GS()->Broadcast(ClientID, BroadcastPriority::VeryImportant, 300, "You are currently positioned at {}({})!\n- World rates: Exp {}% | Gold {}%",
		Server()->GetWorldName(GS()->GetWorldID()), (GS()->IsAllowedPVP() ? "PVE/PVP" : "PVE"), RateExp, RateGold);

	// Check if it is not the first initialization
	if(!FirstInitialize)
	{
		Core()->MailboxManager()->NotifyUnreadMails(pPlayer);
		pAccount->GetBonusManager().SendInfoAboutActiveBonuses();
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
		if(OnLoaded)
			OnLoaded(true);
		return true;
	}

	// Collect account-owned and component-owned data work before starting the
	// plan. Login hooks remain separate from stage registration; independent
	// reads still run concurrently, behind one readiness barrier.
	auto pContext = DbAsync::MakeContext(ClientID);
	auto* pManager = this;
	pAccount->RegisterLoadStages();
	Core()->RegisterAccountLoadStages(pPlayer);
	Core()->OnPlayerLogin(pPlayer);
	pAccount->RunLoadStages([pContext, pManager, ClaimDeadline, OnLoaded = std::move(OnLoaded)](bool Success) mutable
	{
		auto* pPlayer = pContext->GetPlayer();
		if(!Success || !pPlayer)
		{
			if(OnLoaded)
				OnLoaded(false);
			return;
		}

		pManager->FinishAccountLoad(pPlayer, ClaimDeadline);
		if(OnLoaded)
			OnLoaded(true);
	});
	return true;
}

void CAccountManager::FinishAccountLoad(CPlayer* pPlayer, int64_t ClaimDeadline)
{
	auto* pAccount = pPlayer->Account();
	const int ClientID = pPlayer->GetCID();

	// The parts of the account are all in, so the expired ones can go now: the
	// removal clears the equipment slot that still names such an item, which is
	// only possible once the professions of the account are loaded.
	Core()->InventoryManager()->RemoveExpiredItems(pPlayer);

	// initialize default item's & settings
	auto InitSettingsItem = [pPlayer](const std::unordered_map<int, int>& pItems)
	{
		for(auto& [id, defaultValue] : pItems)
		{
			if(auto pItem = pPlayer->GetItem(id))
			{
				if(!pItem->HasItem())
					pItem->Add(1, defaultValue, 0, false);
			}
		}
	};

	InitSettingsItem(
		{
			{ itHammer, 1 },
			{ itShowEquipmentDescription, 0 },
			{ itShowCriticalDamage, 1 },
			{ itShowQuestStarNavigator, 1 },
			{ itShowDetailGainMessages, 0 },
			{ itShowOnlyFunctionModules, 1 },
			{ itShowItemSourceHint, 0 },
		});

	// update player time periods
	Core()->OnHandlePlayerTimePeriod(pPlayer);
	if(ClaimDeadline && time_get() >= ClaimDeadline)
	{
		Server()->Kick(ClientID, "Account session reservation expired");
		return;
	}
	pAccount->SetReadyToPlay(true);

	// the rating belongs to the loaded account, so the client is updated with it
	Server()->UpdateAccountBase(pAccount->GetID(), Server()->ClientName(ClientID), pAccount->GetRatingSystem().GetRating());

	// notify about rank
	const int Rank = Server()->GetAccountRank(pAccount->GetID());
	GS()->Chat(-1, "'{~}' logged to account. Rank '#{}[{~}]' ({~})", Server()->ClientName(ClientID), Rank,
		Server()->ClientCountryIsoCode(ClientID), pPlayer->Account()->GetRatingSystem().GetRankName());

	// Change player's world ID to the latest correct world ID
	const int LatestCorrectWorldID = GetLastVisitedWorldID(pPlayer);
	if(LatestCorrectWorldID != GS()->GetWorldID())
	{
		pPlayer->ChangeWorld(LatestCorrectWorldID);
		return;
	}
}

int CAccountManager::GetLastVisitedWorldID(CPlayer* pPlayer) const
{
	const auto pWorldIterator = std::ranges::find_if(pPlayer->Account()->m_aHistoryWorld, [&](int WorldID)
	{
		if(GS()->GetWorldData(WorldID))
		{
			int RequiredLevel = GS()->GetWorldData(WorldID)->GetRequiredLevel();
			return Server()->IsWorldType(WorldID, WorldType::Default) && pPlayer->Account()->GetLevel() >= RequiredLevel;
		}
		return false;
	});
	return pWorldIterator != pPlayer->Account()->m_aHistoryWorld.end() ? *pWorldIterator : g_Config.m_SvShowWorldWhenConnect;
}
