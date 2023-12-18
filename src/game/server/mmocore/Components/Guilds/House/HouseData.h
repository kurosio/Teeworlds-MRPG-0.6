/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_DATA_H

using GuildHouseDataPtr = std::shared_ptr< class CGuildHouseData >;

class CGuildHouseData : public MultiworldIdentifiableStaticData< std::deque < GuildHouseDataPtr > >
{
	int m_ID;
	int m_PosX;
	int m_PosY;
	int m_DoorX;
	int m_DoorY;
	int m_TextX;
	int m_TextY;
	int m_WorldID;
	int m_Price;
	int m_Payment;
	int m_GuildID;
	class GuildDoor* m_pDoor;

public:
	CGuildHouseData() = default;
	~CGuildHouseData();

	static GuildHouseDataPtr CreateElement(GuildIdentifier ID)
	{
		GuildHouseDataPtr pData = std::make_shared<CGuildData>();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init()
	{
	}
};

#endif
