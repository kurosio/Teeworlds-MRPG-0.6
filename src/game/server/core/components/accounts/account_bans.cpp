/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_bans.h"
#include "account_db.h"

#include <game/server/gamecontext.h>
#include <game/server/core/tools/db_async_context.h>

namespace AccountBans
{
	void Ban(CPlayer* pPlayer, CTimePeriod Time, const std::string& Reason)
	{
		if(!pPlayer || !pPlayer->Account())
			return;

		// The ban is written unless one is active already; the player is removed in
		// the answer, on the game thread.
		const int AccountID = pPlayer->Account()->GetID();
		const int ClientID = pPlayer->GetCID();
		const int64_t DurationSeconds = Time.duration().count();
		auto pContext = DbAsync::MakeContext(ClientID);
		AccountDb::ReadActiveBan(AccountID)
			.Then([pContext, AccountID, ClientID, DurationSeconds, Reason](Db::CResultSet&& ActiveBan)
		{
			auto* pGS = pContext->GS();
			if(!pGS)
				return;
			auto* pConsole = pGS->Console();
			if(!ActiveBan.Ok())
			{
				pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Could not check the account's current ban");
				return;
			}
			if(ActiveBan.HasRows())
			{
				pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "This account is already banned");
				return;
			}

			AccountDb::CreateBan(AccountID, DurationSeconds, Reason).Then([pContext, ClientID](Db::CResultSet&& Created)
			{
				auto* pGS = pContext->GS();
				if(!pGS)
					return;
				if(!Created.Ok() || !Created.Changed())
				{
					pGS->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "The ban could not be saved");
					return;
				}

				pContext->Server()->Kick(ClientID, "Your account was banned");
				pGS->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Successfully banned!");
			});
		});
	}

	void Unban(int BanId)
	{
		// Search for the ban and lift it in the answer.
		AccountDb::ReadActiveBanByID(BanId)
			.Then([BanId](Db::CResultSet&& ActiveBan)
		{
			auto* pConsole = static_cast<CGS*>(Instance::GameServer(INITIALIZER_WORLD_ID))->Console();
			if(!ActiveBan.HasRows())
			{
				// the ban does not exist or has already run out
				pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Ban is not valid anymore or does not exist!");
				return;
			}

			AccountDb::LiftBan(BanId);
			pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Successfully unbanned!");
		});
	}

	void List(std::function<void(std::vector<SBan>&&)> OnLoaded)
	{
		AccountDb::ListActiveBans()
			.Then([OnLoaded = std::move(OnLoaded)](Db::CResultSet&& Bans)
		{
			std::vector<SBan> vBans;
			vBans.reserve(100);
			while(Bans.Next())
			{
				int ID = Bans.GetInt("Id");
				int AccountID = Bans.GetInt("AccountId");
				std::string BannedUntil = Bans.GetString("BannedUntil");
				std::string PlayerNickname = Instance::Server()->GetAccountNickname(AccountID);
				std::string Reason = Bans.GetString("Reason");
				vBans.emplace_back(ID, BannedUntil, std::move(PlayerNickname), std::move(Reason));
			}

			if(OnLoaded)
				OnLoaded(std::move(vBans));
		});
	}
}
