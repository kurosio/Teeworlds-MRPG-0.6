/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "rhythm.h"

#include <base/math.h>
#include <base/system.h>

#include <algorithm>
#include <cmath>
#include <string>

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <generated/protocol.h>
#include <game/gamecore.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>

#include "entities/rhythm_field.h"
#include <generated/server_data.h>

namespace
{
	enum class SoundRhythm : int
	{
		MUSIC,
		PREPARE,
	};

	bool IsValidClientID(int ClientID)
	{
		return ClientID >= 0 && ClientID < MAX_PLAYERS;
	}
}

CGameControllerRhythm::CGameControllerRhythm(CGS* pGameServer)
	: IGameController(pGameServer)
{
	m_Meta = {};
	m_FieldAnchorPos = vec2(0.0f, 0.0f);
	m_FieldAnchorValid = false;
	mem_zero(m_aLanePressTick, sizeof(m_aLanePressTick));
	mem_zero(m_aLaneLastHitTick, sizeof(m_aLaneLastHitTick));
	mem_zero(m_aLaneHoldTick, sizeof(m_aLaneHoldTick));
	mem_zero(m_aLaneHoldStartTick, sizeof(m_aLaneHoldStartTick));
	mem_zero(m_aLaneHoldEndTick, sizeof(m_aLaneHoldEndTick));
	mem_zero(m_aLaneHoldActive, sizeof(m_aLaneHoldActive));
	mem_zero(m_aLaneHoldEndEffectPlayed, sizeof(m_aLaneHoldEndEffectPlayed));
	mem_zero(m_aLanePressId, sizeof(m_aLanePressId));
	mem_zero(m_aLanePressUsedId, sizeof(m_aLanePressUsedId));
	mem_zero(m_aNoteLaneHitMask, sizeof(m_aNoteLaneHitMask));
	mem_zero(m_aHoldSegmentLaneHitMask, sizeof(m_aHoldSegmentLaneHitMask));
	for(auto &Score : m_aScores)
		Score = {};
}

void CGameControllerRhythm::OnInit()
{
	// initialize variables
	m_WarmupTick = 0;
	m_RoundStartTick = 0;
	m_CurrentNote = 0;
	m_NextSpawnNote = 0;
	m_CurrentHoldSegment = 0;
	m_RoundFinished = false;
	m_ResultsSaved = false;
	m_EndTick = 0;
	m_FinishTick = 0;
	m_State = EStageState::STATE_WAIT;

	// initialize
	LoadDanceMapData(Server()->GetWorldName(GS()->GetWorldID()));
	RebuildTickTimings();
	for(int ClientID = 0; ClientID < MAX_PLAYERS; ++ClientID)
		ResetClientState(ClientID);
}

void CGameControllerRhythm::Tick()
{
	const int CurrentTick = Server()->Tick();
	const auto ClientsNum = Server()->GetClientsCountByWorld(GS()->GetWorldID());

	// update start or end by player count
	if(ClientsNum <= 0)
		ChangeState(EStageState::STATE_WAIT);
	else if(m_State == EStageState::STATE_WAIT)
		ChangeState(EStageState::STATE_WARMUP);

	// is active warmup
	if(m_State == EStageState::STATE_WARMUP)
	{
		// warmup sound notify
		if(m_WarmupTick == Server()->TickSpeed() * 3)
		{
			CNetMsg_Sv_MapSoundGlobal Msg;
			Msg.m_SoundId = (int)SoundRhythm::PREPARE;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1, -1, GS()->GetWorldID());
		}

		if(m_WarmupTick > 0)
			m_WarmupTick--;
		if(m_WarmupTick == 0)
			ChangeState(EStageState::STATE_ACTIVE);
	}


	// update active state
	if(m_State == EStageState::STATE_ACTIVE && !m_RoundFinished)
	{
		for(int ClientID = 0; ClientID < MAX_PLAYERS; ++ClientID)
		{
			auto* pPlayer = GS()->GetPlayer(ClientID);
			if(!pPlayer || !GS()->GetPlayerChar(ClientID))
			{
				ResetClientState(ClientID);
				continue;
			}

			if(m_FieldAnchorValid)
				pPlayer->LockedView().ViewLock(m_FieldAnchorPos + vec2(SRhythmFieldConfig::s_FieldViewOffsetX, SRhythmFieldConfig::s_FieldViewOffsetY));
			else
				pPlayer->LockedView().Reset();

			if(m_State == EStageState::STATE_ACTIVE && pPlayer->m_pLastInput)
				ProcessPlayerInput(ClientID, *pPlayer->m_pLastInput, CurrentTick);
		}

		UpdateNotes();
		if(CurrentTick > m_EndTick)
		{
			m_RoundFinished = true;
			ChangeState(EStageState::STATE_FINISHED);
			GS()->ChatWorld(GS()->GetWorldID(), nullptr, "Rhythm round finished.");
		}
	}
	else if(m_State == EStageState::STATE_FINISHED)
	{
		if(!m_ResultsSaved)
		{
			SaveRhythmResults();
			m_ResultsSaved = true;
		}

		if(m_FinishTick > 0)
			m_FinishTick--;
		else if(m_FinishTick == 0)
			ChangeState(EStageState::STATE_WARMUP);
	}

	IGameController::Tick();
}

