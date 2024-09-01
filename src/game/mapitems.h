/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_MAPITEMS_H
#define GAME_MAPITEMS_H

// layer types
enum
{
	LAYERTYPE_INVALID=0,
	LAYERTYPE_GAME,
	LAYERTYPE_TILES,
	LAYERTYPE_QUADS,

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
	ENTITY_NPC_WALL_UP,
	ENTITY_NPC_WALL_LEFT,
	ENTITY_MOB_WALL_UP,
	ENTITY_MOB_WALL_LEFT,
	ENTITY_FARMING,
	ENTITY_MINING,
	ENTITY_SPAWN = 17,
	ENTITY_SPAWN_MOBS,
	ENTITY_SPAWN_SAFE,
	ENTITY_SPAWN_PRISON,
	NUM_ENTITIES,

	TILE_AIR = 0,
	TILE_SOLID,
	TILE_DEATH,
	TILE_NOHOOK,
	TILE_ANTI_PVP,
	TILE_GUILD_HOUSE = 16,
	TILE_AUCTION,
	TILE_SKILL_ZONE,
	TILE_PLAYER_HOUSE,
	TILE_CRAFT_ZONE,
	TILE_QUEST_BOARD,
	TILE_AETHER_TELEPORT,
	TILE_WORLD_SWAP,
	TILE_SHOP_ZONE,
	TILE_TELE_IN = 26,
	TILE_TELE_OUT,
	TILE_TELE_CONFIRM_OUT = 30,
	TILE_GUILD_CHAIR = 32,
	TILE_CHAIR,
	TILE_WATER = 41,
	TILE_INVISIBLE_WALL,
	TILE_INFO_BONUSES,
	TILE_INFO_WANTED,
	TILE_BANK_MANAGER,

	TILE_CLEAR_SPECIAL_EVENTS = 48,
	TILE_SPECIAL_EVENT_PARTY,
	TILE_SPECIAL_EVENT_LIKE,
	TILE_SPECIAL_EVENT_HEALTH,
	MAX_TILES,

	TILEFLAG_VFLIP=1,
	TILEFLAG_HFLIP=2,
	TILEFLAG_OPAQUE=4,
	TILEFLAG_ROTATE=8,

	LAYERFLAG_DETAIL=1,
	TILESLAYERFLAG_GAME=1,
	TILESLAYERFLAG_TELE = 2,
	TILESLAYERFLAG_FRONT = 8,

	ENTITY_OFFSET=255-16*4,
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
	unsigned char m_Reserved;
};

class CTeleTile
{
public:
	unsigned char m_Number;
	unsigned char m_Type;
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
