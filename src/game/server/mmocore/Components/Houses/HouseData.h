/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_DATA_H

#include <game/server/mmocore/Components/Inventory/ItemInfoData.h>

#include "game/server/mmocore/GameEntities/decoration_houses.h"

using HouseIdentifier = int;
using HouseDecorationIdentifier = int;

using HouseDataPtr = std::shared_ptr< class CHouseData >;

/*
 * BANK DATA
 */
class CHouseBankData
{
	CGS* m_pGS;
	int* m_pAccountID{};
	int m_Bank{};

	class CPlayer* GetPlayer() const;

public:
	CHouseBankData() = default;
	CHouseBankData(CGS* pGS, int* pAccountID, int Bank) : m_pGS(pGS), m_pAccountID(pAccountID), m_Bank(Bank) {};

	int Get() const { return m_Bank; }

	void Add(int Value);
	void Take(int Value);
	void Reset() { m_Bank = 0; }
};

/*
 * DOOR DATA
 */
class CHouseDoorData
{
	friend class CHouseData;
	class CGS* m_pGS {};

	vec2 m_Pos{};
	class HouseDoor* m_pDoor{};

public:
	CHouseDoorData() = default;
	CHouseDoorData(class CGS* pGS, vec2 Pos) : m_pGS(pGS), m_Pos(Pos) {}
	~CHouseDoorData();

	const vec2& GetPos() const { return m_Pos; }
	bool GetState() const { return m_pDoor; }

	void Open();
	void Close();
	void Reverse();
};

/*
 * HOUSE DATA
 */
class CHouseData : public MultiworldIdentifiableStaticData< std::deque < HouseDataPtr > >
{
	HouseIdentifier m_ID{};
	vec2 m_Pos{};
	vec2 m_PlantPos{};
	CHouseDoorData* m_pDoorData{};
	CHouseBankData* m_pBank{};
	char m_aClassName[32]{};
	int m_AccountID{};
	int m_WorldID{};
	int m_Price{};
	ItemIdentifier m_PlantItemID{};
	CDecorationHouses* m_apDecorations[MAX_DECORATIONS_HOUSE]{};

	class CGS* GS() const;
	class CPlayer* GetPlayer() const;

public:
	CHouseData() = default;
	~CHouseData()
	{
		delete m_pDoorData;
		delete m_pBank;
		m_pDoorData = nullptr;
		m_pBank = nullptr;
	}

	static HouseDataPtr CreateDataItem(HouseIdentifier ID)
	{
		HouseDataPtr pData = std::make_shared<CHouseData>();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(int AccountID, std::string ClassName, int Price, int Bank, vec2 Pos, vec2 HouseDoorPos, vec2 PlantPos, int PlantItemID, int WorldID)
	{
		m_AccountID = AccountID;
		str_copy(m_aClassName, ClassName.c_str(), sizeof(m_aClassName));
		m_Price = Price;
		m_Pos = Pos;
		m_PlantPos = PlantPos;
		m_PlantItemID = PlantItemID;
		m_WorldID = WorldID;

		// door init
		m_pDoorData = new CHouseDoorData(GS(), HouseDoorPos);
		if(m_AccountID > 0)
			m_pDoorData->Open();

		// bank init
		m_pBank = new CHouseBankData(GS(), &m_AccountID, Bank);

		// init decoration
		InitDecorations();
	}

	HouseIdentifier GetID() const { return m_ID; }
	int GetAccountID() const { return m_AccountID; }
	const vec2& GetPos() const { return m_Pos; }
	const vec2& GetPlantPos() const { return m_PlantPos; }
	int GetWorldID() const { return m_WorldID; }
	ItemIdentifier GetPlantItemID() const { return m_PlantItemID; }
	const char* GetClassName() const { return m_aClassName; }
	bool HasOwner() const { return m_AccountID > 0; }
	int GetPrice() const { return m_Price; }

	CHouseDoorData* GetDoor() const { return m_pDoorData; }
	CHouseBankData* GetBank() const { return m_pBank; }

	bool AddDecoration(ItemIdentifier ItemID, vec2 Pos);
	bool RemoveDecoration(HouseDecorationIdentifier DecoID);

	void Buy(CPlayer* pPlayer);
	void Sell();

	void SetPlantItemID(ItemIdentifier PlantItemID);
	void ShowDecorations() const;

private:
	void InitDecorations();
};

#endif