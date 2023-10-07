/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
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

// laser orbite
enum class EntLaserOrbiteType : unsigned char
{
	DEFAULT, // Default value
	MOVE_LEFT, // Move left value
	MOVE_RIGHT, // Move right value
};

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
enum JobAccountStats
{
	JOB_LEVEL = 0,
	JOB_EXPERIENCE = 1,
	JOB_UPGRADES = 2,
	NUM_JOB_ACCOUNTS_STATS
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
	RefreshClanTitle,
	NUM_TICK,
};

// skills
enum Skill
{
	SkillHeartTurret = 1,	// health recovery turret
	SkillSleepyGravity = 2, // mobbing
	SkillCraftDiscount = 3, // discount on crafting
	SkillMasterWeapon = 4, // automatic gunfire
	SkillBlessingGodWar = 5, // refill ammunition
	SkillAttackTeleport = 6, // ?knockout? teleport
	SkillCureI = 7, // health recovery cure
	SkillProvoke = 8, // provoke
};

// toplist types
enum class ToplistType : int
{
	GUILDS_LEVELING,
	GUILDS_WEALTHY,
	PLAYERS_LEVELING,
	PLAYERS_WEALTHY,
	NUM_TOPLIST_TYPES
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

// item types
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

// quest state
enum class QuestState : int
{
	NO_ACCEPT = 0,
	ACCEPT,
	FINISHED,
};

// npc functions
enum FunctionsNPC
{
	FUNCTION_NPC_NURSE,
	FUNCTION_NPC_GIVE_QUEST,
};

// menu list
enum MenuList
{
	CUSTOM_MENU = -1,
	MENU_MAIN = 1,

	// Main menu options
	MENU_EQUIPMENT,
	MENU_INVENTORY,
	MENU_INBOX,
	MENU_UPGRADES,
	MENU_SETTINGS,

	// Guild-related menus
	MENU_GUILD,
	MENU_GUILD_VIEW_PLAYERS,
	MENU_GUILD_RANK,
	MENU_GUILD_INVITES,
	MENU_GUILD_HISTORY,
	MENU_GUILD_FINDER,
	MENU_GUILD_FINDER_VIEW_PLAYERS,
	MENU_GUILD_HOUSE_DECORATION,

	// House-related menus
	MENU_HOUSE,
	MENU_HOUSE_DECORATION,
	MENU_HOUSE_PLANTS,
	MENU_HOUSE_ACCESS_TO_DOOR,

	// Eidolon Collection menus
	MENU_EIDOLON_COLLECTION,
	MENU_EIDOLON_COLLECTION_SELECTED,

	// Journal menus
	MENU_JOURNAL_MAIN,
	MENU_JOURNAL_FINISHED,
	MENU_JOURNAL_QUEST_INFORMATION,

	// Other menus
	MENU_GUIDE_GRINDING,
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
	TAB_HOUSE_MANAGING,
	TAB_HOUSE_SAFE_INTERACTIVE,
	TAB_HOUSE_ACCESS_TO_DOOR_REMOVE,
	TAB_HOUSE_ACCESS_TO_DOOR_ADD,
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
	TAB_EIDOLONS,
	TAB_EIDOLON_DESCRIPTION,
	TAB_EIDOLON_UNLOCKING_ENHANCEMENTS,
	TAB_LANGUAGES,
	TAB_SETTINGS,
	TAB_SETTINGS_MODULES,
	NUM_TAB_MENU_INTERACTIVES,

