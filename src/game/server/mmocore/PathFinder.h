#ifndef GAME_PATHFIND_H
#define GAME_PATHFIND_H

#include <game/layers.h>
#include "BinaryHeap.h"

#define MAX_WAY_CALC 50000

// player object
class CPathfinder
{
public:
	CPathfinder(class CLayers* Layers, class CCollision* Collision);
	~CPathfinder();

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

	int GetIndex(int XPos, int YPos) const;

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

	class CLayers *m_pLayers;
	class CCollision *m_pCollision;

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

#endif
