/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <engine/map.h>
#include <game/mapitems.h>
#include <game/layers.h>
#include <game/collision.h>

vec2 ClampVel(int MoveRestriction, vec2 Vel)
{
	if(Vel.x > 0 && (MoveRestriction & CANTMOVE_RIGHT))
	{
		Vel.x = 0;
	}
	if(Vel.x < 0 && (MoveRestriction & CANTMOVE_LEFT))
	{
		Vel.x = 0;
	}
	if(Vel.y > 0 && (MoveRestriction & CANTMOVE_DOWN))
	{
		Vel.y = 0;
	}
	if(Vel.y < 0 && (MoveRestriction & CANTMOVE_UP))
	{
		Vel.y = 0;
	}
	return Vel;
}

CCollision::CCollision()
{
	m_pTiles = nullptr;
	m_pLayers = nullptr;
	m_Width = 0;
	m_Height = 0;
}

void CCollision::Init(class CLayers *pLayers)
{
	m_pLayers = pLayers;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));

	for(int i = 0; i < m_Width*m_Height; i++)
	{
		int Index = m_pTiles[i].m_Index;
		if(Index > 128)
			continue;

		switch(Index)
		{
		case TILE_DEATH:
			m_pTiles[i].m_Index = COLFLAG_DEATH;
			break;
		case TILE_SOLID:
			m_pTiles[i].m_Index = COLFLAG_SOLID;
			break;
		case TILE_NOHOOK:
			m_pTiles[i].m_Index = COLFLAG_SOLID|COLFLAG_NOHOOK;
			break;
		case TILE_GUILD_HOUSE:
		case TILE_AUCTION:
		case TILE_PLAYER_HOUSE:
		case TILE_LEARN_SKILL:
		case TILE_SHOP_ZONE:
		case TILE_CRAFT_ZONE:
		case TILE_AETHER_TELEPORT:
		case TILE_GUILD_CHAIRS:
		case TILE_WORLD_SWAP:
			m_pTiles[i].m_Index = COLFLAG_SAFE_AREA;
			m_pTiles[i].m_Reserved = static_cast<char>(Index);
			break;
		case TILE_INVISIBLE_WALL:
			m_pTiles[i].m_Index = COLFLAG_DISALLOW_MOVE;
			m_pTiles[i].m_Reserved = static_cast<char>(Index);
			break;
		default:
			m_pTiles[i].m_Index = 0;
			m_pTiles[i].m_Reserved = static_cast< char >(Index);
		}
	}
}

int CCollision::GetTile(int x, int y) const
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);

	return m_pTiles[Ny*m_Width+Nx].m_Index > 128 ? 0 : m_pTiles[Ny*m_Width+Nx].m_Index;
}

/* another */
int CCollision::GetTileIndex(int Index) const
{
	if (Index < 0)
		return 0;
	return m_pTiles[Index].m_Index;
}

int CCollision::GetTileFlags(int Index) const
{
	if (Index < 0)
		return 0;
	return m_pTiles[Index].m_Flags;
}

unsigned short CCollision::GetParseTile(int x, int y) const
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);

	return static_cast<int>(m_pTiles[Ny * m_Width + Nx].m_Reserved);
}

