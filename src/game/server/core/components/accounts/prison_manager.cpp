#include "prison_manager.h"

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
		GS()->Chat(-1, "'{}', has been imprisoned for '{} seconds'.", GS()->Server()->ClientName(m_ClientID), Seconds);

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
		if(pPlayer->GetCharacter())
			pPlayer->GetCharacter()->Die(m_ClientID, WEAPON_WORLD);

		auto* pStorage = GS()->Storage();
		const auto Filename = getFileName(pPlayer->Account()->GetID());
		pStorage->RemoveFile(Filename.c_str(), IStorageEngine::TYPE_ABSOLUTE);
		GS()->Chat(-1, "'{}' has been released from prison.", GS()->Server()->ClientName(m_ClientID));
	}
}

void PrisonManager::PostTick()
{
	if(!m_PrisonTerm.IsActive())
		return;

	const auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	const int JailWorldID = GS()->GetWorldData()->GetJailWorld()->GetID();
	if(pPlayer->GetCurrentWorldID() != JailWorldID)
	{
		pPlayer->ChangeWorld(JailWorldID);
		return;
	}

	if(!pPlayer->GetCharacter()->GetTiles()->IsActive(TILE_JAIL_ZONE))
	{
		const vec2 JailPosition = GS()->GetJailPosition();
		pPlayer->GetCharacter()->ChangePosition(JailPosition);
		GS()->Chat(m_ClientID, "You cannot leave the prison!");
	}

	const int Remaining = m_PrisonTerm.RemainingTime();
	if((Remaining - 1) <= 0)
	{
		Release();
		return;
	}

	GS()->Broadcast(m_ClientID, BroadcastPriority::TitleInformation, 50, "Remaining prison time: {} seconds.", Remaining);
}

void PrisonManager::Load()
{
	const auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	auto* pStorage = GS()->Storage();
	const auto Filename = getFileName(pPlayer->Account()->GetID());
	if(const auto File = pStorage->OpenFile(Filename.c_str(), IOFLAG_READ, IStorageEngine::TYPE_ABSOLUTE))
	{
		const time_t currentTime = time(nullptr);
		char* pResult = io_read_all_str(File);
		io_close(File);

		if(pResult)
		{
			PrisonTerm prisonTerm;
#if defined(__GNUC__) && __WORDSIZE == 64
			if(sscanf(pResult, "%d %ld", &prisonTerm.ImprisonmentTime, &prisonTerm.StartTime) == 2)
#else
			if(sscanf(pResult, "%d %lld", &prisonTerm.ImprisonmentTime, &prisonTerm.StartTime) == 2)
#endif
			{
				int elapsedTime = static_cast<int>(time(nullptr) - prisonTerm.StartTime);
				if(elapsedTime < prisonTerm.ImprisonmentTime)
				{
					prisonTerm.StartTime = currentTime - elapsedTime;
					m_PrisonTerm = prisonTerm;
				}
			}
			free(pResult);
		}
	}
}

void PrisonManager::Save() const
{
	const auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	auto* pStorage = GS()->Storage();
	const auto Filename = getFileName(pPlayer->Account()->GetID());
	if(const auto File = pStorage->OpenFile(Filename.c_str(), IOFLAG_WRITE, IStorageEngine::TYPE_ABSOLUTE))
	{
		char buffer[128];
#if defined(__GNUC__) && __WORDSIZE == 64
		str_format(buffer, sizeof(buffer), "%d %ld\n", m_PrisonTerm.ImprisonmentTime, m_PrisonTerm.StartTime);
#else
		str_format(buffer, sizeof(buffer), "%d %lld\n", m_PrisonTerm.ImprisonmentTime, m_PrisonTerm.StartTime);
#endif
		io_write(File, buffer, str_length(buffer));
		io_close(File);
	}
}