void CGameControllerRhythm::Snap()
{
	auto* pGameInfoObj = (CNetObj_GameInfo*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	if(m_State == EStageState::STATE_FINISHED)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_GAMEOVER;
	pGameInfoObj->m_RoundStartTick = m_RoundStartTick;
	pGameInfoObj->m_WarmupTimer = m_State == EStageState::STATE_WARMUP ? m_WarmupTick : 0;
	pGameInfoObj->m_RoundNum = 0;
	pGameInfoObj->m_RoundCurrent = 1;

	auto* pGameInfoEx = (CNetObj_GameInfoEx*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFOEX, 0, sizeof(CNetObj_GameInfoEx));
	if(!pGameInfoEx)
		return;

	pGameInfoEx->m_Flags = GAMEINFOFLAG_GAMETYPE_PLUS | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_PREDICT_VANILLA;
	pGameInfoEx->m_Flags2 = GAMEINFOFLAG2_GAMETYPE_CITY | GAMEINFOFLAG2_ALLOW_X_SKINS | GAMEINFOFLAG2_HUD_DDRACE;
	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;
}

bool CGameControllerRhythm::ParseStepBits(const nlohmann::json& Value, uint8_t* pOut) const
{
	if(!Value.is_string())
		return false;

	const std::string BitsStr = Value.get<std::string>();
	const char* pBits = BitsStr.c_str();
	if(str_length(pBits) != 4)
		return false;

	const uint8_t aFlags[4] = {STEP_BIT_LEFT, STEP_BIT_RIGHT, STEP_BIT_UP, STEP_BIT_DOWN};
	uint8_t Bits = 0;
	for(int i = 0; i < 4; ++i)
	{
		if(pBits[i] == '1')
			Bits |= aFlags[i];
		else if(pBits[i] != '0')
			return false;
	}
	*pOut = Bits;
	return true;
}

int CGameControllerRhythm::NoteTimeToTick(double NoteTime) const
{
	return m_RoundStartTick + round_to_int(NoteTime * Server()->TickSpeed());
}

void CGameControllerRhythm::RebuildTickTimings()
{
	m_vNoteTicks.clear();
	m_vNoteTicks.reserve(m_vNotes.size());
	for(const auto &Note : m_vNotes)
		m_vNoteTicks.push_back(NoteTimeToTick(Note.m_Time));

	m_vHoldSegmentTicks.clear();
	m_vHoldSegmentTicks.reserve(m_vHoldSegments.size());
	for(const auto &Segment : m_vHoldSegments)
		m_vHoldSegmentTicks.push_back(NoteTimeToTick(Segment.m_Time));
}

void CGameControllerRhythm::ChangeState(EStageState State)
{
	if(m_State == State)
		return;

	m_State = State;
	if(m_State == EStageState::STATE_WARMUP)
	{
		m_WarmupTick = Server()->TickSpeed() * 15;
		m_FieldAnchorValid = FindFieldAnchorFromMap(m_FieldAnchorPos);
	}

	if(m_State == EStageState::STATE_ACTIVE)
	{
		// reset old field
		if(m_pRhythmField)
		{
			m_pRhythmField->Reset();
			m_pRhythmField = nullptr;
		}

		if(m_FieldAnchorValid && !m_pRhythmField)
		{
			const vec2 FieldPos = m_FieldAnchorPos;
			const float Bpm = m_Meta.m_Bpm > 0.0f ? m_Meta.m_Bpm : 120.0f;
			m_pRhythmField = new CRhythmField(&GS()->m_World, FieldPos, Bpm, 32.0f);
			if(m_pRhythmField)
			{
				m_pRhythmField->SetAutoSpawn(false);
				m_pRhythmField->SetHitZone(FieldPos);
			}
		}

		// start music
		CNetMsg_Sv_MapSoundGlobal Msg;
		Msg.m_SoundId = (int)SoundRhythm::MUSIC;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1, -1, GS()->GetWorldID());

		// prepare
		m_RoundStartTick = Server()->Tick();
		m_CurrentNote = 0;
		m_NextSpawnNote = 0;
		m_CurrentHoldSegment = 0;
		m_RoundFinished = false;
		m_ResultsSaved = false;
		m_EndTick = m_RoundStartTick + round_to_int(m_Meta.m_DurationSeconds * Server()->TickSpeed());
		m_FinishTick = 0;
		RebuildTickTimings();
		return;
	}

	if(m_State == EStageState::STATE_FINISHED)
	{
		m_FinishTick = Server()->TickSpeed() * 10;
		return;
	}

	// warmup reset
	m_WarmupTick = Server()->TickSpeed() * 15;
	m_RoundStartTick = Server()->Tick();
	m_CurrentNote = 0;
	m_NextSpawnNote = 0;
	m_CurrentHoldSegment = 0;
	m_RoundFinished = false;
	m_ResultsSaved = false;
	m_EndTick = 0;
	m_FinishTick = 0;
	RebuildTickTimings();
	for(int ClientID = 0; ClientID < MAX_PLAYERS; ++ClientID)
		ResetClientState(ClientID);
	GS()->ChatWorld(GS()->GetWorldID(), nullptr, "Warmup started. New rhythm round begins in {} sec.", m_WarmupTick / Server()->TickSpeed());
}

