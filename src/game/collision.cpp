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

enum
{
	MR_DIR_HERE = 0,
	MR_DIR_RIGHT,
	MR_DIR_DOWN,
	MR_DIR_LEFT,
	MR_DIR_UP,
	NUM_MR_DIRS
};

static vec2 CalculateTileCenter(int Index, int Width)
{
	if(Width <= 0)
		return vec2(0.0f, 0.0f);

	const float halfTileSize = TILE_SIZE / 2.0f;
	return vec2(
		static_cast<float>(Index % Width) * TILE_SIZE + halfTileSize,
		static_cast<float>(Index / Width) * TILE_SIZE + halfTileSize
	);
}

vec2 ClampVel(int MoveRestriction, vec2 Vel)
{
	if((Vel.x > 0 && (MoveRestriction & CANTMOVE_RIGHT)) || (Vel.x < 0 && (MoveRestriction & CANTMOVE_LEFT)))
		Vel.x = 0;
	if((Vel.y > 0 && (MoveRestriction & CANTMOVE_DOWN)) || (Vel.y < 0 && (MoveRestriction & CANTMOVE_UP)))
		Vel.y = 0;
	return Vel;
}

CCollision::CCollision()
{
	m_pDoor = nullptr;
	m_pLayers = new CLayers();
}

CCollision::~CCollision()
{
	delete[] m_pDoor;
	delete m_pLayers;
}

template <typename TileType>
inline TileType* TryLoadTileLayerData(IMap* pMap,  const CMapItemLayerTilemap* pLayer, int DataIndex)
{
	if(!pLayer)
		return nullptr;

	unsigned int DataSize = pMap->GetDataSize(DataIndex);
	size_t ExpectedSize = (size_t)pLayer->m_Width * pLayer->m_Height * sizeof(TileType);
	if(DataSize < ExpectedSize)
		return nullptr;

	void* pRawData = pMap->GetData(DataIndex);
	return pRawData ? static_cast<TileType*>(pRawData) : nullptr;
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

	// front layer
	m_pFront = TryLoadTileLayerData<CTile>(pMap, m_pLayers->FrontLayer(), m_pLayers->FrontLayer() ? m_pLayers->FrontLayer()->m_Front : -1);
	if(m_pFront)
	{
		InitTiles(m_pFront);
	}

	// tele layer
	m_pTele = TryLoadTileLayerData<CTeleTile>(pMap, m_pLayers->TeleLayer(), m_pLayers->TeleLayer() ? m_pLayers->TeleLayer()->m_Tele : -1);
	if(m_pTele)
	{
		InitTeleports();
	}

	// switch layer
	m_pSwitchExtra = TryLoadTileLayerData<CSwitchTileExtra>(pMap, m_pLayers->SwitchLayer(), m_pLayers->SwitchLayer() ? m_pLayers->SwitchLayer()->m_Switch : -1);
	if(m_pSwitchExtra)
	{
		InitSwitchExtra();
	}

	// speed up layer
	m_pSpeedupExtra = TryLoadTileLayerData<CSpeedupTileExtra>(pMap, m_pLayers->SpeedupLayer(), m_pLayers->SpeedupLayer() ? m_pLayers->SpeedupLayer()->m_Speedup : -1);
	if(m_pSpeedupExtra)
	{
		InitSpeedupExtra();
	}

	// door layer
	auto DoorLayerSize = m_Width * m_Height;
	m_pDoor = new CDoorTile[DoorLayerSize]();
	mem_zero(m_pDoor, DoorLayerSize * sizeof(CDoorTile));
}

