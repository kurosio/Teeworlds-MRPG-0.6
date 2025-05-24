#ifndef GAME_SERVER_ENTITIES_RIFLE_TESLA_SERPENT_H
#define GAME_SERVER_ENTITIES_RIFLE_TESLA_SERPENT_H

#include <game/server/entity.h>
#include <vector>

class CEntityTeslaSerpent : public CEntity
{
    vec2 m_InitialDir;
    int m_LifeSpanTicks;
    float m_Damage;
    float m_ChainRange;
    int m_MaxTargetsInChain;
    float m_DamageFalloff;

    std::vector<int> m_vTargetsHitThisShot;
    std::vector<vec2> m_vChainSegmentEndPoints;
    bool m_ChainCalculationDone;

public:
    CEntityTeslaSerpent(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos, vec2 Direction, float Damage, float ChainRange, int MaxTargets, float DamageFalloff);

    void Tick() override;
    void Snap(int SnappingClient) override;

private:
    void CalculateChainLightning();
};

#endif // GAME_SERVER_ENTITIES_RIFLE_TESLA_SERPENT_H
