#include <game/collision.h>
#include "rope.h"

void RopePhysic::Init(int NumPoints, const vec2& Start, const vec2& Force)
{
    m_StartPoint = Start;
    m_Force = Force;

    m_vPoints.clear();
    m_vPoints.reserve(NumPoints);
    m_vPoints.push_back(m_StartPoint);
    for(int i = 1; i < NumPoints; ++i)
        m_vPoints.push_back(vec2(m_StartPoint.x, m_StartPoint.y));
}

vec2 getCorrection(const vec2& point, const vec2& point2, float tension)
{
    vec2 direction = normalize(point - point2);
    float dist = distance(point, point2);
    float diffLength = dist - tension;
    float springForce = diffLength * 0.5f;
    return direction * springForce;
}

void RopePhysic::UpdatePhysics(CCollision* pCollision, float PointMass, float Tension, float MaxStretch)
{
    constexpr float forceDamping = 0.95f;

    if(m_vPoints.size() <= 2)
        return;

    // update points
    for(size_t i = 1; i < m_vPoints.size() - 1; ++i)
    {
        m_vPoints[i].y += PointMass;
        vec2 correction = getCorrection(m_vPoints[i], m_vPoints[i - 1], Tension);
        m_vPoints[i] -= correction;
        m_vPoints[i - 1] += correction;
    }

    // update last mass point
    auto lastIndex = m_vPoints.size() - 1;
    if(pCollision->CheckPoint(m_vPoints[lastIndex], CCollision::COLFLAG_WATER))
        pCollision->MovePhysicalBox(&m_vPoints[lastIndex], &m_Force, vec2(1.f, 1.f), 0.0f, -0.7f);
    else
        pCollision->MovePhysicalBox(&m_vPoints[lastIndex], &m_Force, vec2(1.f, 1.f), 0.5f, 0.7f);
    m_vPoints[lastIndex - 1] += getCorrection(m_vPoints[lastIndex], m_vPoints[lastIndex - 1], Tension);
    m_Force *= forceDamping;
}

void RopePhysic::SetForce(const vec2& Force)
{
    m_Force = Force;
}