int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	const int Tile0X = round_to_int(Pos0.x)/32;
	const int Tile0Y = round_to_int(Pos0.y)/32;
	const int Tile1X = round_to_int(Pos1.x)/32;
	const int Tile1Y = round_to_int(Pos1.y)/32;

	const float Ratio = (Tile0X == Tile1X) ? 1.f : (Pos1.y - Pos0.y) / (Pos1.x-Pos0.x);

	const float DetPos = Pos0.x * Pos1.y - Pos0.y * Pos1.x;

	const int DeltaTileX = (Tile0X <= Tile1X) ? 1 : -1;
	const int DeltaTileY = (Tile0Y <= Tile1Y) ? 1 : -1;

	const float DeltaError = DeltaTileY * DeltaTileX * Ratio;

	int CurTileX = Tile0X;
	int CurTileY = Tile0Y;
	vec2 Pos = Pos0;

	bool Vertical = false;

	float Error = 0;
	if(Tile0Y != Tile1Y && Tile0X != Tile1X)
	{
		Error = (CurTileX * Ratio - CurTileY - DetPos / (32*(Pos1.x-Pos0.x))) * DeltaTileY;
		if(Tile0X < Tile1X)
			Error += Ratio * DeltaTileY;
		if(Tile0Y < Tile1Y)
			Error -= DeltaTileY;
	}

	while(CurTileX != Tile1X || CurTileY != Tile1Y)
	{
		if(IsTile(CurTileX*32,CurTileY*32, COLFLAG_SOLID))
			break;
		if(CurTileY != Tile1Y && (CurTileX == Tile1X || Error > 0))
		{
			CurTileY += DeltaTileY;
			Error -= 1;
			Vertical = false;
		}
		else
		{
			CurTileX += DeltaTileX;
			Error += DeltaError;
			Vertical = true;
		}
	}
	if(IsTile(CurTileX*32,CurTileY*32, COLFLAG_SOLID))
	{
		if(CurTileX != Tile0X || CurTileY != Tile0Y)
		{
			if(Vertical)
			{
				Pos.x = 32 * (CurTileX + ((Tile0X < Tile1X) ? 0 : 1));
				Pos.y = (Pos.x * (Pos1.y - Pos0.y) - DetPos) / (Pos1.x - Pos0.x);
			}
			else
			{
				Pos.y = 32 * (CurTileY + ((Tile0Y < Tile1Y) ? 0 : 1));
				Pos.x = (Pos.y * (Pos1.x - Pos0.x) + DetPos) / (Pos1.y - Pos0.y);
			}
		}
		if(pOutCollision)
			*pOutCollision = Pos;
		if(pOutBeforeCollision)
		{
			vec2 Dir = normalize(Pos1-Pos0);
			if(Vertical)
				Dir *= 0.5f / absolute(Dir.x) + 1.f;
			else
				Dir *= 0.5f / absolute(Dir.y) + 1.f;
			*pOutBeforeCollision = Pos - Dir;
		}
		return  GetTile(CurTileX*32,CurTileY*32);
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

bool CCollision::IntersectLineColFlag(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision, int ColFlag) const
{
	const int Tile0X = round_to_int(Pos0.x) / 32;
	const int Tile0Y = round_to_int(Pos0.y) / 32;
	const int Tile1X = round_to_int(Pos1.x) / 32;
	const int Tile1Y = round_to_int(Pos1.y) / 32;

	const float Ratio = (Tile0X == Tile1X) ? 1.f : (Pos1.y - Pos0.y) / (Pos1.x - Pos0.x);
	const float DetPos = Pos0.x * Pos1.y - Pos0.y * Pos1.x;

	const int DeltaTileX = (Tile0X <= Tile1X) ? 1 : -1;
	const int DeltaTileY = (Tile0Y <= Tile1Y) ? 1 : -1;

	const float DeltaError = DeltaTileY * DeltaTileX * Ratio;

	int CurTileX = Tile0X;
	int CurTileY = Tile0Y;
	vec2 Pos = Pos0;

	bool Vertical = false;

	float Error = 0;
	if(Tile0Y != Tile1Y && Tile0X != Tile1X)
	{
		Error = (CurTileX * Ratio - CurTileY - DetPos / (32 * (Pos1.x - Pos0.x))) * DeltaTileY;
		if(Tile0X < Tile1X)
			Error += Ratio * DeltaTileY;
		if(Tile0Y < Tile1Y)
			Error -= DeltaTileY;
	}

	while(CurTileX != Tile1X || CurTileY != Tile1Y)
	{
		if(IsTile(CurTileX * 32, CurTileY * 32, ColFlag))
			break;
		if(CurTileY != Tile1Y && (CurTileX == Tile1X || Error > 0))
		{
			CurTileY += DeltaTileY;
			Error -= 1;
			Vertical = false;
		}
		else
		{
			CurTileX += DeltaTileX;
			Error += DeltaError;
			Vertical = true;
		}
	}
	if(IsTile(CurTileX * 32, CurTileY * 32, ColFlag))
	{
		if(CurTileX != Tile0X || CurTileY != Tile0Y)
		{
			if(Vertical)
			{
				Pos.x = 32 * (CurTileX + ((Tile0X < Tile1X) ? 0 : 1));
				Pos.y = (Pos.x * (Pos1.y - Pos0.y) - DetPos) / (Pos1.x - Pos0.x);
			}
			else
			{
				Pos.y = 32 * (CurTileY + ((Tile0Y < Tile1Y) ? 0 : 1));
				Pos.x = (Pos.y * (Pos1.x - Pos0.x) + DetPos) / (Pos1.y - Pos0.y);
			}
		}
		if(pOutCollision)
			*pOutCollision = Pos;
		if(pOutBeforeCollision)
		{
			vec2 Dir = normalize(Pos1 - Pos0);
			if(Vertical)
				Dir *= 0.5f / absolute(Dir.x) + 1.f;
			else
				Dir *= 0.5f / absolute(Dir.y) + 1.f;
			*pOutBeforeCollision = Pos - Dir;
		}
		return true;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return false;
}

// Cord 'X','x' or 'Y','y' | SumSymbol '+' or '-'
vec2 CCollision::FindDirCollision(int CheckNum, vec2 SourceVec, char Cord, char SumSymbol) const
{
	const bool IsCordinateX= (bool)(Cord == 'x' || Cord == 'X');
	const bool IsCordinateY= (bool)(Cord == 'y' || Cord == 'Y');
	if((SumSymbol == '-' || SumSymbol == '+') && (IsCordinateX || IsCordinateY))
	{
		for(int i = 0; i < CheckNum; i++)
		{
			if(SumSymbol == '-')
			{
				if(IsCordinateX)
					SourceVec.x -= i;
				else if(IsCordinateY)
					SourceVec.y -= i;
			}
			else if(SumSymbol == '+')
			{
				if(IsCordinateX)
					SourceVec.x += i;
				else if(IsCordinateY)
					SourceVec.y += i;
			}
			int ColFlag = GetCollisionAt(SourceVec.x, SourceVec.y);
			if(ColFlag & COLFLAG_SOLID || ColFlag & COLFLAG_NOHOOK || ColFlag & COLFLAG_DEATH)
				break;
		}
	}
	return SourceVec;
}
/* end another */

bool CCollision::IsTile(int x, int y, int Flag) const
{
	return GetTile(x, y)&Flag;
}

// TODO: OPT: rewrite this smarter!
void CCollision::MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces) const
{
	if(pBounces)
		*pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if(CheckPoint(Pos + Vel))
	{
		int Affected = 0;
		if(CheckPoint(Pos.x + Vel.x, Pos.y))
		{
			pInoutVel->x *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(CheckPoint(Pos.x, Pos.y + Vel.y))
		{
			pInoutVel->y *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(Affected == 0)
		{
			pInoutVel->x *= -Elasticity;
			pInoutVel->y *= -Elasticity;
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
}

bool CCollision::TestBox(vec2 Pos, vec2 Size, int Flag) const
{
	Size *= 0.5f;
	if(CheckPoint(Pos.x-Size.x, Pos.y-Size.y, Flag))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y-Size.y, Flag))
		return true;
	if(CheckPoint(Pos.x-Size.x, Pos.y+Size.y, Flag))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y+Size.y, Flag))
		return true;
	return false;
}

void CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity, bool *pDeath) const
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;

	float Distance = length(Vel);
	int Max = (int)Distance;

	if(pDeath)
		*pDeath = false;

	if(Distance > 0.00001f)
	{
		float Fraction = 1.0f/(float)(Max+1);
		for(int i = 0; i <= Max; i++)
		{
			vec2 NewPos = Pos + Vel*Fraction; // TODO: this row is not nice
			if(pDeath && TestBox(vec2(NewPos.x, NewPos.y), Size*(2.0f/3.0f), COLFLAG_DEATH))
			{
				*pDeath = true;
			}

			if(TestBox(vec2(NewPos.x, NewPos.y), Size))
			{
				int Hits = 0;

				if(TestBox(vec2(Pos.x, NewPos.y), Size))
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					Hits++;
				}

				if(TestBox(vec2(NewPos.x, Pos.y), Size))
				{
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
					Hits++;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if(Hits == 0)
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
				}
			}

			Pos = NewPos;
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}

