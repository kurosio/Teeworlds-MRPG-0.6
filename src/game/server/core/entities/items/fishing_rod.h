#ifndef GAME_SERVER_CORE_ENTITIES_TOOLS_FISHING_ROD_H
#define GAME_SERVER_CORE_ENTITIES_TOOLS_FISHING_ROD_H

#include "../physics/rope.h"
#include <game/server/entity.h>

class CPlayer;
class CProfession;
struct GatheringNode;
class CEntityFishingRod : public CEntity
{
	struct FishingNow
	{
		enum State
		{
			WAITING,
			HOOKING,
			PULLING,
			SUCCESS,
			AWAY,
		};

		std::optional<vec2> m_FromPoint {};
		float m_InterpolatedX{};
		int m_State {};
		int m_Health {};
		int m_HookingTime {};
	};

	enum
	{
		ROD = 0,
		NUM_ROD_POINTS = 3,

		ROPE,
		NUM_ROPE_POINTS = 10,
	};

	vec2 m_EndRodPoint {};
	RopePhysic m_Rope {};
	FishingNow m_Fishing{};

public:
	CEntityFishingRod(CGameWorld* pGameWorld, int ClientID, vec2 Position, vec2 Force);

	void FishingTick(CPlayer* pPlayer, CProfession* pFisherman, GatheringNode* pNode, std::optional<int> EquippedItemID);
	bool IsWaitingState() const { return m_Fishing.m_State == FishingNow::WAITING; }

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif