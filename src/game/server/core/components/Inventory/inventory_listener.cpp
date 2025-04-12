#include "inventory_listener.h"
#include <game/server/gamecontext.h>

CInventoryListener g_InventoryListener;
constexpr const char* ATTRIBUTE_TRACKING_FILE_NAME = "server_data/attribute_tracking.json";

// inventory listener
void CInventoryListener::Initialize()
{
    g_EventListenerManager.RegisterListener(IEventListener::CharacterSpawn, this);
    g_EventListenerManager.RegisterListener(IEventListener::PlayerProfessionUpgrade, this);
    g_EventListenerManager.RegisterListener(IEventListener::PlayerEquipItem, this);
    g_EventListenerManager.RegisterListener(IEventListener::PlayerUnequipItem, this);
    g_EventListenerManager.RegisterListener(IEventListener::PlayerEnchantItem, this);

    m_AttributesTracker.LoadTrackingData();
}

void CInventoryListener::OnCharacterSpawn(CPlayer* pPlayer)
{
    if(pPlayer->IsBot())
        return;

    struct AttributeData
    {
        AttributeIdentifier identifier;
        int weaponType;
        int orbiteType;
    };

    const auto AccountID = pPlayer->Account()->GetID();
    bool bestExpert = false;
    std::array<AttributeData, 7> attributesData = {
        AttributeData{AttributeIdentifier::HP, WEAPON_SHOTGUN, MULTIPLE_ORBITE_TYPE_EIGHT},
        AttributeData{AttributeIdentifier::MP, WEAPON_SHOTGUN, MULTIPLE_ORBITE_TYPE_DYNAMIC_CENTER},
        AttributeData{AttributeIdentifier::CritDMG, WEAPON_SHOTGUN, MULTIPLE_ORBITE_TYPE_PULSATING},
        AttributeData{AttributeIdentifier::Efficiency, WEAPON_HAMMER, MULTIPLE_ORBITE_TYPE_EIGHT},
        AttributeData{AttributeIdentifier::Extraction, WEAPON_HAMMER, MULTIPLE_ORBITE_TYPE_DYNAMIC_CENTER},
        AttributeData{AttributeIdentifier::Patience, WEAPON_HAMMER, MULTIPLE_ORBITE_TYPE_PULSATING},
        AttributeData{AttributeIdentifier::ProductCapacity, WEAPON_HAMMER, MULTIPLE_ORBITE_TYPE_ELLIPTICAL}
    };

    for(const auto& attribute : attributesData)
    {
        const auto& Biggest = m_AttributesTracker.GetTrackingData((int)attribute.identifier);
        if(Biggest && AccountID == (*Biggest).AccountID)
        {
            pPlayer->GetCharacter()->AddMultipleOrbite(true, 2, attribute.weaponType, 0, attribute.orbiteType);
            bestExpert = true;
        }
    }

    if(bestExpert)
    {
        pPlayer->GS()->Chat(pPlayer->GetCID(), "You are the top expert in your profession on the server. Visual highlighting is now active.");
    }
}

void CInventoryListener::OnPlayerProfessionUpgrade(CPlayer* pPlayer, int AttributeID)
{
    // Update tracking data when a player upgrades an attribute
    auto totalAttribute = pPlayer->GetTotalAttributeValue(static_cast<AttributeIdentifier>(AttributeID));
    m_AttributesTracker.UpdateTrackingDataIfNecessary(pPlayer, AttributeID, totalAttribute);
}

void CInventoryListener::OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
    UpdateAttributesForItem(pPlayer, pItem);
}

void CInventoryListener::OnPlayerUnequipItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
    UpdateAttributesForItem(pPlayer, pItem);
}

void CInventoryListener::OnPlayerEnchantItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
    UpdateAttributesForItem(pPlayer, pItem);
}

void CInventoryListener::UpdateAttributesForItem(CPlayer* pPlayer, CPlayerItem* pItem)
{
    // update player stats when an item is equipped
    if(auto* pCharacter = pPlayer->GetCharacter())
    {
        pCharacter->UpdateEquippedStats(pItem->GetID());
        pPlayer->GS()->MarkUpdatedBroadcast(pPlayer->GetCID());
    }

    // update tracking attributes based on item attributes
    for(const auto& AttributeInfo : pItem->Info()->GetAttributes())
    {
        auto totalAttribute = pPlayer->GetTotalAttributeValue(AttributeInfo.GetID());
        m_AttributesTracker.UpdateTrackingDataIfNecessary(pPlayer, (int)AttributeInfo.GetID(), totalAttribute);
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

    auto j = nlohmann::json::parse((char*)RawData.data());
    for(const auto& item : j["tracking"])
    {
        int attributeID = item.value("attribute_id", -1);
        TrackingAttributeData data = item.value("detail", TrackingAttributeData {});
        m_vTrackingData[attributeID] = data;
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
        dbg_msg("attributes_tracking", "Failed to save the attributes.");
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