void CGameControllerRhythm::FillLaneBits(uint8_t StepBits, int (&aLaneBits)[ms_LaneCount]) const
{
	aLaneBits[0] = StepBits & STEP_BIT_LEFT;
	aLaneBits[1] = StepBits & (STEP_BIT_UP | STEP_BIT_DOWN);
	aLaneBits[2] = StepBits & STEP_BIT_RIGHT;
}

bool CGameControllerRhythm::LoadDanceMapData(const char* pMapName)
{
	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "maps/%s.json", pMapName);

	void* pFileData = nullptr;
	unsigned FileSize = 0;
	if(!GS()->Storage()->ReadFile(aFilename, IStorageEngine::TYPE_ALL, &pFileData, &FileSize))
		return false;

	nlohmann::json JsonData;
	const std::string RawJson((const char*)pFileData, FileSize);
	free(pFileData);
	try
	{
		JsonData = nlohmann::json::parse(RawJson);
	}
	catch(const nlohmann::json::exception&)
	{
		return false;
	}

	if(!JsonData.is_object())
		return false;

	m_vNotes.clear();
	m_vNoteTicks.clear();
	m_Meta = {};

	const nlohmann::json& Meta = JsonData["meta"];
	const nlohmann::json& Notes = JsonData["notes"];
	const nlohmann::json& Holds = JsonData["holds"];

	if(!Meta.is_object() || !Notes.is_array() || !Holds.is_array())
		return false;

	const auto AudioFile = Meta.value("audio_file", std::string());
	str_copy(m_Meta.m_aAudioFile, AudioFile.c_str(), sizeof(m_Meta.m_aAudioFile));
	m_Meta.m_HopLength = Meta.value("hop_length", 0);
	m_Meta.m_Bpm = Meta.value("bpm", 120.0f);
	m_Meta.m_DurationSeconds = Meta.value("duration_seconds", 0.0f);
	m_Meta.m_NotesCount = Meta.value("notes_count", 0);
	m_Meta.m_TapCount = Meta.value("tap_count", 0);
	m_Meta.m_HoldsCount = Meta.value("holds_count", 0);
	m_Meta.m_ParticleFallSpeed = Meta.value("particle_fall_speed", SRhythmFieldConfig::s_DefaultFallSpeedPerBeat);

	for(const auto& Note : Notes)
	{
		if(!Note.is_object())
			continue;

		CNote ParsedNote{};
		ParsedNote.m_Time = Note.value("t", -1.0);
		ParsedNote.m_TimeEnd = ParsedNote.m_Time;
		ParsedNote.m_IsHold = false;
		if(ParsedNote.m_Time < 0.0 || !ParseStepBits(Note["step_bits"], &ParsedNote.m_StepBits))
			continue;
		m_vNotes.push_back(ParsedNote);
	}

	for(const auto& Hold : Holds)
	{
		if(!Hold.is_object())
			continue;

		CNote ParsedHold{};
		ParsedHold.m_Time = Hold.value("t", -1.0);
		ParsedHold.m_TimeEnd = Hold.value("t_end", -1.0);
		ParsedHold.m_IsHold = true;
		if(ParsedHold.m_Time < 0.0 || ParsedHold.m_TimeEnd < ParsedHold.m_Time || !ParseStepBits(Hold["step_bits"], &ParsedHold.m_StepBits))
			continue;
		if(ParsedHold.m_StepBits == STEP_BIT_UP || ParsedHold.m_StepBits == STEP_BIT_DOWN || ParsedHold.m_StepBits == STEP_BIT_RIGHT)
			ParsedHold.m_StepBits = STEP_BIT_UP | STEP_BIT_DOWN;
		m_vNotes.push_back(ParsedHold);
	}

	std::stable_sort(m_vNotes.begin(), m_vNotes.end(), [](const CNote& Left, const CNote& Right)
	{
		return Left.m_Time < Right.m_Time;
	});

	m_vHoldSegments.clear();
	for(const auto& Note : m_vNotes)
	{
		if(!Note.m_IsHold)
			continue;
		const double Duration = Note.m_TimeEnd - Note.m_Time;
		for(int SegmentIndex = 1; SegmentIndex <= 3; ++SegmentIndex)
		{
			const double SegmentTime = Note.m_Time + Duration * (SegmentIndex / 3.0);
			m_vHoldSegments.push_back({SegmentTime, Note.m_StepBits});
		}
	}
	std::stable_sort(m_vHoldSegments.begin(), m_vHoldSegments.end(), [](const CHoldSegment& Left, const CHoldSegment& Right)
	{
		return Left.m_Time < Right.m_Time;
	});
	RebuildTickTimings();

	return true;
}

