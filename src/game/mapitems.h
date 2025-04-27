/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_MAPITEMS_H
#define GAME_MAPITEMS_H

// layer types
enum
{
	// layers
	LAYERTYPE_INVALID = 0,
	LAYERTYPE_GAME, // unused
	LAYERTYPE_TILES,

	// mapitem types
	MAPITEMTYPE_VERSION=0,
	MAPITEMTYPE_INFO,
	MAPITEMTYPE_IMAGE,
	MAPITEMTYPE_ENVELOPE,
	MAPITEMTYPE_GROUP,
	MAPITEMTYPE_LAYER,
	MAPITEMTYPE_ENVPOINTS,

	CURVETYPE_STEP=0,
	CURVETYPE_LINEAR,
	CURVETYPE_SLOW,
	CURVETYPE_FAST,
	CURVETYPE_SMOOTH,
	CURVETYPE_BEZIER,
	NUM_CURVETYPES,

	// game layer tiles
	ENTITY_NULL = 0,
	ENTITY_ARMOR_1,
	ENTITY_HEALTH_1,
	ENTITY_PICKUP_SHOTGUN,
	ENTITY_PICKUP_GRENADE,
	ENTITY_PICKUP_LASER,
	ENTITY_NPC_WALL,
	ENTITY_MOB_WALL,
	ENTITY_PLANT,
	ENTITY_ORE,
	ENTITY_FISH,
	ENTITY_SPAWN = 17,
	ENTITY_SPAWN_MOBS,
	ENTITY_SPAWN_PRISON = 20,
	NUM_ENTITIES,

	TILE_AIR = 0,
	TILE_SOLID = 1,
	TILE_DEATH = 2,
	TILE_NOHOOK = 3,
	TILE_FIXED_CAM = 4,
	TILE_SMOOTH_FIXED_CAM = 5,
	TILE_WATER = 6,
	TILE_PLAYER_HOUSE = 8,
	TILE_DESTROYER_PROJECTILE = 11,
	TILE_WORLD_SWAPPER = 14,
	TILE_JAIL_ZONE = 15,
	TILE_GUILD_HOUSE = 16,
	TILE_AUCTION = 17,
	TILE_SKILL_ZONE = 18,
	TILE_QUEST_BOARD = 21,
	TILE_AETHER_TELEPORT = 28,
	TILE_SHOP_ZONE = 29,
	TILE_CRAFT_ZONE = 30,
	TILE_GUILD_CHAIR = 32,
	TILE_CHAIR_LV1 = 33,
	TILE_CHAIR_LV2 = 34,
	TILE_CHAIR_LV3 = 35,
	TILE_INFO_BONUSES = 36,
	TILE_INFO_WANTED = 37,
	TILE_BANK_MANAGER = 38,
	TILE_FISHING_MODE = 39,

	// stop tiles
	TILE_STOP = 60,
	TILE_STOPS,
	TILE_STOPA,

	// teleport tiles
	TILE_TELE_FROM_CONFIRM = 10,
	TILE_TELE_FROM = 26,
	TILE_TELE_OUT = 27,

	// switch extra
	TILE_SW_ZONE = 22,
	TILE_SW_TEXT = 23,
	TILE_SW_HOUSE_ZONE = 24,

	// speedup extra
	MAX_TILES = 255,

	//Flags
	TILEFLAG_XFLIP = 1,
	TILEFLAG_YFLIP = 2,
	TILEFLAG_OPAQUE = 4,
	TILEFLAG_ROTATE = 8,

	//Rotation
	ROTATION_0 = 0,
	ROTATION_90 = TILEFLAG_ROTATE,
	ROTATION_180 = (TILEFLAG_XFLIP | TILEFLAG_YFLIP),
	ROTATION_270 = (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE),

	// layerflags
	LAYERFLAG_DETAIL = 1,
	TILESLAYERFLAG_GAME = 1,
	TILESLAYERFLAG_TELE = 2,
	TILESLAYERFLAG_SPEEDUP = 4,
	TILESLAYERFLAG_FRONT = 8,
	TILESLAYERFLAG_SWITCH = 16,

	// entity offset
	ENTITY_OFFSET = 255 - 16 * 4,
};

struct CPoint
{
	int x, y; // 22.10 fixed point
};

struct CColor
{
	int r, g, b, a;
};

struct CQuad
{
	CPoint m_aPoints[5];
	CColor m_aColors[4];
	CPoint m_aTexcoords[4];

	int m_PosEnv;
	int m_PosEnvOffset;

	int m_ColorEnv;
	int m_ColorEnvOffset;
};

