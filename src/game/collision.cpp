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
	InitSettings();
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
			m_pSwitchExtra = static_cast<CSwitchTileExtra*>(pMap->GetData(pSwitchLayer->m_Switch));
			InitSwitchExtra();
		}
	}

	// initialize speedup tiles
	if(const CMapItemLayerTilemap* pSwitchLayer = m_pLayers->SpeedupLayer())
	{
		unsigned int SpeedupLayerSize = pMap->GetDataSize(pSwitchLayer->m_Speedup);
		if(SpeedupLayerSize >= (m_Width * m_Height * sizeof(CSpeedupTileExtra)))
		{
			m_pSpeedupExtra = static_cast<CSpeedupTileExtra*>(pMap->GetData(pSwitchLayer->m_Speedup));
			InitSpeedupExtra();
		}
	}
}

void initGatheringNode(const std::string& nodeType, const std::vector<std::string>& vSettings, int Number, std::unordered_map<int, GatheringNode>& vNodesContainer)
{
	std::string ItemsData {};
	GatheringNode detail {};
	const auto Identity = nodeType + " " + std::to_string(Number);

	// load settings for the node
	if(mystd::loadSettings<std::string, int, int, std::string>(Identity, vSettings, &detail.Name, &detail.Level, &detail.Health, &ItemsData))
	{
		dbg_msg("map-init", "%s %d started initialization: Name='%s', Level='%d', Health='%d'", nodeType.c_str(), Number, detail.Name.c_str(), detail.Level, detail.Health);

		// parse and add items
		DBSet ItemsSet(ItemsData);
		for(const auto& [Elem, Size] : ItemsSet.GetDataItems())
		{
			int ItemID;
			float Chance;
			if(sscanf(Elem.c_str(), "[%d/%f]", &ItemID, &Chance) == 2)
			{
				detail.m_vItems.addElement(ItemID, Chance);
				dbg_msg("map-init", "Added item: ItemID='%d', Chance='%f'", ItemID, Chance);
			}
		}

		// store the initialized node
		detail.m_vItems.normalizeChances();
		vNodesContainer[Number] = detail;
		dbg_msg("map-init", "%s %d initialized: Items='%d'", nodeType.c_str(), Number, (int)detail.m_vItems.size());
	}
}

void CCollision::InitSettings()
{
	const auto& settings = m_pLayers->GetSettings();

	dbg_msg("map-init", "------------------------");
	for(int i = 1; i < 255; i++)
	{
		// initialize zones details
		{
			ZoneDetail detail;
			const auto Identity = std::string("#zone ").append(std::to_string(i));
			if(mystd::loadSettings<std::string, bool>(Identity, settings, &detail.Name, &detail.PVP))
			{
				dbg_msg("map-init", "Switch zone %d initialized: Name='%s', PVP=%s", i, detail.Name.c_str(), detail.PVP ? "true" : "false");
				m_vZoneDetail[i] = detail;
			}
		}

		// initialize text zone details
		{
			TextZoneDetail detail;
			const auto Identity = std::string("#text ").append(std::to_string(i));
			if(mystd::loadSettings<std::string>(Identity, settings, &detail.Text))
			{
				dbg_msg("map-init", "Switch text %d initialized: String='%s'", i, detail.Text.c_str());
				m_vZoneTextDetail[i] = detail;
			}
		}

		// initialize gathering nodes
		{
			initGatheringNode("#node_ore", settings, i, m_vOreNodes);
			initGatheringNode("#node_plant", settings, i, m_vPlantNodes);
			initGatheringNode("#node_fish", settings, i, m_vFishNodes);
		}
	}
	dbg_msg("map-init", "------------------------");
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
		{ TILE_WORLD_SWAPPER, COLFLAG_SAFE },
		{ TILE_DESTROYER_PROJECTILE, COLFLAG_DISALLOW_MOVE },
		{ TILE_WATER, COLFLAG_WATER },
	};

	for(int i = 0; i < m_Width * m_Height; i++)
	{
		const int Index = pTiles[i].m_Index;
		if(Index > 128)
			continue;

		if(Index == TILE_FIXED_CAM || Index == TILE_SMOOTH_FIXED_CAM)
		{
			FixedCamZoneDetail data;
			data.Pos = { (i % m_Width) * TILE_SIZE + TILE_SIZE / 2.0f, (i / m_Width) * TILE_SIZE + TILE_SIZE / 2.0f };
			data.Rect = { data.Pos.x - CAM_RADIUS_W, data.Pos.y - CAM_RADIUS_H, data.Pos.x + CAM_RADIUS_W, data.Pos.y + CAM_RADIUS_H };
			data.Smooth = (Index == TILE_SMOOTH_FIXED_CAM);
			m_vFixedCamZones.emplace_back(data);
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
		if(Type == TILE_TELE_FROM)
		{
			m_pTiles[i].m_Index = static_cast<char>(Type);
		}
		else if(Type == TILE_TELE_FROM_CONFIRM)
		{
			m_pTiles[i].m_Index = static_cast<char>(Type);
		}
		else if(Type == TILE_TELE_OUT)
		{
			m_pTiles[i].m_Index = static_cast<char>(Type);
			m_vTeleOuts[Number].push_back(tilePos);
		}
	}
}

void CCollision::InitSwitchExtra()
{
	// const std::vector<std::string>& settingsLines = m_pLayers->GetSettings();

	for(int i = 0; i < m_Width * m_Height; i++)
	{
		const int Number = m_pSwitchExtra[i].m_Number;
		const int Type = m_pSwitchExtra[i].m_Type;
		const vec2 tilePos = { (i % m_Width) * TILE_SIZE + TILE_SIZE / 2.0f, (i / m_Width) * TILE_SIZE + TILE_SIZE / 2.0f };

		if(Type == TILE_SW_ZONE && m_vZoneDetail.contains(Number))
		{
			auto& zone = m_vZoneDetail[Number];
			if(!zone.PVP)
			{
				m_pTiles[i].m_ColFlags |= COLFLAG_SAFE;
			}
		}
		else if(Type == TILE_SW_TEXT && m_vZoneTextDetail.contains(Number))
		{
			auto& zone = m_vZoneTextDetail[Number];
			zone.vPositions.emplace_back(tilePos);
		}
	}
}

void CCollision::InitSpeedupExtra() {}

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

void CCollision::InitSwitchEntities(const std::function<void(int, vec2, int, int)>& funcInit) const
{
	if(!m_pSwitchExtra)
		return;

	for(int y = 0; y < m_Height; ++y)
	{
		for(int x = 0; x < m_Width; ++x)
		{
			const int TileIndex = y * m_Width + x;
			const int Type = m_pSwitchExtra[TileIndex].m_Type;
			const int Number = m_pSwitchExtra[TileIndex].m_Number;
			if(Type >= ENTITY_OFFSET)
			{
				const vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
				const int Flags = m_pSwitchExtra[TileIndex].m_Flags;
				funcInit(Type - ENTITY_OFFSET, Pos, Flags, Number);
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
	if(!m_pSwitchExtra)
		return TILE_AIR;

	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return m_pSwitchExtra[Ny * m_Width + Nx].m_Type;
}

CSwitchTileExtra* CCollision::GetSwitchTile(vec2 Pos) const
{
	if(!m_pSwitchExtra)
		return nullptr;

	int Nx = clamp(round_to_int(Pos.x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(Pos.y) / 32, 0, m_Height - 1);
	return &m_pSwitchExtra[Ny * m_Width + Nx];
}

std::optional<int> CCollision::GetSwitchTileNumber(vec2 Pos) const
{
	auto* pSwitchTile = GetSwitchTile(Pos);
	return pSwitchTile ? std::make_optional(pSwitchTile->m_Number) : std::nullopt;
}

std::optional<int> CCollision::GetSwitchTileNumberAtIndex(vec2 Pos, int Index) const
{
	auto* pSwitchTile = GetSwitchTile(Pos);
	if(pSwitchTile && pSwitchTile->m_Type == Index)
		return std::make_optional(pSwitchTile->m_Number);
	return std::nullopt;
}

std::optional<CCollision::ZoneDetail> CCollision::GetZonedetail(vec2 Pos) const
{
	if(!m_pSwitchExtra)
		return std::nullopt;

	int Nx = clamp(round_to_int(Pos.x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(Pos.y) / 32, 0, m_Height - 1);
	int Number = m_pSwitchExtra[Ny * m_Width + Nx].m_Number;

	if(m_vZoneDetail.contains(Number))
		return m_vZoneDetail.at(Number);

	return std::nullopt;
}

std::optional<std::pair<vec2, bool>> CCollision::TryGetFixedCamPos(vec2 currentPos) const
{
	for(const auto& [Pos, Rect, Smooth] : m_vFixedCamZones)
	{
		if(currentPos.x > Rect.x && currentPos.x < Rect.z &&
			currentPos.y > Rect.y && currentPos.y < Rect.w)
			return std::make_pair(Pos, Smooth);
	}
	return std::nullopt;
}

std::optional<vec2> CCollision::TryGetTeleportOut(vec2 currentPos)
{
	if(!m_pTele)
		return std::nullopt;

	const int Nx = clamp(round_to_int(currentPos.x) / 32, 0, m_Width - 1);
	const int Ny = clamp(round_to_int(currentPos.y) / 32, 0, m_Height - 1);
	const auto& Number = m_pTele[Ny * m_Width + Nx].m_Number;

	if(Number <= 0)
		return std::nullopt;

	const auto& TeleOuts = m_vTeleOuts[Number];
	if(TeleOuts.empty())
		return std::nullopt;

	return TeleOuts[rand() % TeleOuts.size()];
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
	if(!m_pSwitchExtra)
		return 0;

	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return m_pSwitchExtra[Ny * m_Width + Nx].m_Type > 128 ? 0 : m_pSwitchExtra[Ny * m_Width + Nx].m_Flags;
}