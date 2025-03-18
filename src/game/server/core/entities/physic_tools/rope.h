#ifndef GAME_SERVER_CORE_ENTITIES_PHYSIC_TOOLS_ROPE_H
#define GAME_SERVER_CORE_ENTITIES_PHYSIC_TOOLS_ROPE_H

class CCollision;
class RopePhysic
{
    vec2 m_StartPoint {};
    vec2 m_Force {};
    float m_MassAtEnd {};

public:
    std::vector<vec2> m_vPoints {};

    RopePhysic() = default;

    void Init(int NumPoints, const vec2& Start, const vec2& Force, float Mass);
    void UpdatePhysics(CCollision* pCollision, float Gravity, float Tension, float MaxStretch);
    void SetFrontPoint(const vec2& Pos);
};


#endif
