#ifndef GAME_SERVER_COMPONENT_HOUSES_BASE_FARMZONE_MANAGER_H
#define GAME_SERVER_COMPONENT_HOUSES_BASE_FARMZONE_MANAGER_H

#include "interface_house.h"

class CGS;
class CFarmzonesManager;
class CEntityGatheringNode;
class CFarmzone
{
	friend class CFarmzonesManager;
	GatheringNode m_Node {};
	CFarmzonesManager* m_pManager {};
	vec2 m_Pos {};
	float m_Radius {};
	std::vector<CEntityGatheringNode*> m_vFarms {};

public:
	CFarmzone() = delete;
	CFarmzone(CFarmzonesManager* pManager, const std::string& Name, const DBSet& ItemsSet, vec2 Pos, float Radius);

	const char* GetName() const { return m_Node.Name.c_str(); }
	float GetRadius() const { return m_Radius; }
	vec2 GetPos() const { return m_Pos; }
	GatheringNode& GetNode() { return m_Node; }

	void AddItemToNode(int ItemID);
	bool RemoveItemFromNode(int ItemID);
	void Add(CGS* pGS, const vec2& Pos);

private:
	void NormalizeHealth();
};

class CFarmzonesManager
{
	friend class CFarmzone;
	IHouse* m_pHouse {};
	std::unordered_map<int, CFarmzone> m_vFarmzones {};

public:
	CFarmzonesManager() = delete;
	CFarmzonesManager(IHouse* pHouse, const std::string& JsonFarmzones);
	~CFarmzonesManager();

	std::unordered_map<int, CFarmzone>& GetContainer() { return m_vFarmzones; }

	void AddFarmzone(const std::string& Farmname, const DBSet& ItemSet, vec2 Pos, float Radius);
	CFarmzone* GetFarmzoneByPos(vec2 Pos);
	CFarmzone* GetFarmzoneByID(int ID);
	void Save() const;
};

#endif