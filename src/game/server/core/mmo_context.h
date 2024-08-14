/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_MMO_CONTEXT_H
#define GAME_SERVER_CORE_MMO_CONTEXT_H

#include "tools/path_finder_result.h"

// skin data
struct CTeeInfo
{
	char m_aSkinName[64]; // name of the skin
	int m_UseCustomColor; // flag indicating whether custom colors are used
	int m_ColorBody; // color value for the body part
	int m_ColorFeet; // color value for the feet part
};

// world types
enum class WorldType : int
{
	Default,
	Dungeon,
	Tutorial,
};

// drawboard events
enum class DrawboardToolEvent : int
{
	ON_START,
	ON_POINT_ADD,
	ON_POINT_ERASE,
	ON_END,
};

// class groups
enum class ClassGroup : int
{
	None,
	Tank,
	Dps,
	Healer
};

// time period
enum ETimePeriod
{
	DAILY_STAMP,
	WEEK_STAMP,
	MONTH_STAMP,
	NUM_STAMPS
};

// laser orbite types
enum class LaserOrbiteType : unsigned char
{
	DEFAULT,
	MOVE_LEFT,
	MOVE_RIGHT,
	INSIDE_ORBITE,
	INSIDE_ORBITE_RANDOM,
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

// account harvesting stats
enum AccountHarvestingStats
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
	LastSelfKill,
	LastEmote,
	LastChangeInfo,
	LastChangeTeam,
	LastChat,
	LastVote,
	LastDialog,
	LastRandomBox,
	PotionRecast,
	RefreshClanTitle,
	RefreshNickLeveling,
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
	SkillBleedingBlow = 9, // bleeding blow
	SkillEnergyShield = 10, // energy shield
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
	EQUIP_POTION_HEAL,
	EQUIP_POTION_MANA,
	EQUIP_POTION_SPECIAL,
	EQUIP_TITLE,
	NUM_EQUIPPED,


	FUNCTION_ONE_USED,
	FUNCTION_USED,
	FUNCTION_SETTINGS,
	FUNCTION_FARMING,
	FUNCTION_MINING,
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
	FUNCTION_NPC_GUARDIAN,
};

// menu list
enum MenuList
{
	// Main menu
	MENU_MAIN = 1,
	MENU_EQUIPMENT,
	MENU_INVENTORY,
	MENU_UPGRADES,

	// Settings
	MENU_SETTINGS,
	MENU_SETTINGS_LANGUAGE_SELECT,
	MENU_SETTINGS_TITLE_SELECT,

	// Mailbox
	MENU_MAILBOX,
	MENU_MAILBOX_SELECTED,

	// Guild-related
	MENU_GUILD,
	MENU_GUILD_DISBAND,
	MENU_GUILD_HOUSE_SELL,
	MENU_GUILD_UPGRADES,
	MENU_GUILD_HOUSE_DOOR_LIST,
	MENU_GUILD_MEMBERSHIP_LIST,
	MENU_GUILD_MEMBERSHIP_SELECTED,
	MENU_GUILD_RANK_LIST,
	MENU_GUILD_RANK_SELECTED,
	MENU_GUILD_HOUSE_FARMZONE_LIST,
	MENU_GUILD_HOUSE_FARMZONE_SELECTED,
	MENU_GUILD_INVITES,
	MENU_GUILD_LOGS,
	MENU_GUILD_WARS,
	MENU_GUILD_HOUSE_PURCHASE,
	MENU_GUILD_FINDER,
	MENU_GUILD_FINDER_SELECTED,

	// Achievements-related
	MENU_ACHIEVEMENTS,
	MENU_ACHIEVEMENTS_SELECTED,

	// House-related
	MENU_HOUSE,
	MENU_HOUSE_SELL,
	MENU_HOUSE_DOOR_LIST,
	MENU_HOUSE_ACCESS_TO_DOOR,
	MENU_HOUSE_BUY,
	MENU_HOUSE_FARMZONE_LIST,
	MENU_HOUSE_FARMZONE_SELECTED,