void CGameControllerRhythm::ResetClientState(int ClientID)
{
	if(!IsValidClientID(ClientID))
		return;

	m_aPrevInputs[ClientID] = CNetObj_PlayerInput{};
	m_aLatestInputs[ClientID] = CNetObj_PlayerInput{};
	for(int LaneIndex = 0; LaneIndex < ms_LaneCount; ++LaneIndex)
	{
		m_aLanePressTick[ClientID][LaneIndex] = SRhythmFieldConfig::s_InvalidPressTick;
		m_aLaneLastHitTick[ClientID][LaneIndex] = SRhythmFieldConfig::s_InvalidPressTick;
		m_aLaneHoldTick[ClientID][LaneIndex] = SRhythmFieldConfig::s_InvalidPressTick;
		m_aLaneHoldStartTick[ClientID][LaneIndex] = SRhythmFieldConfig::s_InvalidPressTick;
		m_aLaneHoldEndTick[ClientID][LaneIndex] = SRhythmFieldConfig::s_InvalidPressTick;
		m_aLaneHoldActive[ClientID][LaneIndex] = false;
		m_aLaneHoldEndEffectPlayed[ClientID][LaneIndex] = false;
	}
	mem_zero(m_aLanePressId[ClientID], sizeof(m_aLanePressId[ClientID]));
	mem_zero(m_aLanePressUsedId[ClientID], sizeof(m_aLanePressUsedId[ClientID]));
	m_aNoteLaneHitMask[ClientID] = 0;
	m_aHoldSegmentLaneHitMask[ClientID] = 0;
	m_aScores[ClientID] = {};
}

