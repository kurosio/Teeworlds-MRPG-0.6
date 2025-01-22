/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_DATA_H

#include "farmzone_manager.h"
#include <game/server/core/components/Inventory/ItemData.h>

#define TW_HOUSES_TABLE "tw_houses"
#define TW_HOUSES_DECORATION_TABLE "tw_houses_decorations"

class CGS;
class CPlayer;
class CEntityGatheringNode;
class CDrawingData;
class CEntityDrawboard;
class EntityPoint;
class CEntityHouseDecoration;
class CEntityHouseDoor;
using HouseIdentifier = int;

class CHouse : public MultiworldIdentifiableData< std::deque < CHouse* > >
{
	CGS* GS() const;
	CPlayer* GetPlayer() const;

public:

	/* -------------------------------------
	 * Bank impl
	 * ------------------------------------- */
	class CBank
	{
		CGS* GS() const;
		CPlayer* GetPlayer() const;
		CHouse* m_pHouse{};
		int m_Bank {};

	public:
		CBank(CHouse* pHouse, int Bank) : m_pHouse(pHouse), m_Bank(Bank) {}

		int Get() const { return m_Bank; }   // Returns the current bank value
		void Add(int Value);                 // Adds the specified value to the bank
		void Take(int Value);                // Takes the specified value from the bank
		bool Spend(int Value);               // Spends the specified value from the bank
		void Reset() { m_Bank = 0; }         // Resets the bank value to 0
	};

	/* -------------------------------------
	 * Doors impl
	 * ------------------------------------- */
	class CDoorManager
	{
		friend class CHouse;
		CGS* GS() const;
		CPlayer* GetPlayer() const;
		CHouse* m_pHouse {};
		ska::unordered_map<int, CEntityHouseDoor*> m_vpEntDoors {};
		ska::unordered_set<int> m_vAccessUserIDs {};

	public:
		CDoorManager(CHouse* pHouse, std::string&& AccessList, std::string&& JsonDoors);
		~CDoorManager();

		ska::unordered_set<int>& GetAccesses() { return m_vAccessUserIDs; }               // Get the set of user IDs with access to the house
		ska::unordered_map<int, CEntityHouseDoor*>& GetContainer() { return m_vpEntDoors; }  // Get the map of door numbers to CHouseDoor objects
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

	/* -------------------------------------
	 * Decoration impl
	 * ------------------------------------- */
	using DecorationIdentifier = int;
	using DecorationsContainer = std::vector<CEntityHouseDecoration*>;
	class CDecorationManager
	{
		CGS* GS() const;
		CEntityDrawboard* m_pDrawBoard {};
		CHouse* m_pHouse {};

	public:
		CDecorationManager() = delete;
		CDecorationManager(CHouse* pHouse);
		~CDecorationManager();

		bool StartDrawing(CPlayer* pPlayer) const;
		bool HasFreeSlots() const;

	private:
		void Init();
		static bool DrawboardToolEventCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser);
		bool Add(const EntityPoint* pPoint) const;
		bool Remove(const EntityPoint* pPoint) const;
	};

private:
	CFarmzonesManager* m_pFarmzonesManager{};
	CDecorationManager* m_pDecorationManager{};
	CDoorManager* m_pDoorManager {};
	CBank* m_pBank {};

	HouseIdentifier m_ID {};
	vec2 m_Position {};
	vec2 m_TextPosition {};
	vec2 m_FarmPos {};
	float m_Radius {};
	char m_aClass[32] {};
	int m_AccountID {};
	int m_WorldID {};
	int m_Price {};

public:
	CHouse() = default;
	~CHouse();

	static CHouse* CreateElement(HouseIdentifier ID)
	{
		auto pData = new CHouse();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(int AccountID, std::string Class, int Price, int Bank, int WorldID, std::string AccessDoorList, std::string&& JsonDoors, std::string&& JsonFarmzones, std::string&& JsonProperties)
	{
		m_AccountID = AccountID;
		str_copy(m_aClass, Class.c_str(), sizeof(m_aClass));
		m_Price = Price;
		m_WorldID = WorldID;

		// init decoration
		InitProperties(Bank, std::move(AccessDoorList), std::move(JsonDoors), std::move(JsonFarmzones), std::move(JsonProperties));
	}

	CDoorManager* GetDoorManager() const { return m_pDoorManager; }
	CBank* GetBank() const { return m_pBank; }
	CDecorationManager* GetDecorationManager() const { return m_pDecorationManager; }
	CFarmzonesManager* GetFarmzonesManager() const { return m_pFarmzonesManager; }

	HouseIdentifier GetID() const { return m_ID; }
	int GetAccountID() const { return m_AccountID; }
	const char* GetClassName() const { return m_aClass; }
	const vec2& GetPos() const { return m_Position; }
	const vec2& GetFarmPos() const { return m_FarmPos; }
	float GetRadius() const { return m_Radius; }
	bool HasOwner() const { return m_AccountID > 0; }
	int GetPrice() const { return m_Price; }
	int GetWorldID() const { return m_WorldID; }
	int GetRentPrice() const;

	void InitProperties(int Bank, std::string&& AccessDoorList, std::string&& JsonDoors, std::string&& JsonFarmzones, std::string&& JsonProperties);
	void Buy(CPlayer* pPlayer);
	void Sell();
	void UpdateText(int Lifetime) const;
	void HandleTimePeriod(ETimePeriod Period);

	void Save();
};

#endif