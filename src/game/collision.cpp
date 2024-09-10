/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <engine/map.h>
#include <game/mapitems.h>
#include <game/layers.h>
#include <game/collision.h>

constexpr float CAM_RADIUS_H = 380.f;
constexpr float CAM_RADIUS_W = 480.f;
constexpr float TILE_SIZE = 32.0f;

CCollision::CCollision()
{
	m_pLayers = new CLayers();
}

CCollision::~CCollision()
{
	delete m_pLayers;
}

void CCollision::Init(IKernel* pKernel, int WorldID)
{
	m_pLayers->Init(pKernel, WorldID);

	const CMapItemLayerTilemap* pGameLayer = m_pLayers->GameLayer();
	m_Width = pGameLayer->m_Width;
	m_Height = pGameLayer->m_Height;

	// initailize base game tiles
	IMap* pMap = m_pLayers->Map();
	m_pTiles = static_cast<CTile*>(pMap->GetData(pGameLayer->m_Data));
	InitTiles(m_pTiles);

	// initialize front tiles
	if(const CMapItemLayerTilemap* pFrontLayer = m_pLayers->FrontLayer())
	{
		unsigned int FrontLayerSize = pMap->GetDataSize(pFrontLayer->m_Front);
		if(FrontLayerSize >= (m_Width * m_Height * sizeof(CTile)))
		{
			m_pFront = static_cast<CTile*>(pMap->GetData(pFrontLayer->m_Front));
			InitTiles(m_pFront);
		}
	}

	// initialize teleports
	if(const CMapItemLayerTilemap* pTeleLayer = m_pLayers->TeleLayer())
	{
		unsigned int TeleLayerSize = pMap->GetDataSize(pTeleLayer->m_Tele);
		if(TeleLayerSize >= (m_Width * m_Height * sizeof(CTeleTile)))
		{
			m_pTele = static_cast<CTeleTile*>(pMap->GetData(pTeleLayer->m_Tele));
			InitTeleports();
		}
	}

	// initialize switch tiles
	if(const CMapItemLayerTilemap* pSwitchLayer = m_pLayers->SwitchLayer())
	{
		unsigned int SwitchLayerSize = pMap->GetDataSize(pSwitchLayer->m_Switch);
		if(SwitchLayerSize >= (m_Width * m_Height * sizeof(CSwitchTileExtra)))
		{
			m_pExtra = static_cast<CSwitchTileExtra*>(pMap->GetData(pSwitchLayer->m_Switch));
			InitExtra();
		}
	}
}

void CCollision::InitTiles(CTile* pTiles)
{
	// initialze variables
	static std::unordered_map<int, int> TileFlags = 
	{
		{ TILE_DEATH, COLFLAG_DEATH },
		{ TILE_SOLID, COLFLAG_SOLID },
		{ TILE_NOHOOK, COLFLAG_SOLID | COLFLAG_NOHOOK },
		{ TILE_GUILD_HOUSE, COLFLAG_SAFE },
		{ TILE_AUCTION, COLFLAG_SAFE },
		{ TILE_PLAYER_HOUSE, COLFLAG_SAFE },
		{ TILE_SKILL_ZONE, COLFLAG_SAFE },
		{ TILE_SHOP_ZONE, COLFLAG_SAFE },
		{ TILE_QUEST_BOARD, COLFLAG_SAFE },
		{ TILE_CRAFT_ZONE, COLFLAG_SAFE },
		{ TILE_AETHER_TELEPORT, COLFLAG_SAFE },
		{ TILE_GUILD_CHAIR, COLFLAG_SAFE },
		{ TILE_INFO_BONUSES, COLFLAG_SAFE },
		{ TILE_INFO_WANTED, COLFLAG_SAFE },
		{ TILE_WORLD_SWAP, COLFLAG_SAFE },
		{ TILE_INVISIBLE_WALL, COLFLAG_DISALLOW_MOVE }
	};

	for(int i = 0; i < m_Width * m_Height; i++)
	{
		const int Index = pTiles[i].m_Index;
		if(Index > 128)
			continue;

		if(Index == TILE_FIXED_CAM)
		{
			vec2 camPos = { (i % m_Width) * TILE_SIZE + TILE_SIZE / 2.0f, (i / m_Width) * TILE_SIZE + TILE_SIZE / 2.0f };
			vec4 camRect = { camPos.x - CAM_RADIUS_W, camPos.y - CAM_RADIUS_H, camPos.x + CAM_RADIUS_W, camPos.y + CAM_RADIUS_H };
			m_vFixedCamZones.emplace_back(camPos, camRect);
			pTiles[i].m_ColFlags = 0;
			continue;
		}

		auto it = TileFlags.find(Index);
		pTiles[i].m_ColFlags = it != TileFlags.end() ? static_cast<char>(it->second) : 0;
	}
}