static void initGatheringNode(const std::string& nodeType, const std::vector<std::string>& vSettings, int Number, std::unordered_map<int, GatheringNode>& vNodesContainer)
{
	std::string ItemsData {};
	GatheringNode detail {};
	const auto Identity = nodeType + " " + std::to_string(Number);

	// load settings for the node
	if(mystd::loadSettings<std::string, int, int, std::string>(Identity, vSettings, &detail.Name, &detail.Level, &detail.Health, &ItemsData))
	{
		// parse and add items
		DBSet ItemsSet(ItemsData);
		for(const auto& Elem : ItemsSet.getItems())
		{
			int ItemID;
			float Chance;
			if(sscanf(Elem.c_str(), "[%d/%f]", &ItemID, &Chance) == 2)
				detail.m_vItems.addElement(ItemID, Chance);
		}

		// store the initialized node
		detail.m_vItems.normalizeChances();
		vNodesContainer[Number] = detail;
		dbg_msg("map-init", "Gathering '%s' (switch: %d) initialized: Items='%d'", detail.Name.c_str(), Number, (int)detail.m_vItems.size());
	}
}

void CCollision::InitSettings()
{
	const auto& settings = m_pLayers->GetSettings();

	for(int i = 1; i < 255; i++)
	{
		// initialize zones details
		{
			ZoneDetail detail;
			const auto Identity = std::string("#zone ").append(std::to_string(i));
			if(mystd::loadSettings<std::string, bool>(Identity, settings, &detail.Name, &detail.PVP))
			{
				dbg_msg("map-init", "Zone '%s' (switch: %d) initialized: PVP=%s", detail.Name.c_str(), i, detail.PVP ? "true" : "false");
				m_vZoneDetail[i] = detail;
			}
		}

		// initialize text zone details
		{
			TextZoneDetail detail;
			const auto Identity = std::string("#text ").append(std::to_string(i));
			if(mystd::loadSettings<std::string>(Identity, settings, &detail.Text))
			{
				dbg_msg("map-init", "Text (switch: %d) initialized: String='%s'", i, detail.Text.c_str());
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
}

void CCollision::InitTiles(CTile* pTiles)
{
	if(!pTiles)
		return;

	static const std::unordered_map<int, int> TILE_COLFLAG_MAP =
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

	const int NumTiles = m_Width * m_Height;
	for(int i = 0; i < NumTiles; ++i)
	{
		CTile& currentTile = pTiles[i];
		const int tileIndex = currentTile.m_Index;
		currentTile.m_ColFlags = 0;

		if(tileIndex > 128)
			continue;

		if(tileIndex == TILE_FIXED_CAM || tileIndex == TILE_SMOOTH_FIXED_CAM)
		{
			FixedCamZoneDetail camZoneData;
			camZoneData.Pos = CalculateTileCenter(i, m_Width);
			camZoneData.Rect = {
				camZoneData.Pos.x - CAM_RADIUS_W, camZoneData.Pos.y - CAM_RADIUS_H,
				camZoneData.Pos.x + CAM_RADIUS_W, camZoneData.Pos.y + CAM_RADIUS_H
			};
			camZoneData.Smooth = (tileIndex == TILE_SMOOTH_FIXED_CAM);

			m_vFixedCamZones.emplace_back(std::move(camZoneData));
			continue;
		}

		if(tileIndex == TILE_DESTROYER_PROJECTILE)
		{
			vec2 Pos = CalculateTileCenter(i, m_Width);
			SetDoorCollisionAt(Pos.x, Pos.y, TILE_STOPA, 0, 0);
		}

		if(auto it = TILE_COLFLAG_MAP.find(tileIndex); it != TILE_COLFLAG_MAP.end())
			currentTile.m_ColFlags = static_cast<char>(it->second);

	}
}

void CCollision::InitTeleports()
{
	if(!m_pTele)
		return;

	const int NumTiles = m_Width * m_Height;
	for(int i = 0; i < NumTiles; ++i)
	{
		const CTeleTile& teleTile = m_pTele[i];
		const int teleNumber = teleTile.m_Number;
		const int teleType = teleTile.m_Type;

		switch(teleType)
		{
			case TILE_TELE_FROM:
			case TILE_TELE_FROM_CONFIRM:
				m_pTiles[i].m_Index = static_cast<char>(teleType);
				break;

			case TILE_TELE_OUT:
				m_pTiles[i].m_Index = static_cast<char>(teleType);
				{
					const vec2 tilePos = CalculateTileCenter(i, m_Width);
					m_vTeleOuts[teleNumber].push_back(tilePos);
				}
				break;

			default:
				break;
		}
	}
}

void CCollision::InitSwitchExtra()
{
	if(!m_pSwitchExtra)
		return;

	const int NumTiles = m_Width * m_Height;
	for(int i = 0; i < NumTiles; ++i)
	{
		const CSwitchTileExtra& switchTile = m_pSwitchExtra[i];
		const int switchNumber = switchTile.m_Number;
		const int switchType = switchTile.m_Type;

		switch(switchType)
		{
			case TILE_SW_ZONE:
				if(m_vZoneDetail.contains(switchNumber))
				{
					const auto& zone = m_vZoneDetail.at(switchNumber);
					if(!zone.PVP)
						m_pTiles[i].m_ColFlags |= COLFLAG_SAFE;
				}
				break;

			case TILE_SW_TEXT:
				if(m_vZoneTextDetail.contains(switchNumber))
				{
					auto& zoneText = m_vZoneTextDetail.at(switchNumber);
					const vec2 tilePos = CalculateTileCenter(i, m_Width);
					zoneText.vPositions.emplace_back(tilePos);
				}
				break;

				default:
					break;
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

int CCollision::GetMainTileIndex(int Index) const
{
	if(Index < 0)
		return TILE_AIR;

	return m_pTiles[Index].m_Index;
}

int CCollision::GetFrontTileIndex(int Index) const
{
	if(Index < 0 || !m_pFront)
		return TILE_AIR;

	return m_pFront[Index].m_Index;
}

int CCollision::GetExtraTileIndex(int Index) const
{
	if(Index < 0 || !m_pSwitchExtra)
		return TILE_AIR;

	return m_pSwitchExtra[Index].m_Type;
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

std::optional<int> CCollision::GetSwitchTileNumberAtTileIndex(vec2 Pos, int TileIndex) const
{
	auto* pSwitchTile = GetSwitchTile(Pos);
	if(pSwitchTile && pSwitchTile->m_Type == TileIndex)
		return std::make_optional(pSwitchTile->m_Number);
	return std::nullopt;
}


bool CCollision::TileExists(int Index) const
{
	if(Index < 0)
		return false;

	if(m_pTiles[Index].m_Index > TILE_AIR)
		return true;

	if(m_pFront && m_pFront[Index].m_Index > TILE_AIR)
		return true;

	if(m_pTele && m_pTele[Index].m_Type > TILE_AIR)
		return true;

	if(m_pSpeedupExtra && m_pSpeedupExtra[Index].m_Force > 0)
		return true;

	if(m_pDoor && m_pDoor[Index].m_Index > TILE_AIR)
		return true;

	if(m_pSwitchExtra && m_pSwitchExtra[Index].m_Type > TILE_AIR)
		return true;

	return TileExistsNext(Index);
}

bool CCollision::TileExistsNext(int Index) const
{
	if(Index < 0)
		return false;

	int TileOnTheLeftInd = (Index - 1 > 0) ? Index - 1 : Index;
	int TileOnTheRightInd = (Index + 1 < m_Width * m_Height) ? Index + 1 : Index;
	int TileBelowInd = (Index + m_Width < m_Width * m_Height) ? Index + m_Width : Index;
	int TileAboveInd = (Index - m_Width > 0) ? Index - m_Width : Index;

	{
		const auto& TileBelow = m_pTiles[TileBelowInd];
		const auto& TileAbove = m_pTiles[TileAboveInd];
		const auto& TileRight = m_pTiles[TileOnTheRightInd];
		const auto& TileLeft = m_pTiles[TileOnTheLeftInd];

		if((TileRight.m_Index == TILE_STOP && TileRight.m_Flags == ROTATION_270) || (TileLeft.m_Index == TILE_STOP && TileLeft.m_Flags == ROTATION_90))
			return true;

		if((TileBelow.m_Index == TILE_STOP && TileBelow.m_Flags == ROTATION_0) || (TileAbove.m_Index == TILE_STOP && TileAbove.m_Flags == ROTATION_180))
			return true;

		if(TileRight.m_Index == TILE_STOPA || TileLeft.m_Index == TILE_STOPA || ((TileRight.m_Index == TILE_STOPS || TileLeft.m_Index == TILE_STOPS)))
			return true;

		if(TileBelow.m_Index == TILE_STOPA || TileAbove.m_Index == TILE_STOPA || ((TileBelow.m_Index == TILE_STOPS || TileAbove.m_Index == TILE_STOPS) && TileBelow.m_Flags | ROTATION_180 | ROTATION_0))
			return true;
	}

	if(m_pFront)
	{
		const auto& TileBelow = m_pFront[TileBelowInd];
		const auto& TileAbove = m_pFront[TileAboveInd];
		const auto& TileRight = m_pFront[TileOnTheRightInd];
		const auto& TileLeft = m_pFront[TileOnTheLeftInd];

		if(TileRight.m_Index == TILE_STOPA || TileLeft.m_Index == TILE_STOPA || ((TileRight.m_Index == TILE_STOPS || TileLeft.m_Index == TILE_STOPS)))
			return true;
		if(TileBelow.m_Index == TILE_STOPA || TileAbove.m_Index == TILE_STOPA || ((TileBelow.m_Index == TILE_STOPS || TileAbove.m_Index == TILE_STOPS) && TileBelow.m_Flags | ROTATION_180 | ROTATION_0))
			return true;
		if((TileRight.m_Index == TILE_STOP && TileRight.m_Flags == ROTATION_270) || (TileLeft.m_Index == TILE_STOP && TileLeft.m_Flags == ROTATION_90))
			return true;
		if((TileBelow.m_Index == TILE_STOP && TileBelow.m_Flags == ROTATION_0) || (TileAbove.m_Index == TILE_STOP && TileAbove.m_Flags == ROTATION_180))
			return true;
	}

	if(m_pDoor)
	{
		const auto& TileBelow = m_pDoor[TileBelowInd];
		const auto& TileAbove = m_pDoor[TileAboveInd];
		const auto& TileRight = m_pDoor[TileOnTheRightInd];
		const auto& TileLeft = m_pDoor[TileOnTheLeftInd];

		if(TileRight.m_Index == TILE_STOPA || TileLeft.m_Index == TILE_STOPA || ((TileRight.m_Index == TILE_STOPS || TileLeft.m_Index == TILE_STOPS)))
			return true;
		if(TileBelow.m_Index == TILE_STOPA || TileAbove.m_Index == TILE_STOPA || ((TileBelow.m_Index == TILE_STOPS || TileAbove.m_Index == TILE_STOPS) && TileBelow.m_Flags | ROTATION_180 | ROTATION_0))
			return true;
		if((TileRight.m_Index == TILE_STOP && TileRight.m_Flags == ROTATION_270) || (TileLeft.m_Index == TILE_STOP && TileLeft.m_Flags == ROTATION_90))
			return true;
		if((TileBelow.m_Index == TILE_STOP && TileBelow.m_Flags == ROTATION_0) || (TileAbove.m_Index == TILE_STOP && TileAbove.m_Flags == ROTATION_180))
			return true;
	}
	return false;
}

int CCollision::GetMapIndex(vec2 Pos) const
{
	int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
	int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);
	int Index = Ny * m_Width + Nx;
	if(TileExists(Index))
		return Index;
	else
		return -1;
}

int CCollision::GetPureMapIndex(float x, float y) const
{
	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);
	return Ny * m_Width + Nx;
}

std::vector<int> CCollision::GetMapIndices(vec2 PrevPos, vec2 Pos, unsigned MaxIndices) const
{
	std::vector<int> vIndices;
	float d = distance(PrevPos, Pos);
	int End(d + 1);
	if(!d)
	{
		int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
		int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);
		int Index = Ny * m_Width + Nx;

		if(TileExists(Index))
		{
			vIndices.push_back(Index);
			return vIndices;
		}
		else
			return vIndices;
	}
	else
	{
		int LastIndex = 0;
		for(int i = 0; i < End; i++)
		{
			float a = i / d;
			vec2 Tmp = mix(PrevPos, Pos, a);
			int Nx = clamp((int)Tmp.x / 32, 0, m_Width - 1);
			int Ny = clamp((int)Tmp.y / 32, 0, m_Height - 1);
			int Index = Ny * m_Width + Nx;
			if(LastIndex != Index && TileExists(Index))
			{
				if(MaxIndices && vIndices.size() > MaxIndices)
					return vIndices;

				vIndices.push_back(Index);
				LastIndex = Index;
			}
		}

		return vIndices;
	}
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

void CCollision::SetDoorCollisionAt(float x, float y, int Type, int Flags, int Number)
{
	if(!m_pDoor)
		return;

	int Nx = clamp(round_to_int(x) / 32, 0, m_Width - 1);
	int Ny = clamp(round_to_int(y) / 32, 0, m_Height - 1);

	m_pDoor[Ny * m_Width + Nx].m_Index = Type;
	m_pDoor[Ny * m_Width + Nx].m_Flags = Flags;
	m_pDoor[Ny * m_Width + Nx].m_Number = Number;
}

void CCollision::SetDoorFromToCollisionAt(vec2 From, vec2 To, int Type, int Flags, int Number)
{
	if(!m_pDoor || m_Width <= 0 || m_Height <= 0)
		return;

	float MinX = minimum(From.x, To.x);
	float MinY = minimum(From.y, To.y);
	float MaxX = maximum(From.x, To.x);
	float MaxY = maximum(From.y, To.y);

	int StartX = clamp(round_to_int(MinX) / 32, 0, m_Width - 1);
	int StartY = clamp(round_to_int(MinY) / 32, 0, m_Height - 1);
	int EndX = clamp(round_to_int(MaxX) / 32, 0, m_Width - 1);
	int EndY = clamp(round_to_int(MaxY) / 32, 0, m_Height - 1);

	for(int Ny = StartY; Ny <= EndY; ++Ny)
	{
		for(int Nx = StartX; Nx <= EndX; ++Nx)
		{
			int Index = Ny * m_Width + Nx;
			m_pDoor[Index].m_Index = Type;
			m_pDoor[Index].m_Flags = Flags;
			m_pDoor[Index].m_Number = Number;
		}
	}
}

void CCollision::GetDoorTile(int Index, CDoorTile* pDoorTile) const
{
	if(!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
	{
		pDoorTile->m_Index = 0;
		pDoorTile->m_Flags = 0;
		pDoorTile->m_Number = 0;
		return;
	}
	*pDoorTile = m_pDoor[Index];
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

void CCollision::FillLengthWall(vec2 Direction, vec2* pPos, vec2* pPosTo, int DepthTiles, bool AlignPos, bool OffsetStartlineOneTile)
{
	if(!pPos || !pPosTo)
		return;

	vec2 NormDir = normalize(Direction);
	vec2 StartPos = *pPos;
	vec2 EndPos = StartPos + NormDir * (DepthTiles * 32.0f);

	if(!IntersectLine(StartPos, EndPos, nullptr, pPosTo))
		*pPosTo = EndPos;

	if(OffsetStartlineOneTile)
		*pPos = StartPos - NormDir * 30.0f;

	if(AlignPos)
	{
		if(Direction.x > 0.f || Direction.x < 0.f)
		{
			(*pPos).y = (float)(round_to_int((*pPos).y) / 32 * 32) + 16.f;
			(*pPosTo).y = (float)(round_to_int((*pPosTo).y) / 32 * 32) + 16.f;
		}

		if(Direction.y > 0.f || Direction.y < 0.f)
		{
			(*pPos).x = (float)(round_to_int((*pPos).x) / 32 * 32) + 16.f;
			(*pPosTo).x = (float)(round_to_int((*pPosTo).x) / 32 * 32) + 16.f;
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

static int GetMoveRestrictionsRaw(int Direction, int Tile, int Flags)
{
	Flags = Flags & (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE);
	switch(Tile)
	{
		case TILE_STOP:
			switch(Flags)
			{
				case ROTATION_0: return CANTMOVE_DOWN;
				case ROTATION_90: return CANTMOVE_LEFT;
				case ROTATION_180: return CANTMOVE_UP;
				case ROTATION_270: return CANTMOVE_RIGHT;

				case TILEFLAG_YFLIP^ ROTATION_0: return CANTMOVE_UP;
				case TILEFLAG_YFLIP^ ROTATION_90: return CANTMOVE_RIGHT;
				case TILEFLAG_YFLIP^ ROTATION_180: return CANTMOVE_DOWN;
				case TILEFLAG_YFLIP^ ROTATION_270: return CANTMOVE_LEFT;
			}
			break;
		case TILE_STOPS:
			switch(Flags)
			{
				case ROTATION_0:
				case ROTATION_180:
				case TILEFLAG_YFLIP^ ROTATION_0:
				case TILEFLAG_YFLIP^ ROTATION_180:
					return CANTMOVE_DOWN | CANTMOVE_UP;
				case ROTATION_90:
				case ROTATION_270:
				case TILEFLAG_YFLIP^ ROTATION_90:
				case TILEFLAG_YFLIP^ ROTATION_270:
					return CANTMOVE_LEFT | CANTMOVE_RIGHT;
			}
			break;
		case TILE_STOPA:
			return CANTMOVE_LEFT | CANTMOVE_RIGHT | CANTMOVE_UP | CANTMOVE_DOWN;
	}
	return 0;
}

static int GetMoveRestrictionsMask(int Direction)
{
	switch(Direction)
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
	if(Direction == MR_DIR_HERE && Tile == TILE_STOP)
	{
		return Result;
	}
	return Result & GetMoveRestrictionsMask(Direction);
}

int CCollision::GetMoveRestrictions(CALLBACK_SWITCHACTIVE pfnSwitchActive, void* pUser, vec2 Pos, float Distance, int OverrideCenterTileIndex) const
{
	static const vec2 DIRECTIONS[NUM_MR_DIRS] =
	{
		vec2(0, 0),
		vec2(1, 0),
		vec2(0, 1),
		vec2(-1, 0),
		vec2(0, -1)
	};

	dbg_assert(0.0f <= Distance && Distance <= 32.0f, "invalid distance");

	int Restrictions = 0;
	for(int d = 0; d < NUM_MR_DIRS; d++)
	{
		vec2 ModPos = Pos + DIRECTIONS[d] * Distance;
		int ModMapIndex = GetPureMapIndex(ModPos);

		if(d == MR_DIR_HERE && OverrideCenterTileIndex >= 0)
			ModMapIndex = OverrideCenterTileIndex;

		// main
		int Tile = GetMainTileIndex(ModMapIndex);
		int Flags = GetMainTileFlags(ModPos.x, ModPos.y);
		Restrictions |= ::GetMoveRestrictions(d, Tile, Flags);

		// front
		Tile = GetFrontTileIndex(ModMapIndex);
		Flags = GetFrontTileFlags(ModPos.x, ModPos.y);
		Restrictions |= ::GetMoveRestrictions(d, Tile, Flags);

		// door
		if(pfnSwitchActive)
		{
			CDoorTile DoorTile;
			GetDoorTile(ModMapIndex, &DoorTile);
			if(pfnSwitchActive(DoorTile.m_Number, pUser))
				Restrictions |= ::GetMoveRestrictions(d, DoorTile.m_Index, DoorTile.m_Flags);
		}
	}

	return Restrictions;
}