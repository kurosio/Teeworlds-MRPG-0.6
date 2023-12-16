/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_DATA_H

#include "GuildBankData.h"
#include "GuildRankData.h"
#include <game/server/mmocore/Utils/FieldData.h>

#define TW_GUILD_TABLE "tw_guilds"

using GuildIdentifier = int;
using GuildDataPtr = std::shared_ptr< class CGuildData >;

class CGuildData : public MultiworldIdentifiableStaticData< std::deque < GuildDataPtr > >
{
	enum
	{
		AVAILABLE_SLOTS = 0,
		CHAIR_EXPERIENCE = 1,
		NUM_GUILD_UPGRADES,
	};

	CFieldContainer m_UpgradeData
	{
		CFieldData<int>{AVAILABLE_SLOTS, "AvailableSlots", "Available slots"},
		CFieldData<int>{CHAIR_EXPERIENCE, "ChairExperience", "Chair experience"},
	};

	GuildIdentifier m_ID {};
	std::string m_Name {};
	int m_OwnerUID {};
	int m_Level {};
	int m_Experience {};
	int m_Score {};

	CGuildBankData* m_pBank {};
	GuildRankContainer m_aRanks{};

public:
	CGuildData() = default;
	~CGuildData();

	static GuildDataPtr CreateElement(GuildIdentifier ID)
	{
		GuildDataPtr pData = std::make_shared<CGuildData>();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(std::string Name, int Level, int Experience, int Score, int OwnerUID, int Bank)
	{
		m_Name = Name;
		m_OwnerUID = OwnerUID;
		m_Level = Level;
		m_Experience = Experience;
		m_Score = Score;

		// bank init
		m_pBank = new CGuildBankData(GS(), &m_AccountID, Bank);
	}

	GuildIdentifier GetID() const { return m_ID; }

	void InitRanks();
	GuildRankContainer& GetRanks() { return m_aRanks; }
};

struct CGuildHouseData
{
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

	static std::map < int, CGuildHouseData > ms_aHouseGuild;
};


#endif