void CCollision::InitTeleports()
{
	// initialize tiles
	for(int i = 0; i < m_Width * m_Height; i++)
	{
		const int Number = m_pTele[i].m_Number;
		const int Type = m_pTele[i].m_Type;

		if(Number < 0)
			continue;

		// initialize teleports
		vec2 tilePos = { (i % m_Width) * TILE_SIZE + TILE_SIZE / 2.0f, (i / m_Width) * TILE_SIZE + TILE_SIZE / 2.0f };
		if(Type == TILE_TELE_IN)
		{
			m_pTiles[i].m_Index = static_cast<char>(Type);
			m_vTeleIns[Number].push_back(tilePos);
		}
		else if(Type == TILE_TELE_OUT)
		{
			m_pTiles[i].m_Index = static_cast<char>(Type);
			m_vTeleOuts[Number].push_back(tilePos);
		}
		else if(Type == TILE_TELE_CONFIRM_OUT)
		{
			m_pTiles[i].m_Index = static_cast<char>(Type);
			m_vConfirmTeleOuts[Number].push_back(tilePos);
		}
	}
}

void CCollision::InitExtra()
{
	const std::vector<std::string>& settings = m_pLayers->GetSettings();

	for(int i = 0; i < m_Width * m_Height; i++)
	{
		const int Number = m_pExtra[i].m_Number;
		const int Type = m_pExtra[i].m_Type;

		if(Type == TILE_ZONE)
		{
			if(auto name = mystd::loadSetting<std::string>("#zone_name", settings, { Number }))
				m_vZoneNames[Number] = name.value();
			if(auto pvp = mystd::loadSetting<int>("#zone_pvp", settings, { Number }); (!pvp.has_value() || pvp <= 0))
				m_pTiles[i].m_ColFlags |= COLFLAG_SAFE;
		}
		else if(Type == TILE_INTERACT_OBJECT)
		{
			if(auto result = mystd::loadSetting<std::string>("#switch", settings, { Number }))
			{
				const vec2 tilePos = { (i % m_Width) * TILE_SIZE + TILE_SIZE / 2.0f, (i / m_Width) * TILE_SIZE + TILE_SIZE / 2.0f };
				m_vInteractObjects[result.value()] = tilePos;
			}
		}
	}
}

void CCollision::InitEntities(const std::function<void(int, vec2, int)>& funcInit) const
{
	for(int y = 0; y < m_Height; ++y)
	{
		for(int x = 0; x < m_Width; ++x)
		{
			const int TileIndex = y * m_Width + x;
			const int Index = m_pTiles[TileIndex].m_Index;
			if(Index >= ENTITY_OFFSET)
			{
				const vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
				const int Flags = m_pTiles[TileIndex].m_Flags;
				funcInit(Index - ENTITY_OFFSET, Pos, Flags);
			}
		}
	}
}

vec2 CCollision::GetRotateDirByFlags(int Flags)
{
	if(Flags == ROTATION_90)
		return {1, 0};
	if(Flags == ROTATION_180)
		return {0, 1};
	if(Flags == ROTATION_270)
		return {-1, 0};
	return {0, -1};
}

int CCollision::GetMainTileIndex(float x, float y) const
{
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return m_pTiles[Ny * m_Width + Nx].m_Index;
}

int CCollision::GetFrontTileIndex(float x, float y) const
{
	if(!m_pFront)
		return TILE_AIR;

	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return m_pFront[Ny * m_Width + Nx].m_Index;
}

int CCollision::GetExtraTileIndex(float x, float y) const
{
	if(!m_pExtra)
		return TILE_AIR;

	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return m_pExtra[Ny * m_Width + Nx].m_Type;
}

