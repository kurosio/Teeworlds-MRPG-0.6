/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_ENTITIES_ITEMS_HARVESTING_ITEM_H
#define GAME_SERVER_CORE_ENTITIES_ITEMS_HARVESTING_ITEM_H
#include <game/server/entity.h>

class CPlayer;
class CPlayerItem;
class CItemDescription;

constexpr int PickupPhysSize = 14;

class CEntityHarvestingItem : public CEntity
{
	int m_Level{};
	int m_Health{};
	int m_Damage{};
	int m_SpawnTick{};
	int m_Type{};

public:
	int m_ItemID {};
	int m_HouseID {};

	enum
	{
		HARVESTINGITEM_TYPE_NONE = -1,
		HARVESTINGITEM_TYPE_FARMING = 0,
		HARVESTINGITEM_TYPE_MINING = 1
	};

	CEntityHarvestingItem(CGameWorld *pGameWorld, int ItemID, vec2 Pos, int Type, int HouseID = -1);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;

	void SetSpawn(int Sec);
	void Process(int ClientID);
	void SpawnPositions();

private:
	bool TakeDamage(AttributeIdentifier Attribute, CPlayer* pPlayer, const CPlayerItem* pWorkedItem, ItemType EquipID, int SelfLevel);
	int GetPickupType() const;
	CItemDescription* GetItemInfo() const;
};

#endif
