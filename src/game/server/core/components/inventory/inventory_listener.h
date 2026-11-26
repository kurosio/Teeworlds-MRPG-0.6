#ifndef GAME_SERVER_CORE_COMPONENTS_INVENTORY_INVENTORY_LISTENER_H
#define GAME_SERVER_CORE_COMPONENTS_INVENTORY_INVENTORY_LISTENER_H

#include <game/server/core/tools/event_listener.h>

class CPlayer;
class CPlayerItem;

class CInventoryListener : public IEventListener
{
public:
    void Initialize();

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
    void UpdateAttributes(CPlayer* pPlayer, CPlayerItem* pItem = nullptr);
};

extern CInventoryListener g_InventoryListener;

#endif