#include "roulette_arrow.h"
#include <game/server/gamecontext.h>
#include <base/math.h>

// Slot count on the wheel
static constexpr int ROULETTE_SLOTS = 37;

// wheel layout as read from the map (clockwise starting from '0' at bottom):
static const int g_aRouletteOrder[ROULETTE_SLOTS] =
{
	0, 32, 15, 19, 4, 21, 2, 25, 17, 27, 6, 34, 13, 36, 11, 30, 8, 23, 
	10, 5, 24, 16, 33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26
};

// slot "0" is at the bottom of the wheel (angle = +pi/2 in screen coords where +y is down).
// each slot occupies 2*pi / 37 radians. Rotation is clockwise.
static constexpr float SLOT_ANGLE = 2.0f * pi / (float)ROULETTE_SLOTS;
static constexpr float BASE_ANGLE = pi * 0.5f;

static float NormalizeAngleRad(float a)
{
	const float twoPi = 2.0f * pi;
	while(a < 0.0f)
		a += twoPi;
	while(a >= twoPi)
		a -= twoPi;
	return a;
}

static float AngleForSlotIndex(int slotIndex)
{
	return NormalizeAngleRad(BASE_ANGLE + slotIndex * SLOT_ANGLE);
}

static int SlotIndexFromAngle(float angle)
{
	const float ang = NormalizeAngleRad(angle - BASE_ANGLE);
	int slotIndex = ((int)floorf(ang / SLOT_ANGLE + 0.5f)) % ROULETTE_SLOTS;
	if (slotIndex < 0) 
		slotIndex += ROULETTE_SLOTS;
	return slotIndex;
}

float CEntityRouletteArrow::EaseInOutSine(float t)
{
	if (t <= 0.0f) 
		return 0.0f;
	if (t >= 1.0f) 
		return 1.0f;

	const float accelTime = 0.25f;
	const float accelDist = 0.25f;
	if (t < accelTime)
	{
		float x = t / accelTime;
		return accelDist * (x * x);
	}
	else
	{
		float x = (t - accelTime) / (1.0f - accelTime);
		float inv = 1.0f - x;
		float inv2 = inv * inv;
		float inv4 = inv2 * inv2;
		float eased = 1.0f - inv4;
		return accelDist + (1.0f - accelDist) * eased;
	}
}

CEntityRouletteArrow::CEntityRouletteArrow(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_VISUAL, Pos, 0, OwnerCID)
{
	m_Angle = AngleForSlotIndex(0);
	GameWorld()->InsertEntity(this);
}

void CEntityRouletteArrow::Spin(int DurationTicks, int FinalNumber, FSpinFinishedCallback pfnCallback)
{
	if (DurationTicks <= 0)
	{
		m_IsSpinning = false;
		m_FinalNumber = FinalNumber;
		if (pfnCallback)
			pfnCallback(FinalNumber);
		return;
	}

	// find slot index for the requested number
	int slotIndex = 0;
	for (int i = 0; i < ROULETTE_SLOTS; ++i)
	{
		if (g_aRouletteOrder[i] == FinalNumber)
		{
			slotIndex = i;
			break;
		}
	}

	m_TargetSlotIndex = slotIndex;
	m_DurationTicks = DurationTicks;
	m_RemainingTicks = DurationTicks;
	m_FinalNumber = FinalNumber;
	m_StartAngle = m_Angle;
	m_IsFading = false;
	m_FadeRemaining = 0;
	m_LastTickedSlot = SlotIndexFromAngle(m_StartAngle);

	const float targetAngle = AngleForSlotIndex(slotIndex);
	const int extraSpins = 3 + (rand() % 2);
	const float startNorm = NormalizeAngleRad(m_StartAngle);
	m_TotalRotation = extraSpins * 2.0f * pi + NormalizeAngleRad(targetAngle - startNorm);
	m_pfnOnSpinFinished = std::move(pfnCallback);
	m_IsSpinning = true;
}

void CEntityRouletteArrow::Tick()
{
	if (m_IsSpinning)
	{
		const int elapsed = m_DurationTicks - m_RemainingTicks;
		const float t = clamp(elapsed / (float)m_DurationTicks, 0.0f, 1.0f);
		m_Angle = m_StartAngle + m_TotalRotation * EaseInOutSine(t);

		// create sound effect when passing a slot
		const int currentSlot = SlotIndexFromAngle(m_Angle);
		if (currentSlot != m_LastTickedSlot)
		{
			m_LastTickedSlot = currentSlot;
			if (Server()->Tick() - m_LastSoundTick >= 2)
			{
				m_LastSoundTick = Server()->Tick();
				GS()->CreateSound(m_Pos, SOUND_PLAYER_JUMP);
			}
		}

		// check if the spin has finished
		if (--m_RemainingTicks <= 0)
		{
			const float targetAngle = AngleForSlotIndex(m_TargetSlotIndex);
			const int actualNumber = g_aRouletteOrder[m_TargetSlotIndex];
			m_IsSpinning = false;
			m_Angle = targetAngle;

			if (m_pfnOnSpinFinished)
			{
				auto cb = std::move(m_pfnOnSpinFinished);
				m_pfnOnSpinFinished = nullptr;
				cb(actualNumber);
			}

			m_IsFading = true;
			m_FadeTicks = Server()->TickSpeed();
			m_FadeRemaining = m_FadeTicks;

			GS()->CreateSound(m_Pos, SOUND_CTF_CAPTURE);
			GS()->CreateFinishEffect(m_Pos);
		}
	}
	else if (m_IsFading)
	{
		if (--m_FadeRemaining <= 0)
		{
			m_IsFading = false;
			m_FadeRemaining = 0;
		}
	}
}

void CEntityRouletteArrow::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	const float Len = 180.0f;
	const vec2 From = m_Pos;
	const vec2 To = m_Pos + vec2(cosf(m_Angle), sinf(m_Angle)) * Len;

	int startTick = Server()->Tick();
	if (m_IsFading && m_FadeTicks > 0)
		startTick -= (m_FadeTicks - m_FadeRemaining);

	GS()->SnapLaser(SnappingClient, GetID(), From, To, startTick, LASERTYPE_DOOR);
}