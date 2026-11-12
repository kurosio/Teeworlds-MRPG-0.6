#include "inventory_listener.h"
#include <game/server/gamecontext.h>
#include <game/server/worldmodes/dungeon/dungeon.h>
#include <game/server/core/entities/tools/flying_point.h>

CInventoryListener g_InventoryListener;
constexpr const char* ATTRIBUTE_TRACKING_FILE_NAME = "server_data/attribute_tracking.json";

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
	m_AttributesTracker.LoadTrackingData();
}

void CInventoryListener::OnPlayerLogin(CPlayer* pPlayer, CAccountData* pAccount)
{
	if (pPlayer && pAccount)
	{
		pAccount->AutoEquipSlots(true);
		pPlayer->InvalidateAttributeCache();
	}
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
	auto attID = static_cast<AttributeIdentifier>(AttributeID);
	auto totalAttribute = pPlayer->GetTotalRawAttributeValue(attID);
	pPlayer->UpdateTotalAttributeValue(attID, totalAttribute);
	m_AttributesTracker.UpdateTrackingDataIfNecessary(pPlayer, AttributeID, totalAttribute);
}

void CInventoryListener::OnPlayerProfessionChange(CPlayer* pPlayer, CProfession* pOldProf, CProfession* pNewProf)
{
	if(pPlayer && pOldProf != pNewProf)
	{
		pPlayer->Account()->AutoEquipSlots(true);
		pPlayer->InvalidateAttributeCache();
		UpdateAttributesFull(pPlayer);
	}
}

void CInventoryListener::OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
	UpdateAttributesForItem(pPlayer, pItem);
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
		UpdateAttributesForItem(pPlayer, pItem);

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
			UpdateAttributesForItem(pPlayer, pItem);
		}
	}
}

void CInventoryListener::OnPlayerUnequipItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
	UpdateAttributesForItem(pPlayer, pItem);
}

void CInventoryListener::OnPlayerEnchantItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
	UpdateAttributesForItem(pPlayer, pItem);
}

void CInventoryListener::OnPlayerChangePreset(CPlayer* pPlayer, int PresetSlotIndex)
{
	if (pPlayer)
	{
		pPlayer->InvalidateAttributeCache();
		UpdateAttributesFull(pPlayer);
	}
}

void CInventoryListener::OnCharacterSpawn(CPlayer* pPlayer)
{
	UpdateAttributesFull(pPlayer);
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

void CInventoryListener::UpdateAttributesForItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
	if(!pPlayer || !pItem || !pItem->Info())
		return;

	pPlayer->InvalidateAttributeCache();

	// update tracking attributes based on item attributes
	for(const auto& AttributeInfo : pItem->Info()->GetAttributes())
	{
		// update total player stats
		auto totalAttribute = pPlayer->GetTotalRawAttributeValue(AttributeInfo.GetID());
		pPlayer->UpdateTotalAttributeValue(AttributeInfo.GetID(), totalAttribute);
		m_AttributesTracker.UpdateTrackingDataIfNecessary(pPlayer, (int)AttributeInfo.GetID(), totalAttribute);
	}

	// update player stats when an item is equipped
	if(auto* pCharacter = pPlayer->GetCharacter())
	{
		pCharacter->UpdateEquippedStats(pItem->GetID());
		pPlayer->GS()->MarkUpdatedBroadcast(pPlayer->GetCID());
	}
}

void CInventoryListener::UpdateAttributesFull(CPlayer* pPlayer)
{
	if(!pPlayer)
		return;

	pPlayer->InvalidateAttributeCache();

	// update total player stats
	for(auto& [Id, Info] : CAttributeDescription::Data())
	{
		auto totalAttribute = pPlayer->GetTotalRawAttributeValue(Id);
		pPlayer->UpdateTotalAttributeValue(Id, totalAttribute);
		if(!pPlayer->IsBot())
			m_AttributesTracker.UpdateTrackingDataIfNecessary(pPlayer, (int)Id, totalAttribute);
	}

	// update player stats when an item is equipped
	if(auto* pCharacter = pPlayer->GetCharacter())
	{
		pCharacter->UpdateEquippedStats();
		pPlayer->GS()->MarkUpdatedBroadcast(pPlayer->GetCID());
	}
}


// attributes tracker
void CAttributesTracker::LoadTrackingData()
{
	ByteArray RawData;
	if(!mystd::file::load(ATTRIBUTE_TRACKING_FILE_NAME, &RawData))
	{
		SaveTrackingData();
		return;
	}

	std::string rawString = (char*)RawData.data();
	bool hasError = mystd::json::parse(rawString, [this](nlohmann::json& jsonData)
	{
		for(const auto& item : jsonData["tracking"])
		{
			int attributeID = item.value("attribute_id", -1);
			TrackingAttributeData data = item.value("detail", TrackingAttributeData {});
			m_vTrackingData[attributeID] = data;
		}
	});

	if(hasError)
	{
		dbg_msg("attributes_tracking", "Error with initialized '%s'. Creating new...", ATTRIBUTE_TRACKING_FILE_NAME);
		m_vTrackingData.clear();
		SaveTrackingData();
	}
}

void CAttributesTracker::SaveTrackingData()
{
	nlohmann::json j;
	for(const auto& [attributeID, data] : m_vTrackingData)
	{
		j["tracking"].push_back(
			{
				{"attribute_id", attributeID},
				{"detail", data}
			});
	}

	std::string Data = j.dump(4);
	auto Result = mystd::file::save(ATTRIBUTE_TRACKING_FILE_NAME, Data.data(), static_cast<unsigned>(Data.size()));
	if(Result != mystd::file::result::SUCCESSFUL)
	{
		dbg_msg("attributes_tracking", "Failed to save the attributes. Re-creating file.");
		mystd::file::remove(ATTRIBUTE_TRACKING_FILE_NAME);
		m_vTrackingData.clear();
		SaveTrackingData();
	}
}

void CAttributesTracker::UpdateTrackingDataIfNecessary(CPlayer* pPlayer, int AttributeID, size_t NewValue)
{
	auto& TrackingData = m_vTrackingData[AttributeID];
	auto* pGS = pPlayer->GS();
	bool isUpdateNeeded = false;

	// check and update for another player higher amount
	if(auto* pPlayerHighter = pGS->GetPlayerByUserID(TrackingData.AccountID))
	{
		if(pPlayerHighter->GetCID() != pPlayer->GetCID())
		{
			auto Current = pPlayerHighter->GetTotalAttributeValue((AttributeIdentifier)AttributeID);
			TrackingData.Amount = Current;
			isUpdateNeeded = true;
		}
	}

	// update player's personal amount
	if(TrackingData.Amount < NewValue)
	{
		TrackingData.AccountID = pPlayer->Account()->GetID();
		TrackingData.Amount = NewValue;
		isUpdateNeeded = true;
	}

	// save
	if(isUpdateNeeded)
	{
		SaveTrackingData();
	}
}

std::optional<TrackingAttributeData> CAttributesTracker::GetTrackingData(int AttributeID) const
{
	if(!m_vTrackingData.contains(AttributeID))
		return std::nullopt;

	return m_vTrackingData.at(AttributeID);
}