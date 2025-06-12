/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "drop_items.h"

#include <game/server/gamecontext.h>
#include <base/tl/base.h>

enum
{
	MAIN_GROUP = 1,
	NUM_MAIN_IDS = 2,
};

CEntityDropItem::CEntityDropItem(CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, CItem DropItem, int OwnerID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP_ITEM, Pos, 24.0f)
{
	m_Pos = Pos;
	m_Vel = Vel;
	m_ClientID = OwnerID;
	m_DropItem = DropItem;
	m_DropItem.SetSettings(0);
	m_LifeSpan = Server()->TickSpeed() * g_Config.m_SvDroppedItemLifetime;
	m_IsCurrency = m_DropItem.Info()->IsGroup(ItemGroup::Currency);
	AddSnappingGroupIds(MAIN_GROUP, NUM_MAIN_IDS);

	GameWorld()->InsertEntity(this);
}

bool CEntityDropItem::TakeItem(int ClientID)
{
	auto *pPlayer = GS()->GetPlayer(ClientID, true, true);
	if(!pPlayer || (m_ClientID >= 0 && m_ClientID != ClientID))
		return false;

	// change of enchanted objects
	auto* pPlayerItem = pPlayer->GetItem(m_DropItem.GetID());
	if(pPlayerItem->GetValue() > 0 && !pPlayerItem->Info()->IsStackable())
	{
		bool LastEquipped = pPlayerItem->IsEquipped();
		tl_swap(static_cast<CItem&>(*pPlayerItem), m_DropItem);
		if(!pPlayerItem->IsEquipped() && LastEquipped)
			pPlayerItem->Equip();

		GS()->Chat(ClientID, "You now own '{}{}'.", pPlayerItem->Info()->GetName(), pPlayerItem->GetStringEnchantLevel().c_str());
		pPlayer->m_VotesData.UpdateVotesIf(MENU_INVENTORY);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_EQUIPMENT);
		pPlayerItem->Save();
		return true;
	}

	// simple subject delivery
	pPlayerItem->Add(m_DropItem.GetValue(), 0, m_DropItem.GetEnchant());
	GS()->Broadcast(ClientID, BroadcastPriority::GameWarning, 10, "\0");
	pPlayer->m_VotesData.UpdateVotesIf(MENU_INVENTORY);
	pPlayer->m_VotesData.UpdateVotesIf(MENU_EQUIPMENT);
	GS()->CreatePlayerSound(m_ClientID, SOUND_PICK_UP);
	GameWorld()->DestroyEntity(this);
	return true;
}

void CEntityDropItem::Tick()
{
	m_LifeSpan--;
	if(m_LifeSpan < 0)
	{
		GS()->CreatePlayerSpawn(m_Pos);
		GameWorld()->DestroyEntity(this);
		return;
	}
	m_Flash.Tick(m_LifeSpan);

	if(!HasPlayersInView())
		return;

	GS()->Collision()->MovePhysicalBox(&m_Pos, &m_Vel, vec2(m_Radius, m_Radius), 0.5f);

	// set without owner if there is no player owner
	if(m_ClientID != -1)
	{
		auto* pPlayer = GS()->GetPlayer(m_ClientID, true, true);
		if(pPlayer && pPlayer->GetItem(itMagnetItems)->IsEquipped())
			m_Vel += normalize(pPlayer->GetCharacter()->m_Core.m_Pos - m_Pos) * 0.55f;
		else if(!pPlayer)
			m_ClientID = -1;
	}

	// information
	auto *pChar = (CCharacter*)GameWorld()->ClosestEntity(m_Pos, 32.0f, CGameWorld::ENTTYPE_CHARACTER, nullptr);
	if(pChar && !pChar->GetPlayer()->IsBot())
	{
		bool CanPick = m_ClientID == -1 || m_ClientID == pChar->GetClientID();
		if(distance(pChar->m_Core.m_Pos, m_Pos) > 24.0f)
		{
			if(CanPick)
				m_Vel += normalize(pChar->m_Core.m_Pos - m_Pos) * 0.55f;
			return;
		}

		if(m_IsCurrency && CanPick)
		{
			TakeItem(pChar->GetClientID());
			return;
		}

		const int ClientID = pChar->GetPlayer()->GetCID();
		const auto* pPlayerItem = pChar->GetPlayer()->GetItem(m_DropItem.GetID());
		const char* pOwnerNick = (m_ClientID != -1 ? Server()->ClientName(m_ClientID) : "\0");

		if(!pPlayerItem->Info()->IsStackable())
		{
			if(pPlayerItem->HasItem())
			{
				GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 100, "You have: {}{}\nReplace with: {}{} {}",
					pPlayerItem->Info()->GetName(), pPlayerItem->GetStringEnchantLevel().c_str(), m_DropItem.Info()->GetName(), m_DropItem.GetStringEnchantLevel().c_str(), pOwnerNick);
			}
			else
			{
				GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 100, "{}(+{}) {}",
					m_DropItem.Info()->GetName(), m_DropItem.GetEnchant(), pOwnerNick);
			}
		}
		else
		{
			GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 100, "{} x{} {}",
				m_DropItem.Info()->GetName(), m_DropItem.GetValue(), pOwnerNick);
		}
	}
}

void CEntityDropItem::Snap(int SnappingClient)
{
	if(m_Flash.IsFlashing() || NetworkClipped(SnappingClient))
		return;

	if(m_IsCurrency)
	{
		m_Radius = 12.f;
		GS()->SnapProjectile(SnappingClient, GetID(), m_Pos, {}, Server()->Tick() - 2, WEAPON_LASER, m_ClientID);
	}
	else
	{
		m_Radius = 24.f;
		GS()->SnapPickup(SnappingClient, GetID(), m_Pos, POWERUP_ARMOR_LASER);
	}

	if(const auto* pvMainIds = FindSnappingGroupIds(MAIN_GROUP))
	{
		for(int i = 0; i < NUM_MAIN_IDS; i++)
		{
			float AngleStep = 2.0f * pi / (float)NUM_MAIN_IDS;
			float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) * 1.55f;
			float X = m_Radius * cos(AngleStart + AngleStep * (float)i);
			float Y = m_Radius * sin(AngleStart + AngleStep * (float)i);
			GS()->SnapProjectile(SnappingClient, (*pvMainIds)[i], m_Pos + vec2(X, Y), {}, Server()->Tick(), WEAPON_HAMMER);
		}
	}
}