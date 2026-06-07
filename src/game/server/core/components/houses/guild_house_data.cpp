/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_house_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/items/gathering_node.h>
#include <game/server/core/entities/tools/draw_board.h>
#include <game/server/core/components/guilds/guild_data.h>

constexpr int s_SecondsPerRentDay = 60 * 60 * 24;

static int GetRentDaysLeft(time_t RentEndTimestamp)
{
	const int SecondsLeft = maximum((time_t)0, RentEndTimestamp - time(nullptr));
	return (SecondsLeft + s_SecondsPerRentDay - 1) / s_SecondsPerRentDay;
}

static time_t AddRentDaysToTimestamp(time_t RentEndTimestamp, int Days)
{
	const time_t CurrentTimestamp = time(nullptr);
	const time_t BaseTimestamp = maximum(CurrentTimestamp, RentEndTimestamp);
	return BaseTimestamp + (time_t)(Days * s_SecondsPerRentDay);
}


CGS* CGuildHouse::GS() const { return (CGS*)Instance::GameServer(m_WorldID); }
CGuildHouse::~CGuildHouse()
{
	delete m_pDoorManager;
	delete m_pFarmzonesManager;
	delete m_pDecorationManager;
}

void CGuildHouse::InitComponents(const std::string& DoorsData, const std::string& FarmzonesData, const std::string& PropertiesData)
{
	// assert main properties
	dbg_assert(PropertiesData.length() > 0, "The properties string is empty");


	// initialize properties
	mystd::json::parse(PropertiesData, [this](nlohmann::json& pJson)
	{
		dbg_assert(pJson.find("position") != pJson.end(), "The importal properties value is empty");
		m_Position = pJson.value("position", vec2());
		m_TextPosition = pJson.value("text_position", vec2());
	});


	// initialize components
	m_pDecorationManager = new CDecorationManager(this, TW_GUILD_HOUSES_DECORATION_TABLE);
	m_pDoorManager = new CDoorManager(this, DoorsData);
	m_pFarmzonesManager = new CFarmzonesManager(this, FarmzonesData);


	// asserts
	dbg_assert(m_pFarmzonesManager != nullptr, "The house farmzones manager is null");
	dbg_assert(m_pDecorationManager != nullptr, "The house decorations manager is null");
	dbg_assert(m_pDoorManager != nullptr, "The house doors manager is null");
}

int CGuildHouse::GetMaxDecorationSlots() const
{
	return m_pGuild ? m_pGuild->GetUpgrades().getRef<int>((int)GuildUpgrade::DecorationSlots) : 0;
}

int CGuildHouse::GetRentPrice() const
{
	const auto DoorCount = (int)GetDoorManager()->GetContainer().size();
	const auto FarmzoneCount = (int)GetFarmzonesManager()->GetContainer().size();
	const auto PartPrice = translate_to_percent_rest(m_InitialFee, 33.f);

	return (int)PartPrice + ((DoorCount * 200) + (FarmzoneCount * 500));
}

const char* CGuildHouse::GetOwnerName() const
{
	if(!m_pGuild)
		return "NONE";
	return m_pGuild->GetName();
}

bool CGuildHouse::ExtendRentDays(int Days)
{
	// check validity
	if(!m_pGuild || !m_pGuild->GetBankManager())
		return false;

	// try spend for rent days
	if(m_pGuild->GetBankManager()->Spend(GetRentPrice() * Days))
	{
		m_RentEndTimestamp = AddRentDaysToTimestamp(m_RentEndTimestamp, Days);
		Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "RentDays = '{}' WHERE ID = '{}'", m_RentEndTimestamp, m_ID);
		return true;
	}

	return false;
}

bool CGuildHouse::ReduceRentDays(int Days)
{
	// check validity
	const int DaysLeft = GetRentDays();
	if(!m_pGuild || !m_pGuild->GetBankManager() || DaysLeft < Days)
		return false;

	// reduce rent days
	m_RentEndTimestamp = maximum(time(nullptr), m_RentEndTimestamp - (time_t)(clamp(Days, 1, DaysLeft) * s_SecondsPerRentDay));
	Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "RentDays = '{}' WHERE ID = '{}'", m_RentEndTimestamp, m_ID);
	return true;
}

int CGuildHouse::GetRentDays() const
{
	return GetRentDaysLeft(m_RentEndTimestamp);
}

bool CGuildHouse::IsRentExpired() const
{
	return IsPurchased() && m_RentEndTimestamp > 0 && time(nullptr) >= m_RentEndTimestamp;
}

void CGuildHouse::UpdateText(int Lifetime) const
{
	// update text
	const char* pName = IsPurchased() ? m_pGuild->GetName() : "NONE";
	GS()->EntityManager()->Text(m_TextPosition, Lifetime, pName);
}

void CGuildHouse::UpdateGuild(CGuild* pGuild)
{
	m_pGuild = pGuild;
	if(m_pGuild)
	{
		m_pGuild->m_pHouse = this;
	}
}
