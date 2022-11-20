/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <any>
#ifndef GAME_ENUM_CONTEXT_H
#define GAME_ENUM_CONTEXT_H

#define GRAY_COLOR vec3(40, 42, 45)
#define LIGHT_GRAY_COLOR vec3(15, 15, 16)
#define SMALL_LIGHT_GRAY_COLOR vec3(10, 11, 11)
#define GOLDEN_COLOR vec3(35, 20, 2)
#define LIGHT_GOLDEN_COLOR vec3(16, 7, 0)
#define RED_COLOR vec3(40, 15, 15)
#define LIGHT_RED_COLOR vec3(16, 7, 5)
#define SMALL_LIGHT_RED_COLOR vec3(10, 5, 3)
#define BLUE_COLOR vec3(10, 22, 40)
#define LIGHT_BLUE_COLOR vec3(2, 7, 16)
#define PURPLE_COLOR vec3(32, 10, 40)
#define LIGHT_PURPLE_COLOR vec3(16, 5, 20)
#define GREEN_COLOR vec3(15, 40, 15)
#define LIGHT_GREEN_COLOR vec3(0, 16, 0)


// mood type
enum class Mood : short
{
	NORMAL = 0,
	ANGRY,
	AGRESSED,
	FRIENDLY,
	QUEST,
	TANK,
};

// jobs
enum
{
	JOB_LEVEL = 0,
	JOB_EXPERIENCE = 1,
	JOB_UPGRADES = 2,
	NUM_JOB_ACCOUNTS_STATS,
};

// player ticks
enum TickState
{
	Die = 1,
	Respawn,
	LastKill,
	LastEmote,
	LastChangeInfo,
	LastChat,
	LastVoteTry,
	LastDialog,
	LastRandomBox,
	PotionRecast,
	NUM_TICK,
};

enum Skill
{
	SkillHeartTurret = 1,	// health recovery turret
	SkillSleepyGravity = 2, // mobbing
	SkillCraftDiscount = 3, // discount on crafting
	SkillMasterWeapon = 4, // automatic gunfire
	SkillBlessingGodWar = 5, // refill ammunition
	SkillAttackTeleport = 6, // ?knockout? teleport
	SkillCureI = 7, // health recovery cure
};

enum ToplistTypes
{
	GUILDS_LEVELING,
	GUILDS_WEALTHY,
	PLAYERS_LEVELING,
	PLAYERS_WEALTHY,
};

// item functionals
enum ItemFunctional : int
{
	EQUIP_HAMMER = 0,
	EQUIP_GUN,
	EQUIP_SHOTGUN,
	EQUIP_GRENADE,
	EQUIP_LASER,
	EQUIP_PICKAXE,
	EQUIP_RAKE,
	EQUIP_ARMOR,
	EQUIP_EIDOLON,
	NUM_EQUIPPED,

	FUNCTION_ONE_USED = 9,
	FUNCTION_USED,
	FUNCTION_SETTINGS,
	FUNCTION_PLANTS,
	FUNCTION_MINER,
	NUM_FUNCTIONS,
};

enum class ItemType : short
{
	TYPE_INVISIBLE = 0,
	TYPE_USED,
	TYPE_CRAFT,
	TYPE_MODULE,
	TYPE_OTHER,
	TYPE_SETTINGS,
	TYPE_EQUIP,
	TYPE_DECORATION,
	TYPE_POTION,
	NUM_TYPES,
};

enum class QuestState : int
{
	NO_ACCEPT = 0,
	ACCEPT,
	FINISHED,
};

enum FunctionsNPC
{
	FUNCTION_NPC_NURSE,
	FUNCTION_NPC_GIVE_QUEST,
};

enum MenuList
{
	CUSTOM_MENU = -1,
	MENU_MAIN = 1,
	MENU_INVENTORY,
	MENU_INBOX,
	MENU_UPGRADES,
	MENU_SETTINGS,
	MENU_GUIDE_GRINDING,
	MENU_EQUIPMENT,
	MENU_GUILD,
	MENU_GUILD_VIEW_PLAYERS,
	MENU_GUILD_RANK,
	MENU_GUILD_INVITES,
	MENU_GUILD_HISTORY,
	MENU_GUILD_FINDER,
	MENU_GUILD_FINDER_VIEW_PLAYERS,
	MENU_HOUSE,
	MENU_HOUSE_DECORATION,
	MENU_GUILD_HOUSE_DECORATION,
	MENU_HOUSE_PLANTS,
	MENU_JOURNAL_MAIN,
	MENU_JOURNAL_FINISHED,
	MENU_JOURNAL_QUEST_INFORMATION,
	MENU_TOP_LIST,
	MENU_DUNGEONS,
	MENU_AUCTION_CREATE_SLOT,
	MENU_SELECT_LANGUAGE,
};

