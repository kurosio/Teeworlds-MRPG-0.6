/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include <base/vmath.h>

class CTile;
class CDoorTile;
class CTeleTile;
class CSwitchTileExtra;
class CSpeedupTileExtra;
class CLayers;

enum
{
	CANTMOVE_LEFT = 1 << 0,
	CANTMOVE_RIGHT = 1 << 1,
	CANTMOVE_UP = 1 << 2,
	CANTMOVE_DOWN = 1 << 3,
};

vec2 ClampVel(int MoveRestriction, vec2 Vel);
typedef bool (*CALLBACK_SWITCHACTIVE)(int Number, void* pUser);

class CCollision
{
public:
	enum
	{
		COLFLAG_SOLID = 1 << 0,
		COLFLAG_DEATH = 1 << 1,
		COLFLAG_NOHOOK = 1 << 2,
		COLFLAG_SAFE = 1 << 3,
		COLFLAG_DISALLOW_MOVE = 1 << 4,
		COLFLAG_WATER = 1 << 5,
	};

	struct ZoneDetail
	{
		bool PVP{};
		std::string Name{};
	};
	struct TextZoneDetail
	{
		std::vector<vec2> vPositions {};
		std::string Text {};
	};

	struct FixedCamZoneDetail
	{
		vec2 Pos {};
		vec4 Rect {};
		bool Smooth {};
	};

private:
	int m_Width{};
	int m_Height{};
	CTile *m_pTiles{};
	CTile *m_pFront{};
	CTeleTile* m_pTele{};
	CSwitchTileExtra* m_pSwitchExtra{};
	CSpeedupTileExtra* m_pSpeedupExtra{};
	CDoorTile* m_pDoor;
	CLayers* m_pLayers {};

	std::map<int, std::vector<vec2>> m_vTeleOuts {};
	std::vector<FixedCamZoneDetail> m_vFixedCamZones {};
	std::map<int, ZoneDetail> m_vZoneDetail {};
	std::map<int, TextZoneDetail> m_vZoneTextDetail {};
	std::unordered_map<int, GatheringNode> m_vOreNodes {};
	std::unordered_map<int, GatheringNode> m_vPlantNodes {};
	std::unordered_map<int, GatheringNode> m_vFishNodes {};

	// initialization
	void InitSettings();
	void InitTiles(CTile* pTiles);
	void InitTeleports();
	void InitSwitchExtra();
	void InitSpeedupExtra();

	// flags
	int GetMainTileFlags(float x, float y) const;
	int GetFrontTileFlags(float x, float y) const;
	int GetExtraTileFlags(float x, float y) const;

	// collision flags
	int GetMainTileCollisionFlags(int x, int y) const;
	int GetFrontTileCollisionFlags(int x, int y) const;

public:
	CCollision();
	~CCollision();

	void Init(class IKernel* pKernel, int WorldID);
	void InitEntities(const std::function<void(int, vec2, int)>& funcInit) const;
	void InitSwitchEntities(const std::function<void(int, vec2, int, int)>& funcInit) const;

	int GetWidth() const { return m_Width; }
	int GetHeight() const { return m_Height; }
	vec2 GetRotateDirByFlags(int Flags);
	CLayers* GetLayers() const { return m_pLayers; }

	// tile index
	int GetMainTileIndex(int Index) const;
	int GetFrontTileIndex(int Index) const;
	int GetExtraTileIndex(int Index) const;

	// switch
	CSwitchTileExtra* GetSwitchTile(vec2 Pos) const;
	std::optional<int> GetSwitchTileNumber(vec2 Pos) const;
	std::optional<int> GetSwitchTileNumberAtTileIndex(vec2 Pos, int TileIndex) const;

	// collision flags
	bool CheckPoint(float x, float y, int Flag = COLFLAG_SOLID) const { return (GetCollisionFlagsAt(x, y) & Flag) != 0; }
	int GetCollisionFlagsAt(float x, float y) const
	{
		const ivec2 roundPos(round_to_int(x), round_to_int(y));
		const int TileCollisionFlags = GetMainTileCollisionFlags(roundPos.x, roundPos.y);
		const int FrontTileCollisionFlags = GetFrontTileCollisionFlags(roundPos.x, roundPos.y);
		return TileCollisionFlags | FrontTileCollisionFlags;
	}
	bool CheckPoint(vec2 Pos, int Flag = COLFLAG_SOLID) const { return CheckPoint(Pos.x, Pos.y, Flag); }
	int GetCollisionFlagsAt(vec2 Pos) const { return GetCollisionFlagsAt(Pos.x, Pos.y); }

