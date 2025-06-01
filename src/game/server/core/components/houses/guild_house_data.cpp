/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_house_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/items/gathering_node.h>
#include <game/server/core/entities/tools/draw_board.h>
#include <game/server/core/components/guilds/guild_data.h>

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
		return "FREE";
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
		m_RentDays += Days;
		Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "RentDays = '{}' WHERE ID = '{}'", m_RentDays, m_ID);
		return true;
	}

	return false;
}

bool CGuildHouse::ReduceRentDays(int Days)
{
	// check validity
	if(!m_pGuild || !m_pGuild->GetBankManager() || m_RentDays < Days)
		return false;

	// reduce rent days
	m_RentDays -= clamp(Days, 1, m_RentDays);
	Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "RentDays = '{}' WHERE ID = '{}'", m_RentDays, m_ID);
	return true;
}

void CGuildHouse::UpdateText(int Lifetime) const
{
	// update text
	const char* pName = IsPurchased() ? m_pGuild->GetName() : "FREE";
	GS()->EntityManager()->Text(m_TextPosition, Lifetime - 5, pName);
}

void CGuildHouse::UpdateGuild(CGuild* pGuild)
{
	m_pGuild = pGuild;
	if(m_pGuild)
	{
		m_pGuild->m_pHouse = this;
	}
}
