/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_DATA_H

#include <game/server/mmocore/Components/Inventory/ItemData.h>

#include "HouseBankData.h"
#include "HouseDoorData.h"

#define TW_HOUSES_TABLE "tw_houses"
#define TW_HOUSES_DECORATION_TABLE "tw_houses_decorations"

using HouseIdentifier = int;
using HouseDecorationIdentifier = int;
using HouseDataPtr = std::shared_ptr< class CHouseData >;

class CHouseData : public MultiworldIdentifiableStaticData< std::deque < HouseDataPtr > >
{
	HouseIdentifier m_ID {};
	vec2 m_Pos {};
	vec2 m_PlantPos {};

	class CDecorationHouses* m_apDecorations[MAX_DECORATIONS_HOUSE] {};
	CHouseDoorData* m_pDoorData {};
	CHouseBankData* m_pBank {};
	CItem m_PlantedItem {};

	char m_aClassName[32] {};
	int m_AccountID {};
	int m_WorldID {};
	int m_Price {};

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

	void Init(int AccountID, std::string ClassName, int Price, int Bank, vec2 Pos, vec2 HouseDoorPos, vec2 PlantPos, CItem&& PlantedItem, int WorldID, std::string AccessSet)
	{
		m_AccountID = AccountID;
		str_copy(m_aClassName, ClassName.c_str(), sizeof(m_aClassName));
		m_Price = Price;
		m_Pos = Pos;
		m_PlantPos = PlantPos;
		m_WorldID = WorldID;
		m_PlantedItem = std::move(PlantedItem);

		// door init
		m_pDoorData = new CHouseDoorData(GS(), HouseDoorPos, std::move(AccessSet), this);
		if(m_AccountID <= 0)
		{
			m_pDoorData->Open();
		}
		else
		{
			m_pDoorData->Close();
		}
		
		// bank init
		m_pBank = new CHouseBankData(GS(), &m_AccountID, Bank);

		// init decoration
		InitDecorations();
	}

	// Returns the ID of the house
	HouseIdentifier GetID() const { return m_ID; }

	// Returns the account ID associated with the house
	int GetAccountID() const { return m_AccountID; }

	// Returns the class name of the house
	const char* GetClassName() const { return m_aClassName; }

	// Returns the position of the house
	const vec2& GetPos() const { return m_Pos; }

	// Returns the position of the plant in the house
	const vec2& GetPlantPos() const { return m_PlantPos; }

	// Checks if the house has an owner
	bool HasOwner() const { return m_AccountID > 0; }

	// Returns the price of the house
	int GetPrice() const { return m_Price; }

	// Returns the world ID of the house
	int GetWorldID() const { return m_WorldID; }

	// Return a pointer to the planted item
	CItem* GetPlantedItem() { return &m_PlantedItem; }

	// Return a pointer to the door data of the house
	CHouseDoorData* GetDoor() const { return m_pDoorData; }

	// Return a pointer to the bank data of the house
	CHouseBankData* GetBank() const { return m_pBank; }

	// A decoration functions
	bool AddDecoration(ItemIdentifier ItemID, vec2 Pos);
	bool RemoveDecoration(HouseDecorationIdentifier DecoID);
	void ShowDecorationList() const;

	// House functions
	void Buy(CPlayer* pPlayer);
	void Sell();

	// Planting functions
	void SetPlantItemID(ItemIdentifier ItemID);

private:
	// Function to initialize decorations
	void InitDecorations();
};

#endif