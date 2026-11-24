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
    if(!j.is_object())
        return;

    data.AccountID = j.value("account_id", 0);
    data.Amount = j.value("amount", 0);
}

// attributes tracker
class CAttributesTracker
{
    friend class CInventoryListener;
    bool m_Loaded = false;
    std::unordered_map<int, TrackingAttributeData> m_vTrackingData {};

    void UpdateTrackingDataIfNecessary(CPlayer* pPlayer, int AttributeID, size_t NewValue);
    void LoadTrackingData();
    void SaveTrackingData();

public:
    std::optional<TrackingAttributeData> GetTrackingData(int AttributeID) const;
    const std::unordered_map<int, TrackingAttributeData>& GetTrackings() const { return m_vTrackingData; }
};

// inventory listener
class CInventoryListener : public IEventListener
{
    CAttributesTracker m_AttributesTracker;

public:
    void Initialize();
    CAttributesTracker& AttributeTracker() { return m_AttributesTracker; }

protected:
    void OnPlayerLogin(CPlayer* pPlayer, CAccountData* pAccount) override;
    void OnPlayerGotItem(CPlayer* pPlayer, CPlayerItem* pItem, int Got) override;
    void OnPlayerEquipItem(CPlayer* pPlayer, CPlayerItem* pItem) override;
    void OnPlayerDurabilityItem(CPlayer* pPlayer, CPlayerItem* pItem, int OldDurability) override;
    void OnPlayerUnequipItem(CPlayer* pPlayer, CPlayerItem* pItem) override;
    void OnPlayerEnchantItem(CPlayer* pPlayer, CPlayerItem* pItem) override;
	void OnPlayerChangePreset(CPlayer* pPlayer, int PresetSlotIndex) override;
    void OnPlayerProfessionUpgrade(CPlayer* pPlayer, int AttributeID) override;
    void OnPlayerProfessionChange(CPlayer* pPlayer, CProfession* pOld, CProfession* pNew) override;
    void OnCharacterSpawn(CPlayer* pPlayer) override;
    void OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage) override;

private:
    void UpdateAttributesForItem(CPlayer* pPlayer, CPlayerItem* pItem);
    void UpdateAttributesFull(CPlayer* pPlayer);
};

extern CInventoryListener g_InventoryListener;

#endif