	// tiles
	bool TileExists(int Index) const;
	bool TileExistsNext(int Index) const;
	int GetMapIndex(vec2 Pos) const;
	int GetPureMapIndex(float x, float y) const;
	int GetPureMapIndex(vec2 pos) const { return GetPureMapIndex(pos.x, pos.y); }
	std::vector<int> GetMapIndices(vec2 PrevPos, vec2 Pos, unsigned MaxIndices = 0) const;

	// doors
	void SetDoorCollisionAt(float x, float y, int Type, int Flags, int Number = TEAM_ALL);
	void SetDoorFromToCollisionAt(vec2 From, vec2 To, int Type, int Flags, int Number = TEAM_ALL);
	void GetDoorTile(int Index, CDoorTile* pDoorTile) const;

	// other
	std::map<int, TextZoneDetail>& GetTextZones() { return m_vZoneTextDetail; }
	std::optional<ZoneDetail> GetZonedetail(vec2 Pos) const;
	std::optional<vec2> TryGetTeleportOut(vec2 currentPos);
	std::optional<std::pair<vec2, bool>> TryGetFixedCamPos(vec2 currentPos) const;
	const std::unordered_map<int, GatheringNode>& GetOreNodes() const { return m_vOreNodes; }
	const std::unordered_map<int, GatheringNode>& GetPlantNodes() const { return m_vPlantNodes; }
	const std::unordered_map<int, GatheringNode>& GetFishNodes() const { return m_vFishNodes; }
	GatheringNode* GetOreNode(int SwitchNumber) { return m_vOreNodes.contains(SwitchNumber) ? &m_vOreNodes.at(SwitchNumber) : nullptr; }
	GatheringNode* GetPlantNode(int SwitchNumber) { return m_vPlantNodes.contains(SwitchNumber) ? &m_vPlantNodes.at(SwitchNumber) : nullptr; }
	GatheringNode* GetFishNode(int SwitchNumber) { return m_vFishNodes.contains(SwitchNumber) ? &m_vFishNodes.at(SwitchNumber) : nullptr; }

	// tools
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision, int ColFlag = COLFLAG_SOLID) const;
	bool IntersectLineWithInvisible(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision) const
	{
		return IntersectLineColFlag(Pos0, Pos1, pOutCollision, pOutBeforeCollision, COLFLAG_DISALLOW_MOVE | COLFLAG_SOLID);
	}
	bool IntersectLineColFlag(vec2 Pos0, vec2 Pos1, vec2* pOutCollision, vec2* pOutBeforeCollision, int ColFlag) const;
	void FillLengthWall(vec2 Direction, vec2* pPos, vec2* pPosTo, int DepthTiles = 32, bool AlignPos = true, bool OffsetStartlineOneTile = true);
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces) const;
	void MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity, bool *pDeath=NULL) const;
	bool TestBox(vec2 Pos, vec2 Size, int Flag=COLFLAG_SOLID) const;
	void MovePhysicalAngleBox(vec2* pPos, vec2* pVel, vec2 Size, float* pAngle, float* pAngleForce, float Elasticity, float Gravity = 0.5f) const;
	void MovePhysicalBox(vec2* pPos, vec2* pVel, vec2 Size, float Elasticity, float Gravity = 0.5f) const;
	int GetMoveRestrictions(CALLBACK_SWITCHACTIVE pfnSwitchActive, void* pUser, vec2 Pos, float Distance = 18.0f, int OverrideCenterTileIndex = -1) const;
	int GetMoveRestrictions(vec2 Pos, float Distance = 18.0f) const
	{
		return GetMoveRestrictions(nullptr, nullptr, Pos, Distance);
	}

	template <typename ... Ts>
	vec2 VerifyPoint(int64_t CollisionFlags, vec2 Pos, std::string_view Message, const Ts&... FormatArgs) const
	{
		dbg_assert(CheckPoint(Pos, CollisionFlags) == false, fmt_default(Message.data(), FormatArgs...).c_str());
		return Pos;
	}
};

#endif
