/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_RHYTHM_ENTITIES_RHYTHM_ARROW_H
#define GAME_SERVER_GAMEMODES_RHYTHM_ENTITIES_RHYTHM_ARROW_H

#include <game/server/entity.h>
#include <engine/shared/protocol.h>

class CRhythmField;

class CRhythmArrow : public CEntity
{
public:
	CRhythmArrow(CGameWorld *pGameWorld, CRhythmField *pField, vec2 Origin, vec2 Direction, float SpeedPerTick, int HitTick, int LaneIndex, float MissY, float VelScale, float TailLength);
	~CRhythmArrow() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;

	void DetachField();
	void HideForClient(int ClientId);
	bool IsHiddenForClient(int ClientId) const;

	vec2 Direction() const { return m_Direction; }
	float Phase() const { return m_Phase; }
	float Speed() const { return m_Speed; }
	int HitTick() const { return m_HitTick; }
	int LaneIndex() const { return m_LaneIndex; }
	float MissY() const { return m_MissY; }

private:
	CRhythmField *m_pField;
	vec2 m_Origin;
	vec2 m_Direction;
	float m_Phase;
	float m_Speed;
	int m_SpawnTick;
	int m_HitTick;
	int m_LaneIndex;
	float m_MissY;
	float m_VelScale;
	float m_TailLength;
	int m_TailLaserId;
	CClientMask m_HiddenMask;
};

#endif