	// start info
	TAB_INFO_INVENTORY,
	TAB_INFO_HOUSE,
	TAB_INFO_HOUSE_INVITES_TO_DOOR,
	TAB_INFO_STAT,
	TAB_INFO_CRAFT,
	TAB_INFO_TOP,
	TAB_INFO_DUNGEON,
	TAB_INFO_UPGR,
	TAB_INFO_DECORATION,
	TAB_INFO_HOUSE_PLANT,
	TAB_INFO_GUILD_HOUSE,
	TAB_INFO_EIDOLONS,
	TAB_INFO_LOOT,
	TAB_INFO_SKILL,
	TAB_INFO_LANGUAGES,
	TAB_INFO_AUCTION,
	TAB_INFO_AUCTION_BIND,
	TAB_INFO_EQUIP,
	TAB_INFO_STATISTIC_QUESTS,
	NUM_TAB_MENU,
};

// primary
enum
{
	/*
		Basic kernel server settings
		This is where the most basic server settings are stored
	*/
	MAX_HOUSE_INVITED_PLAYERS = 3,			// maximum player what can have access for house door
	MAX_DECORATIONS_HOUSE = 20,				// maximum decorations for houses
	MIN_SKINCHANGE_CLIENTVERSION = 0x0703,	// minimum client version for skin change
	MIN_RACE_CLIENTVERSION = 0x0704,		// minimum client version for race type
	MAX_INBOX_LIST = 30,					// maximum number of emails what is displayed
	STATS_MAX_FOR_ITEM = 2,					// maximum number of stats per item
	POTION_RECAST_APPEND_TIME = 15,			// recast append time for potion in seconds

	// settings items
	itModePVP = 22,						// PVP mode setting
	itShowEquipmentDescription = 25,	// Description setting
	itShowCriticalDamage = 34,			// Critical damage setting
	itShowQuestNavigator = 93,			// Show quest path when idle

	// items
	NOPE = -1,
	itGold = 1,							// Money ordinary currency
	itHammer = 2,						// Equipment Hammers
	itMaterial = 7,						// Scraping material
	itTicketGuild = 8,					// Ticket for the creation of the guild
	itSkillPoint = 9,					// Skillpoint
	itDecoArmor = 10,					// Shield Decoration
	itEliteDecoHeart = 11,				// Elite Heart Decoration
	itEliteDecoNinja = 12,				// Elite Ninja Decoration
	itDecoHeart = 13,					// Decoration Heart
	itPotionManaRegen = 14,				// Mana regeneration potion
	itTinyHealthPotion = 15,			// Tiny health potion
	itCapsuleSurvivalExperience = 16,	// Gives 10-50 experience
	itLittleBagGold = 17,				// Gives 10-50 gold
	itPotionResurrection = 18,			// Resurrection potion :: UNUSED
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
	itEidolonDryad = 80,				// Eidolon
	itEidolonPigQueen = 88,				// Eidolon
	itAdventurersBadge = 92,			// The adventurer's badge

	// all sorting sheets that exist on the server
	SORT_INVENTORY = 0,
	SORT_EQUIPING,
	SORT_GUIDE_WORLD,
	SORT_TOP,
	NUM_SORT_TAB,

	// type of decorations
	DECORATIONS_HOUSE = 0,
	DECORATIONS_GUILD_HOUSE,