void CCollision::MovePhysicalAngleBox(vec2* pPos, vec2* pVel, vec2 Size, float* pAngle, float* pAngleForce, float Elasticity, float Gravity) const
{
	// physic
	vec2 Pos = *pPos;
	vec2 Vel = *pVel;
	float Angle = *pAngle;
	float AngleForce = *pAngleForce;

	Vel.y += Gravity;
	const float CheckSizeX = (Size.x / 2.0f);
	const float CheckSizeY = (Size.y / 2.0f);
	const bool IsCollide = (bool)CheckPoint(Pos.x - CheckSizeX, Pos.y + CheckSizeY + 5) || CheckPoint(Pos.x + CheckSizeX, Pos.y + CheckSizeY + 5);
	if(IsCollide)
	{
		AngleForce += (Vel.x - 0.74f * 6.0f - AngleForce) / 2.0f;
		Vel.x *= 0.8f;
	}
	else
	{
		Angle += clamp(AngleForce * 0.04f, -0.6f, 0.6f);
		Vel.x *= 0.99f;
	}

	// move box
	MoveBox(&Pos, &Vel, Size, Elasticity);

	// transfer the changes
	*pPos = Pos;
	*pVel = Vel;
	*pAngle = Angle;
	*pAngleForce = AngleForce;
}