	// Warehouse
	MENU_WAREHOUSE,
	MENU_WAREHOUSE_BUY_ITEM_SELECTED,

	// Eidolon Collection menus
	MENU_EIDOLON_COLLECTION,
	MENU_EIDOLON_COLLECTION_SELECTED,

	// Journal menus
	MENU_JOURNAL_MAIN,
	MENU_JOURNAL_QUEST_SELECTED,

	// Skills
	MENU_SKILL_LIST,
	MENU_SKILL_SELECTED,

	// Quest menus
	MENU_BOARD,
	MENU_BOARD_QUEST_SELECTED,

	// Aethernet menus
	MENU_AETHERNET_LIST,

	// Auction
	MENU_AUCTION_LIST,
	MENU_AUCTION_SLOT_SELECTED,
	MENU_AUCTION_CREATE_SLOT,

	// Group menus
	MENU_GROUP,

	// Grinding
	MENU_GUIDE_GRINDING,
	MENU_GUIDE_GRINDING_SELECT,

	// Crafting
	MENU_CRAFT_LIST,
	MENU_CRAFT_SELECTED,

	// Other menus
	MENU_TOP_LIST,
	MENU_DUNGEONS,

	//
	MOTD_MENU_ABOUT_BONUSES,
	MOTD_MENU_ABOUT_WANTED,
	MOTD_MENU_TEST,
	MOTD_MENU_SUBTEST,
};

// primary
enum
{
	/*
		Basic kernel server settings
		This is where the most basic server settings are stored
	*/
	MAX_GROUP_MEMBERS = 4,					// maximum number of players in a group
	MAX_HOUSE_DOOR_INVITED_PLAYERS = 3,		// maximum player what can have access for house door
	MAX_DECORATIONS_PER_HOUSE = 20,				// maximum decorations for houses
	MIN_SKINCHANGE_CLIENTVERSION = 0x0703,	// minimum client version for skin change
	MIN_RACE_CLIENTVERSION = 0x0704,		// minimum client version for race type
	MAILS_MAX_CAPACITY = 10,			// maximum number of emails what is displayed
	MAX_ATTRIBUTES_FOR_ITEM = 2,			// maximum number of stats per item
	POTION_RECAST_APPEND_TIME = 15,			// recast append time for potion in seconds
	MAX_DAILY_QUESTS_BY_BOARD = 3,			// maximum number of daily quests that can be assigned to a specific board in a game. 

	// settings items
	itShowEquipmentDescription = 25,	// Description setting
	itShowCriticalDamage = 34,			// Critical damage setting
	itShowQuestNavigator = 93,			// Show quest path when idle

	// items
	NOPE = -1,
	itGold = 1,							// Money ordinary currency
	itHammer = 2,						// Equipment Hammers
	itMaterial = 7,						// Scraping material
	itProduct = 8,						// Scraping material
	itSkillPoint = 9,					// Skillpoint
	itAchievementPoint = 10,			// Achievement point
	itPotionManaRegen = 14,				// Mana regeneration potion
	itTinyHealthPotion = 15,			// Tiny health potion
	itCapsuleSurvivalExperience = 16,	// Gives 10-50 experience
	itLittleBagGold = 17,				// Gives 10-50 gold
	itExplosiveGun = 19,				// Explosion for gun
	itExplosiveShotgun = 20,			// Explosion for shotgun
	itTicketResetClassStats = 21,		// Ticket to reset the statistics of class upgrades
	itPermissionExceedLimits = 22,		// Reserve
	itTicketResetWeaponStats = 23,		// Ticket to reset the statistics cartridge upgrade
	itTicketDiscountCraft = 24,			// Discount ticket for crafting
	itRandomHomeDecoration = 26,		// Random home decor
	itAlliedSeals = 30,					// Allied seals
	itEidolonOtohime = 57,				// Eidolon
	itRandomRelicsBox = 58,				// Random Relics box
	itEidolonMerrilee = 59,				// Eidolon
	itPoisonHook = 64,					// Poison hook
	itExplodeHook = 65,					// Explode hook
	itSpiderHook = 66,					// Spider hook
	itEidolonDryad = 80,				// Eidolon
	itEidolonPigQueen = 88,				// Eidolon
	itAdventurersBadge = 92,			// The adventurer's badge
	itTicketGuild = 95,					// Ticket for the creation of the guild
	itCustomizer = 96,                  // Curomizer for personal skins
	itDamageEqualizer = 97,				// Module for dissable self dmg

