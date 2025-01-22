#ifndef GAME_SERVER_CORE_ENTITIES_ITEMS_GATHERING_NODE_H
#define GAME_SERVER_CORE_ENTITIES_ITEMS_GATHERING_NODE_H

#include <game/server/entity.h>

class CPlayer;
class CPlayerItem;
class CItemDescription;
class CProfession;
struct GatheringNode;

constexpr int PickupPhysSize = 14;

class CEntityGatheringNode : public CEntity
{
	int m_CurrentHealth{};
	int m_SpawnTick{};
	int m_Type{};
	GatheringNode* m_pNode {};

public:
	enum
	{
		GATHERING_NODE_NONE = -1,
		GATHERING_NODE_PLANT,
		GATHERING_NODE_ORE
	};

	CEntityGatheringNode(CGameWorld *pGameWorld, GatheringNode* pNode, vec2 Pos, int Type);

	void SpawnPositions();
	void SetSpawn(int Sec);
	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	bool TakeDamage(CPlayer* pPlayer);

private:
	int GetPickupType() const;
	void Die(CPlayer* pPlayer, CProfession* pProfession);
};

#endif
