/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_DATA_H

#include "base/interface_house.h"
#include "base/bank_manager.h"
#include "base/farmzone_manager.h"
#include "base/door_manager.h"
#include "base/decoration_manager.h"

#include "../Inventory/ItemData.h"

#define TW_HOUSES_TABLE "tw_houses"

class CGS;
class CPlayer;
using HouseIdentifier = int;
class CHouse : public IHouse, public MultiworldIdentifiableData< std::deque < CHouse* > >
{
public:
	CGS* GS() const override;
	CPlayer* GetPlayer() const override;
	int GetID() const override { return m_ID; }
	vec2 GetPos() const override { return m_Position; }
	const char* GetTableName() const override { return TW_HOUSES_TABLE; }
	IHouse::Type GetHouseType() const override { return IHouse::Type::Player; }

private:
	CDecorationManager* m_pDecorationManager {};
	CFarmzonesManager* m_pFarmzonesManager{};
	CBankManager* m_pBankManager {};
	CDoorManager* m_pDoorManager {};

	HouseIdentifier m_ID {};
	std::string m_ClassName {};
	vec2 m_Position {};
	std::optional<vec2> m_TextPosition {};
	int m_AccountID {};
	int m_WorldID {};
	int m_InitialFee {};
	int m_RentDays {};

public:
	CHouse() = default;
	~CHouse() override;

	static CHouse* CreateElement(HouseIdentifier ID)
	{
		auto pData = new CHouse();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(int AccountID, const std::string& ClassName, int RentDays, int InitialFee, const BigInt& Bank, int WorldID,
		const std::string& DoorsData, const std::string& FarmzonesData, const std::string& PropertiesData)
	{
		m_AccountID = AccountID;
		m_ClassName = ClassName;
		m_InitialFee = InitialFee;
		m_RentDays = RentDays;
		m_WorldID = WorldID;

		// init components
		InitComponents(Bank, DoorsData, FarmzonesData, PropertiesData);
	}

	CDecorationManager* GetDecorationManager() const { return m_pDecorationManager; }
	CFarmzonesManager* GetFarmzonesManager() const { return m_pFarmzonesManager; }
	CBankManager* GetBankManager() const { return m_pBankManager; }
	CDoorManager* GetDoorManager() const { return m_pDoorManager; }

	int GetAccountID() const { return m_AccountID; }
	const char* GetClassName() const { return m_ClassName.c_str(); }
	bool HasOwner() const { return m_AccountID > 0; }
	int GetInitialFee() const { return m_InitialFee; }
	int GetWorldID() const { return m_WorldID; }
	int GetRentPrice() const;
	int GetRentDays() const { return m_RentDays; }

	void InitComponents(const BigInt& Bank, const std::string& DoorsData, const std::string& FarmzonesData, const std::string& PropertiesData);
	void Buy(CPlayer* pPlayer);
	void Sell();
	void UpdateText(int Lifetime) const;
	bool ExtendRentDays(int Days);

	void HandleTimePeriod(ETimePeriod Period);
};

#endif