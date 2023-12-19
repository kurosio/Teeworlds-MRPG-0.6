/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_DATA_H

#include "Manager/Doors/GuildHouseDoorsController.h"
#include "Manager/Decorations/GuildHouseDecorationsManager.h"

using GuildHouseIdentifier = int;
using GuildHouseDataPtr = std::shared_ptr< class CGuildHouseData >;

class CGuildData;

class CGuildHouseData : public MultiworldIdentifiableStaticData< std::deque < GuildHouseDataPtr > >
{
	CGS* GS() const;

	CGuildData* m_pGuild {};
	GuildHouseIdentifier m_ID{};
	vec2 m_Position{};
	vec2 m_TextPosisiton{};
	int m_Price {};
	int m_WorldID{};

	CGuildHouseDoorsController* m_pDoors {};
	CGuildHouseDecorationManager* m_pDecorations {};

public:
	CGuildHouseData() = default;
	~CGuildHouseData();

	static GuildHouseDataPtr CreateElement(GuildHouseIdentifier ID)
	{
		GuildHouseDataPtr pData = std::make_shared<CGuildHouseData>();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(CGuildData* pGuild, int Price, vec2 Position, vec2 TextPosition, int WorldID, std::string&& JsonDoorsData)
	{
		m_pGuild = pGuild;
		m_Price = Price;
		m_Position = Position;
		m_TextPosisiton = TextPosition;
		m_WorldID = WorldID;

		// components
		m_pDoors = new CGuildHouseDoorsController(GS(), std::move(JsonDoorsData), this);
		m_pDecorations = new CGuildHouseDecorationManager();
	}

	CGuildHouseDoorsController* GetDoors() const { return m_pDoors; }
	CGuildHouseDecorationManager* GetDecorations() const { return m_pDecorations; }

	void SetGuild(CGuildData* pGuild);
};

#endif