void CGameControllerRhythm::TryStartHold(int ClientID, int LaneIndex, int PressTick, int HitWindowTicks)
{
	if(!IsValidClientID(ClientID) || PressTick == SRhythmFieldConfig::s_InvalidPressTick)
		return;

	if(m_aLaneHoldActive[ClientID][LaneIndex] && PressTick <= m_aLaneHoldEndTick[ClientID][LaneIndex])
		return;

	const int HoldWindowTicks = SRhythmFieldConfig::s_BadWindowTicks + HitWindowTicks;
	int BestNoteIndex = -1;
	int BestDelta = 0;

	for(int NoteIndex = m_CurrentNote; NoteIndex < (int)m_vNotes.size(); ++NoteIndex)
	{
		const CNote& Note = m_vNotes[NoteIndex];
		if(!Note.m_IsHold)
			continue;

		const int NoteTick = m_vNoteTicks[NoteIndex];
		if(NoteTick < PressTick - HoldWindowTicks)
			continue;
		if(NoteTick > PressTick + HoldWindowTicks)
			break;

		const int NoteEndTick = NoteTimeToTick(Note.m_TimeEnd);
		if(PressTick > NoteEndTick + HoldWindowTicks)
			continue;

		int aLaneBits[ms_LaneCount];
		FillLaneBits(Note.m_StepBits, aLaneBits);
		if(!aLaneBits[LaneIndex])
			continue;

		const int RawDelta = std::abs(PressTick - NoteTick);
		if(BestNoteIndex == -1 || RawDelta < BestDelta)
		{
			BestNoteIndex = NoteIndex;
			BestDelta = RawDelta;
		}
	}

	if(BestNoteIndex == -1)
		return;

	const CNote& Note = m_vNotes[BestNoteIndex];
	const int NoteTick = m_vNoteTicks[BestNoteIndex];
	const int NoteEndTick = NoteTimeToTick(Note.m_TimeEnd);
	m_aLaneHoldActive[ClientID][LaneIndex] = true;
	m_aLaneHoldStartTick[ClientID][LaneIndex] = NoteTick;
	m_aLaneHoldEndTick[ClientID][LaneIndex] = NoteEndTick;
	m_aLaneHoldTick[ClientID][LaneIndex] = PressTick;
	m_aLaneHoldEndEffectPlayed[ClientID][LaneIndex] = false;
	m_aLanePressUsedId[ClientID][LaneIndex] = m_aLanePressId[ClientID][LaneIndex];

	const int RatingDelta = maximum(0, BestDelta - HitWindowTicks);
	ScoreHit(ClientID, RatingDelta);
}

void CGameControllerRhythm::ScoreHit(int ClientID, int RatingDelta)
{
	if(!IsValidClientID(ClientID))
		return;

	if(RatingDelta <= SRhythmFieldConfig::s_PerfectWindowTicks)
	{
		m_aScores[ClientID].m_Perfect++;
		m_aScores[ClientID].m_LastGrade = ERhythmHitGrade::PERFECT;
	}
	else if(RatingDelta <= SRhythmFieldConfig::s_GoodWindowTicks)
	{
		m_aScores[ClientID].m_Good++;
		m_aScores[ClientID].m_LastGrade = ERhythmHitGrade::GOOD;
	}
	else if(RatingDelta <= SRhythmFieldConfig::s_BadWindowTicks)
	{
		m_aScores[ClientID].m_Bad++;
		m_aScores[ClientID].m_LastGrade = ERhythmHitGrade::BAD;
	}
	else
	{
		m_aScores[ClientID].m_Miss++;
		m_aScores[ClientID].m_LastGrade = ERhythmHitGrade::MISS;
	}
}