	// decoration items
	itPickupHealth = 18,				// Pickup heart
	itPickupMana = 13,					// Pickup mana
	itPickupShotgun = 11,				// Pickup shotgun
	itPickupGrenade = 12,				// Pickup grenade
	itPickupLaser = 94,				    // Pickup laser

	// all sorting sheets that exist on the server
	SORT_INVENTORY = 0,
	SORT_EQUIPING,
	SORT_GUIDE_WORLD,
	SORT_TOP,
	NUM_SORT_TAB,

	// type of decorations
	DECORATIONS_HOUSE = 0,
	DECORATIONS_GUILD_HOUSE,
};

// broadcast
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
	SPAWN_HUMAN_TREATMENT = 2,   // Spawn a human player in a safe location
	SPAWN_HUMAN_PRISON = 3, // Spawn a human prison
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
	TYPE_BOT_QUEST_MOB = 6, // type for quest mob bots
};

// save types
enum SaveType
{
	SAVE_ACCOUNT,			// Save Login Password Data
	SAVE_STATS,				// Save Stats Level Exp and other this type
	SAVE_UPGRADES,			// Save Upgrades Damage and other this type
	SAVE_FARMING_DATA,		// Save Farming Account
	SAVE_MINING_DATA,		// Save Mining Account
	SAVE_GUILD_DATA,		// Save Guild Data
	SAVE_SOCIAL_STATUS,		// Save Social status
	SAVE_POSITION,			// Save Position Player
	SAVE_LANGUAGE,			// Save Language Client
	SAVE_TIME_PERIODS,		// Save Time Periods
	SAVE_ACHIEVEMENTS,		// Save Achievements
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

// Enum class declaration for different attribute types
enum class AttributeGroup : int
{
	Tank,      // Tank attribute
	Healer,    // Healer attribute
	Dps,       // Damage Per Second attribute
	Weapon,    // Weapon attribute
	Hardtype,  // Hard type attribute
	Job,       // Job attribute
	Other,     // Other attribute
};

// Attribute context
enum class AttributeIdentifier : int
{
	SpreadShotgun = 1,           // Attribute identifier for spread shotgun
	SpreadGrenade = 2,           // Attribute identifier for spread grenade
	SpreadRifle = 3,             // Attribute identifier for spread rifle
	DMG = 4,                     // Attribute identifier for damage
	AttackSPD = 5,               // Attribute identifier for attack speed
	CritDMG = 6,                 // Attribute identifier for critical damage
	Crit = 7,                    // Attribute identifier for critical chance
	HP = 8,                      // Attribute identifier for health points
	Lucky = 9,                   // Attribute identifier for luck
	MP = 10,                     // Attribute identifier for mana points
	Vampirism = 11,              // Attribute identifier for vampirism
	AmmoRegen = 12,              // Attribute identifier for ammo regeneration
	Ammo = 13,                   // Attribute identifier for ammo
	Efficiency = 14,             // Attribute identifier for efficiency
	Extraction = 15,             // Attribute identifier for extraction
	HammerDMG = 16,              // Attribute identifier for hammer damage
	GunDMG = 17,                 // Attribute identifier for gun damage
	ShotgunDMG = 18,             // Attribute identifier for shotgun damage
	GrenadeDMG = 19,             // Attribute identifier for grenade damage
	RifleDMG = 20,               // Attribute identifier for rifle damage
	LuckyDropItem = 21,          // Attribute identifier for lucky drop item
	EidolonPWR = 22,             // Attribute identifier for eidolon power
	ATTRIBUTES_NUM,              // The number of total attributes
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
			auto p = std::ranges::find_if(m_PotionHealthInfo, [ItemID](const Heal& p){ return p.m_ItemID == ItemID; });
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

#endif
