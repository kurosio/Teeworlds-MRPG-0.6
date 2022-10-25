/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_DATA_H

#include <game/server/mmocore/Components/Inventory/ItemInfoData.h>

#include "game/server/gamecontext.h"
#include "game/server/mmocore/GameEntities/decoration_houses.h"

using HouseIdentifier = int;
using HouseDecorationIdentifier = int;

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
	CHouseBankData(CGS* pGS, int* pAccountID) : m_pGS(pGS), m_pAccountID(pAccountID) {};

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
	void InitGameServer(class CGS* pGS) { m_pGS = pGS; }

public:
	CHouseDoorData() = default;
	CHouseDoorData(vec2 Pos) : m_Pos(Pos) {}
	~CHouseDoorData();

	const vec2& GetPos() const { return m_Pos; }
	bool GetState() const { return m_pDoor; }

	void Open();
	void Close();
	void Reverse();
};

/*
 * DECORATION DATA
 */
class CHouseDecoration
{
	HouseDecorationIdentifier m_ID{-1};
	ItemIdentifier m_DecorationItemID {};
	class CDecorationHouses* m_pDecoration{};

public:
	~CHouseDecoration();

	HouseDecorationIdentifier GetID() const { return m_ID; }
	ItemIdentifier GetDecorationItemID() const { return m_DecorationItemID; }
	bool HasUsed() const { return m_pDecoration; }

	void Init(CGS* pGS, HouseDecorationIdentifier ID, vec2 Pos, int AccountID, ItemIdentifier DecorationItemID);
	void Reset();
};

/*
 * HOUSE DATA
 */
class CHouseData : public MultiworldIdentifiableStaticData< veque::veque < CHouseData > >
{
	HouseIdentifier m_ID{};
	vec2 m_Pos{};
	vec2 m_PlantPos{};
	CHouseDoorData m_DoorData{};
	CHouseBankData m_Bank{};
	char m_aClassName[32]{};
	int m_AccountID{};
	int m_WorldID{};
	int m_Price{};
	ItemIdentifier m_PlantItemID{};
	CHouseDecoration m_aDecorations[MAX_DECORATIONS_HOUSE]{};

	class CGS* GS() const;
	class CPlayer* GetPlayer() const;

public:
	CHouseData() = default;
	CHouseData(HouseIdentifier ID) : m_ID(ID) { }

	void Init(int AccountID, std::string ClassName, int Price, int Bank, vec2 Pos, CHouseDoorData&& Door, vec2 PlantPos, int PlantItemID, int WorldID)
	{
		m_AccountID = AccountID;
		str_copy(m_aClassName, ClassName.c_str(), sizeof(m_aClassName));
		m_Price = Price;
		m_Pos = Pos;
		m_PlantPos = PlantPos;
		m_PlantItemID = PlantItemID;
		m_WorldID = WorldID;

		// door init
		m_DoorData = std::forward<CHouseDoorData>(Door);
		m_DoorData.InitGameServer(GS());
		if(m_AccountID > 0)
			m_DoorData.Open();

		// bank init
		m_Bank = CHouseBankData { GS(), &m_AccountID };

		CHouseData::Data().push_back(*this);
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

	CHouseDoorData* GetDoor() { return &m_DoorData; }
	CHouseBankData* GetBank() { return &m_Bank; }

	bool AddDecoration(ItemIdentifier DecoItemID, vec2 Pos);
	bool RemoveDecoration(HouseDecorationIdentifier ID);

	void Buy(CPlayer* pPlayer);
	void Sell();

	void UpdatePlantItemID(ItemIdentifier PlantItemID);

	void ShowDecorations();

	static std::map< int, CHouseData > ms_aHouse;
};

#endif