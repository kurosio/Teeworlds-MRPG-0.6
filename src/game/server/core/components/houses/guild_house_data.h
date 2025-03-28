/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_DATA_H

#include "base/interface_house.h"
#include "base/decoration_manager.h"
#include "base/door_manager.h"
#include "base/farmzone_manager.h"

#define TW_GUILDS_HOUSES "tw_guilds_houses"
#define TW_GUILD_HOUSES_DECORATION_TABLE "tw_guilds_decorations"
static constexpr float s_GuildChancePlanting = 0.025f;

class CGS;
class CPlayer;
class CGuild;
using GuildHouseIdentifier = int;
class CGuildHouse : public IHouse, public MultiworldIdentifiableData< std::deque < CGuildHouse* > >
{
	friend class CGuild;
	friend class CGuildHouseDoorManager;

private:
	CGS* GS() const override;

	CGuild* m_pGuild {};
	GuildHouseIdentifier m_ID{};
	vec2 m_Position{};
	vec2 m_TextPosition{};
	int m_InitialFee {};
	int m_WorldID{};
	int m_RentDays {};

	CDoorManager* m_pDoorManager {};
	CDecorationManager* m_pDecorationManager {};
	CFarmzonesManager* m_pFarmzonesManager {};

public:
	CGuildHouse() = default;
	~CGuildHouse() override;

	static CGuildHouse* CreateElement(const GuildHouseIdentifier& ID)
	{
		auto pData = new CGuildHouse;
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(CGuild* pGuild, int RentDays, int InitialFee, int WorldID, std::string&& DoorsData, std::string&& FarmzonesData, std::string&& PropertiesData)
	{
		UpdateGuild(pGuild);

		m_InitialFee = InitialFee;
		m_RentDays = RentDays;
		m_WorldID = WorldID;

		InitComponents(DoorsData, FarmzonesData, PropertiesData);
	}
	void InitComponents(const std::string& DoorsData, const std::string& FarmzonesData, const std::string& PropertiesData);

	CGuild* GetGuild() const { return m_pGuild; }
	CDoorManager* GetDoorManager() const { return m_pDoorManager; }

	// decoration manager
	CDecorationManager* GetDecorationManager() const
	{
		return m_pDecorationManager;
	}


	CFarmzonesManager* GetFarmzonesManager() const
	{
		return m_pFarmzonesManager;
	}

	int GetID() const override { return m_ID; }
	vec2 GetPos() const override { return m_Position; }
	const char* GetTableName() const override { return TW_GUILDS_HOUSES; }
	IHouse::Type GetHouseType() const override { return IHouse::Type::Guild; }

	int GetWorldID() const { return m_WorldID; }
	int GetInitialFee() const { return m_InitialFee; }
	int GetRentPrice() const;
	int GetRentDays() const { return m_RentDays; }
	bool IsPurchased() const { return m_pGuild != nullptr; }
	const char* GetOwnerName() const;

	bool ExtendRentDays(int Days);
	bool ReduceRentDays(int Days);
	void UpdateText(int Lifetime) const;
	void UpdateGuild(CGuild* pGuild);
};

#endif
