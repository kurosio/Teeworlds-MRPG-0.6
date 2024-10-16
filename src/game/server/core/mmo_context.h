﻿/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_MMO_CONTEXT_H
#define GAME_SERVER_CORE_MMO_CONTEXT_H

#include "tools/path_finder_result.h"

// special sounds
enum ESpecialSound
{
	SOUND_NOPE = -1,
	SOUND_VOTE_SELECT = SOUND_MENU + 1,
	SOUND_ITEM_EQUIP,
	SOUND_ITEM_SELL_BUY,
	SOUND_USE_POTION,
};

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
enum EAccountHarvestingStats
{
	JOB_LEVEL = 0,
	JOB_EXPERIENCE = 1,
	JOB_UPGRADES = 2,
	NUM_JOB_ACCOUNTS_STATS
};

// player ticks
enum ETickState
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
enum ESkill
{
	SkillHeartTurret = 1,	// health recovery turret
	SkillSleepyGravity = 2, // mobbing
	SkillCraftDiscount = 3, // discount on crafting
	SkillMasterWeapon = 4, // automatic gunfire
	SkillBlessingGodWar = 5, // refill ammunition
	SkillAttackTeleport = 6, // ?knockout? teleport
	SkillCureI = 7, // health recovery cure
	SkillProvoke = 8, // provoke
	SkillEnergyShield = 9, // energy shield
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
enum EFunctionsNPC
{
	FUNCTION_NPC_NURSE,
	FUNCTION_NPC_GIVE_QUEST,
	FUNCTION_NPC_GUARDIAN,
};

// menu list
enum EMenuList
{
	// Main menu
	MENU_MAIN = 1,
	MENU_ACCOUNT_INFO,
	MENU_EQUIPMENT,
	MENU_INVENTORY,
	MENU_UPGRADES,

	// Settings
	MENU_SETTINGS,
	MENU_SETTINGS_ACCOUNT,
	MENU_SETTINGS_LANGUAGE,
	MENU_SETTINGS_TITLE,

	// Mailbox
	MENU_MAILBOX,
	MENU_MAILBOX_SELECT,

	// Guild-related
	MENU_GUILD,
	MENU_GUILD_DISBAND,
	MENU_GUILD_UPGRADES,
	MENU_GUILD_MEMBER_LIST,
	MENU_GUILD_MEMBER_SELECT,
	MENU_GUILD_RANK_LIST,
	MENU_GUILD_RANK_SELECT,
	MENU_GUILD_INVITATIONS,
	MENU_GUILD_LOGS,
	MENU_GUILD_WARS,
	MENU_GUILD_FINDER,
	MENU_GUILD_FINDER_SELECT,

	MENU_GUILD_SELL_HOUSE,
	MENU_GUILD_BUY_HOUSE,
	MENU_GUILD_HOUSE_FARMZONE_LIST,
	MENU_GUILD_HOUSE_FARMZONE_SELECT,
	MENU_GUILD_HOUSE_DOOR_LIST,

	// Achievements-related
	MENU_ACHIEVEMENTS,
	MENU_ACHIEVEMENTS_SELECT,

	// House-related
	MENU_HOUSE,
	MENU_HOUSE_SELL,
	MENU_HOUSE_DOOR_LIST,
	MENU_HOUSE_DOOR_ACCESS,
	MENU_HOUSE_BUY,
	MENU_HOUSE_FARMZONE_LIST,
	MENU_HOUSE_FARMZONE_SELECT,

	// Warehouse
	MENU_WAREHOUSE,
	MENU_WAREHOUSE_ITEM_SELECT,

	// Eidolon Collection menus
	MENU_EIDOLON,
	MENU_EIDOLON_SELECT,

	// Journal menus
	MENU_JOURNAL_MAIN,
	MENU_JOURNAL_QUEST_DETAILS,

	// Skills
	MENU_SKILL_LIST,
	MENU_SKILL_SELECT,

	// Quest menus
	MENU_BOARD,
	MENU_BOARD_QUEST_SELECT,

	// Aethernet menus
	MENU_AETHERNET_LIST,

	// Auction
	MENU_AUCTION_LIST,
	MENU_AUCTION_SLOT_SELECT,
	MENU_AUCTION_CREATE_SLOT,

	// Group menus
	MENU_GROUP,

	// Grinding
	MENU_GUIDE,
	MENU_GUIDE_SELECT,

	// Crafting
	MENU_CRAFTING_LIST,
	MENU_CRAFTING_SELECT,

	// Other menus
	MENU_LEADERBOARD,
	MENU_DUNGEONS,

	// motd menus
	MOTD_MENU_BANK_MANAGER,
	MOTD_MENU_BONUSES_INFO,
	MOTD_MENU_WANTED_INFO,
};

// player scenarios
enum
{
	SCENARIO_UNIVERSAL = 1,
	SCENARIO_TUTORIAL,
	SCENARIO_EIDOLON,
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
	MAX_DECORATIONS_PER_HOUSE = 20,			// maximum decorations for houses
	MAIL_MAX_CAPACITY = 10,					// maximum number of emails what is displayed
	MAX_ATTRIBUTES_FOR_ITEM = 2,			// maximum number of stats per item
	POTION_RECAST_APPEND_TIME = 15,			// recast append time for potion in seconds
	DEFAULT_MAX_PLAYER_BAG_GOLD = 5000,		// player gold limit
	MIN_SKINCHANGE_CLIENTVERSION = 0x0703,	// minimum client version for skin change
	MIN_RACE_CLIENTVERSION = 0x0704,		// minimum client version for race type

