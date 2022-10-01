/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/mmocore/Components/Inventory/ItemData.h>
#include "drop_items.h"

#include <game/server/gamecontext.h>

#include <base/tl/base.h>

CDropItem::CDropItem(CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, CItemData DropItem, int OwnerID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPITEM, Pos, 28.0f)
{
	m_Pos = Pos;
	m_Vel = Vel;
	m_Angle = 0.0f;
	m_AngleForce = AngleForce;
	m_Flashing = false;
	m_LifeSpan = Server()->TickSpeed() * 20;

	m_OwnerID = OwnerID;
	m_DropItem = DropItem;
	m_DropItem.m_Settings = 0;

	GameWorld()->InsertEntity(this);
}

bool CDropItem::TakeItem(int ClientID)
{
	CPlayer *pPlayer = GS()->GetPlayer(ClientID, true, true);
	if(!pPlayer || (m_OwnerID >= 0 && m_OwnerID != ClientID))
		return false;

	// change of enchanted objects
	CItemData &pPlayerDroppedItem = pPlayer->GetItem(m_DropItem.GetID());
	if(pPlayerDroppedItem.m_Value > 0 && pPlayerDroppedItem.Info().IsEnchantable())
	{
		tl_swap(pPlayerDroppedItem, m_DropItem);
		GS()->Chat(ClientID, "You now own {STR}(+{INT})", pPlayerDroppedItem.Info().GetName(), pPlayerDroppedItem.m_Enchant);
		GS()->StrongUpdateVotes(ClientID, MENU_INVENTORY);
		GS()->StrongUpdateVotes(ClientID, MENU_EQUIPMENT);
		return true;
	}

	// simple subject delivery
	pPlayerDroppedItem.Add(m_DropItem.m_Value, 0, m_DropItem.m_Enchant);
	GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 10, "\0");
	GS()->StrongUpdateVotes(ClientID, MENU_INVENTORY);
	GS()->StrongUpdateVotes(ClientID, MENU_EQUIPMENT);
	GS()->m_World.DestroyEntity(this);
	return true;
}

void CDropItem::Tick()
{
	// lifetime dk
	m_LifeSpan--;
	if(m_LifeSpan < 0)
	{
		GS()->CreatePlayerSpawn(m_Pos);
		GS()->m_World.DestroyEntity(this);
		return;
	}

	// flashing
	if(m_LifeSpan < 150)
	{
		m_FlashTimer--;
		if (m_FlashTimer > 5)
			m_Flashing = true;
		else
		{
			m_Flashing = false;
			if (m_FlashTimer <= 0)
				m_FlashTimer = 10;
		}
	}

	// set without owner if there is no player owner
	if(m_OwnerID != -1 && !GS()->GetPlayer(m_OwnerID, true, true))
		m_OwnerID = -1;

	// physic
	vec2 ItemSize = vec2(GetProximityRadius(), GetProximityRadius());
	GS()->Collision()->MovePhysicalAngleBox(&m_Pos, &m_Vel, ItemSize, &m_Angle, &m_AngleForce, 0.5f);
	if(length(m_Vel) < 0.3f)
		m_Angle = 0.0f;

	// interactive
	CCharacter *pChar = (CCharacter*)GameWorld()->ClosestEntity(m_Pos, 64, CGameWorld::ENTTYPE_CHARACTER, 0);
	if(!pChar || pChar->GetPlayer()->IsBot())
		return;

	const char* pToNickname = (m_OwnerID != -1 ? Server()->ClientName(m_OwnerID) : "\0");
	const CItemData pPlayerItem = pChar->GetPlayer()->GetItem(m_DropItem.GetID());

	// enchantable item
	if(pPlayerItem.Info().IsEnchantable())
	{
		if(pPlayerItem.m_Value > 0)
			GS()->Broadcast(pChar->GetPlayer()->GetCID(), BroadcastPriority::GAME_INFORMATION, 100, "{STR}(+{INT}) -> (+{INT}) {STR}",
				m_DropItem.Info().GetName(), pPlayerItem.m_Enchant, m_DropItem.m_Enchant, pToNickname);
		else
			GS()->Broadcast(pChar->GetPlayer()->GetCID(), BroadcastPriority::GAME_INFORMATION, 100, "{STR}(+{INT}) {STR}",
				m_DropItem.Info().GetName(), m_DropItem.m_Enchant, pToNickname);

		return;
	}

	// non enchantable item
	GS()->Broadcast(pChar->GetPlayer()->GetCID(), BroadcastPriority::GAME_INFORMATION, 100, "{STR}x{VAL} {STR}",
		m_DropItem.Info().GetName(), m_DropItem.m_Value, pToNickname);
}

void CDropItem::Snap(int SnappingClient)
{
	if(m_Flashing || NetworkClipped(SnappingClient))
		return;

	// vanilla
	CNetObj_Pickup *pPickup = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(pPickup)
	{
		pPickup->m_X = (int)m_Pos.x;
		pPickup->m_Y = (int)m_Pos.y;
		pPickup->m_Type = POWERUP_WEAPON;
		pPickup->m_Subtype = WEAPON_HAMMER;
	}
}