#include "money_bag.h"

#include <game/server/gamecontext.h>

CEntityMoneyBag::CEntityMoneyBag(CGameWorld *pGameWorld, vec2 Pos)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_BONUS_DROP, Pos, 16.0f)
{
	GameWorld()->InsertEntity(this);
}

void CEntityMoneyBag::Tick()
{
	if(!HasPlayersInView())
		return;

	const auto *pChar = (CCharacter*)GameWorld()->ClosestEntity(m_Pos, m_Radius, CGameWorld::ENTTYPE_CHARACTER, nullptr);
	if(!pChar || !pChar->IsAlive() || pChar->GetPlayer()->IsBot())
		return;

	auto* pPlayer = pChar->GetPlayer();
	auto* pPlayerItem = pPlayer->GetItem(itLittleBagGold);
	const int RandomValue = 1 + rand() % 3;

	pPlayerItem->Add(RandomValue);
	GS()->Chat(-1, "Player '{}' has found '{} x{}'.", Server()->ClientName(pPlayer->GetCID()), pPlayerItem->Info()->GetName(), RandomValue);
	GS()->CreateDeath(m_Pos, pPlayer->GetCID());
	GameWorld()->DestroyEntity(this);
}

void CEntityMoneyBag::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	GS()->SnapProjectile(SnappingClient, GetID(), m_Pos, {}, Server()->Tick() - 3, WEAPON_LASER);
}