enum TabHideList
{
	TAB_STAT = 1,
	TAB_PERSONAL,
	TAB_INFORMATION,
	TAB_HOUSE_COMMAND,
	TAB_EQUIP_SELECT,
	TAB_UPGR_DPS,
	TAB_UPGR_TANK,
	TAB_UPGR_HEALER,
	TAB_UPGR_WEAPON,
	TAB_INVENTORY_SELECT,
	TAB_UPGR_JOB,
	TAB_GUILD_STAT,
	TAB_STORAGE,
	TAB_HOUSE_STAT,
	TAB_AETHER,
	TAB_LANGUAGES,
	TAB_SETTINGS,
	TAB_SETTINGS_MODULES,
	NUM_TAB_MENU_INTERACTIVES,

	// start info
	TAB_INFO_INVENTORY,
	TAB_INFO_HOUSE,
	TAB_INFO_STAT,
	TAB_INFO_CRAFT,
	TAB_INFO_TOP,
	TAB_INFO_DUNGEON,
	TAB_INFO_UPGR,
	TAB_INFO_DECORATION,
	TAB_INFO_HOUSE_PLANT,
	TAB_INFO_GUILD_HOUSE,
	TAB_INFO_LOOT,
	TAB_INFO_SKILL,
	TAB_INFO_LANGUAGES,
	TAB_INFO_AUCTION,
	TAB_INFO_AUCTION_BIND,
	TAB_INFO_EQUIP,
	NUM_TAB_MENU,
};

// primary
enum
{
	/*
		Basic kernel server settings
		This is where the most basic server settings are stored
	*/
	MIN_SKINCHANGE_CLIENTVERSION = 0x0703,	// minimum client version for skin change
	MIN_RACE_CLIENTVERSION = 0x0704,		// minimum client version for race type
	MAX_INBOX_LIST = 30,					// maximum number of emails what is displayed
	STATS_MAX_FOR_ITEM = 2,					// maximum number of stats per item
	POTION_RECAST_APPEND_TIME = 15,			// recast append time for potion in seconds

	// settings items
	itModePVP = 22,						// PVP mode setting
	itShowEquipmentDescription = 25,	// Description setting
	itShowCriticalDamage = 34,			// Critical damage setting

	// items
	NOPE = -1,
	itGold = 1,							// Money ordinary currency
	itHammer = 2,						// Equipment Hammers
	itMaterial = 7,						// Scraping material
	itTicketGuild = 8,					// Ticket for the creation of the guild
	itSkillPoint = 9,					// Skillpoint
	itDecoArmor = 10,					// Shield Decoration
	itEliteDecoHealth = 11,				// Elite Heart Decoration
	itEliteDecoNinja = 12,				// Elite Ninja Decoration
	itDecoHealth = 13,					// Decoration Heart
	itPotionManaRegen = 14,				// Mana regeneration potion
	itTinyHealthPotion = 15,			// Tiny health potion
	itCapsuleSurvivalExperience = 16,	// Gives 10-50 experience
	itLittleBagGold = 17,				// Gives 10-50 gold
	itPotionResurrection = 18,			// Resurrection potion
	itExplosiveGun = 19,				// Explosion for gun
	itExplosiveShotgun = 20,			// Explosion for shotgun
	itTicketResetClassStats = 21,		// Ticket to reset the statistics of class upgrades
	itTicketResetWeaponStats = 23,		// Ticket to reset the statistics cartridge upgrade
	itTicketDiscountCraft = 24,			// Discount ticket for crafting
	itRandomHomeDecoration = 26,		// Random home decor
	itEidolonOtohime = 57,				// Eidolon
	itRandomRelicsBox = 58,				// Random Relics box
	itEidolonMerrilee = 59,				// Eidolon
	itPoisonHook = 64,					// Poison hook
	itExplodeImpulseHook = 65,			// Explode hook
	itSpiderHook = 66,					// Spider hook

	// all sorting sheets that exist on the server
	SORT_INVENTORY = 0,
	SORT_EQUIPING,
	SORT_GUIDE_WORLD,
	SORT_TOP,
	NUM_SORT_TAB,

	// type of decorations
	DECORATIONS_HOUSE = 0,
	DECORATIONS_GUILD_HOUSE,

	// bot dialogues
	IS_TALKING_EMPTY = 999,

	// max mails for page
	MAILLETTER_MAX_CAPACITY = 30,
};

// todo use template class 
class PotionTools
{
public:
	class Heal
	{
		int m_ItemID{};
		std::string m_Effect{};
		int m_Recovery{};
		int m_Time{};

	public:
		Heal() = delete;
		Heal(int ItemID, std::string Effect, int Recovery, int Time) : m_ItemID(ItemID), m_Effect(Effect), m_Recovery(Recovery), m_Time(Time) {}

		static const Heal* getHealInfo(int ItemID)
		{
			auto p = std::find_if(m_PotionHealthInfo.begin(), m_PotionHealthInfo.end(), [ItemID](const Heal& p){ return p.m_ItemID == ItemID; });
			return p != m_PotionHealthInfo.end() ? p : nullptr;
		}
		static std::initializer_list<Heal>& getList() { return m_PotionHealthInfo; }

		int getItemID() const { return m_ItemID; }
		const char* getEffect() const { return m_Effect.c_str(); }
		int getRecovery() const { return m_Recovery; }
		int getTime() const { return m_Time; }
	};

private:
	inline static std::initializer_list<Heal> m_PotionHealthInfo
	{
		{ itTinyHealthPotion, "TinyHP", 7, 15 }
	};
};



