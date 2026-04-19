/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_RHYTHM_ENTITIES_RHYTHM_FIELD_H
#define GAME_SERVER_GAMEMODES_RHYTHM_ENTITIES_RHYTHM_FIELD_H

#include <game/server/entity.h>

class CRhythmArrow;

struct SRhythmFieldConfig
{
	static constexpr int s_LaneCount = 3;
	static constexpr float s_LaneWidth = 128.0f;
	static constexpr float s_FieldHeight = 364.0f;
	static constexpr float s_SpawnOffset = 256.0f;
	static constexpr float s_ArrowTravelDistance = s_FieldHeight + s_SpawnOffset;
	static constexpr float s_ReferenceBpm = 120.0f;
	static constexpr float s_MinFieldScale = 0.6f;
	static constexpr float s_MaxFieldScale = 1.4f;
	static constexpr float s_FieldViewOffsetX = -256.0f;
	static constexpr float s_FieldViewOffsetY = -200.0f;
	static constexpr float s_MissOffset = 64.0f;
	static constexpr float s_LeadBeats = 2.0f;
	static constexpr float s_DefaultFallSpeedPerBeat = s_ArrowTravelDistance / s_LeadBeats;
	static constexpr int s_AutoSpawnLaneIndex = 1;
	static constexpr int s_HitWindowTicks = 7;
	static constexpr int s_PerfectWindowTicks = 2;
	static constexpr int s_GoodWindowTicks = 4;
	static constexpr int s_BadWindowTicks = 6;
	static constexpr int s_InvalidPressTick = -1000000;
};

class CRhythmField : public CEntity
{
public:
	CRhythmField(CGameWorld *pGameWorld, vec2 Pos, float Bpm, float HitRadius);
	~CRhythmField() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;

	void SetBpm(float Bpm);
	void SetAutoSpawn(bool Auto);
	void SetHitZone(vec2 Pos);
	int BeatIntervalTicks() const { return m_BeatIntervalTicks; }
	void SpawnLaneArrow(int LaneIndex, int HitTick, int HoldDurationTicks = 0);
	void HideArrowForClient(int LaneIndex, int HitTick, int ClientId);
	bool IsHiddenArrowForClient(int LaneIndex, int HitTick, int ClientId) const;

	void RegisterArrow(CRhythmArrow *pArrow);
	void UnregisterArrow(CRhythmArrow *pArrow);

	const std::vector<CRhythmArrow *> &ActiveArrows() const { return m_vArrows; }
	float Bpm() const { return m_Bpm; }
	float BeatPeriod() const { return m_BeatPeriod; }
	vec2 HitZonePos() const { return m_HitZonePos; }
	float HitZoneRadius() const { return m_HitZoneRadius; }
	float ArrowTravelDistance() const { return m_ArrowTravelDistance + m_SpawnOffset; }

private:
	void EnsureSnapIds();
	void UpdateBeatTiming();
	void SpawnArrow(vec2 Origin, vec2 Direction, int HitTick, int LaneIndex, int HoldDurationTicks);
	void SpawnArrow();

	float m_Bpm;
	float m_BeatPeriod;
	int m_BeatIntervalTicks;
	int m_SpawnIntervalTicks;
	int m_NextSpawnTick;
	float m_FieldScale;
	float m_SpawnOffset;
	float m_ArrowTravelDistance;
	bool m_AutoSpawn;

	std::vector<CRhythmArrow *> m_vArrows;
	vec2 m_HitZonePos;
	float m_HitZoneRadius;
	int m_HitLineLaserId;
};

#endif
