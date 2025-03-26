#include "profession.h"
#include <engine/server.h>

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/achievements/achievement_data.h>

CGS* CProfession::GS() const
{
	return (CGS*)Instance::GameServerPlayer(m_ClientID);
}

CPlayer* CProfession::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

CProfession::CProfession(ProfessionIdentifier ProfID, int ProfessionType)
{
	m_Level = 1;
	m_Experience = 0;
	m_UpgradePoint = 0;
	m_ProfessionID = ProfID;
	m_ProfessionType = ProfessionType;
}

void CProfession::Init(int ClientID, const std::optional<std::string>& jsonData)
{
	m_ClientID = ClientID;

	// first initialize
	if(!jsonData.has_value())
	{
		const auto Data = GetPreparedJsonString();
		const auto AccountID = GetPlayer()->Account()->GetID();
		Database->Execute<DB::INSERT>("tw_accounts_professions", "(UserID, ProfessionID, Data) VALUES ('{}', '{}', '{}')",
			AccountID, (int)m_ProfessionID, Data);
		return;
	}

	// loading from data
	mystd::json::parse(jsonData.value(), [this](nlohmann::json& json)
	{
		m_Level = json.value("level", 1);
		m_Experience = json.value("exp", 0);
		m_UpgradePoint = json.value("up", 0);
		for(auto& [ID, Value] : json["attributes"].items())
		{
			m_Attributes[(AttributeIdentifier)std::stoi(ID)] = Value;
		}
	});
}

void CProfession::Save()
{
	// save to database
	const auto* pPlayer = GetPlayer();
	const auto Data = GetPreparedJsonString();
	const auto AccountID = pPlayer->Account()->GetID();
	Database->Execute<DB::UPDATE>("tw_accounts_professions", "Data = '{}' WHERE ProfessionID = '{}' AND UserID = '{}'",
		Data, (int)m_ProfessionID, AccountID);
}

void CProfession::AddExperience(uint64_t Experience)
{
	auto* pPlayer = GetPlayer();
	auto ExperienceNeed = computeExperience(m_Level);
	const char* pProfessionName = GetProfessionName(m_ProfessionID);

	// append experience
	m_Experience += Experience;

	// check level up
	while(m_Experience >= ExperienceNeed)
	{
		// implement level up
		m_Experience -= ExperienceNeed;
		m_Level++;
		m_UpgradePoint++;
		ExperienceNeed = computeExperience(m_Level);

		// post leveling visual effects and messages
		if(m_Experience < ExperienceNeed)
		{
			if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
			{
				GS()->CreateSound(pPlayer->GetCharacter()->m_Core.m_Pos, 4);
				GS()->CreateBirthdayEffect(pPlayer->GetCharacter()->m_Core.m_Pos, m_ClientID);
				GS()->EntityManager()->Text(pPlayer->GetCharacter()->m_Core.m_Pos + vec2(0, -40), 40, pProfessionName);
			}

			GS()->Chat(m_ClientID, "'{}' Level UP. Now Level {}!", pProfessionName, m_Level);
			GS()->Core()->SaveAccount(pPlayer, SAVE_PROFESSION);
			Save();
		}
	}

	// randomly save the account stats
	if(rand() % 2 == 0)
	{
		Save();
	}

	// notify listener & progress bar
	g_EventListenerManager.Notify<IEventListener::PlayerProfessionLeveling>(pPlayer, this, m_Level);
	pPlayer->ProgressBar(pProfessionName, m_Level, m_Experience, ExperienceNeed, Experience);
}

bool CProfession::Upgrade(AttributeIdentifier ID, int Units, int PricePerUnit)
{
	if(!m_Attributes.contains(ID))
		return false;

	const auto TotalPrice = Units * PricePerUnit;
	if(m_UpgradePoint < TotalPrice)
		return false;

	m_Attributes[ID] += Units;
	m_UpgradePoint -= TotalPrice;
	Save();
	return true;
}

std::string CProfession::GetPreparedJsonString() const
{
	nlohmann::json json;
	json["level"] = m_Level;
	json["exp"] = m_Experience;
	json["up"] = m_UpgradePoint;
	for(auto [ID, Value] : m_Attributes)
	{
		json["attributes"][std::to_string((int)ID)] = Value;
	}

	return json.dump();
}
