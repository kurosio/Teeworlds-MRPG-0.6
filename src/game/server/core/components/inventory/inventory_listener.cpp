#include "inventory_listener.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/accounts/account_listener.h>
#include <game/server/worldmodes/dungeon/dungeon.h>
#include <game/server/core/entities/tools/flying_point.h>

CInventoryListener g_InventoryListener;

// inventory listener
void CInventoryListener::Initialize()
{
	g_EventListenerManager.RegisterListener(IEventListener::PlayerLogin, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerProfessionUpgrade, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerProfessionChange, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerEquipItem, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerGotItem, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerUnequipItem, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerEnchantItem, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerDurabilityItem, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerChangePreset, this);
	g_EventListenerManager.RegisterListener(IEventListener::CharacterSpawn, this);
	g_EventListenerManager.RegisterListener(IEventListener::CharacterDamage, this);
}

void CInventoryListener::OnPlayerLogin(CPlayer* pPlayer, CAccountData* pAccount)
{
	if(!pPlayer || !pAccount)
		return;

	pAccount->AutoEquipSlots(true);
	UpdateAttributes(pPlayer);
}

void CInventoryListener::OnPlayerGotItem(CPlayer* pPlayer, CPlayerItem* pItem, int Got)
{
	if (!pPlayer || !pItem)
		return;

	// Module: Trash resources may turn into materials
	auto* pGS = pPlayer->GS();
	if(pItem->GetID() == itTrash)
	{
		const int TrashValue = pItem->GetValue();
		auto* pLuckySieve = pPlayer->GetItem(itLuckySieve);
		if (pLuckySieve->IsEquipped() && pItem->Remove(TrashValue))
		{
			pPlayer->GetItem(itMaterial)->Add(TrashValue);
			pGS->Chat(pPlayer->GetCID(), "Your trash has been processed by the Lucky Sieve and turned into materials!");
		}
	}
}

void CInventoryListener::OnPlayerProfessionUpgrade(CPlayer* pPlayer, int AttributeID)
{
	if(!pPlayer)
		return;

	pPlayer->InvalidateAttributeCache();
	const auto Attribute = static_cast<AttributeIdentifier>(AttributeID);
	const int Value = pPlayer->GetTotalRawAttributeValue(Attribute);
	pPlayer->UpdateTotalAttributeValue(Attribute, Value);
	if(g_AccountListener.TrackingData().UpdateAttribute(pPlayer, Attribute, Value))
		g_AccountListener.TrackingData().SaveAttributes();
}

void CInventoryListener::OnPlayerProfessionChange(CPlayer* pPlayer, CProfession* pOldProf, CProfession* pNewProf)
{
	if(pPlayer && pOldProf != pNewProf)
	{
		pPlayer->Account()->AutoEquipSlots(true);
		UpdateAttributes(pPlayer);
	}
}

void CInventoryListener::OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
	UpdateAttributes(pPlayer, pItem);
}

void CInventoryListener::OnPlayerDurabilityItem(CPlayer* pPlayer, CPlayerItem* pItem, int OldDurability)
{
	if (!pPlayer || !pItem)
		return;

	const auto currentDurability = pItem->GetDurability();
	if (currentDurability > OldDurability && pItem->Info()->IsType(ItemType::EquipEidolon))
		pPlayer->GetSharedData().m_PendingEidolonItemBreaks.erase(pItem->GetID());
	if ((OldDurability <= 0 && currentDurability > 0) ||
		(OldDurability > 0 && currentDurability <= 0))
		UpdateAttributes(pPlayer, pItem);

	// Module: Repair equipment anywhere (uses materials)
	auto* pGS = pPlayer->GS();
	if (currentDurability <= 0 && pPlayer->GetItem(itPortableAnvil)->IsEquipped())
	{
		const int RepairCost = (pItem->GetEnchantPrice() * 0.10f);
		if (pPlayer->GetItem(itMaterial)->GetValue() >= RepairCost)
		{
			pPlayer->GetItem(itMaterial)->Remove(RepairCost);
			pItem->SetDurability(100);
			pGS->Chat(pPlayer->GetCID(), "Your {} has been repaired using {} materials!", pItem->Info()->GetName(), RepairCost);
			UpdateAttributes(pPlayer, pItem);
		}
	}
}

void CInventoryListener::OnPlayerUnequipItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
	UpdateAttributes(pPlayer, pItem);
}

void CInventoryListener::OnPlayerEnchantItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
	UpdateAttributes(pPlayer, pItem);
}

void CInventoryListener::OnPlayerChangePreset(CPlayer* pPlayer, int PresetSlotIndex)
{
	if (pPlayer)
	{
		UpdateAttributes(pPlayer);
	}
}

void CInventoryListener::OnCharacterSpawn(CPlayer* pPlayer)
{
	UpdateAttributes(pPlayer);
}

void CInventoryListener::OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage)
{
	if (!pFrom || !pTo || Damage <= 0)
		return;

	auto* pGS = pFrom->GS();

	// Module: Reflects 10% of taken damage back
	if (pTo->GetItem(itMirrorOfPain)->IsEquipped() && pFrom->GetCharacter() && pTo->GetCharacter())
	{
		auto* pTargetChar = pFrom->GetCharacter();
		auto* pFromChar = pTo->GetCharacter();
		const vec2 Direction = length(pFromChar->GetPos() - pTargetChar->GetPos()) > 0.0f
			? normalize(pFromChar->GetPos() - pTargetChar->GetPos()) : vec2(0.0f, -1.0f);
		const vec2 Force = vec2(0.0f, -1.0f) + normalize(Direction + vec2(0.0f, -1.1f)) * 10.0f;
		const auto reflectedDamage = maximum(1, round_to_int(Damage * 0.10f));
		auto* pPoint = new CEntityFlyingPoint(&pGS->m_World, pTo->GetCharacter()->GetPos(), {}, pFrom->GetCID(), pTo->GetCID());
		pPoint->Register([pGS, Force, reflectedDamage](CPlayer* pFrom, CPlayer* pPlayer)
			{
				pGS->CreateDamage(pPlayer->GetCharacter()->GetPos(), pFrom->GetCID(), reflectedDamage, 0.f, -1);
				pPlayer->GetCharacter()->TakeDamage({}, reflectedDamage, pFrom->GetCID(), WEAPON_SELF);
			});
	}
}

void CInventoryListener::UpdateAttributes(CPlayer* pPlayer, CPlayerItem* pItem)
{
	if(!pPlayer)
		return;

	pPlayer->InvalidateAttributeCache();
	bool Changed = false;
	auto Update = [&](AttributeIdentifier ID)
	{
		const int Value = pPlayer->GetTotalRawAttributeValue(ID);
		pPlayer->UpdateTotalAttributeValue(ID, Value);
		Changed |= g_AccountListener.TrackingData().UpdateAttribute(pPlayer, ID, Value);
	};

	if(pItem && pItem->Info())
		for(const auto& Attribute : pItem->Info()->GetAttributes())
			Update(Attribute.GetID());
	else
		for(const auto& [ID, Info] : CAttributeDescription::Data())
			Update(ID);

	if(Changed)
		g_AccountListener.TrackingData().SaveAttributes();

	if(auto* pCharacter = pPlayer->GetCharacter())
	{
		pItem ? pCharacter->UpdateEquippedStats(pItem->GetID()) : pCharacter->UpdateEquippedStats();
		pPlayer->GS()->MarkUpdatedBroadcast(pPlayer->GetCID());
	}
}
