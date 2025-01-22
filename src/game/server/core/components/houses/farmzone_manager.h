/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_FARMZONE_MANAGER_H
#define GAME_SERVER_COMPONENT_HOUSE_FARMZONE_MANAGER_H

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
	CFarmzone(CFarmzonesManager* pManager, std::string&& Name, const DBSet& ItemsSet, vec2 Pos, float Radius) : m_pManager(pManager)
	{
		for(auto& [str, size] : ItemsSet.GetDataItems())
		{
			try
			{
				int itemID = std::stoi(str);
				AddItemToNode(itemID);
			}
			catch(const std::invalid_argument& e)
			{
				dbg_msg("house_farmzone", "%s", e.what());
			}
		}

		m_Node.Name = std::move(Name);
		m_Node.Level = 1;
		m_Node.Health = 100;
		m_Pos = Pos;
		m_Radius = Radius;
	}

	const char* GetName() const { return m_Node.Name.c_str(); }
	int GetItemID() const { return m_Node.m_vItems.getRandomElement(); }
	float GetRadius() const { return m_Radius; }
	vec2 GetPos() const { return m_Pos; }

	GatheringNode& GetNode() { return m_Node; }

	void AddItemToNode(int ItemID);
	bool RemoveItemFromNode(int ItemID);

	void Add(CGS* pGS, const vec2& Pos);
};

class CFarmzonesManager
{
	friend class CFarmzone;
	std::unordered_map<int, CFarmzone> m_vFarmzones {};

public:
	CFarmzonesManager() = delete;
	explicit CFarmzonesManager(std::string&& JsonFarmzones);
	~CFarmzonesManager();

	std::unordered_map<int, CFarmzone>& GetContainer() { return m_vFarmzones; }

	void AddFarmzone(CFarmzone&& Farmzone);
	CFarmzone* GetFarmzoneByPos(vec2 Pos);
	CFarmzone* GetFarmzoneByID(int ID);
	std::string DumpJsonString() const;
};

#endif