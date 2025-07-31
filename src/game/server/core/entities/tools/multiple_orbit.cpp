/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "multiple_orbit.h"

#include <game/server/gamecontext.h>

CMultipleOrbit::CMultipleOrbit(CGameWorld* pGameWorld, CEntity* pParent)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_VISUAL, vec2(0, 0), 64.f)
{
	m_pParent = pParent;
	GameWorld()->InsertEntity(this);
}

CMultipleOrbit::~CMultipleOrbit()
{
	for(const auto& pItems : m_Items)
		Server()->SnapFreeID(pItems.m_ID);
	m_Items.clear();
}

void CMultipleOrbit::Add(bool Projectile, int Value, int Type, int Subtype, int OrbitType)
{
	m_Items.reserve(m_Items.size() + Value);

	for(int i = 0; i < Value; i++)
	{
		SnapItem Item;
		Item.m_ID = Server()->SnapNewID();
		Item.m_Type = Type;
		Item.m_Subtype = Subtype;
		Item.m_OrbitType = OrbitType;
		Item.m_Projectile = Projectile;
		m_Items.push_back(Item);
	}
}

void CMultipleOrbit::Remove(bool Projectile, int Value, int Type, int Subtype, int OrbitType)
{
	if(m_Items.empty())
		return;

	auto it = std::remove_if(m_Items.begin(), m_Items.end(),
		[&](const SnapItem& item)
	{
		return item.m_Type == Type &&
			item.m_Subtype == Subtype &&
			item.m_Projectile == Projectile &&
			item.m_OrbitType == OrbitType;
	});
	if(it == m_Items.end())
		return;

	int Count = std::distance(it, m_Items.end());
	Count = std::min(Count, Value);

	// free snap ids
	auto LastIt = m_Items.end();
	for(auto itDel = LastIt - Count; itDel != LastIt; ++itDel)
		Server()->SnapFreeID(itDel->m_ID);

	// erase item's
	m_Items.erase(LastIt - Count, LastIt);
}

void CMultipleOrbit::Tick()
{
	if(!GameWorld()->ExistEntity(m_pParent))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	m_Pos = m_pParent->GetPos();
}

vec2 CMultipleOrbit::RoseCurvePos(float Angle, float Time) const
{
	const float k = 2.5f;
	float Theta = Time + Angle;
	float R = GetRadius() * cos(k * Theta);
	float X = R * cos(Theta);
	float Y = R * sin(Theta);
	return { X, Y };
}

vec2 CMultipleOrbit::HypotrochoidPos(float Angle, float Time) const
{
	const float R = GetRadius();
	const float r = R / 3.5f;
	const float d = GetRadius() / 2.0f;
	float Theta = Time + Angle;

	float X = (R - r) * cos(Theta) + d * cos(((R - r) / r) * Theta);
	float Y = (R - r) * sin(Theta) - d * sin(((R - r) / r) * Theta);
	return { X, Y };
}

vec2 CMultipleOrbit::UtilityOrbitPos(int OrbitType, int Iter) const
{
	if(m_Items.empty())
		return { 0.f, 0.f };

	const float AngleStep = 2.0f * pi / (float)m_Items.size();
	const float BaseAngle = AngleStep * (float)Iter;
	const float Time = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed());

	switch(OrbitType)
	{
		case MULTIPLE_ORBIT_TYPE_DEFAULT:
		{
			float Angle = Time * 0.55f + BaseAngle;
			return vec2(GetRadius() * cos(Angle), GetRadius() * sin(Angle));
		}
		case MULTIPLE_ORBIT_TYPE_PULSATING:
		{
			float Angle = Time * 0.55f + BaseAngle;
			float Modifier = 0.75f + 0.25f * cos(Time * 2.0f);
			return vec2(GetRadius() * cos(Angle) * Modifier, GetRadius() * sin(Angle) * Modifier);
		}
		case MULTIPLE_ORBIT_TYPE_ELLIPTICAL:
		{
			float Angle = Time * 0.55f + BaseAngle;
			return vec2(GetRadius() * cos(Angle), GetRadius() * 0.5f * sin(Angle));
		}
		case MULTIPLE_ORBIT_TYPE_VIBRATION:
		{
			float Angle = Time * 0.5f + BaseAngle;
			float Vibration = sin(Time * 5.0f) * 10.0f;
			return vec2((GetRadius() + Vibration) * cos(Angle), (GetRadius() + Vibration) * sin(Angle));
		}
		case MULTIPLE_ORBIT_TYPE_VARIABLE_RADIUS:
		{
			float Angle = Time * 0.5f + BaseAngle;
			float Radius = GetRadius() + sin(Angle * 0.5f) * 20.0f;
			return vec2(Radius * cos(Angle), Radius * sin(Angle));
		}
		case MULTIPLE_ORBIT_TYPE_EIGHT:
		{
			float Angle = Time * 0.5f + BaseAngle;
			float Denominator = 1 + pow(sin(Angle), 2);
			float X = GetRadius() * cos(Angle) / Denominator;
			float Y = GetRadius() * sin(Angle) * cos(Angle) / Denominator;
			return vec2(X, Y);
		}
		case MULTIPLE_ORBIT_TYPE_LOOPING:
		{
			float Angle = Time * 0.55f + BaseAngle;
			float B = 5.f;
			float X = GetRadius() * cos(Angle) * cos(B * Time * 0.02f);
			float Y = GetRadius() * sin(Angle) * sin(B * Time * 0.02f);
			return vec2(X, Y);
		}
		case MULTIPLE_ORBIT_TYPE_DYNAMIC_CENTER:
		{
			float Angle = Time * 0.55f + BaseAngle;
			float DynX = sin(Time / 1.2f) * 20.0f;
			float DynY = cos(Time / 1.6f) * 15.0f;
			return vec2(DynX + GetRadius() * cos(Angle), DynY + GetRadius() * sin(Angle));
		}

		case MULTIPLE_ORBIT_TYPE_ROSE:
		{
			return RoseCurvePos(BaseAngle, Time * 0.5f);
		}
		case MULTIPLE_ORBIT_TYPE_HYPOTROCHOID:
		{
			return HypotrochoidPos(BaseAngle, Time * 0.5f);
		}

		default:
			return { 0.f, 0.f };
	}
}

void CMultipleOrbit::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	int Iter = 0;
	for(const auto& [ID, Type, Subtype, OrbitType, Projectile] : m_Items)
	{
		const vec2 PosStart = m_Pos + UtilityOrbitPos(OrbitType, Iter);
		if(Projectile)
			GS()->SnapProjectile(SnappingClient, ID, PosStart, {}, Server()->Tick(), Type);
		else
			GS()->SnapPickup(SnappingClient, ID, PosStart, Type, Subtype);
		Iter++;
	}
}