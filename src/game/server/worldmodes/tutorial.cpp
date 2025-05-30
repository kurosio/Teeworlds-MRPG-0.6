/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tutorial.h"

#include <game/server/core/tools/scenario_player_manager.h>
#include <game/server/core/scenarios/scenario_universal.h>
#include <game/server/gamecontext.h>

CGameControllerTutorial::CGameControllerTutorial(class CGS* pGS)
	: IGameController(pGS)
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
	IGameController::Tick();
}

bool CGameControllerTutorial::OnCharacterSpawn(CCharacter* pChr)
{
	// start tutorial scenario
	pChr->GetPlayer()->StartUniversalScenario(m_JsonTutorialData.dump(), EScenarios::SCENARIO_TUTORIAL);
	return IGameController::OnCharacterSpawn(pChr);
}

void CGameControllerTutorial::Snap()
{
	// vanilla snap
	CNetObj_GameInfo* pGameInfoObj = (CNetObj_GameInfo*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	pGameInfoObj->m_RoundStartTick = 0;
	pGameInfoObj->m_WarmupTimer = 0;
	pGameInfoObj->m_RoundNum = 0;
	pGameInfoObj->m_RoundCurrent = 1;

	// ddnet snap
	CNetObj_GameInfoEx* pGameInfoEx = (CNetObj_GameInfoEx*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFOEX, 0, sizeof(CNetObj_GameInfoEx));
	if(!pGameInfoEx)
		return;

	pGameInfoEx->m_Flags = GAMEINFOFLAG_GAMETYPE_PLUS | GAMEINFOFLAG_ALLOW_EYE_WHEEL | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_PREDICT_VANILLA;
	pGameInfoEx->m_Flags2 = GAMEINFOFLAG2_GAMETYPE_CITY | GAMEINFOFLAG2_ALLOW_X_SKINS | GAMEINFOFLAG2_HUD_DDRACE;
	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;
}
