/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_PLANT_CORE_H
#define GAME_SERVER_COMPONENT_ACCOUNT_PLANT_CORE_H
#include <game/server/core/mmo_component.h>

class CAccountPlantManager : public MmoComponent
{
	~CAccountPlantManager() override
	{
		ms_aPlants.clear();
	};

	struct StructPlants
	{
		int m_ItemID;
		int m_Level;
		int m_StartHealth;
		vec2 m_Position;
		int m_Distance;
		int m_WorldID;
	};
	static std::map < int, StructPlants > ms_aPlants;

	void OnInitWorld(const char* pWhereLocalWorld) override;
	void OnInitAccount(CPlayer* pPlayer) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

public:
	int GetPlantLevel(vec2 Pos) const;
	int GetPlantItemID(vec2 Pos) const;
	int GetPlantHealth(vec2 Pos) const;

	//void ShowMenu(CPlayer* pPlayer) const;
	void Work(CPlayer* pPlayer, int Level);
	
	bool ShowGuideDropByWorld(int WorldID, CPlayer* pPlayer);
};

#endif