int CGameControllerRhythm::ScorePoints(const SRhythmScore& Score) const
{
	return Score.m_Perfect * 3 + Score.m_Good * 2 + Score.m_Bad;
}

void CGameControllerRhythm::ProcessPlayerInput(int ClientID, const CNetObj_PlayerInput& Input, int CurrentTick)
{
	if(!m_pRhythmField)
		return;

	CNetObj_PlayerInput& PrevInput = m_aPrevInputs[ClientID];
	m_aLatestInputs[ClientID] = Input;
	const int HitWindowTicks = 0;

	const bool aLanePressed[ms_LaneCount] = {
		Input.m_Direction < 0 && PrevInput.m_Direction >= 0,
		CountInput(PrevInput.m_Jump, Input.m_Jump).m_Presses > 0,
		Input.m_Direction > 0 && PrevInput.m_Direction <= 0,
	};
	const bool aLaneHeld[ms_LaneCount] = {
		Input.m_Direction < 0,
		(Input.m_Jump & 1) != 0,
		Input.m_Direction > 0,
	};

	for(int LaneIndex = 0; LaneIndex < ms_LaneCount; ++LaneIndex)
	{
		if(aLanePressed[LaneIndex])
		{
			m_aLanePressId[ClientID][LaneIndex]++;
			m_aLanePressTick[ClientID][LaneIndex] = CurrentTick;
		}
		if(aLaneHeld[LaneIndex])
			m_aLaneHoldTick[ClientID][LaneIndex] = CurrentTick;
	}

	for(int LaneIndex = 0; LaneIndex < ms_LaneCount; ++LaneIndex)
	{
		if(aLanePressed[LaneIndex])
			TryStartHold(ClientID, LaneIndex, m_aLanePressTick[ClientID][LaneIndex], HitWindowTicks);
		else if(aLaneHeld[LaneIndex] && !m_aLaneHoldActive[ClientID][LaneIndex])
			TryStartHold(ClientID, LaneIndex, m_aLaneHoldTick[ClientID][LaneIndex], HitWindowTicks);
	}

	const vec2 HitPos = m_pRhythmField->HitZonePos();
	const float HalfWidth = SRhythmFieldConfig::s_LaneWidth * 1.5f;
	for(int NoteIndex = m_CurrentNote; NoteIndex < (int)m_vNotes.size(); ++NoteIndex)
	{
		const CNote& Note = m_vNotes[NoteIndex];
		if(Note.m_IsHold)
			continue;

		const int NoteTick = m_vNoteTicks[NoteIndex];
		if(CurrentTick < NoteTick - SRhythmFieldConfig::s_BadWindowTicks)
			break;
		if(CurrentTick > NoteTick + SRhythmFieldConfig::s_BadWindowTicks)
			continue;

		int aLaneBits[ms_LaneCount];
		FillLaneBits(Note.m_StepBits, aLaneBits);
		for(int LaneIndex = 0; LaneIndex < ms_LaneCount; ++LaneIndex)
		{
			if(!aLaneBits[LaneIndex])
				continue;

			const uint8_t LaneMask = 1u << LaneIndex;
			if(m_aNoteLaneHitMask[ClientID] & LaneMask)
				continue;
			if(m_pRhythmField->IsHiddenArrowForClient(LaneIndex, NoteTick, ClientID))
				continue;
			if(m_aLanePressId[ClientID][LaneIndex] == m_aLanePressUsedId[ClientID][LaneIndex])
				continue;

			int PressTick = m_aLanePressTick[ClientID][LaneIndex];
			int RawDelta = std::abs(PressTick - NoteTick);
			if(RawDelta > SRhythmFieldConfig::s_BadWindowTicks && aLaneHeld[LaneIndex])
			{
				const int HeldTick = m_aLaneHoldTick[ClientID][LaneIndex];
				const int HeldDelta = std::abs(HeldTick - NoteTick);
				if(HeldDelta <= SRhythmFieldConfig::s_BadWindowTicks)
				{
					PressTick = HeldTick;
					RawDelta = HeldDelta;
				}
			}
			if(RawDelta > SRhythmFieldConfig::s_BadWindowTicks)
				continue;

			const float X = HitPos.x - HalfWidth + SRhythmFieldConfig::s_LaneWidth * (LaneIndex + 0.5f);
			const vec2 EffectPos(X, HitPos.y);
			GS()->CreateExplosion(EffectPos, -1, WEAPON_GRENADE, 0);
			GS()->CreateSound(EffectPos, SOUND_PICKUP_HEALTH);
			ScoreHit(ClientID, RawDelta);
			m_pRhythmField->HideArrowForClient(LaneIndex, NoteTick, ClientID);
			m_aNoteLaneHitMask[ClientID] |= LaneMask;
			m_aLaneLastHitTick[ClientID][LaneIndex] = NoteTick;
			m_aLanePressUsedId[ClientID][LaneIndex] = m_aLanePressId[ClientID][LaneIndex];
		}
	}

	PrevInput = Input;
}

