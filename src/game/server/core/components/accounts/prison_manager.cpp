#include "prison_manager.h"

#include <engine/server/async_file.h>
#include <game/server/core/tools/db_async_context.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/core/components/worlds/world_data.h>

inline static std::string getFileName(int AccountID)
{
	return fmt_default("server_data/account_prison/{}.txt", AccountID);
}

CGS* PrisonManager::GS() const
{
	return (CGS*)Instance::GameServerPlayer(m_ClientID);
}

CPlayer* PrisonManager::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

void PrisonManager::Imprison(int Seconds)
{
	m_PrisonTerm.ImprisonmentTime = Seconds;
	m_PrisonTerm.StartTime = time(nullptr);

	vec2 PrisonPos;
	if(GS()->m_pController->CanSpawn(SPAWN_HUMAN_PRISON, &PrisonPos))
	{
		GS()->Chat(-1, "'{~}', has been imprisoned for '{} seconds'.", GS()->Server()->ClientName(m_ClientID), Seconds);

		const auto* pPlayer = GetPlayer();
		if(pPlayer && pPlayer->GetCharacter())
		{
			pPlayer->GetCharacter()->ChangePosition(PrisonPos);
		}
	}

	Save();
}

void PrisonManager::Release()
{
	m_PrisonTerm.ImprisonmentTime = 0;
	m_PrisonTerm.StartTime = 0;

	if(const auto* pPlayer = GetPlayer())
	{
		const auto& vQuestList = CQuestDescription::Data();
		if(vQuestList.contains(g_Config.m_SvRelatedJailQuestId))
			pPlayer->GetQuest(g_Config.m_SvRelatedJailQuestId)->Refuse();

		if(pPlayer->GetCharacter())
			pPlayer->GetCharacter()->Die(m_ClientID, WEAPON_WORLD);

		// the prison file is removed in the background, after any queued save
		const std::string Filename = getFileName(pPlayer->Account()->GetID());
		AsyncFile::Remove(GS()->Storage(), Filename.c_str(), IStorageEngine::TYPE_ABSOLUTE);
		GS()->Chat(-1, "'{~}' has been released from prison.", GS()->Server()->ClientName(m_ClientID));
	}
}

void PrisonManager::PostTick()
{
	if(!m_PrisonTerm.IsActive())
		return;

	auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	// check valid world id
	const int JailWorldID = GS()->GetWorldData()->GetJailWorld()->GetID();
	if(pPlayer->GetCurrentWorldID() != JailWorldID)
	{
		pPlayer->ChangeWorld(JailWorldID);
		return;
	}

	// check valid position
	if(!pPlayer->GetCharacter()->GetTiles()->IsActive(TILE_JAIL_ZONE))
	{
		const vec2 JailPosition = GS()->GetJailPosition();
		pPlayer->GetCharacter()->ChangePosition(JailPosition);
		GS()->Chat(m_ClientID, "You cannot leave the prison!");
	}

	// special try reduce jail time
	auto* pCigarettes = pPlayer->GetItem(itPackCigarettes);
	if(pCigarettes->HasItem())
	{
		int Value = pCigarettes->GetValue();
		GS()->Chat(m_ClientID, "Your time in prison has been reduced.");
		m_PrisonTerm.ImprisonmentTime -= (Value * 30);
		pCigarettes->Remove(Value);
		Save();
	}

	// check remaining time
	const int Remaining = m_PrisonTerm.RemainingTime();
	if((Remaining - 1) <= 0)
	{
		Release();
		return;
	}

	// send information
	GS()->Broadcast(m_ClientID, BroadcastPriority::TitleInformation, 50, "Remaining prison time: {} seconds.", Remaining);
}

void PrisonManager::RegisterLoadStage()
{
	auto* pGS = GS();
	const auto* pPlayer = pGS ? pGS->GetPlayer(m_ClientID) : nullptr;
	if(!pPlayer || !pPlayer->Account())
		return;

	const int ClientID = m_ClientID;
	const std::string Filename = getFileName(pPlayer->Account()->GetID());
	IStorageEngine* pStorage = pGS->Storage();
	pPlayer->Account()->AddLoadStage("prison", [ClientID, Filename, pStorage](CAccountData::CLoadDone Done)
	{
		auto pContext = DbAsync::MakeContext(ClientID);
		AsyncFile::Read(pStorage, Filename.c_str(), IStorageEngine::TYPE_ABSOLUTE,
			[pContext, Done = std::move(Done)](AsyncFile::SReadResult&& Read) mutable
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer || !pPlayer->Account())
			{
				Done(false);
				return;
			}
			if(!Read.Ok() && !Read.Missing())
			{
				dbg_msg("prison", "could not read account prison file: %s", Read.m_Error.c_str());
				Done(false);
				return;
			}

			const std::string& Content = Read.m_Content;
			PrisonTerm Term;
#if defined(__GNUC__) && __WORDSIZE == 64
			const int Fields = sscanf(Content.c_str(), "%d %ld", &Term.ImprisonmentTime, &Term.StartTime);
#else
			const int Fields = sscanf(Content.c_str(), "%d %lld", &Term.ImprisonmentTime, &Term.StartTime);
#endif
			const int ElapsedTime = static_cast<int>(time(nullptr) - Term.StartTime);
			if(Fields == 2 && ElapsedTime < Term.ImprisonmentTime)
			{
				Term.StartTime = time(nullptr) - ElapsedTime;
				pPlayer->Account()->GetPrisonManager().m_PrisonTerm = Term;
			}
			Done(true);
		});
	});
}

void PrisonManager::Save() const
{
	const auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->Account())
		return;

	// The line is built here; the write leaves the game thread at once.
	char aBuffer[128];
#if defined(__GNUC__) && __WORDSIZE == 64
	str_format(aBuffer, sizeof(aBuffer), "%d %ld\n", m_PrisonTerm.ImprisonmentTime, m_PrisonTerm.StartTime);
#else
	str_format(aBuffer, sizeof(aBuffer), "%d %lld\n", m_PrisonTerm.ImprisonmentTime, m_PrisonTerm.StartTime);
#endif

	const std::string Filename = getFileName(pPlayer->Account()->GetID());
	AsyncFile::Write(GS()->Storage(), Filename.c_str(), IStorageEngine::TYPE_ABSOLUTE, std::string(aBuffer));
}
