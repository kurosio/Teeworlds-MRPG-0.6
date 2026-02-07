#include <game/collision.h>

#include "rope.h"

namespace
{
	vec2 GetCorrection(const vec2& PointA, const vec2& PointB, float Tension)
	{
		const vec2 Delta = PointA - PointB;
		const float DistanceSquared = length_squared(Delta);
		if(DistanceSquared <= 0.000001f)
			return vec2(0.f, 0.f);

		const float Distance = sqrtf(DistanceSquared);
		const float DiffLength = Distance - Tension;
		return (Delta / Distance) * (DiffLength * 0.5f);
	}

	void ConstrainSegmentLength(vec2* pCurrentPoint, const vec2& PrevPoint, float MaxStretch)
	{
		if(MaxStretch <= 0.f)
			return;

		const vec2 Delta = *pCurrentPoint - PrevPoint;
		const float DistanceSquared = length_squared(Delta);
		const float MaxStretchSquared = MaxStretch * MaxStretch;
		if(DistanceSquared <= MaxStretchSquared || DistanceSquared <= 0.000001f)
			return;

		const float Distance = sqrtf(DistanceSquared);
		*pCurrentPoint = PrevPoint + (Delta / Distance) * MaxStretch;
	}
}

void RopePhysic::Init(int NumPoints, const vec2& Start, const vec2& Force)
{
	m_StartPoint = Start;
	m_Force = Force;

	m_vPoints.clear();
	if(NumPoints <= 0)
		return;

	m_vPoints.reserve(NumPoints);
	m_vPoints.push_back(m_StartPoint);
	for(int i = 1; i < NumPoints; ++i)
		m_vPoints.push_back(m_StartPoint);
}

void RopePhysic::UpdatePhysics(CCollision* pCollision, float PointMass, float Tension, float MaxStretch)
{
	constexpr float ForceDamping = 0.95f;
	if(!pCollision || m_vPoints.size() < 2)
		return;

	const size_t LastPointIndex = m_vPoints.size() - 1;
	for(size_t i = 1; i < LastPointIndex; ++i)
	{
		auto& CurrentPoint = m_vPoints[i];
		CurrentPoint.y += PointMass;

		vec2 Correction = GetCorrection(CurrentPoint, m_vPoints[i - 1], Tension);
		CurrentPoint -= Correction;
		m_vPoints[i - 1] += Correction;
		ConstrainSegmentLength(&CurrentPoint, m_vPoints[i - 1], MaxStretch);
	}

	auto& LastPoint = m_vPoints[LastPointIndex];
	if(pCollision->CheckPoint(LastPoint, CCollision::COLFLAG_WATER))
		pCollision->MovePhysicalBox(&LastPoint, &m_Force, vec2(1.f, 1.f), 0.0f, -0.7f);
	else
		pCollision->MovePhysicalBox(&LastPoint, &m_Force, vec2(1.f, 1.f), 0.5f, 0.7f);

	m_vPoints[LastPointIndex - 1] += GetCorrection(LastPoint, m_vPoints[LastPointIndex - 1], Tension);
	ConstrainSegmentLength(&LastPoint, m_vPoints[LastPointIndex - 1], MaxStretch);
	m_Force *= ForceDamping;
}

void RopePhysic::SetForce(const vec2& Force)
{
	m_Force = Force;
}
