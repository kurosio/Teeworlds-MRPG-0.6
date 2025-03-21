#include <game/collision.h>
#include "rope.h"

void RopePhysic::Init(int NumPoints, const vec2& Start, const vec2& Force, float EndPointMass)
{
    m_StartPoint = Start;
    m_Force = Force;
    m_EndPointMass = EndPointMass;

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

    // update
    for(int i = 1; i < m_vPoints.size(); ++i)
    {
        bool isLastPoint = (i == m_vPoints.size() - 1);
        if(isLastPoint)
        {
            if(pCollision->CheckPoint(m_vPoints[i], CCollision::COLFLAG_WATER))
                pCollision->MovePhysicalBox(&m_vPoints[i], &m_Force, vec2(1.f, 1.f), 0.0f, -0.7f);
            else
                pCollision->MovePhysicalBox(&m_vPoints[i], &m_Force, vec2(1.f, 1.f), 0.5f, 0.7f);

            m_vPoints[i - 1] += getCorrection(m_vPoints[i], m_vPoints[i - 1], Tension);
            m_Force *= forceDamping;
            return;
        }

        m_vPoints[i].y += PointMass;
        vec2 correction = getCorrection(m_vPoints[i], m_vPoints[i - 1], Tension);
        m_vPoints[i] -= correction;
        m_vPoints[i - 1] += correction;
    }

    // normalize points
    const auto lastPointIndex = m_vPoints.size() - 1;
    float stretch = distance(m_vPoints[lastPointIndex], m_vPoints[lastPointIndex - 1]);
    if(stretch > MaxStretch)
    {
        for(int i = lastPointIndex - 1; i > 0; --i)
        {
            float distToPrev = distance(m_vPoints[i], m_vPoints[i - 1]);
            if(distToPrev <= MaxStretch)
            {
                vec2 stretchDir = normalize(m_vPoints[i] - m_vPoints[i - 1]);
                m_vPoints[i] = m_vPoints[i - 1] + stretchDir * MaxStretch;
            }
        }
    }
}

void RopePhysic::SetFrontPoint(const vec2& Pos)
{
    if(!m_vPoints.empty())
        m_vPoints.front() = Pos;
}