void CGameControllerRhythm::UpdateNotes()
{
	const int CurrentTick = Server()->Tick();
	const int LeadTicks = std::max(1, (int)std::round(m_pRhythmField->BeatIntervalTicks() * SRhythmFieldConfig::s_LeadBeats));

	while(m_NextSpawnNote < (int)m_vNotes.size() && CurrentTick >= m_vNoteTicks[m_NextSpawnNote] - LeadTicks)
	{
		const CNote& Note = m_vNotes[m_NextSpawnNote];
		const int NoteTick = m_vNoteTicks[m_NextSpawnNote];
		const int NoteEndTick = Note.m_IsHold ? NoteTimeToTick(Note.m_TimeEnd) : NoteTick;
		const int HoldDurationTicks = maximum(0, NoteEndTick - NoteTick);
		int aLaneBits[ms_LaneCount];
		FillLaneBits(Note.m_StepBits, aLaneBits);
		for(int LaneIndex = 0; LaneIndex < ms_LaneCount; ++LaneIndex)
		{
			if(aLaneBits[LaneIndex])
				m_pRhythmField->SpawnLaneArrow(LaneIndex, NoteTick, HoldDurationTicks);
		}
		++m_NextSpawnNote;
	}

	while(m_CurrentNote < (int)m_vNotes.size())
	{
		const CNote& Note = m_vNotes[m_CurrentNote];
		const int NoteTick = m_vNoteTicks[m_CurrentNote];
		if(CurrentTick < NoteTick - SRhythmFieldConfig::s_BadWindowTicks)
			break;

		const bool WindowExpired = CurrentTick > NoteTick + SRhythmFieldConfig::s_BadWindowTicks;
		if(Note.m_IsHold)
		{
			if(!WindowExpired)
				break;
			++m_CurrentNote;
			for(int i = 0; i < MAX_PLAYERS; ++i)
				m_aNoteLaneHitMask[i] = 0;
			continue;
		}

		int aLaneBits[ms_LaneCount];
		FillLaneBits(Note.m_StepBits, aLaneBits);
		if(WindowExpired)
		{
			for(int i = 0; i < MAX_PLAYERS; ++i)
			{
				for(int LaneIndex = 0; LaneIndex < ms_LaneCount; ++LaneIndex)
				{
					if(!aLaneBits[LaneIndex])
						continue;
					const uint8_t LaneMask = 1u << LaneIndex;
					if(m_aNoteLaneHitMask[i] & LaneMask)
						continue;
					m_aScores[i].m_Miss++;
					m_aScores[i].m_LastGrade = ERhythmHitGrade::MISS;
				}
			}
		}

		if(!WindowExpired)
			break;

		++m_CurrentNote;
		for(int i = 0; i < MAX_PLAYERS; ++i)
			m_aNoteLaneHitMask[i] = 0;
	}

	const int HoldWindowTicks = SRhythmFieldConfig::s_BadWindowTicks;
	auto LaneHeld = [](const CNetObj_PlayerInput& Input, int LaneIndex)
	{
		if(LaneIndex == 0)
			return Input.m_Direction < 0;
		if(LaneIndex == 1)
			return (Input.m_Jump & 1) != 0;
		return Input.m_Direction > 0;
	};

	while(m_CurrentHoldSegment < (int)m_vHoldSegments.size())
	{
		const CHoldSegment& Segment = m_vHoldSegments[m_CurrentHoldSegment];
		const int SegmentTick = m_vHoldSegmentTicks[m_CurrentHoldSegment];
		if(CurrentTick < SegmentTick - HoldWindowTicks)
			break;

		int aLaneBits[ms_LaneCount];
		FillLaneBits(Segment.m_StepBits, aLaneBits);
		for(int i = 0; i < MAX_PLAYERS; ++i)
		{
			for(int LaneIndex = 0; LaneIndex < ms_LaneCount; ++LaneIndex)
			{
				if(!aLaneBits[LaneIndex])
					continue;
				const uint8_t LaneMask = 1u << LaneIndex;
				if(m_aHoldSegmentLaneHitMask[i] & LaneMask)
					continue;
				if(!m_aLaneHoldActive[i][LaneIndex] || !LaneHeld(m_aLatestInputs[i], LaneIndex))
					continue;

				const int RawDelta = std::abs(m_aLaneHoldTick[i][LaneIndex] - SegmentTick);
				if(RawDelta > HoldWindowTicks)
					continue;

				ScoreHit(i, RawDelta);
				m_aHoldSegmentLaneHitMask[i] |= LaneMask;
			}
		}

		if(CurrentTick <= SegmentTick + HoldWindowTicks)
			break;

		for(int i = 0; i < MAX_PLAYERS; ++i)
		{
			for(int LaneIndex = 0; LaneIndex < ms_LaneCount; ++LaneIndex)
			{
				if(!aLaneBits[LaneIndex])
					continue;
				const uint8_t LaneMask = 1u << LaneIndex;
				if(m_aHoldSegmentLaneHitMask[i] & LaneMask)
					continue;
				m_aScores[i].m_Miss++;
				m_aScores[i].m_LastGrade = ERhythmHitGrade::MISS;
			}
			m_aHoldSegmentLaneHitMask[i] = 0;
		}
		++m_CurrentHoldSegment;
	}
}