class CTile
{
public:
	unsigned char m_Index;
	unsigned char m_Flags;
	unsigned char m_Skip;
	unsigned char m_ColFlags;
};

class CTeleTile
{
public:
	unsigned char m_Number;
	unsigned char m_Type;
};

class CSwitchTileExtra
{
public:
	unsigned char m_Number;
	unsigned char m_Type;
	unsigned char m_Flags;
	unsigned char m_Delay;
};

class CSpeedupTileExtra
{
public:
	unsigned char m_Force;
	unsigned char m_MaxSpeed;
	unsigned char m_Type;
	short m_Angle;
};

class CDoorTile
{
public:
	unsigned char m_Index;
	unsigned char m_Flags;
	int m_Number;
};

struct CMapItemInfo
{
	int m_Version;
	int m_Author;
	int m_MapVersion;
	int m_Credits;
	int m_License;
};

struct CMapItemInfoSettings : CMapItemInfo
{
	int m_Settings;
};

struct CMapItemImage
{
	int m_Version;
	int m_Width;
	int m_Height;
	int m_External;
	int m_ImageName;
	int m_ImageData;
};

struct CMapItemGroup_v1
{
	int m_Version;
	int m_OffsetX;
	int m_OffsetY;
	int m_ParallaxX;
	int m_ParallaxY;

	int m_StartLayer;
	int m_NumLayers;
};

struct CMapItemGroup : public CMapItemGroup_v1
{
	enum
	{
		CURRENT_VERSION = 3
	};

	int m_UseClipping;
	int m_ClipX;
	int m_ClipY;
	int m_ClipW;
	int m_ClipH;

	int m_aName[3];
};

struct CMapItemLayer
{
	int m_Version;
	int m_Type;
	int m_Flags;
};

struct CMapItemLayerTilemap
{
	enum
	{
		CURRENT_VERSION = 3,
		TILE_SKIP_MIN_VERSION = 4, // supported for loading but not saving
	};

	CMapItemLayer m_Layer;
	int m_Version;

	int m_Width;
	int m_Height;
	int m_Flags;

	CColor m_Color;
	int m_ColorEnv;
	int m_ColorEnvOffset;

	int m_Image;
	int m_Data;

	int m_aName[3];

	// DDRace

	int m_Tele;
	int m_Speedup;
	int m_Front;
	int m_Switch;
	int m_Tune;
};

struct CMapItemLayerQuads
{
	CMapItemLayer m_Layer;
	int m_Version;

	int m_NumQuads;
	int m_Data;
	int m_Image;

	int m_aName[3];
};

struct CMapItemVersion
{
	enum
	{
		CURRENT_VERSION = 1
	};

	int m_Version;
};

struct CEnvPoint
{
	int m_Time; // in ms
	int m_Curvetype;
	int m_aValues[4]; // 1-4 depending on envelope (22.10 fixed point)

	bool operator<(const CEnvPoint& Other) const { return m_Time < Other.m_Time; }
};

struct CMapItemEnvelope_v1
{
	int m_Version;
	int m_Channels;
	int m_StartPoint;
	int m_NumPoints;
	int m_aName[8];
};

struct CMapItemEnvelope : public CMapItemEnvelope_v1
{
	enum
	{
		CURRENT_VERSION = 2
	};
	int m_Synchronized;
};

struct CSoundShape
{
	enum
	{
		SHAPE_RECTANGLE = 0,
		SHAPE_CIRCLE,
		NUM_SHAPES,
	};

	struct CRectangle
	{
		int m_Width, m_Height; // fxp 22.10
	};

	struct CCircle
	{
		int m_Radius;
	};

	int m_Type;

	union
	{
		CRectangle m_Rectangle;
		CCircle m_Circle;
	};
};

struct CSoundSource
{
	CPoint m_Position;
	int m_Loop;
	int m_Pan; // 0 - no panning, 1 - panning
	int m_TimeDelay; // in s
	int m_Falloff; // [0,255] // 0 - No falloff, 255 - full

	int m_PosEnv;
	int m_PosEnvOffset;
	int m_SoundEnv;
	int m_SoundEnvOffset;

	CSoundShape m_Shape;
};

struct CMapItemLayerSounds
{
	enum
	{
		CURRENT_VERSION = 2
	};

	CMapItemLayer m_Layer;
	int m_Version;

	int m_NumSources;
	int m_Data;
	int m_Sound;

	int m_aName[3];
};

struct CMapItemSound
{
	int m_Version;

	int m_External;

	int m_SoundName;
	int m_SoundData;
	int m_SoundDataSize;
};

#endif
