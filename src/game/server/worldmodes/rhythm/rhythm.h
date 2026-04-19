/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_RHYTHM_RHYTHM_H
#define GAME_SERVER_GAMEMODES_RHYTHM_RHYTHM_H

#include <game/server/gamecontroller.h>

#include <array>
#include <cstdint>
#include <vector>

class CRhythmField;

class CGameControllerRhythm : public IGameController
{
public:
	CGameControllerRhythm(class CGS* pGameServer);
	void OnInit() override;
	void Tick() override;
	void Snap() override;

private:
	enum class EStageState
	{
		STATE_WAIT,
		STATE_WARMUP,
		STATE_ACTIVE,
		STATE_FINISHED,
	};

	struct CNote
	{
		double m_Time{};
		double m_TimeEnd{};
		uint8_t m_StepBits{};
		bool m_IsHold{};
	};

	struct CHoldSegment
	{
		double m_Time{};
		uint8_t m_StepBits{};
	};

	enum class ERhythmHitGrade
	{
		NONE,
		PERFECT,
		GOOD,
		BAD,
		MISS,
	};

	struct SRhythmMeta
	{
		char m_aAudioFile[128]{};
		int m_HopLength{};
		float m_Bpm{};
		float m_DurationSeconds{};
		float m_ParticleFallSpeed{};
		int m_NotesCount{};
		int m_TapCount{};
		int m_HoldsCount{};
	};

	struct SRhythmScore
	{
		int m_Perfect{};
		int m_Good{};
		int m_Bad{};
		int m_Miss{};
		ERhythmHitGrade m_LastGrade{ERhythmHitGrade::NONE};
	};

	static constexpr int ms_LaneCount = 3;
	static constexpr uint8_t STEP_BIT_LEFT = 1 << 0;
	static constexpr uint8_t STEP_BIT_RIGHT = 1 << 1;
	static constexpr uint8_t STEP_BIT_UP = 1 << 2;
	static constexpr uint8_t STEP_BIT_DOWN = 1 << 3;

	void OnEntity(int Index, vec2 Pos, int Flags) override;
	bool OnCharacterSpawn(class CCharacter* pChr) override;

	bool LoadDanceMapData(const char* pMapName);
	bool ParseStepBits(const nlohmann::json& Value, uint8_t* pOut) const;
	void UpdateNotes();
	void ProcessPlayerInput(int ClientID, const class CNetObj_PlayerInput& Input, int CurrentTick);
	void TryStartHold(int ClientID, int LaneIndex, int PressTick, int HitWindowTicks);
	void FillLaneBits(uint8_t StepBits, int (&aLaneBits)[ms_LaneCount]) const;
	void ScoreHit(int ClientID, int RatingDelta);
	int ScorePoints(const SRhythmScore& Score) const;
	void ResetClientState(int ClientID);
	void SaveRhythmResults();
	bool FindFieldAnchorFromMap(vec2& OutPos) const;
	int NoteTimeToTick(double NoteTime) const;
	void ChangeState(EStageState State);
	void RebuildTickTimings();

	SRhythmMeta m_Meta{};
	std::vector<CNote> m_vNotes{};
	std::vector<int> m_vNoteTicks{};
	std::vector<CHoldSegment> m_vHoldSegments{};
	std::vector<int> m_vHoldSegmentTicks{};
	CRhythmField* m_pRhythmField{};
	EStageState m_State{EStageState::STATE_WARMUP};
	int m_WarmupTick{};
	int m_RoundStartTick{};
	int m_CurrentNote{};
	int m_NextSpawnNote{};
	int m_CurrentHoldSegment{};
	int m_EndTick{};
	int m_FinishTick{};
	bool m_ResultsSaved{};

	std::array<CNetObj_PlayerInput, MAX_PLAYERS> m_aPrevInputs{};
	std::array<CNetObj_PlayerInput, MAX_PLAYERS> m_aLatestInputs{};
	int m_aLanePressTick[MAX_PLAYERS][ms_LaneCount]{};
	int m_aLaneLastHitTick[MAX_PLAYERS][ms_LaneCount]{};
	int m_aLaneHoldTick[MAX_PLAYERS][ms_LaneCount]{};
	int m_aLaneHoldStartTick[MAX_PLAYERS][ms_LaneCount]{};
	int m_aLaneHoldEndTick[MAX_PLAYERS][ms_LaneCount]{};
	bool m_aLaneHoldActive[MAX_PLAYERS][ms_LaneCount]{};
	bool m_aLaneHoldEndEffectPlayed[MAX_PLAYERS][ms_LaneCount]{};
	uint16_t m_aLanePressId[MAX_PLAYERS][ms_LaneCount]{};
	uint16_t m_aLanePressUsedId[MAX_PLAYERS][ms_LaneCount]{};
	uint8_t m_aNoteLaneHitMask[MAX_PLAYERS]{};
	uint8_t m_aHoldSegmentLaneHitMask[MAX_PLAYERS]{};
	SRhythmScore m_aScores[MAX_PLAYERS]{};

	vec2 m_FieldAnchorPos;
	bool m_FieldAnchorValid;
};

#endif
