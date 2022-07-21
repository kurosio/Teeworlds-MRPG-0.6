/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_JOBITEMS_H
#define GAME_SERVER_ENTITIES_JOBITEMS_H
#include <game/server/entity.h>

class CPlayer;
class CItemData;
const int PickupPhysSize = 14;

class CJobItems : public CEntity
{
public:
	enum
	{
		JOB_ITEM_FARMING = 0,
		JOB_ITEM_MINING = 1
	};

	CJobItems(CGameWorld *pGameWorld, int ItemID, int Level, vec2 Pos, int Type, int Health, int HouseID = -1);

	void Reset() override;
	void Tick() override;
	virtual void TickPaused();
	void Snap(int SnappingClient) override;

	void SetSpawn(int Sec);
	void Work(int ClientID);
	void SpawnPositions();

	int m_ItemID;
	int m_HouseID;
private:
	int m_Level;
	int m_TotalDamage;
	int m_Health;
	int m_SpawnTick;
	int m_Type;

	void FarmingWork(int ClientID, CPlayer* pPlayer, CItemData& pWorkedItem);
	void MiningWork(int ClientID, CPlayer* pPlayer, CItemData& pWorkedItem);
	int SwitchToObject(bool MmoItem) const;
};

#endif