	// max mails for page
	MAILLETTER_MAX_CAPACITY = 30,
};

// todo use template class 
class PotionTools
{
public:
	class Heal
	{
		int m_ItemID {};
		std::string m_Effect {};
		int m_Recovery {};
		int m_Time {};

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

// access guild
enum GuildAccess
{
	ACCESS_LEADER = -1,      // Leader has full control over the guild
	ACCESS_NO,               // No access to guild functions
	ACCESS_INVITE_KICK,      // Can invite or kick members but not upgrade the guild house
	ACCESS_UPGRADE_HOUSE,    // Can upgrade the guild house but not invite or kick members
	ACCESS_FULL              // Full access to all guild functions
};

// broadcast priority
enum class BroadcastPriority
{
	LOWER,
	GAME_BASIC_STATS,
	GAME_INFORMATION,
	GAME_PRIORITY,
	GAME_WARNING,
	MAIN_INFORMATION,
	TITLE_INFORMATION,
	VERY_IMPORTANT,
};

// spawn types
enum SpawnTypes
{
	SPAWN_HUMAN = 0,        // Spawn a human player
	SPAWN_BOT = 1,          // Spawn a bot player
	SPAWN_HUMAN_SAFE = 2,   // Spawn a human player in a safe location
	SPAWN_NUM               // The total number of spawn types available
};

// bot types
enum BotsTypes
{
	TYPE_BOT_MOB = 1,       // type for mob bots
	TYPE_BOT_QUEST = 2,     // type for quest bots
	TYPE_BOT_NPC = 3,       // type for NPC bots
	TYPE_BOT_FAKE = 4,      // type for fake bots
	TYPE_BOT_EIDOLON = 5,   // type for eidolon bots
};

enum
{
	SNAPPLAYER = 1,
	SNAPBOTS = 2,
};

// save types
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

// world day types
enum DayType
{
	NIGHT_TYPE = 0,
	MORNING_TYPE = 1,
	DAY_TYPE = 2,
	EVENING_TYPE = 3
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
	char m_aSkinName[64]; // name of the skin
	int m_UseCustomColor; // flag indicating whether custom colors are used
	int m_ColorBody; // color value for the body part
	int m_ColorFeet; // color value for the feet part
};

// Attribute context
enum class AttributeIdentifier : int
{
	SpreadShotgun = 1, // Attribute identifier for spread shotgun
	SpreadGrenade = 2, // Attribute identifier for spread grenade
	SpreadRifle = 3, // Attribute identifier for spread rifle
	DMG = 4, // Attribute identifier for damage
	AttackSPD = 5, // Attribute identifier for attack speed
	CritDMG = 6, // Attribute identifier for critical damage
	Crit = 7, // Attribute identifier for critical chance
	HP = 8, // Attribute identifier for health points
	Lucky = 9, // Attribute identifier for luck
	MP = 10, // Attribute identifier for mana points
	Vampirism = 11, // Attribute identifier for vampirism
	AmmoRegen = 12, // Attribute identifier for ammo regeneration
	Ammo = 13, // Attribute identifier for ammo
	Efficiency = 14, // Attribute identifier for efficiency
	Extraction = 15, // Attribute identifier for extraction
	HammerDMG = 16, // Attribute identifier for hammer damage
	GunDMG = 17, // Attribute identifier for gun damage
	ShotgunDMG = 18, // Attribute identifier for shotgun damage
	GrenadeDMG = 19, // Attribute identifier for grenade damage
	RifleDMG = 20, // Attribute identifier for rifle damage
	LuckyDropItem = 21, // Attribute identifier for lucky drop item
	EidolonPWR = 22, // Attribute identifier for eidolon power
	ATTRIBUTES_NUM, // The number of total attributes
};

// Enum class declaration for different attribute types
enum class AttributeType : int
{
	Tank,      // Tank attribute
	Healer,    // Healer attribute
	Dps,       // Damage Per Second attribute
	Weapon,    // Weapon attribute
	Hardtype,  // Hard type attribute
	Job,       // Job attribute
	Other,     // Other attribute
};

class JsonTools
{
public:
	// Define a static function called parseFromString that takes in a string Data and a callback function pFuncCallback as parameters
	static void parseFromString(const std::string& Data, const std::function<void(nlohmann::json& pJson)>& pFuncCallback)
	{
		// Check data empty
		if(!Data.empty())
		{
			try
			{
				// Parse the input string into a nlohmann::json object called JsonData
				nlohmann::json JsonData = nlohmann::json::parse(Data);

				// Call the callback function with JsonData as the parameter
				pFuncCallback(JsonData);
			}
			// Catch any exceptions thrown during the parsing process and handle them
			catch(nlohmann::json::exception& s)
			{
				// Output the error message to the debug log
				dbg_msg("dialog error", "%s", s.what());
			}
		}
	}
};

class Instance
{
	// Declare CServer as a friend of Instance class
	friend class CServer;

	// Declare a static member variable m_pServer as  pointer to IServer class
	inline static class IServer* m_pServer {};

public:
	// Define a static member function GetServer that returns a pointer to IServer object
	static IServer* GetServer() { return m_pServer; }
};

// This class is a template class that stores static data shared across multiple worlds.
// It is intended to be inherited by other classes.
class _StoreMultiworldIdentifiableStaticData
{
	// This pointer stores the instance of the server class.
	// It is declared as inline static to allow for its initialization outside of the class.
	inline static class IServer* m_pServer {};

public:
	// This method returns the instance of the server class.
	class IServer* Server() const { return m_pServer; }

	// This method initializes the instance of the server class.
	static void Init(IServer* pServer) { m_pServer = pServer; }
};

template < typename T >
class MultiworldIdentifiableStaticData : public _StoreMultiworldIdentifiableStaticData
{
protected:
	// This static pointer stores the shared static data.
	// It is declared as inline static to allow for its initialization outside of the class.
	static inline T m_pData {};

public:
	// This method returns the shared static data.
	static T& Data() { return m_pData; }
};

#endif