void CGameControllerRhythm::SaveRhythmResults()
{
	for(int ClientID = 0; ClientID < MAX_PLAYERS; ++ClientID)
	{
		auto* pPlayer = GS()->GetPlayer(ClientID);
		if(!pPlayer || !GS()->GetPlayerChar(ClientID))
			continue;

		const auto& Score = m_aScores[ClientID];
		const int Points = ScorePoints(Score);
		if(!(Score.m_Perfect || Score.m_Good || Score.m_Bad || Score.m_Miss))
			continue;

		Server()->SetClientScore(ClientID, Points);
		GS()->Chat(ClientID, "Rhythm result: {} points (Perfect: {}, Good: {}, Bad: {}, Miss: {}).",
			Points, Score.m_Perfect, Score.m_Good, Score.m_Bad, Score.m_Miss);
	}
}

bool CGameControllerRhythm::FindFieldAnchorFromMap(vec2& OutPos) const
{
	CCollision* pCollision = GS()->Collision();
	const int Width = pCollision->GetWidth();
	const int Height = pCollision->GetHeight();
	const int TargetIndex = ENTITY_OFFSET + ENTITY_RHYTHM_FIELD;
	for(int y = 0; y < Height; ++y)
	{
		for(int x = 0; x < Width; ++x)
		{
			const int Index = y * Width + x;
			if(pCollision->GetMainTileIndex(Index) != TargetIndex)
				continue;
			OutPos = pCollision->GetPos(Index);
			return true;
		}
	}
	return false;
}