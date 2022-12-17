/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_DATA_H

#include <game/server/mmocore/Components/Inventory/ItemInfoData.h>

#include "HouseBankData.h"
#include "HouseDoorData.h"

using HouseIdentifier = int;
using HouseDecorationIdentifier = int;

using HouseDataPtr = std::shared_ptr< class CHouseData >;

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
	class CDecorationHouses* m_apDecorations[MAX_DECORATIONS_HOUSE]{};

	class CGS* GS() const;
	class CPlayer* GetPlayer() const;

public:
	CHouseData() = default;
	~CHouseData();

	static HouseDataPtr CreateDataItem(HouseIdentifier ID)
	{
		HouseDataPtr pData = std::make_shared<CHouseData>();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(int AccountID, std::string ClassName, int Price, int Bank, vec2 Pos, vec2 HouseDoorPos, vec2 PlantPos, int PlantItemID, int WorldID, std::string AccessData)
	{
		m_AccountID = AccountID;
		str_copy(m_aClassName, ClassName.c_str(), sizeof(m_aClassName));
		m_Price = Price;
		m_Pos = Pos;
		m_PlantPos = PlantPos;
		m_PlantItemID = PlantItemID;
		m_WorldID = WorldID;

		// door init
		m_pDoorData = new CHouseDoorData(GS(), HouseDoorPos, std::move(AccessData), this);
		if(m_AccountID <= 0)
			m_pDoorData->Open();
		else
			m_pDoorData->Close();

		// bank init
		m_pBank = new CHouseBankData(GS(), &m_AccountID, Bank);

		// init decoration
		InitDecorations();
	}

	HouseIdentifier GetID() const { return m_ID; }
	int GetAccountID() const { return m_AccountID; }
	const char* GetClassName() const { return m_aClassName; }
	const vec2& GetPos() const { return m_Pos; }
	const vec2& GetPlantPos() const { return m_PlantPos; }
	int GetPrice() const { return m_Price; }
	ItemIdentifier GetPlantItemID() const { return m_PlantItemID; }
	int GetWorldID() const { return m_WorldID; }
	bool HasOwner() const { return m_AccountID > 0; }
	CHouseDoorData* GetDoor() const { return m_pDoorData; }
	CHouseBankData* GetBank() const { return m_pBank; }

	bool AddDecoration(ItemIdentifier ItemID, vec2 Pos);
	bool RemoveDecoration(HouseDecorationIdentifier DecoID);

	void Buy(CPlayer* pPlayer);
	void Sell();

	void SetPlantItemID(ItemIdentifier ItemID);
	void ShowDecorations() const;

private:
	void InitDecorations();
};

#endif