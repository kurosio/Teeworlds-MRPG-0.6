/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_DATA_H
#include <game/server/mmocore/Utils/FieldData.h>

#include "GuildHistoryLogData.h"
#include "GuildBankData.h"

#include "Ranks/GuildRanksController.h"

#define TW_GUILD_TABLE "tw_guilds"
#define TW_GUILDS_RANKS_TABLE "tw_guilds_ranks"

using GuildIdentifier = int;
using GuildDataPtr = std::shared_ptr< class CGuildData >;

class CGuildData : public MultiworldIdentifiableStaticData< std::deque < GuildDataPtr > >
{
	friend class CGuildRanksController;
	friend class CGuildRankData;
	friend class CGuildBankData;

	CGS* GS() const;

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

	CGuildBankController* m_pBank {};
	CGuildHistoryController* m_pHistory {};
	CGuildRanksController* m_pRanks {};
	// decorations
	// history

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

		// components init
		m_pBank = new CGuildBankController(Bank, this);
		m_pRanks = new CGuildRanksController(this);
		m_pHistory = new CGuildHistoryController(this);
	}

	GuildIdentifier GetID() const { return m_ID; }
	CGuildBankController* GetBank() const { return m_pBank; }
	CGuildHistoryController* GetHistory() const { return m_pHistory; }
	CGuildRanksController* GetRanks() const { return m_pRanks; }

	const char* GetName() const { return m_Name.c_str(); }
	int GetOwnerUID() const { return m_OwnerUID; }
	int GetLevel() const { return m_Level; }
	int GetExperience() const { return m_Experience;}
	int GetScore() const { return m_Score; }

	void AddExperience(int Experience);

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
