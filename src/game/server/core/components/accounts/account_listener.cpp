#include "account_listener.h"

#include "account_db.h"

#include <engine/server/async_file.h>
#include <game/server/gamecontext.h>

CAccountListener g_AccountListener;

constexpr const char* ATTRIBUTE_TRACKING_FILE = "server_data/attribute_tracking.json";
constexpr int TRACKING_REFRESH_SECONDS = 300;
constexpr int TRACKING_RETRY_SECONDS = 5;

// account listener
void CAccountListener::Initialize()
{
	g_EventListenerManager.RegisterListener(IEventListener::CharacterSpawn, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerLogin, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerProfessionLeveling, this);

	m_TrackingData.Initialize();
}

void CAccountListener::OnTick()
{
	m_TrackingData.Tick();
}

void CAccountListener::OnCharacterSpawn(CPlayer* pPlayer)
{
	if(!g_Config.m_SvHighlightExperts || pPlayer->IsBot())
		return;

	struct LevelingData
	{
		ProfessionIdentifier m_Identifier;
		bool m_Projectile;
		int m_Type;
		int m_Subtype;
		int m_OrbitType;
	};

	static constexpr std::array<LevelingData, (int)ProfessionIdentifier::NUM_PROFESSIONS> s_aLevelingData { {
		{ ProfessionIdentifier::Tank,      false, POWERUP_ARMOR,  0,          MULTIPLE_ORBIT_TYPE_ELLIPTICAL },
		{ ProfessionIdentifier::Dps,       false, POWERUP_WEAPON, WEAPON_GUN, MULTIPLE_ORBIT_TYPE_ELLIPTICAL },
		{ ProfessionIdentifier::Healer,    false, POWERUP_HEALTH, 0,          MULTIPLE_ORBIT_TYPE_ELLIPTICAL },
		{ ProfessionIdentifier::Miner,     true,  WEAPON_HAMMER,  0,          MULTIPLE_ORBIT_TYPE_EIGHT },
		{ ProfessionIdentifier::Farmer,    true,  WEAPON_HAMMER,  0,          MULTIPLE_ORBIT_TYPE_DYNAMIC_CENTER },
		{ ProfessionIdentifier::Fisherman, true,  WEAPON_HAMMER,  0,          MULTIPLE_ORBIT_TYPE_PULSATING },
		{ ProfessionIdentifier::Loader,    true,  WEAPON_HAMMER,  0,          MULTIPLE_ORBIT_TYPE_ELLIPTICAL }
	} };


	// Each matching profession gets two aura layers.
	const auto AccountID = pPlayer->Account()->GetID();
	bool IsTopExpert = false;
	for(int Layer = 0; Layer < 2; ++Layer)
	{
		for(const auto& Profession : s_aLevelingData)
		{
			const auto* pLeader = m_TrackingData.GetProfessionLeader(Profession.m_Identifier);
			if(pLeader && pLeader->AccountID == AccountID)
			{
				pPlayer->GetCharacter()->AddMultipleOrbit(Profession.m_Projectile, 1, Profession.m_Type, Profession.m_Subtype, Profession.m_OrbitType);
				IsTopExpert = true;
			}
		}
	}

	if(IsTopExpert)
		pPlayer->GS()->Chat(pPlayer->GetCID(), "You are the top expert in your profession on the server. Visual highlighting is now active.");
}


void CAccountListener::OnPlayerLogin(CPlayer* pPlayer, CAccountData* pAccount)
{
	if(!pPlayer || !pAccount)
		return;

	for(const auto& Profession : pAccount->GetProfessions())
		m_TrackingData.UpdateProfession(pPlayer, Profession.GetProfessionID(), Profession.GetLevel());
}


void CAccountListener::OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel)
{
	if(pProfession)
		m_TrackingData.UpdateProfession(pPlayer, pProfession->GetProfessionID(), NewLevel);
}


// Shared tracking data
void CTrackingData::Initialize()
{
	LoadAttributes();
	RefreshProfessions();
}

void CTrackingData::Tick()
{
	if(m_RefreshingProfessions)
		return;

	if(m_RefreshDelay > 0)
	{
		--m_RefreshDelay;
		return;
	}

	RefreshProfessions();
}

void CTrackingData::RefreshProfessions()
{
	if(m_RefreshingProfessions)
		return;

	m_RefreshingProfessions = true;
	AccountDb::ListProfessionLeaders(PROFESSION_TAG).Then([this](Db::CResultSet&& Rows)
	{
		ApplyProfessions(std::move(Rows));
	});
}

void CTrackingData::ApplyProfessions(Db::CResultSet&& Rows)
{
	m_RefreshingProfessions = false;
	if(!Rows.Ok())
	{
		m_RefreshDelay = TRACKING_RETRY_SECONDS * std::max(1, Instance::Server()->TickSpeed());
		return;
	}

	while(Rows.Next())
		SetProfessionLeader(Rows.GetInt("ProfessionID"), Rows.GetInt("UserID"), Rows.GetInt("Level"));

	m_ProfessionsLoaded = true;
	m_RefreshDelay = TRACKING_REFRESH_SECONDS * std::max(1, Instance::Server()->TickSpeed());
}

void CTrackingData::SetProfessionLeader(int ProfessionID, int AccountID, int Level)
{
	if(ProfessionID < 0 || ProfessionID >= NUM_PROFESSIONS || AccountID <= 0 || Level <= 0)
		return;

	auto& Leader = m_aProfessionLeaders[ProfessionID];
	if(Level > Leader.Level || (Level == Leader.Level && (Leader.AccountID <= 0 || AccountID < Leader.AccountID)))
		Leader = { AccountID, Level };
}

void CTrackingData::UpdateProfession(CPlayer* pPlayer, ProfessionIdentifier Profession, int Level)
{
	if(pPlayer && pPlayer->IsAuthed())
		SetProfessionLeader((int)Profession, pPlayer->Account()->GetID(), Level);
}

const TrackingLevelingData* CTrackingData::GetProfessionLeader(ProfessionIdentifier Profession) const
{
	const int ID = (int)Profession;
	if(ID < 0 || ID >= NUM_PROFESSIONS)
		return nullptr;

	const auto& Leader = m_aProfessionLeaders[ID];
	return Leader.AccountID > 0 ? &Leader : nullptr;
}

void CTrackingData::LoadAttributes()
{
	ByteArray RawData;
	if(!mystd::file::load(ATTRIBUTE_TRACKING_FILE, &RawData))
		return;

	const std::string Json((const char*)RawData.data(), RawData.size());
	const bool HasError = mystd::json::parse(Json, [this](nlohmann::json& Data)
	{
		if(!Data.is_object() || !Data.contains("tracking") || !Data["tracking"].is_array())
			return;

		for(const auto& Item : Data["tracking"])
		{
			const int ID = Item.value("attribute_id", -1);
			const auto Leader = Item.value("detail", TrackingAttributeData {});
			if(ID >= 0 && Leader.AccountID > 0 && CAttributeDescription::Data().contains((AttributeIdentifier)ID))
				m_AttributeLeaders[ID] = Leader;
		}
	});

	if(HasError)
		dbg_msg("attributes_tracking", "Failed to load '%s'.", ATTRIBUTE_TRACKING_FILE);
}

bool CTrackingData::UpdateAttribute(CPlayer* pPlayer, AttributeIdentifier Attribute, int Value)
{
	if(!pPlayer || pPlayer->IsBot() || !pPlayer->IsAuthed() || Value <= 0 ||
		!CAttributeDescription::Data().contains(Attribute))
		return false;

	auto& Leader = m_AttributeLeaders[(int)Attribute];
	if(Value <= Leader.Amount)
		return false;

	Leader = { pPlayer->Account()->GetID(), Value };
	return true;
}

void CTrackingData::SaveAttributes() const
{
	nlohmann::json Data = nlohmann::json::object();
	for(const auto& [ID, Leader] : m_AttributeLeaders)
		Data["tracking"].push_back({ {"attribute_id", ID}, {"detail", Leader} });

	AsyncFile::Write(ATTRIBUTE_TRACKING_FILE, Data.dump());
}
