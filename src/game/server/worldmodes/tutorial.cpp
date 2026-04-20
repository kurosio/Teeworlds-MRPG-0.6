/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tutorial.h"

#include <scenarios/managers/scenario_player_manager.h>
#include <scenarios/impl/scenario_universal.h>

#include <game/server/gamecontext.h>

CGameControllerTutorial::CGameControllerTutorial(class CGS* pGS)
	: CGameControllerDefault(pGS)
{
	m_GameFlags = 0;
}

void CGameControllerTutorial::OnInit()
{
	ByteArray RawData;
	if(!mystd::file::load("server_data/tutorial_data.json", &RawData))
		return;

	// initialize jsonData
	const std::string jsonRawData = (char*)RawData.data();
	bool hasError = mystd::json::parse(jsonRawData, [this](nlohmann::json& j)
	{
		m_JsonTutorialData = std::move(j);
	});

	dbg_assert(!hasError, "scenario-tutorial: invalid JSON file.");
}

void CGameControllerTutorial::Tick()
{
	CGameControllerDefault::Tick();
}

bool CGameControllerTutorial::OnCharacterSpawn(CCharacter* pChr)
{
	// start tutorial scenario
	if(pChr->GetPlayer()->IsAuthed())
		pChr->GetPlayer()->StartUniversalScenario(m_JsonTutorialData.dump(), EScenarios::SCENARIO_TUTORIAL);
	return CGameControllerDefault::OnCharacterSpawn(pChr);
}

void CGameControllerTutorial::Snap()
{
	constexpr int Flags = GAMEINFOFLAG_GAMETYPE_PLUS | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_PREDICT_VANILLA;
	constexpr int Flags2 = GAMEINFOFLAG2_GAMETYPE_CITY | GAMEINFOFLAG2_ALLOW_X_SKINS | GAMEINFOFLAG2_HUD_DDRACE;
	SnapGameInfo(0, 0, 0, Flags, Flags2);
}