enum GuildAccess
{
	ACCESS_LEADER = -1,
	ACCESS_NO,
	ACCESS_INVITE_KICK,
	ACCESS_UPGRADE_HOUSE,
	ACCESS_FULL,
};

enum class BroadcastPriority
{
	LOWER,
	GAME_BASIC_STATS,
	GAME_INFORMATION,
	GAME_PRIORITY,
	GAME_WARNING,
	MAIN_INFORMATION,
	VERY_IMPORTANT,
};

enum SpawnTypes
{
	SPAWN_HUMAN = 0,
	SPAWN_BOT = 1,
	SPAWN_HUMAN_SAFE = 2,
	SPAWN_NUM
};

enum BotsTypes
{
	TYPE_BOT_MOB = 1,
	TYPE_BOT_QUEST = 2,
	TYPE_BOT_NPC = 3,
	TYPE_BOT_FAKE = 4,
	TYPE_BOT_EIDOLON = 5,
};

enum
{
	SNAPPLAYER = 1,
	SNAPBOTS = 2,
};

enum SaveType
{
	SAVE_ACCOUNT,			// Save Login Password Data
	SAVE_STATS,				// Save Stats Level Exp and other this type
	SAVE_UPGRADES,			// Save Upgrades Damage and other this type
	SAVE_PLANT_DATA,		// Save Plant Account
	SAVE_MINER_DATA,		// Save Mining Account
	SAVE_GUILD_DATA,		// Save Guild Data
	SAVE_POSITION,			// Save Position Player
	SAVE_LANGUAGE,			// Save Language Client
};

enum DayType
{
	NIGHT_TYPE = 1,
	DAY_TYPE,
	MORNING_TYPE,
	EVENING_TYPE
};

/*
	Basic kernel server settings
	This is where the most basic server settings are stored
*/
enum
{
	MAX_DROPPED_FROM_MOBS = 5,  // maximum number of items that can be dropped by mobs
};

enum CDataList
{
	MMO_DATA_INVENTORY_INFORMATION = 0,
};

// skin data
struct CTeeInfo
{
	char m_aSkinName[64];
	int m_UseCustomColor;
	int m_ColorBody;
	int m_ColorFeet;
};

// attribute context
enum class AttributeIdentifier : int
{
	SpreadShotgun			= 1,
	SpreadGrenade			= 2,
	SpreadRifle				= 3,
    Strength				= 4,
	Dexterity				= 5,
 	CriticalHit				= 6,
	DirectCriticalHit		= 7,
    Hardness				= 8,
	Lucky					= 9,
	Piety					= 10,
	Vampirism				= 11,
	AmmoRegen				= 12,
	Ammo					= 13,
	Efficiency				= 14,
	Extraction				= 15,
	HammerPower				= 16,
	GunPower				= 17,
	ShotgunPower			= 18,
	GrenadePower			= 19,
	RiflePower				= 20,
	LuckyDropItem			= 21,
	Eidolon					= 22,
	ATTRIBUTES_NUM,
};

enum class AttributeType : int
{
	Tank,
	Healer,
	Dps,
	Weapon,
	Hardtype,
	Job,
	Other,
};

// helpers
class EidolonsTools
{
	inline static std::initializer_list< std::pair < int, int > > m_Eidons
	{
		{ itEidolonOtohime, 45 },
		{ itEidolonMerrilee, 46 }
	};

public:
	static int getEidolonBot(int ItemID)
	{
		auto Iter = std::find_if(m_Eidons.begin(), m_Eidons.end(), [ItemID](const std::pair < int, int >& p) { return p.first == ItemID; });
		return Iter != m_Eidons.end() ? (*Iter).second : -1;
	}
	static int getEidolonItemID(int BotID)
	{
		auto Iter = std::find_if(m_Eidons.begin(), m_Eidons.end(), [BotID](const std::pair < int, int >& p) { return p.second == BotID; });
		return Iter != m_Eidons.end() ? (*Iter).first : -1;
	}
};

class JsonTools
{
public:
	static void parseFromString(const std::string& Data, const std::function<void(nlohmann::json& pJson)>& pFuncCallback)
	{
		try
		{
			if(!Data.empty())
			{
				nlohmann::json JsonData = nlohmann::json::parse(Data);
				pFuncCallback(JsonData);
			}
		}
		catch(nlohmann::json::exception& s)
		{
			dbg_msg("dialog error", "%s", s.what());
		}
	}
};

class Instance
{
	friend class CServer;
	inline static class IServer* m_pServer{};

public:
	static IServer* GetServer() { return m_pServer; }
};

class _StoreMultiworldIdentifiableStaticData
{
	inline static class IServer* m_pServer {};

public:
	class IServer* Server() const { return m_pServer; }
	static void Init(IServer* pServer) { m_pServer = pServer; }
};

template < typename T >
class MultiworldIdentifiableStaticData : public _StoreMultiworldIdentifiableStaticData
{
protected:
	inline static T m_pData {};

public:
	static T& Data() { return m_pData; }
};

#endif
