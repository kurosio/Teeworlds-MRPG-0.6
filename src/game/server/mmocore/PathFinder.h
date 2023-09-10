#ifndef GAME_PATHFIND_H
#define GAME_PATHFIND_H

#include <game/layers.h>
#include "BinaryHeap.h"

#include "PathFinderData.h"

#define MAX_WAY_CALC 50000

// player object
class CPathFinder
{
	class CPathFinderHandler* m_pHandler{};

public:
	CPathFinder(CLayers* Layers, class CCollision* Collision);
	~CPathFinder();

	struct CNode
	{
		vec2 m_Pos;
		int m_Parent;
		int m_ID;
		int m_G;
		int m_H;
		int m_F;
		bool m_IsCol;
		bool m_IsClosed;
		bool m_IsOpen;

		bool operator<(const CNode& Other) const { return (this->m_F < Other.m_F); }
		bool operator==(const CNode& Other) const { return (this->m_ID < Other.m_ID); }
	};

	void Init();
	void SetStart(vec2 Pos);
	void SetEnd(vec2 Pos);

	std::mutex m_mtxLimitedOnceUse;
	int GetIndex(int XPos, int YPos) const;
	CPathFinderHandler* SyncHandler() const { return m_pHandler; }

	vec2 GetRandomWaypoint();
	vec2 GetRandomWaypointRadius(vec2 Pos, float Radius);

	void FindPath();

	array<CNode> m_lMap;
	array<CNode> m_lFinalPath;

	int m_FinalSize;
private:
	enum
	{
		START = -3,
		END = -4,
	};

	CLayers *m_pLayers;
	CCollision *m_pCollision;

	array<CNode> m_lNodes;

	// binary heap for open nodes
	CBinaryHeap<CNode> m_Open;

	int m_StartIndex;
	int m_EndIndex;

	int m_LayerWidth;
	int m_LayerHeight;

	// fake array sizes
	int m_ClosedNodes;
};

class CPathFinderHandler
{
	inline static ThreadPool m_Pool { 4 };
	struct HandleArgsPack
	{
		CPathFinder* m_PathFinder {};
		vec2 m_StartFrom {};
		vec2 m_Search {};
		float m_Radius {};

		[[nodiscard]] bool IsValid() const { return m_PathFinder; }
	};

	static CPathFinderData CallbackFindPath(const ::std::shared_ptr<HandleArgsPack>& pHandleData);
	static CPathFinderData CallbackRandomRadiusWaypoint(const ::std::shared_ptr<HandleArgsPack>& pHandleData);

public:
	template<CPathFinderData::TYPE type>
	std::future<CPathFinderData> Prepare(CPathFinder* pPathFinder, vec2 StartPos, vec2 SearchPos, float Radius = 800.0f) const
	{
		auto Handle = std::make_shared<HandleArgsPack>(HandleArgsPack({ pPathFinder, StartPos, SearchPos, Radius }));

		if constexpr(type == CPathFinderData::TYPE::RANDOM)
			return m_Pool.enqueue(&CallbackRandomRadiusWaypoint, Handle);
		else
			return m_Pool.enqueue(&CallbackFindPath, Handle);
	}

	bool TryGetPreparedData(std::future<CPathFinderData>& pft, CPathFinderData* pData, vec2* pTarget = nullptr, vec2* pOldTarget = nullptr);
};

#endif
