/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "drop_items.h"

#include <game/server/gamecontext.h>

#include <base/tl/base.h>

CDropItem::CDropItem(CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, CItem DropItem, int OwnerID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPITEM, Pos, 28.0f)
{
	m_Pos = Pos;
	m_Vel = Vel;
	m_Flash.InitFlashing(&m_LifeSpan);
	m_LifeSpan = Server()->TickSpeed() * 20;

	m_OwnerID = OwnerID;
	m_DropItem = DropItem;
	m_DropItem.SetSettings(0);
	GameWorld()->InsertEntity(this);
}

bool CDropItem::TakeItem(int ClientID)
{
	CPlayer *pPlayer = GS()->GetPlayer(ClientID, true, true);
	if(!pPlayer || (m_OwnerID >= 0 && m_OwnerID != ClientID))
		return false;

	// change of enchanted objects
	CPlayerItem* pPlayerItem = pPlayer->GetItem(m_DropItem.GetID());
	if(pPlayerItem->GetValue() > 0 && pPlayerItem->Info()->IsEnchantable())
	{
		bool LastEquipped = pPlayerItem->IsEquipped();
		tl_swap(static_cast<CItem&>(*pPlayerItem), m_DropItem);
		if(!pPlayerItem->IsEquipped() && LastEquipped)
			pPlayerItem->Equip();

		GS()->Chat(ClientID, "You now own {STR}(+{INT})", pPlayerItem->Info()->GetName(), pPlayerItem->GetEnchant());
		GS()->StrongUpdateVotes(ClientID, MENU_INVENTORY);
		GS()->StrongUpdateVotes(ClientID, MENU_EQUIPMENT);
		return true;
	}

	// simple subject delivery
	pPlayerItem->Add(m_DropItem.GetValue(), 0, m_DropItem.GetEnchant());
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
	m_Flash.OnTick();

	// set without owner if there is no player owner
	if(m_OwnerID != -1 && !GS()->GetPlayer(m_OwnerID, true, true))
		m_OwnerID = -1;

	// physic
	GS()->Collision()->MovePhysicalBox(&m_Pos, &m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.5f);

	// information
	CCharacter *pChar = (CCharacter*)GameWorld()->ClosestEntity(m_Pos, 64.0f, CGameWorld::ENTTYPE_CHARACTER, nullptr);
	if(pChar && !pChar->GetPlayer()->IsBot())
	{
		const int ClientID = pChar->GetPlayer()->GetCID();
		const CPlayerItem* pPlayerItem = pChar->GetPlayer()->GetItem(m_DropItem.GetID());
		const char* pOwnerNick = (m_OwnerID != -1 ? Server()->ClientName(m_OwnerID) : "\0");

		if(pPlayerItem->Info()->IsEnchantable())
		{
			if(pPlayerItem->GetValue() > 0)
			{
				GS()->Broadcast(ClientID, BroadcastPriority::GAME_INFORMATION, 100, "You have: [{STR}(+{INT})]\nReplace with: [{STR}(+{INT}) {STR}]",
					m_DropItem.Info()->GetName(), pPlayerItem->GetEnchant(), m_DropItem.Info()->GetName(), m_DropItem.GetEnchant(), pOwnerNick);
			}
			else
			{
				GS()->Broadcast(ClientID, BroadcastPriority::GAME_INFORMATION, 100, "{STR}(+{INT}) {STR}",
					m_DropItem.Info()->GetName(), m_DropItem.GetEnchant(), pOwnerNick);
			}
		}
		else
		{
			GS()->Broadcast(ClientID, BroadcastPriority::GAME_INFORMATION, 100, "{STR}x{VAL} {STR}",
				m_DropItem.Info()->GetName(), m_DropItem.GetValue(), pOwnerNick);
		}
	}
}

void CDropItem::Snap(int SnappingClient)
{
	if(m_Flash.IsFlashing() || NetworkClipped(SnappingClient))
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