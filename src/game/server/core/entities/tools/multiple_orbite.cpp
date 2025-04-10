/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "multiple_orbite.h"

#include <game/server/gamecontext.h>

CMultipleOrbite::CMultipleOrbite(CGameWorld* pGameWorld, CEntity* pParent)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_MULTIPLE_ORBITE, vec2(0, 0), 64.f)
{
	m_pParent = pParent;
	GameWorld()->InsertEntity(this);
}

CMultipleOrbite::~CMultipleOrbite()
{
	for(const auto& pItems : m_Items)
		Server()->SnapFreeID(pItems.m_ID);
	m_Items.clear();
}

void CMultipleOrbite::Add(bool Projectile, int Value, int Type, int Subtype, int Orbitetype)
{
	m_Items.reserve(m_Items.size() + Value);

	for(int i = 0; i < Value; i++)
	{
		SnapItem Item;
		Item.m_ID = Server()->SnapNewID();
		Item.m_Type = Type;
		Item.m_Subtype = Subtype;
		Item.m_Orbitetype = Orbitetype;
		Item.m_Projectile = Projectile;
		m_Items.push_back(Item);
	}
}

void CMultipleOrbite::Remove(bool Projectile, int Value, int Type, int Subtype, int Orbitetype)
{
	auto it = std::remove_if(m_Items.begin(), m_Items.end(),
		[&](const SnapItem& item)
	{
		return item.m_Type == Type &&
			item.m_Subtype == Subtype &&
			item.m_Projectile == Projectile &&
			item.m_Orbitetype == Orbitetype;
	});

	int Count = std::distance(it, m_Items.end());
	Count = minimum(Count, Value);

	// free snap ids
	for(auto itDel = m_Items.end() - Count; itDel != m_Items.end(); ++itDel)
		Server()->SnapFreeID(itDel->m_ID);

	// erase item's
	m_Items.erase(it, it + Count);
}

void CMultipleOrbite::Tick()
{
	if(!GameWorld()->ExistEntity(m_pParent))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	m_Pos = m_pParent->GetPos();
}

vec2 CMultipleOrbite::UtilityOrbitePos(int Orbitetype, int Iter) const
{
	float AngleStep = 2.0f * pi / (float)m_Items.size();

	if(Orbitetype == MULTIPLE_ORBITE_TYPE_DEFAULT)
	{
		float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) * 0.55f;
		float X = GetRadius() * cos(AngleStart + AngleStep * (float)Iter);
		float Y = GetRadius() * sin(AngleStart + AngleStep * (float)Iter);
		return { X, Y };
	}
	else if(Orbitetype == MULTIPLE_ORBITE_TYPE_PULSATING)
	{
		float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) * 0.55f;
		float DirectionModifier = cos((float)Server()->Tick() / (float)Server()->TickSpeed() * 2.0f);
		float X = GetRadius() * cos(AngleStart + AngleStep * (float)Iter) * DirectionModifier;
		float Y = GetRadius() * sin(AngleStart + AngleStep * (float)Iter) * DirectionModifier;
		return { X, Y };
	}
	else if(Orbitetype == MULTIPLE_ORBITE_TYPE_ELLIPTICAL)
	{
		float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) * 0.55f;
		float XRadius = GetRadius();
		float YRadius = GetRadius() * 0.5f;
		float X = XRadius * cos(AngleStart + AngleStep * (float)Iter);
		float Y = YRadius * sin(AngleStart + AngleStep * (float)Iter);
		return { X, Y };
	}
	else if(Orbitetype == MULTIPLE_ORBITE_TYPE_VIBRATION)
	{
		float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) * 0.5f;
		float Vibration = sin((float)Server()->Tick() / (float)Server()->TickSpeed() * 5.0f) * 10.0f;
		float X = (GetRadius() + Vibration) * cos(AngleStart + AngleStep * (float)Iter);
		float Y = (GetRadius() + Vibration) * sin(AngleStart + AngleStep * (float)Iter);
		return { X, Y };
	}
	else if(Orbitetype == MULTIPLE_ORBITE_TYPE_VARIABLE_RADIUS)
	{
		float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) * 0.5f;
		float Radius = GetRadius() + sin(AngleStart * 0.5f) * 20.0f;
		float X = Radius * cos(AngleStart + AngleStep * (float)Iter);
		float Y = Radius * sin(AngleStart + AngleStep * (float)Iter);
		return { X, Y };
	}
	else if(Orbitetype == MULTIPLE_ORBITE_TYPE_EIGHT)
	{
		float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) * 0.55f;
		float A = GetRadius();
		float B = GetRadius() / 2.f;
		float X = A * cos(AngleStart + AngleStep * (float)Iter) * cos(AngleStep * (float)Iter);
		float Y = B * sin(AngleStart + AngleStep * (float)Iter);
		return { X, Y };
	}
	else if(Orbitetype == MULTIPLE_ORBITE_TYPE_LOOPING)
	{
		float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) * 0.55f;
		float A = GetRadius();
		float B = 5.f;
		float X = A * cos(AngleStart + AngleStep * (float)Iter) * cos(B * (float)Server()->Tick() / 50.0f);
		float Y = A * sin(AngleStart + AngleStep * (float)Iter) * sin(B * (float)Server()->Tick() / 50.0f);
		return { X, Y };
	}
	else if(Orbitetype == MULTIPLE_ORBITE_TYPE_DYNAMIC_CENTER)
	{
		float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) * 0.55f;
		float DynX = sin((float)Server()->Tick() / 60.0f) * 20.0f;
		float DynY = cos((float)Server()->Tick() / 80.0f) * 15.0f;
		vec2 DynamicCenter = { DynX,  DynY };
		float Radius = GetRadius();
		float X = (DynamicCenter.x + Radius) * cos(AngleStart + AngleStep * (float)Iter);
		float Y = (DynamicCenter.y + Radius) * sin(AngleStart + AngleStep * (float)Iter);
		return { X, Y };
	}
	return { 0.f, 0.f };
}

void CMultipleOrbite::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	int Iter = 0;
	for(const auto& [ID, Type, Subtype, Orbitetype, Projectile] : m_Items)
	{
		const vec2 PosStart = m_Pos + UtilityOrbitePos(Orbitetype, Iter);
		if(Projectile)
			GS()->SnapProjectile(SnappingClient, ID, PosStart, {}, Server()->Tick(), Type, m_ClientID);
		else
			GS()->SnapPickup(SnappingClient, ID, PosStart, Type, Subtype);
		Iter++;
	}
}