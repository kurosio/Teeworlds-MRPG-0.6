/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_FARMING_CORE_H
#define GAME_SERVER_COMPONENT_ACCOUNT_FARMING_CORE_H
#include <game/server/core/mmo_component.h>

class CItemDescription;

class CAccountFarmingManager : public MmoComponent
{
	~CAccountFarmingManager() override
	{
		ms_vmFarmingPoints.clear();
	};

	struct FarmingPoint
	{
		int m_ItemID;
		vec2 m_Position;
		int m_Distance;
		int m_WorldID;
	};
	static std::map < int, FarmingPoint > ms_vmFarmingPoints;

	void OnInitWorld(const char* pWhereLocalWorld) override;

public:
	CItemDescription* GetFarmingItemInfoByPos(vec2 Pos) const;

	//void ShowMenu(CPlayer* pPlayer) const;
	bool InsertItemsDetailVotes(CPlayer* pPlayer, int WorldID);
};

#endif