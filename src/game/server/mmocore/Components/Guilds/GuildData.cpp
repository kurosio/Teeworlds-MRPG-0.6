/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildData.h"

#include <game/server/gamecontext.h>

CGS* CGuildData::GS() const
{
	/*
	CGuildData* pGuild;

	// members
	pGuild->GetMembers()->Join(2); // TODO: BY STATUS
	pGuild->GetMembers()->Kick(2); // TODO: BY STATUS
	for(auto& p : pGuild->GetMembers()->GetContainer())
	{
		p->GetAccountID();
		p->GetDeposit();
		p->GetRankID();
	}

	// rank's
	pGuild->GetRanks()->Add("Chucka");
	pGuild->GetRanks()->Get("Chucka");
	pGuild->GetRanks()->Remove("Chucka");
	for(auto& p : pGuild->GetRanks()->GetContainer())
	{
		p->GetName();
		p->ChangeAccess(ACCESS_NO);
		p->ChangeName("PornoHub")
	}

	// bank
	pGuild->GetBank()->Add(100, );
	pGuild->GetBank()->Take(100, );
	pGuild->GetBank()->Get();

	// history
	pGuild->GetHistory()->Add("%s changes access for rank '{STR}'", "Popa", "Co-leader");
	for(auto& p : pGuild->GetHistory()->GetLogs())
	{
		p.m_Text;
		p.m_Time;
	}

	// house's
	CGuildHouseData* pHouse = pGuild->GetHouse();
	for(auto& [DoorID, pDoor] : pHouse->GetDoors()->GetContainer())
	{
		pDoor->Close();
		pDoor->Open();
		pDoor->IsClosed();
		pDoor->GetName();
		pDoor->GetPos();
	}
	// TODO: ADD COMING
	*/

	return (CGS*)Instance::GetServer()->GameServer(m_pHouse != nullptr ? m_pHouse->GetWorldID() : MAIN_WORLD_ID);
}

CGuildData::~CGuildData()
{
	delete m_pHistory;
	delete m_pRanks;
	delete m_pBank;
}

bool CGuildData::BuyHouse(int HouseID)
{
	// check if the guild has a house
	if(m_pHouse != nullptr)
	{
		GS()->ChatGuild(m_ID, "Your Guild can't have 2 houses. Purchase canceled!");
		return false;
	}

	// check valid house
	auto IterHouse = std::find_if(CGuildHouseData::Data().begin(), CGuildHouseData::Data().end(), [&HouseID](const GuildHouseDataPtr p){ return p->GetID() == HouseID; });
	if(IterHouse == CGuildHouseData::Data().end())
	{
		GS()->ChatGuild(m_ID, "The house is unavailable.");
		return false;
	}

	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILD_HOUSES, "WHERE ID = '%d' AND GuildID IS NULL", HouseID);
	if(pRes->next())
	{
		const int Price = pRes->getInt("Price");
		if(!GetBank()->Spend(Price))
		{
			GS()->ChatGuild(m_ID, "This Guild house requires {VAL}gold!", Price);
			return false;
		}

		m_pHouse = IterHouse->get();
		m_pHouse->SetGuild(this);
		Database->Execute<DB::UPDATE>("tw_guilds_houses", "GuildID = '%d' WHERE ID = '%d'", m_ID, HouseID);

		const char* WorldName = Server()->GetWorldName(m_pHouse->GetWorldID());
		GS()->Chat(-1, "{STR} bought guild house on {STR}!", GetName(), WorldName);
		GS()->ChatDiscord(DC_SERVER_INFO, "Information", "{STR} bought guild house on {STR}!", GetName(), WorldName);
		return true;
	}

	GS()->ChatGuild(m_ID, "House has already been purchased!");
	return false;
}

bool CGuildData::SellHouse()
{
	// check is not has house
	if(m_pHouse == nullptr)
	{
		GS()->ChatGuild(m_ID, "Your Guild doesn't have a home!");
		return false;
	}

	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", TW_GUILD_HOUSES, "WHERE ID = '%d' AND GuildID IS NOT NULL", m_pHouse->GetID());
	if(pRes->next())
	{
		Database->Execute<DB::UPDATE>(TW_GUILD_HOUSES, "GuildID = NULL WHERE ID = '%d'", m_pHouse->GetID());

		const int ReturnedGold = m_pHouse->GetPrice();
		GS()->SendInbox("System", m_OwnerUID, "Your guild house sold.", "We returned some gold from your guild.", itGold, ReturnedGold);

		GS()->ChatGuild(m_ID, "House sold, {VAL}gold returned to leader", ReturnedGold);
		m_pHistory->Add("Lost a house on '%s'.", Server()->GetWorldName(m_pHouse->GetWorldID()));

		m_pHouse->GetDoors()->CloseAll();
		m_pHouse->SetGuild(nullptr);
		m_pHouse = nullptr;
		return true;
	}

	GS()->ChatGuild(m_ID, "Your Guild doesn't have a home!");
	m_pHouse->GetDoors()->CloseAll();
	m_pHouse->SetGuild(nullptr);
	m_pHouse = nullptr;
	return false;
}

void CGuildData::AddExperience(int Experience)
{
	m_Experience += Experience;

	bool UpdateTable = false;
	int ExperienceNeed = (int)computeExperience(m_Level);
	while(m_Experience >= ExperienceNeed)
	{
		m_Experience -= ExperienceNeed;
		m_Level++;

		ExperienceNeed = (int)computeExperience(m_Level);
		if(m_Experience < ExperienceNeed)
			UpdateTable = true;

		GS()->Chat(-1, "Guild {STR} raised the level up to {INT}", GetName(), m_Level);
		GS()->ChatDiscord(DC_SERVER_INFO, "Information", "Guild {STR} raised the level up to {INT}", GetName(), m_Level);
		m_pHistory->Add("Guild raised level to '%d'.", m_Level);
	}

	if(rand() % 10 == 2 || UpdateTable)
	{
		Database->Execute<DB::UPDATE>("tw_guilds", "Level = '%d', Experience = '%d' WHERE ID = '%d'", m_Level, m_Experience, m_ID);
	}
}