const char* CCollision::GetZonename(vec2 Pos) const
{
	if(!m_pExtra)
		return nullptr;

	int Nx = clamp(round_to_int(Pos.x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(Pos.y) / 32, 0, m_Height - 1);
	int Number = m_pExtra[Ny * m_Width + Nx].m_Number;
	return m_vZoneNames.contains(Number) ? m_vZoneNames.at(Number).c_str() : nullptr;
}

std::optional<vec2> CCollision::TryGetFixedCamPos(vec2 currentPos) const
{
	for(const auto& [camPos, zoneRect] : m_vFixedCamZones)
	{
		if(currentPos.x > zoneRect.x && currentPos.x < zoneRect.z &&
			currentPos.y > zoneRect.y && currentPos.y < zoneRect.w)
			return camPos;
	}
	return std::nullopt;
}

bool CCollision::TryGetTeleportOut(vec2 Ins, vec2& Out, int ToIndex)
{
	if(!m_pTele)
		return false;

	const int Nx = clamp(round_to_int(Ins.x) / 32, 0, m_Width - 1);
	const int Ny = clamp(round_to_int(Ins.y) / 32, 0, m_Height - 1);
	const auto& Number = m_pTele[Ny * m_Width + Nx].m_Number;

	if(Number <= 0)
		return false;

	const auto& TeleOuts = (ToIndex == TILE_TELE_OUT) ? m_vTeleOuts[Number] : m_vConfirmTeleOuts[Number];
	if(TeleOuts.empty())
		return false;

	Out = TeleOuts[rand() % TeleOuts.size()];
	return true;
}

int CCollision::GetMainTileCollisionFlags(int x, int y) const
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);
	return m_pTiles[Ny*m_Width+Nx].m_Index > 128 ? 0 : m_pTiles[Ny*m_Width+Nx].m_ColFlags;
}

int CCollision::GetFrontTileCollisionFlags(int x, int y) const
{
	if(!m_pFront)
		return 0;

	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);
	return m_pFront[Ny*m_Width+Nx].m_Index > 128 ? 0 : m_pFront[Ny*m_Width+Nx].m_ColFlags;
}

int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int ColFlag) const
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
		Error = (CurTileX * Ratio - CurTileY - DetPos / (32 * (Pos1.x - Pos0.x))) * DeltaTileY;
		if(Tile0X < Tile1X)
			Error += Ratio * DeltaTileY;
		if(Tile0Y < Tile1Y)
			Error -= DeltaTileY;
	}

	while (CurTileX != Tile1X || CurTileY != Tile1Y)
	{
		if (CheckPoint(CurTileX * 32, CurTileY * 32, ColFlag))
			break;

		if (CurTileY != Tile1Y && (CurTileX == Tile1X || Error > 0))
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

	if(CheckPoint(CurTileX * 32, CurTileY * 32, ColFlag))
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
		return GetCollisionFlagsAt(CurTileX * 32, CurTileY * 32);
	}

	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;

	return 0;
}

bool CCollision::IntersectLineColFlag(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision, int ColFlag) const
{
	return IntersectLine(Pos0, Pos1, pOutCollision, pOutBeforeCollision) & ColFlag;
}

void CCollision::FillLengthWall(int DepthTiles, vec2 Direction, vec2* pPos, vec2* pPosTo, bool OffsetStartlineOneTile) const
{
	if(pPos && pPosTo)
	{
		vec2 DirPos = Direction * ((float)DepthTiles * 32.f);
		if(!IntersectLine(*pPos, *pPos + DirPos, nullptr, pPosTo))
		{
			*pPosTo = (*pPos + DirPos);
		}

		if(OffsetStartlineOneTile)
		{
			*pPos -= Direction * 30;
		}
	}
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

vec2 CCollision::GetDoorNormal(vec2 doorStart, vec2 doorEnd, vec2 from)
{
	const vec2 doorVector = doorEnd - doorStart;
	const vec2 normalizedDoorVector = normalize(doorVector);

	auto doorNormal = vec2(-normalizedDoorVector.y, normalizedDoorVector.x);
	const vec2 toObjectVector = from - doorStart;

	if(dot(doorNormal, toObjectVector) >= 0)
		doorNormal = -doorNormal;

	return doorNormal;
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
	if(CheckPoint(Pos.x - CheckSizeX, Pos.y + CheckSizeY + 5) || CheckPoint(Pos.x + CheckSizeX, Pos.y + CheckSizeY + 5))
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
	if(CheckPoint(Pos.x - CheckSizeX, Pos.y + CheckSizeY + 5) || CheckPoint(Pos.x + CheckSizeX, Pos.y + CheckSizeY + 5))
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

int CCollision::GetMainTileFlags(float x, float y) const
{
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return m_pTiles[Ny * m_Width + Nx].m_Index > 128 ? 0 : m_pTiles[Ny * m_Width + Nx].m_Flags;
}

int CCollision::GetFrontTileFlags(float x, float y) const
{
	if(!m_pFront)
		return 0;

	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return m_pFront[Ny * m_Width + Nx].m_Index > 128 ? 0 : m_pFront[Ny * m_Width + Nx].m_Flags;
}

int CCollision::GetExtraTileFlags(float x, float y) const
{
	if(!m_pExtra)
		return 0;

	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return m_pExtra[Ny * m_Width + Nx].m_Type > 128 ? 0 : m_pExtra[Ny * m_Width + Nx].m_Flags;
}