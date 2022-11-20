/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include <base/vmath.h>

enum
{
	CANTMOVE_LEFT = 1 << 0,
	CANTMOVE_RIGHT = 1 << 1,
	CANTMOVE_UP = 1 << 2,
	CANTMOVE_DOWN = 1 << 3,
};

vec2 ClampVel(int MoveRestriction, vec2 Vel);

class CCollision
{
	class CTile *m_pTiles;
	unsigned short GetParseTile(int x, int y) const;
	/*end another*/

	int m_Width;
	int m_Height;

	class CLayers *m_pLayers;
	bool IsTile(int x, int y, int Flag=COLFLAG_SOLID) const;
	int GetTile(int x, int y) const;

public:
	enum
	{
		COLFLAG_SOLID=1,
		COLFLAG_DEATH=2,
		COLFLAG_NOHOOK=4,
		COLFLAG_SAFE_AREA = 1 << 3,
		COLFLAG_DISALLOW_MOVE = 1 << 4,
	};

	CCollision();
	void Init(class CLayers *pLayers);
	bool CheckPoint(float x, float y, int Flag=COLFLAG_SOLID) const { return IsTile(round_to_int(x), round_to_int(y), Flag); }
	bool CheckPoint(vec2 Pos, int Flag=COLFLAG_SOLID) const { return CheckPoint(Pos.x, Pos.y, Flag); }
	int GetCollisionAt(float x, float y) const { return GetTile(round_to_int(x), round_to_int(y)); }

	int GetTileIndex(int Index) const;
	int GetTileFlags(int Index) const;
	int GetTile(vec2 Pos) const { return GetTile(round_to_int(Pos.x), round_to_int(Pos.y)); }

	int GetMoveRestrictions(void* pUser, vec2 Pos, float Distance = 18.0f, int OverrideCenterTileIndex = -1);
	int GetMoveRestrictions(vec2 Pos, float Distance = 18.0f)
	{
		return GetMoveRestrictions(nullptr, Pos, Distance);
	}

	int GetParseTilesAt(float x, float y) const { return GetParseTile(round_to_int(x), round_to_int(y)); }
	vec2 FindDirCollision(int CheckNum, vec2 SourceVec, char Cord, char SumSymbol) const;

	int GetWidth() const { return m_Width; };
	int GetHeight() const { return m_Height; };
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision) const;
	bool IntersectLineWithInvisible(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision) const
	{
		return IntersectLineColFlag(Pos0, Pos1, pOutCollision, pOutBeforeCollision, COLFLAG_DISALLOW_MOVE | COLFLAG_SOLID);
	};
	bool IntersectLineColFlag(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision, int ColFlag) const;
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces) const;
	void MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity, bool *pDeath=NULL) const;
	bool TestBox(vec2 Pos, vec2 Size, int Flag=COLFLAG_SOLID) const;

	void MovePhysicalAngleBox(vec2* pPos, vec2* pVel, vec2 Size, float* pAngle, float* pAngleForce, float Elasticity, float Gravity = 0.5f) const;
	void MovePhysicalBox(vec2* pPos, vec2* pVel, vec2 Size, float Elasticity, float Gravity = 0.5f) const;
};

#endif
