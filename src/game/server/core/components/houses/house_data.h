/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_DATA_H

#include <game/server/core/components/Inventory/ItemData.h>

#define TW_HOUSES_TABLE "tw_houses"
#define TW_HOUSES_DECORATION_TABLE "tw_houses_decorations"

class CGS;
class CPlayer;
class EntityPoint;
class CEntityDrawboard;
class CEntityHouseDoor;
using HouseIdentifier = int;
using HouseDecorationIdentifier = int;
using HouseDataPtr = std::shared_ptr< class CHouseData >;

class CHouseData : public MultiworldIdentifiableStaticData< std::deque < HouseDataPtr > >
{
public:
	/* -------------------------------------
	 * Bank impl
	 * ------------------------------------- */
	class CBank
	{
		CGS* GS() const;
		CPlayer* GetPlayer() const;
		CHouseData* m_pHouse{};
		int m_Bank {};

	public:
		CBank(CHouseData* pHouse, int Bank) : m_pHouse(pHouse), m_Bank(Bank) {}

		int Get() const { return m_Bank; }   // Returns the current bank value
		void Add(int Value);                 // Adds the specified value to the bank
		void Take(int Value);                // Takes the specified value from the bank
		void Reset() { m_Bank = 0; }         // Resets the bank value to 0
	};

	/* -------------------------------------
	 * Doors impl
	 * ------------------------------------- */
	class CDoorManager
	{
		friend class CHouseData;
		CGS* GS() const;
		CPlayer* GetPlayer() const;
		CHouseData* m_pHouse {};
		ska::unordered_map<int, CEntityHouseDoor*> m_apEntDoors {};
		ska::unordered_set<int> m_AccessUserIDs {};

	public:
		CDoorManager(CHouseData* pHouse, std::string&& AccessData, std::string&& JsonDoors);
		~CDoorManager();
		
		ska::unordered_set<int>& GetAccesses() { return m_AccessUserIDs; }               // Get the set of user IDs with access to the house
		ska::unordered_map<int, CEntityHouseDoor*>& GetContainer() { return m_apEntDoors; }  // Get the map of door numbers to CHouseDoor objects
		void AddAccess(int UserID);                                                      // Add access for a user
		void RemoveAccess(int UserID);                                                   // Remove access for a user
		bool HasAccess(int UserID);                                                      // Check if a user has access
		int GetAvailableAccessSlots() const;                                             // Get the number of available access slots
		void Open(int Number);                                                           // Open a specific door
		void Close(int Number);                                                          // Close a specific door
		void Reverse(int Number);                                                        // Reverse the state of a specific door
		void OpenAll();                                                                  // Open all doors
		void CloseAll();                                                                 // Close all doors
		void ReverseAll();                                                               // Reverse the state of all doors
		void AddDoor(const char* pDoorname, vec2 Position);
		void RemoveDoor(const char* pDoorname, vec2 Position);

	private:
		void SaveAccessList() const;                                                     // Save the access list to a file
	};

private:
	HouseIdentifier m_ID {};
	vec2 m_Pos {};
	vec2 m_TextPos {};
	vec2 m_PlantPos {};

	CEntityDrawboard* m_pDrawBoard {};
	CDoorManager* m_pDoorManager {};
	CBank* m_pBank {};
	CItem m_PlantedItem {};

	char m_aClassName[32] {};
	int m_AccountID {};
	int m_WorldID {};
	int m_Price {};

	int m_LastTickTextUpdated{};

	class CGS* GS() const;
	class CPlayer* GetPlayer() const;

public:
	CHouseData() = default;
	~CHouseData();

	static HouseDataPtr CreateElement(HouseIdentifier ID)
	{
		HouseDataPtr pData = std::make_shared<CHouseData>();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(int AccountID, std::string ClassName, int Price, int Bank, vec2 Pos, vec2 TextPos, vec2 PlantPos, CItem&& PlantedItem, int WorldID, std::string&& AccessSet, std::string&& JsonDoorData)
	{
		m_AccountID = AccountID;
		str_copy(m_aClassName, ClassName.c_str(), sizeof(m_aClassName));
		m_Price = Price;
		m_Pos = Pos;
		m_TextPos = TextPos;
		m_PlantPos = PlantPos;
		m_WorldID = WorldID;
		m_PlantedItem = std::move(PlantedItem);
		
		// initialize components
		m_pDoorManager = new CDoorManager(this, std::move(AccessSet), std::move(JsonDoorData));
		m_pBank = new CBank(this, Bank);

		// init decoration
		InitDecorations();
	}

	HouseIdentifier GetID() const { return m_ID; }
	int GetAccountID() const { return m_AccountID; }
	const char* GetClassName() const { return m_aClassName; }
	const vec2& GetPos() const { return m_Pos; }
	const vec2& GetPlantPos() const { return m_PlantPos; }
	bool HasOwner() const { return m_AccountID > 0; }
	int GetPrice() const { return m_Price; }
	int GetWorldID() const { return m_WorldID; }
	CItem* GetPlantedItem() { return &m_PlantedItem; }
	CDoorManager* GetDoorManager() const { return m_pDoorManager; }
	CBank* GetBank() const { return m_pBank; }

	// A decoration functions
	bool StartDrawing() const;
	static bool DrawboardToolEventCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser);
	bool AddDecoration(const EntityPoint* pPoint) const;
	bool RemoveDecoration(const EntityPoint* pPoint) const;

	// House functions
	void Buy(CPlayer* pPlayer);
	void Sell();

	// Planting functions
	void SetPlantItemID(ItemIdentifier ItemID);

	// Text update house
	void TextUpdate(int LifeTime);

private:
	// Function to initialize decorations
	void InitDecorations();
};

#endif