void CCollision::MovePhysicalBox(vec2* pPos, vec2* pVel, vec2 Size, float Elasticity, float Gravity) const
{
	// physic
	vec2 Pos = *pPos;
	vec2 Vel = *pVel;

	Vel.y += Gravity;
	const float CheckSizeX = (Size.x / 2.0f);
	const float CheckSizeY = (Size.y / 2.0f);
	const bool IsCollide = (bool)CheckPoint(Pos.x - CheckSizeX, Pos.y + CheckSizeY + 5) || CheckPoint(Pos.x + CheckSizeX, Pos.y + CheckSizeY + 5);
	if(IsCollide)
	{
		Vel.x *= 0.8f;
	}
	else
	{
		Vel.x *= 0.99f;
	}

	// move box
	MoveBox(&Pos, &Vel, Size, Elasticity);

	// transfer the changes
	*pPos = Pos;
	*pVel = Vel;
}

enum
{
	MR_DIR_HERE = 0,
	MR_DIR_RIGHT,
	MR_DIR_DOWN,
	MR_DIR_LEFT,
	MR_DIR_UP,
	NUM_MR_DIRS
};

static int GetMoveRestrictionsRaw(int Direction, int Tile, int Flags)
{
	Flags = Flags & (TILEFLAG_VFLIP | TILEFLAG_HFLIP | TILEFLAG_ROTATE);
	/*switch (Tile)
	{
	case TILE_STOP:
		switch (Flags)
		{
		case ROTATION_0: return CANTMOVE_DOWN;
		case ROTATION_90: return CANTMOVE_LEFT;
		case ROTATION_180: return CANTMOVE_UP;
		case ROTATION_270: return CANTMOVE_RIGHT;

		case TILEFLAG_HFLIP^ ROTATION_0: return CANTMOVE_UP;
		case TILEFLAG_HFLIP^ ROTATION_90: return CANTMOVE_RIGHT;
		case TILEFLAG_HFLIP^ ROTATION_180: return CANTMOVE_DOWN;
		case TILEFLAG_HFLIP^ ROTATION_270: return CANTMOVE_LEFT;
		}
		break;
	case TILE_STOPS:
		switch (Flags)
		{
		case ROTATION_0:
		case ROTATION_180:
		case TILEFLAG_HFLIP^ ROTATION_0:
		case TILEFLAG_HFLIP^ ROTATION_180:
			return CANTMOVE_DOWN | CANTMOVE_UP;
		case ROTATION_90:
		case ROTATION_270:
		case TILEFLAG_HFLIP^ ROTATION_90:
		case TILEFLAG_HFLIP^ ROTATION_270:
			return CANTMOVE_LEFT | CANTMOVE_RIGHT;
		}
		break;
	case TILE_STOPA:
		return CANTMOVE_LEFT | CANTMOVE_RIGHT | CANTMOVE_UP | CANTMOVE_DOWN;
	}*/
	return 0;
}

static int GetMoveRestrictionsMask(int Direction)
{
	switch (Direction)
	{
	case MR_DIR_HERE: return 0;
	case MR_DIR_RIGHT: return CANTMOVE_RIGHT;
	case MR_DIR_DOWN: return CANTMOVE_DOWN;
	case MR_DIR_LEFT: return CANTMOVE_LEFT;
	case MR_DIR_UP: return CANTMOVE_UP;
	default: dbg_assert(false, "invalid dir");
	}
	return 0;
}

static int GetMoveRestrictions(int Direction, int Tile, int Flags)
{
	int Result = GetMoveRestrictionsRaw(Direction, Tile, Flags);
	// Generally, stoppers only have an effect if they block us from moving
	// *onto* them. The one exception is one-way blockers, they can also
	// block us from moving if we're on top of them.
	/*if (Direction == MR_DIR_HERE && Tile == TILE_STOP)
	{
		return Result;
	}*/
	return Result & GetMoveRestrictionsMask(Direction);
}

int CCollision::GetMoveRestrictions(void* pUser, vec2 Pos, float Distance, int OverrideCenterTileIndex)
{
	static const vec2 DIRECTIONS[NUM_MR_DIRS] =
	{
		vec2(0, 0),
		vec2(1, 0),
		vec2(0, 1),
		vec2(-1, 0),
		vec2(0, -1) };
	dbg_assert(0.0f <= Distance && Distance <= 32.0f, "invalid distance");

	int Restrictions = 0;
	for (int d = 0; d < NUM_MR_DIRS; d++)
	{
		vec2 ModPos = Pos + DIRECTIONS[d] * Distance;
		int ModMapIndex = GetTile(ModPos);
		if (d == MR_DIR_HERE && OverrideCenterTileIndex >= 0)
		{
			ModMapIndex = OverrideCenterTileIndex;
		}
		for (int Front = 0; Front < 2; Front++)
		{
			int Tile = GetTileIndex(ModMapIndex);
			int Flags = GetTileFlags(ModMapIndex);
			Restrictions |= ::GetMoveRestrictions(d, Tile, Flags);
		}
	}
	return Restrictions;
}
