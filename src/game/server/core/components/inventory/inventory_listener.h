#ifndef GAME_SERVER_CORE_COMPONENTS_INVENTORY_INVENTORY_LISTENER_H
#define GAME_SERVER_CORE_COMPONENTS_INVENTORY_INVENTORY_LISTENER_H

#include <game/server/core/tools/event_listener.h>

class CGS;
class CPlayer;
class CPlayerItem;

struct TrackingAttributeData
{
    int AccountID {};
    size_t Amount {};
};

inline void to_json(nlohmann::json& j, const TrackingAttributeData& data)
{
    j = nlohmann::json::object({
        {"account_id", data.AccountID},
        {"amount", data.Amount}
        });
}

inline void from_json(const nlohmann::json& j, TrackingAttributeData& data)
{
    j.at("account_id").get_to(data.AccountID);
    j.at("amount").get_to(data.Amount);
}

// attributes tracker
class CAttributesTracker
{
    friend class CInventoryListener;
    std::unordered_map<int, TrackingAttributeData> m_vTrackingData {};

    void UpdateTrackingDataIfNecessary(CPlayer* pPlayer, int AttributeID, size_t NewValue);
    void LoadTrackingData();
    void SaveTrackingData();

public:
    std::optional<TrackingAttributeData> GetTrackingData(int AttributeID) const;
};

// inventory listener
class CInventoryListener : public IEventListener
{
    CAttributesTracker m_AttributesTracker;

public:
    void Initialize();
    CAttributesTracker& AttributeTracker() { return m_AttributesTracker; }

protected:
    void OnCharacterSpawn(CPlayer* pPlayer) override;
    void OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem) override;
    void OnPlayerUnequipItem(CPlayer* pPlayer, CPlayerItem* pItem) override;
    void OnPlayerEnchantItem(CPlayer* pPlayer, CPlayerItem* pItem) override;
    void OnPlayerProfessionUpgrade(CPlayer* pPlayer, int AttributeID) override;

private:
    void UpdateAttributesForItem(CPlayer* pPlayer, CPlayerItem* pItem);
};

extern CInventoryListener g_InventoryListener;

#endif