	// items
	NOPE = -1,
	itCapsuleSurvivalExperience = 16,	// Gives 10-50 experience
	itLittleBagGold = 17,				// Gives 10-50 gold
	itTicketResetClassStats = 21,		// Ticket to reset the statistics of class upgrades
	itTicketResetWeaponStats = 23,		// Ticket to reset the statistics cartridge upgrade
	itTicketDiscountCraft = 24,			// Discount ticket for crafting
	itRandomHomeDecoration = 26,		// Random home decor
	itRandomRelicsBox = 58,				// Random Relics box

	// potions
	itPotionManaRegen = 14,				// Mana regeneration potion
	itTinyHealthPotion = 15,			// Tiny health potion

	// eidolons
	itEidolonOtohime = 57,				// Eidolon
	itEidolonMerrilee = 59,				// Eidolon
	itEidolonDryad = 80,				// Eidolon
	itEidolonPigQueen = 88,				// Eidolon

	// currency
	itGold = 1,							// Money ordinary currency
	itMaterial = 7,						// Scraping material
	itProduct = 8,						// Scraping material
	itSkillPoint = 9,					// Skillpoint
	itAchievementPoint = 10,			// Achievement point
	itAlliedSeals = 30,					// Allied seals
	itTicketGuild = 95,					// Ticket for the creation of the guild

	// modules
	itExplosiveGun = 19,				// Explosion for gun
	itExplosiveShotgun = 20,			// Explosion for shotgun
	itPoisonHook = 64,					// Poison hook
	itExplodeHook = 65,					// Explode hook
	itSpiderHook = 66,					// Spider hook
	itCustomizer = 96,                  // Curomizer for personal skins
	itDamageEqualizer = 97,				// Module for dissable self dmg

	// weapons
	itHammer = 2,						// Equipment Hammers
	itHammerLamp = 99,					// Equipment Lamp hammer
	itHammerBlast = 102,				// Equipment Blast hammer

	itGun = 3,							// Equipment Gun

	itShotgun = 4,						// Equipment Shotgun

	itGrenade = 5,						// Equipment Grenade
	itPizdamet = 100,					// Equipment Pizdamet

	itLaser = 6,						// Equipment Rifle
	itRifleWallPusher = 101,			// Equipment Rifle Plazma wall
	itRifleMagneticPulse = 103,			// Equpment Magnetic pulse rifle

	// decoration items
	itPickupHealth = 18,				// Pickup heart
	itPickupMana = 13,					// Pickup mana
	itPickupShotgun = 11,				// Pickup shotgun
	itPickupGrenade = 12,				// Pickup grenade
	itPickupLaser = 94,				    // Pickup laser

	// settings items
	itShowEquipmentDescription = 25,	// Description setting
	itShowCriticalDamage = 34,			// Critical damage setting
	itShowQuestStarNavigator = 93,		// Show quest path when idle
	itShowDetailGainMessages = 98,      // Show detail gain messages
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
enum ESpawnType
{
	SPAWN_HUMAN = 0,        // Spawn a human player
	SPAWN_BOT = 1,          // Spawn a bot player
	SPAWN_HUMAN_TREATMENT = 2,   // Spawn a human player in a safe location
	SPAWN_HUMAN_PRISON = 3, // Spawn a human prison
	SPAWN_NUM               // The total number of spawn types available
};

// bot types
enum EBotsType
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
	DMG = 1,                     // Attribute identifier for damage
	AttackSPD = 2,               // Attribute identifier for attack speed
	CritDMG = 3,                 // Attribute identifier for critical damage
	Crit = 4,                    // Attribute identifier for critical chance
	HP = 5,                      // Attribute identifier for health points
	Lucky = 6,                   // Attribute identifier for luck
	MP = 7,                      // Attribute identifier for mana points
	Vampirism = 8,               // Attribute identifier for vampirism
	AmmoRegen = 9,               // Attribute identifier for ammo regeneration
	Ammo = 10,                   // Attribute identifier for ammo
	Efficiency = 11,             // Attribute identifier for efficiency
	Extraction = 12,             // Attribute identifier for extraction
	HammerDMG = 13,              // Attribute identifier for hammer damage
	GunDMG = 14,                 // Attribute identifier for gun damage
	ShotgunDMG = 15,             // Attribute identifier for shotgun damage
	GrenadeDMG = 16,             // Attribute identifier for grenade damage
	RifleDMG = 17,               // Attribute identifier for rifle damage
	LuckyDropItem = 18,          // Attribute identifier for lucky drop item
	EidolonPWR = 19,             // Attribute identifier for eidolon power
	GoldCapacity = 20,           // Attribute identifier for gold capacity
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

class IServer;
namespace detail
{
	class _MultiworldIdentifiableData
	{
		inline static IServer* m_pServer {};

	public:
		IServer* Server() const { return m_pServer; }
		static void Init(IServer* pServer) { m_pServer = pServer; }
	};
}

template < typename T >
class MultiworldIdentifiableData : public detail::_MultiworldIdentifiableData
{
protected:
	static inline T m_pData {};

public:
	static T& Data() { return m_pData; }
};

#endif
