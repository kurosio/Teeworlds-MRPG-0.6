/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_DATA_H

#define TW_GUILDS_HOUSES "tw_guilds_houses"
#define TW_GUILD_HOUSES_DECORATION_TABLE "tw_guilds_decorations"

class CGS;
class CPlayer;
class CGuild;
class CDrawingData;
class CEntityHouseDecoration;
class CEntityDrawboard;
class CEntityGuildDoor;
class EntityPoint;
class CJobItems;
using GuildHouseIdentifier = int;

class CGuildHouse : public MultiworldIdentifiableStaticData< std::deque < CGuildHouse* > >
{
	friend class CGuildHouseDoorManager;
	friend class CGuildHouseDecorationManager;
	friend class CPlantzonesManager;

public:
	/* -------------------------------------
	 * Plantzones impl
	 * ------------------------------------- */
	class CPlantzonesManager;
	class CPlantzone
	{
		CPlantzonesManager* m_pManager {};
		std::string m_Name {};
		int m_ItemID {};
		vec2 m_Pos {};
		float m_Radius {};
		std::vector<CJobItems*> m_vPlants {};

	public:
		CPlantzone() = delete;
		CPlantzone(CPlantzonesManager* pManager, std::string&& Name, int ItemID, vec2 Pos, float Radius) : m_pManager(pManager)
		{
			m_Name = std::move(Name);
			m_ItemID = ItemID;
			m_Pos = Pos;
			m_Radius = Radius;
		}

		const char* GetName() const { return m_Name.c_str(); }
		float GetRadius() const { return m_Radius; }
		int GetItemID() const { return m_ItemID; }
		vec2 GetPos() const { return m_Pos; }
		std::vector<CJobItems*>& GetContainer() { return m_vPlants; }

		void ChangeItem(int ItemID);
		void Add(CJobItems* pItem) { m_vPlants.push_back(pItem); }
		void Remove(CJobItems* pItem) { m_vPlants.erase(std::remove(m_vPlants.begin(), m_vPlants.end(), pItem), m_vPlants.end()); }
	};

	class CPlantzonesManager
	{
		friend class CPlantzone;
		CGS* GS() const;
		CGuildHouse* m_pHouse {};
		std::unordered_map<int, CPlantzone> m_vPlantzones {};

	public:
		CPlantzonesManager() = delete;
		CPlantzonesManager(CGuildHouse* pHouse, std::string&& JsPlantzones);
		~CPlantzonesManager();

		std::unordered_map<int, CPlantzone>& GetContainer() { return m_vPlantzones; }

		void AddPlantzone(CPlantzone&& Plantzone);
		CPlantzone* GetPlantzoneByPos(vec2 Pos);
		CPlantzone* GetPlantzoneByID(int ID);

	private:
		void Save() const;
	};

	/* -------------------------------------
	 * Decorations impl
	 * ------------------------------------- */
	using DecorationIdentifier = int;
	using DecorationsContainer = std::vector<CEntityHouseDecoration*>;
	class CDecorationManager
	{
		CGS* GS() const;
		CEntityDrawboard* m_pDrawBoard {};
		CGuildHouse* m_pHouse {};

	public:
		CDecorationManager() = delete;
		CDecorationManager(CGuildHouse* pHouse);
		~CDecorationManager();

		bool StartDrawing(CPlayer* pPlayer) const;
		bool HasFreeSlots() const;

	private:
		void Init();
		static bool DrawboardToolEventCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser);
		bool Add(const EntityPoint* pPoint) const;
		bool Remove(const EntityPoint* pPoint) const;
	};

	/* -------------------------------------
	 * Doors impl
	 * ------------------------------------- */
	class CDoorManager
	{
		CGS* GS() const;
		CGuildHouse* m_pHouse {};
		ska::unordered_map<int, CEntityGuildDoor*> m_apEntDoors {};

	public:
		CDoorManager() = delete;
		CDoorManager(CGuildHouse* pHouse);
		~CDoorManager();

		ska::unordered_map<int, CEntityGuildDoor*>& GetContainer() { return m_apEntDoors; }
		void Open(int Number); // Open a specific door
		void Close(int Number); // Close a specific door
		void Reverse(int Number); // Reverse the state of a specific door
		void OpenAll(); // Open all doors
		void CloseAll(); // Close all doors
		void ReverseAll(); // Reverse the state of all doors
		void AddDoor(const char* pDoorname, vec2 Position);
		void RemoveDoor(const char* pDoorname, vec2 Position);
	};

private:
	CGS* GS() const;

	CGuild* m_pGuild {};
	GuildHouseIdentifier m_ID{};
	float m_Radius {};
	vec2 m_Position{};
	vec2 m_TextPosition{};
	int m_Price {};
	int m_WorldID{};
	int m_LastTickTextUpdated {};

	CDoorManager* m_pDoors {};
	CDecorationManager* m_pDecorations {};
	CPlantzonesManager* m_pPlantzones {};

public:
	CGuildHouse() = default;
	~CGuildHouse();

	static CGuildHouse* CreateElement(const GuildHouseIdentifier& ID)
	{
		auto pData = new CGuildHouse;
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(CGuild* pGuild, int Price, int WorldID, std::string&& JsPlantzones, std::string&& JsProperties)
	{
		m_Price = Price;
		m_WorldID = WorldID;

		InitProperties(std::move(JsPlantzones), std::move(JsProperties));
		UpdateGuild(pGuild);
	}

	void InitProperties(std::string&& Plantzones, std::string&& Properties);

	CGuild* GetGuild() const { return m_pGuild; }
	CDoorManager* GetDoorManager() const { return m_pDoors; }
	CDecorationManager* GetDecorationManager() const { return m_pDecorations; }
	CPlantzonesManager* GetPlantzonesManager() const { return m_pPlantzones; }

	GuildHouseIdentifier GetID() const { return m_ID; }
	vec2 GetPos() const { return m_Position; }
	float GetRadius() const { return m_Radius; }
	int GetWorldID() const { return m_WorldID; }
	int GetPrice() const { return m_Price; }
	int GetRentPrice() const;
	void GetRentTimeStamp(char* aBuffer, size_t Size) const;
	bool IsPurchased() const { return m_pGuild != nullptr; }
	const char* GetOwnerName() const;

	void TextUpdate(int LifeTime);
	void UpdateGuild(CGuild* pGuild);
};

#endif
