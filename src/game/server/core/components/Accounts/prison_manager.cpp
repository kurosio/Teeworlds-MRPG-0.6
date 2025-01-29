#include <engine/shared/linereader.h>
#include "prison_manager.h"

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/core/components/worlds/world_data.h>

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

    SavePrisonData();
}

void PrisonManager::Free()
{
    m_PrisonTerm.ImprisonmentTime = 0;
    m_PrisonTerm.StartTime = 0;

    GS()->Chat(-1, "'{}' has been released from prison.", GS()->Server()->ClientName(m_ClientID));

    if(const auto* pPlayer = GetPlayer())
    {
        if(pPlayer->GetCharacter())
        {
            pPlayer->GetCharacter()->Die(m_ClientID, WEAPON_WORLD);
        }

        IStorageEngine* pStorage = GS()->Storage();
        const std::string filePath = fmt_default("server_data/account_prison/{}.txt", pPlayer->Account()->GetID());
        pStorage->RemoveFile(filePath.c_str(), IStorageEngine::TYPE_ABSOLUTE);
    }
}

void PrisonManager::UpdatePrison()
{
    if(!m_PrisonTerm.IsActive())
        return;

    CPlayer* pPlayer = GetPlayer();
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
        Free();
    }
    else
    {
        GS()->Broadcast(m_ClientID, BroadcastPriority::TitleInformation, 50, "Remaining prison time: {} seconds.", Remaining);
    }
}

std::pair<int, std::string> PrisonManager::GetPrisonStatusString() const
{
    int RemainingTime = m_PrisonTerm.RemainingTime();
    char aBuf[64];
    str_format(aBuf, sizeof(aBuf), "Remaining Prison Time: %d seconds", RemainingTime);
    return { RemainingTime, std::string(aBuf) };
}

void PrisonManager::LoadPrisonData()
{
    CPlayer* pPlayer = GetPlayer();
    if(!pPlayer)
        return;

    IStorageEngine* pStorage = GS()->Storage();
    const time_t currentTime = time(nullptr);

    if(const auto File = pStorage->OpenFile(fmt_default("server_data/account_prison/{}.txt", pPlayer->Account()->GetID()).c_str(), IOFLAG_READ | IOFLAG_SKIP_BOM, IStorageEngine::TYPE_ABSOLUTE))
    {
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

void PrisonManager::SavePrisonData() const
{
    CPlayer* pPlayer = GetPlayer();
    if(!pPlayer)
        return;

    IStorageEngine* pStorage = GS()->Storage();

    if(const auto File = pStorage->OpenFile(fmt_default("server_data/account_prison/{}.txt", pPlayer->Account()->GetID()).c_str(), IOFLAG_WRITE, IStorageEngine::TYPE_ABSOLUTE))
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