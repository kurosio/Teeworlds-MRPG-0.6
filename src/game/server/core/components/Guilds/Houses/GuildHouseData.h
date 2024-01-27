/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_DATA_H

#include "Manager/Doors/GuildHouseDoorsController.h"
#include "Manager/Decorations/GuildHouseDecorationsManager.h"

#define TW_GUILDS_HOUSES "tw_guilds_houses"
#define TW_GUILD_HOUSES_DECORATION_TABLE "tw_guilds_decorations"

class CGuildData;
using GuildHouseIdentifier = int;

class CGuildHouseData : public MultiworldIdentifiableStaticData< std::deque < CGuildHouseData* > >
{
	friend class CGuildHouseDoorsController;
	friend class CGuildHouseDecorationManager;

	CGS* GS() const;

	CGuildData* m_pGuild {};
	GuildHouseIdentifier m_ID{};
	float m_Radius {};
	vec2 m_Position{};
	vec2 m_TextPosition{};
	int m_Price {};
	int m_WorldID{};

	int m_LastTickTextUpdated {};

	CGuildHouseDoorsController* m_pDoors {};
	CGuildHouseDecorationManager* m_pDecorations {};

public:
	CGuildHouseData() = default;
	~CGuildHouseData();

	static CGuildHouseData* CreateElement(const GuildHouseIdentifier& ID)
	{
		auto pData = new CGuildHouseData;
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(CGuildData* pGuild, int Price, int WorldID, std::string&& JsProperties)
	{
		m_Price = Price;
		m_WorldID = WorldID;

		// components
		m_pDoors = new CGuildHouseDoorsController(this);
		m_pDecorations = new CGuildHouseDecorationManager(this);

		InitProperties(std::move(JsProperties));
		UpdateGuild(pGuild);
	}

	void InitProperties(std::string&& Properties);

	CGuildData* GetGuild() const { return m_pGuild; }
	CGuildHouseDoorsController* GetDoors() const { return m_pDoors; }
	CGuildHouseDecorationManager* GetDecorations() const { return m_pDecorations; }

	GuildHouseIdentifier GetID() const { return m_ID; }
	int GetWorldID() const { return m_WorldID; }
	vec2 GetPos() const { return m_Position; }
	float GetRadius() const { return m_Radius; }
	int GetPrice() const { return m_Price; }
	bool IsPurchased() const { return m_pGuild != nullptr; }

	void TextUpdate(int LifeTime);

	void UpdateGuild(CGuildData* pGuild);
};

#endif
