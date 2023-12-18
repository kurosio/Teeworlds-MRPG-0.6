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
	CGuildData* m_pGuild {};
	GuildHouseIdentifier m_ID{};
	vec2 m_Pos{};
	vec2 m_TextPos{};
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

	void Init()
	{
	}
};

#endif
