-- phpMyAdmin SQL Dump
-- version 5.2.1
-- https://www.phpmyadmin.net/
--
-- Host: 127.0.0.1
-- Generation Time: Dec 20, 2024 at 02:48 PM
-- Server version: 10.4.32-MariaDB
-- PHP Version: 8.2.12

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `mrpg_dev`
--

-- --------------------------------------------------------

--
-- Table structure for table `enum_behavior_mobs`
--

CREATE TABLE `enum_behavior_mobs` (
  `ID` int(11) NOT NULL,
  `Behavior` varchar(32) NOT NULL DEFAULT 'Standard'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `enum_behavior_mobs`
--

INSERT INTO `enum_behavior_mobs` (`ID`, `Behavior`) VALUES
(3, 'Sleepy'),
(2, 'Slower'),
(1, 'Standard');

-- --------------------------------------------------------

--
-- Table structure for table `enum_effects_list`
--

CREATE TABLE `enum_effects_list` (
  `ID` int(11) NOT NULL,
  `Name` varchar(16) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `enum_effects_list`
--

INSERT INTO `enum_effects_list` (`ID`, `Name`) VALUES
(3, 'Fire'),
(2, 'Poison'),
(1, 'Slowdown');

-- --------------------------------------------------------

--
-- Table structure for table `enum_items_functional`
--

CREATE TABLE `enum_items_functional` (
  `FunctionID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `enum_items_functional`
--

INSERT INTO `enum_items_functional` (`FunctionID`, `Name`) VALUES
(-1, 'Not have function'),
(0, 'Equip hammer(Only equip type)'),
(1, 'Equip gun(Only equip type)'),
(2, 'Equip shotgun(Only equip type)'),
(3, 'Equip grenade(Only equip type)'),
(4, 'Equip rifle(Only equip type)'),
(5, 'Equip miner(Only equip type)'),
(6, 'Equip rake(Only equip type)'),
(7, 'Equip armor(Only equip type)'),
(8, 'Equip eidolon(Only equip type)'),
(9, 'Once use item x1'),
(10, 'Several times use item x99'),
(11, 'Settings(Only settings or modules type)'),
(12, 'Plants item'),
(13, 'Mining item');

-- --------------------------------------------------------

--
-- Table structure for table `enum_items_types`
--

CREATE TABLE `enum_items_types` (
  `TypeID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `enum_items_types`
--

INSERT INTO `enum_items_types` (`TypeID`, `Name`) VALUES
(0, 'Invisible'),
(1, 'Useds'),
(2, 'Crafts'),
(3, 'Modules'),
(4, 'Others'),
(5, 'Settings'),
(6, 'Equipping'),
(7, 'Decorations'),
(8, 'Potions');

-- --------------------------------------------------------

--
-- Table structure for table `enum_quest_interactive`
--

CREATE TABLE `enum_quest_interactive` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `enum_quest_interactive`
--

INSERT INTO `enum_quest_interactive` (`ID`, `Name`) VALUES
(1, 'Randomly accept or refuse with the item'),
(2, 'Pick up items that NPC will drop.'),
(1, 'Randomly accept or refuse with the item'),
(2, 'Pick up items that NPC will drop.'),
(3, 'Show the item'),
(3, 'Show the item');

-- --------------------------------------------------------

--
-- Table structure for table `enum_worlds`
--

CREATE TABLE `enum_worlds` (
  `WorldID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL,
  `RespawnWorld` int(11) DEFAULT NULL,
  `RequiredQuestID` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `enum_worlds`
--

INSERT INTO `enum_worlds` (`WorldID`, `Name`, `RespawnWorld`, `RequiredQuestID`) VALUES
(0, 'Bad Dreams and Mornings', NULL, NULL),
(1, 'Port Skandia', 1, 1),
(2, 'Logging Site', 2, 3),
(3, 'Statue Ruins', 3, 11),
(4, 'Abandoned mine', 1, NULL),
(5, 'Helonia Coast', 5, 26),
(6, 'Helonia region Azuria', 6, 28),
(7, 'Helonia region Port', 7, 36);

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts`
--

CREATE TABLE `tw_accounts` (
  `ID` int(11) NOT NULL,
  `Username` varchar(64) NOT NULL,
  `Password` varchar(64) NOT NULL,
  `PasswordSalt` varchar(64) DEFAULT NULL,
  `RegisterDate` varchar(64) NOT NULL,
  `LoginDate` varchar(64) NOT NULL DEFAULT 'First log in',
  `RegisteredIP` varchar(64) NOT NULL DEFAULT '0.0.0.0',
  `LoginIP` varchar(64) NOT NULL DEFAULT '0.0.0.0',
  `Language` varchar(8) NOT NULL DEFAULT 'en'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_aethers`
--

CREATE TABLE `tw_accounts_aethers` (
  `ID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `AetherID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_bans`
--

CREATE TABLE `tw_accounts_bans` (
  `Id` int(11) NOT NULL,
  `AccountId` int(11) NOT NULL,
  `BannedSince` timestamp NULL DEFAULT current_timestamp(),
  `BannedUntil` timestamp NOT NULL DEFAULT current_timestamp(),
  `Reason` varchar(512) NOT NULL DEFAULT 'No Reason Given'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_data`
--

CREATE TABLE `tw_accounts_data` (
  `ID` int(11) NOT NULL,
  `Nick` varchar(32) NOT NULL,
  `DiscordID` varchar(64) NOT NULL DEFAULT 'null',
  `WorldID` int(11) DEFAULT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Exp` int(11) NOT NULL DEFAULT 0,
  `GuildID` int(11) DEFAULT NULL,
  `GuildDeposit` int(11) NOT NULL DEFAULT 0,
  `GuildRank` int(11) DEFAULT NULL,
  `Upgrade` int(11) NOT NULL DEFAULT 0,
  `SpreadShotgun` int(11) NOT NULL DEFAULT 3,
  `SpreadGrenade` int(11) NOT NULL DEFAULT 1,
  `SpreadRifle` int(11) NOT NULL DEFAULT 1,
  `AttackSPD` int(11) NOT NULL DEFAULT 0,
  `CritDMG` int(11) NOT NULL DEFAULT 0,
  `Crit` int(11) NOT NULL DEFAULT 0,
  `HP` int(11) NOT NULL DEFAULT 0,
  `Tenacity` int(11) NOT NULL DEFAULT 0,
  `Lucky` int(11) NOT NULL DEFAULT 0,
  `MP` int(11) NOT NULL DEFAULT 0,
  `Vampirism` int(11) NOT NULL DEFAULT 0,
  `AmmoRegen` int(11) NOT NULL DEFAULT 0,
  `Ammo` int(11) NOT NULL DEFAULT 0,
  `Efficiency` int(11) NOT NULL DEFAULT 0,
  `Extraction` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_farming`
--

CREATE TABLE `tw_accounts_farming` (
  `UserID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Exp` int(11) NOT NULL DEFAULT 0,
  `Upgrade` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_items`
--

CREATE TABLE `tw_accounts_items` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `Value` int(11) NOT NULL,
  `Settings` int(11) NOT NULL,
  `Enchant` int(11) NOT NULL,
  `Durability` int(11) NOT NULL DEFAULT 100,
  `UserID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_mailbox`
--

CREATE TABLE `tw_accounts_mailbox` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) DEFAULT NULL,
  `ItemValue` int(11) DEFAULT NULL,
  `Enchant` int(11) DEFAULT NULL,
  `Name` varchar(64) NOT NULL,
  `Description` varchar(64) NOT NULL,
  `UserID` int(11) NOT NULL,
  `IsRead` tinyint(4) NOT NULL DEFAULT 0,
  `FromSend` varchar(32) NOT NULL DEFAULT 'Game'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_mining`
--

CREATE TABLE `tw_accounts_mining` (
  `UserID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Exp` int(11) NOT NULL DEFAULT 0,
  `Upgrade` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_quests`
--

CREATE TABLE `tw_accounts_quests` (
  `ID` int(11) NOT NULL,
  `QuestID` int(11) DEFAULT NULL,
  `UserID` int(11) NOT NULL,
  `Type` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_skills`
--

CREATE TABLE `tw_accounts_skills` (
  `ID` int(11) NOT NULL,
  `SkillID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `Level` int(11) NOT NULL,
  `UsedByEmoticon` int(11) DEFAULT -1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_account_eidolon_enhancements`
--

CREATE TABLE `tw_account_eidolon_enhancements` (
  `ID` int(11) NOT NULL,
  `Index` int(11) NOT NULL,
  `UserID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_aethers`
--

CREATE TABLE `tw_aethers` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL DEFAULT 'Teleport name',
  `WorldID` int(11) NOT NULL,
  `TeleX` int(11) NOT NULL,
  `TeleY` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_aethers`
--

INSERT INTO `tw_aethers` (`ID`, `Name`, `WorldID`, `TeleX`, `TeleY`) VALUES
(1, 'Logging Site', 2, 5857, 1041),
(2, 'Center', 1, 8003, 7089),
(3, 'East', 1, 3194, 7441),
(4, 'Craftsman', 1, 9412, 8465),
(5, 'West', 1, 12385, 6833),
(6, 'Central Street', 5, 7069, 2001),
(7, 'Block post', 6, 1325, 3653),
(8, 'Nurse', 7, 721, 1620);

-- --------------------------------------------------------

--
-- Table structure for table `tw_attributs`
--

CREATE TABLE `tw_attributs` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL,
  `FieldName` varchar(32) DEFAULT NULL,
  `Price` int(11) NOT NULL,
  `Type` int(11) NOT NULL COMMENT '0.tank1.healer2.dps3.weapon4.hard5.jobs 6. others'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_attributs`
--

INSERT INTO `tw_attributs` (`ID`, `Name`, `FieldName`, `Price`, `Type`) VALUES
(1, 'Shotgun Spread', 'SpreadShotgun', 12, 3),
(2, 'Grenade Spread', 'SpreadGrenade', 12, 3),
(3, 'Rifle Spread', 'SpreadRifle', 12, 3),
(4, 'DMG', 'unfield', 0, 4),
(5, 'Attack SPD', 'AttackSPD', 1, 2),
(6, 'Crit DMG', 'CritDMG', 1, 2),
(7, 'Crit', 'Crit', 1, 2),
(8, 'HP', 'HP', 1, 0),
(9, 'Lucky', 'Lucky', 1, 0),
(10, 'MP', 'MP', 1, 1),
(11, 'Vampirism', 'Vampirism', 1, 1),
(12, 'Ammo Regen', 'AmmoRegen', 1, 3),
(13, 'Ammo', 'Ammo', 4, 3),
(14, 'Efficiency', NULL, -1, 5),
(15, 'Extraction', NULL, -1, 5),
(16, 'Hammer DMG', NULL, -1, 4),
(17, 'Gun DMG', NULL, -1, 4),
(18, 'Shotgun DMG', NULL, -1, 4),
(19, 'Grenade DMG', NULL, -1, 4),
(20, 'Rifle DMG', NULL, -1, 4),
(21, 'Lucky Drop', NULL, -1, 6),
(22, 'Eidolon PWR', NULL, -1, 6);

-- --------------------------------------------------------

--
-- Table structure for table `tw_auction_items`
--

CREATE TABLE `tw_auction_items` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `ItemValue` int(11) NOT NULL,
  `Price` int(11) NOT NULL,
  `UserID` int(11) NOT NULL DEFAULT 0,
  `Enchant` int(11) NOT NULL DEFAULT 0,
  `ValidUntil` bigint(20) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_bots_info`
--

CREATE TABLE `tw_bots_info` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT 'Bot name',
  `JsonTeeInfo` varchar(128) NOT NULL DEFAULT '{ "skin": "default", 	"custom_color": 0, 	"color_body": 0, 	"color_feer": 0}',
  `SlotHammer` int(11) DEFAULT 2,
  `SlotGun` int(11) DEFAULT 3,
  `SlotShotgun` int(11) DEFAULT 4,
  `SlotGrenade` int(11) DEFAULT 5,
  `SlotRifle` int(11) DEFAULT 6,
  `SlotArmor` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

--
-- Dumping data for table `tw_bots_info`
--

INSERT INTO `tw_bots_info` (`ID`, `Name`, `JsonTeeInfo`, `SlotHammer`, `SlotGun`, `SlotShotgun`, `SlotGrenade`, `SlotRifle`, `SlotArmor`) VALUES
(5, 'Pig', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"pinky\"}', 2, NULL, NULL, NULL, NULL, NULL),
(6, 'Pig Queen', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"pinky\"}', 2, NULL, NULL, NULL, NULL, NULL),
(7, 'Skill master', '{\"color_body\":9895735,\"color_feet\":9633571,\"custom_color\":1,\"skin\":\"jeet\"}', 2, 3, 4, 5, 6, NULL),
(8, 'Craftsman', '{\"color_body\":8545280,\"color_feet\":8257280,\"custom_color\":1,\"skin\":\"coala\"}', 2, 3, 4, 5, 6, NULL),
(9, 'Nurse', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"tika\"}', 2, 3, 4, 5, 6, NULL),
(10, 'Lady Sheila', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"nanami\"}', 2, 3, 4, 5, 6, NULL),
(11, 'Willie', '{\"color_body\":6029183,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"kitty_redbopp\"}', 2, 3, 4, 5, 6, NULL),
(12, 'Joel', '{\"color_body\":6029183,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"cammostripes\"}', 2, 3, 4, 5, 6, NULL),
(13, 'Anita', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Jigglypuff\"}', 2, 3, 4, 5, 6, NULL),
(14, 'Betsy', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"mermydon\"}', 2, 3, 4, 5, 6, NULL),
(15, 'Corey', '{\"color_body\":65280,\"color_feet\":10477151,\"custom_color\":1,\"skin\":\"kintaro_2\"}', 2, 3, 4, 5, 6, NULL),
(16, 'York', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"greensward\"}', 2, 3, 4, 5, 6, NULL),
(17, 'Warren', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"musmann\"}', 2, 3, 4, 5, 6, NULL),
(18, 'Caine', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"mermydon-coala\"}', 2, 3, 4, 5, 6, NULL),
(19, 'Brian', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"nanas\"}', 2, 3, 4, 5, 6, NULL),
(20, 'Bunny', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"bunny\"}', 2, 3, 4, 5, 6, NULL),
(21, 'Workman', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"default\"}', 2, 3, 4, 5, 6, NULL),
(22, 'Lumberjacks', '{\"color_body\":12942592,\"color_feet\":11009792,\"custom_color\":1,\"skin\":\"nanami_glow\"}', 2, 3, 4, 5, 6, NULL),
(23, 'Doctor Cal', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"kitty_redstripe\"}', 2, 3, 4, 5, 6, NULL),
(24, 'Antelope', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Fluffi\"}', 2, 3, 4, 5, 6, NULL),
(25, 'Cyclope', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Beast_1\"}', 2, 3, 4, 5, 6, NULL),
(26, 'Auctionist', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"veteran\"}', 2, 3, 4, 5, 6, NULL),
(27, 'Sunbird', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"amor\"}', 2, 3, NULL, NULL, NULL, NULL),
(28, 'Bloody Bat', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":1,\"skin\":\"Bat\"}', 2, 3, 4, 5, 6, NULL),
(29, 'Sneaky Cat', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"blackcat\"}', 2, 3, 4, 5, 6, NULL),
(30, 'Dead Miner', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"coffee_hairy\"}', 2, 3, 4, 5, 6, NULL),
(31, 'Shadow Dark', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"darky\"}', 2, 3, 4, 5, 6, NULL),
(32, 'Bat', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Bat\"}', 2, 3, 4, 5, 6, NULL),
(33, 'Flower', '{\"color_body\":8060863,\"color_feet\":2874975,\"custom_color\":0,\"skin\":\"blackcat\"}', 2, 3, 4, 5, 6, NULL),
(34, 'Petal Fairy', '{\"color_body\":8191744,\"color_feet\":9100881,\"custom_color\":1,\"skin\":\"Foxberry\"}', 2, NULL, NULL, NULL, NULL, NULL),
(35, 'Green Fairy', '{\"color_body\":3211008,\"color_feet\":8379985,\"custom_color\":1,\"skin\":\"Foxberry\"}', 2, NULL, NULL, NULL, NULL, NULL),
(36, 'Beebis', '{\"color_body\":0,\"color_feet\":0,\"custom_color\":0,\"skin\":\"coala_phoenix\"}', 2, NULL, NULL, NULL, NULL, NULL),
(37, 'Ruin Giant', '{\"color_body\":0,\"color_feet\":0,\"custom_color\":0,\"skin\":\"Demon\"}', 2, NULL, NULL, NULL, NULL, NULL),
(38, 'Larry', '{\"color_body\":0,\"color_feet\":0,\"custom_color\":0,\"skin\":\"warpaint\"}', 2, NULL, NULL, NULL, NULL, NULL),
(39, 'Bentley', '{\"color_body\":9240320,\"color_feet\":16777134,\"custom_color\":1,\"skin\":\"bluestripe\"}', 2, 3, 4, 5, 6, NULL),
(40, 'Kid Sprout', '{\"color_body\":5963520,\"color_feet\":2817792,\"custom_color\":1,\"skin\":\"chera\"}', 2, NULL, NULL, NULL, NULL, NULL),
(41, 'Wet Sprout', '{\"color_body\":9174784,\"color_feet\":3997440,\"custom_color\":1,\"skin\":\"chera\"}', 2, NULL, NULL, NULL, NULL, NULL),
(42, 'Elena', '{\"color_body\":0,\"color_feet\":0,\"custom_color\":0,\"skin\":\"clan_wl_nanami\"}', 2, NULL, NULL, NULL, NULL, NULL),
(43, 'Creature', '{\"color_body\":10878976,\"color_feet\":65280,\"custom_color\":1,\"skin\":\"jeet\"}', 2, 3, 4, 5, 6, NULL),
(44, 'Kane', '{\"color_body\":7456086,\"color_feet\":12320512,\"custom_color\":1,\"skin\":\"greensward\"}', 2, 3, 4, 5, 6, NULL),
(45, 'Otohime', '{\"color_body\":9174784,\"color_feet\":3997440,\"custom_color\":0,\"skin\":\"vulpi\"}', 2, NULL, 4, NULL, NULL, NULL),
(46, 'Merrilee', '{\"color_body\":8519424,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"dragon 2\"}', 2, NULL, NULL, 5, NULL, NULL),
(47, 'Luther', '{\"color_body\":65280,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"bluekitty\"}', 2, 3, 4, 5, 6, NULL),
(48, 'Madeline', '{\"color_body\":13565696,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"vulpi\"}', 2, 3, 4, 5, 6, NULL),
(49, 'Tanuki', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"Bonsly\"}', 2, 3, 4, 5, 6, NULL),
(50, 'Isaac', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"toptri\"}', 2, 3, 4, 5, 6, NULL),
(51, 'Braeden', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"greensward\"}', 2, 3, 4, 5, 6, NULL),
(52, 'Sun Crab', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"kitty_redstripe\"}', 2, 3, 4, 5, 6, NULL),
(53, 'Mobray', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"PaladiN\"}', 2, 3, 4, 5, 6, NULL),
(54, 'Beetle', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"beast\"}', 2, 3, 4, 5, 6, NULL),
(55, 'Aged Man', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"veteran\"}', 2, 3, 4, 5, 6, NULL),
(56, 'Helonian Man', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"chinese_by_whis\"}', 2, 3, 4, 5, 6, NULL),
(57, 'Grandpa', '{\"color_body\":15697152,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"greyfox\"}', 2, 3, 4, 5, 6, NULL),
(58, 'Dryad', '{\"color_body\":12779264,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"amina_kitty\"}', 2, 3, 4, 5, NULL, NULL),
(59, 'Spirit', '{\"color_body\":2752256,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"asteroid\"}', 2, 3, 4, 5, 6, NULL),
(60, 'Parrot', '{\"color_body\":2752256,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"clefairy\"}', 2, NULL, NULL, NULL, NULL, NULL),
(61, 'Soran', '{\"color_body\":11155259,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"coala_warpaint\"}', 2, 3, 4, 5, 6, NULL);

-- --------------------------------------------------------

--
-- Table structure for table `tw_bots_mobs`
--

CREATE TABLE `tw_bots_mobs` (
  `ID` int(11) NOT NULL,
  `BotID` int(11) NOT NULL DEFAULT -1,
  `WorldID` int(11) DEFAULT NULL,
  `PositionX` int(11) NOT NULL,
  `PositionY` int(11) NOT NULL,
  `Effect` set('','Slowdown','Poison','Fire') NOT NULL DEFAULT '',
  `Behavior` set('','Sleepy','Slower') DEFAULT '',
  `Level` int(11) NOT NULL DEFAULT 1,
  `Power` int(11) NOT NULL DEFAULT 10,
  `Spread` int(11) NOT NULL DEFAULT 0,
  `Number` int(11) NOT NULL DEFAULT 1,
  `Respawn` int(11) NOT NULL DEFAULT 1,
  `Boss` tinyint(1) NOT NULL DEFAULT 0,
  `it_drop_0` int(11) DEFAULT NULL,
  `it_drop_1` int(11) DEFAULT NULL,
  `it_drop_2` int(11) DEFAULT NULL,
  `it_drop_3` int(11) DEFAULT NULL,
  `it_drop_4` int(11) DEFAULT NULL,
  `it_drop_count` varchar(64) NOT NULL DEFAULT '|0|0|0|0|0|',
  `it_drop_chance` varchar(64) NOT NULL DEFAULT '|0|0|0|0|0|'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_bots_mobs`
--

INSERT INTO `tw_bots_mobs` (`ID`, `BotID`, `WorldID`, `PositionX`, `PositionY`, `Effect`, `Behavior`, `Level`, `Power`, `Spread`, `Number`, `Respawn`, `Boss`, `it_drop_0`, `it_drop_1`, `it_drop_2`, `it_drop_3`, `it_drop_4`, `it_drop_count`, `it_drop_chance`) VALUES
(1, 20, 2, 1216, 704, '', 'Slower', 3, 8, 0, 7, 1, 0, 40, 44, NULL, NULL, NULL, '|1|1|0|0|0|', '|5|2.11|0|0|0|'),
(3, 24, 2, 2610, 832, '', 'Slower', 4, 8, 0, 7, 1, 0, 32, 40, NULL, NULL, NULL, '|2|1|0|0|0|', '|5.5|5|0|0|0|'),
(4, 27, 3, 9512, 1915, 'Fire', NULL, 9, 25, 0, 12, 3, 0, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|'),
(5, 28, 4, 2982, 2494, 'Poison', NULL, 10, 50, 2, 10, 1, 0, 48, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|5|0|0|0|0|'),
(6, 29, 4, 4840, 2560, 'Poison', NULL, 12, 60, 2, 8, 1, 0, 48, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|5|0|0|0|0|'),
(7, 30, 4, 1150, 3700, 'Poison', NULL, 12, 70, 1, 10, 1, 0, 48, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|5|0|0|0|0|'),
(8, 32, 4, 1440, 5100, 'Poison', NULL, 10, 80, 2, 8, 1, 0, 48, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|8|0|0|0|0|'),
(9, 31, 4, 3960, 4595, 'Slowdown,Fire', NULL, 15, 800, 3, 1, 1, 1, 48, NULL, NULL, NULL, NULL, '|8|0|0|0|0|', '|100|0|0|0|0|'),
(10, 34, 3, 7970, 750, '', 'Slower', 7, 18, 1, 4, 1, 0, 46, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|5|0|0|0|0|'),
(11, 35, 3, 7970, 750, '', 'Slower', 7, 18, 1, 4, 1, 0, 47, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|5|0|0|0|0|'),
(12, 37, 3, 10300, 1028, '', NULL, 10, 240, 0, 1, 320, 1, 48, 48, NULL, NULL, NULL, '|4|2|0|0|0|', '|100|25|0|0|0|'),
(13, 40, 3, 12530, 1530, 'Slowdown', 'Sleepy', 10, 40, 0, 4, 1, 0, 52, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|5|0|0|0|0|'),
(14, 41, 3, 12530, 1530, 'Slowdown', 'Slower', 10, 40, 0, 4, 1, 0, 53, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|5|0|0|0|0|'),
(15, 49, 6, 10472, 3784, '', 'Slower', 15, 70, 0, 10, 1, 0, 67, 84, NULL, NULL, NULL, '|1|1|0|0|0|', '|5|6.28|0|0|0|'),
(16, 52, 6, 7475, 1585, 'Fire', 'Slower', 16, 80, 0, 5, 1, 0, 81, 82, NULL, NULL, NULL, '|1|1|0|0|0|', '|4.75|3.92|0|0|0|'),
(17, 53, 6, 7475, 1585, 'Poison', 'Slower', 18, 1020, 0, 1, 320, 1, 68, NULL, NULL, NULL, NULL, '|1|1|0|0|0|', '|100|0|0|0|0|'),
(18, 54, 6, 4898, 2400, 'Poison', 'Slower', 20, 100, 0, 6, 1, 0, 72, 64, NULL, NULL, NULL, '|1|1|0|0|0|', '|7|0.2|0|0|0|'),
(19, 58, 7, 750, 2812, '', 'Sleepy', 21, 120, 0, 8, 1, 0, 67, 78, 84, NULL, NULL, '|1|2|1|0|0|', '|2.5|7.32|8.45|0|0|'),
(20, 59, 7, 2280, 553, 'Fire', 'Slower', 21, 160, 0, 10, 1, 0, 78, 72, NULL, NULL, NULL, '|2|1|0|0|0|', '|7.82|1|0|0|0|'),
(21, 60, 7, 3628, 3640, 'Poison', 'Slower', 22, 180, 0, 5, 1, 0, 90, 70, NULL, NULL, NULL, '|1|1|0|0|0|', '|3.8|4.0|0|0|0|');

-- --------------------------------------------------------

--
-- Table structure for table `tw_bots_npc`
--

CREATE TABLE `tw_bots_npc` (
  `ID` int(11) NOT NULL,
  `BotID` int(11) NOT NULL DEFAULT -1,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `GiveQuestID` int(11) DEFAULT NULL,
  `DialogData` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL CHECK (json_valid(`DialogData`)),
  `Function` int(11) NOT NULL DEFAULT -1,
  `Static` int(11) NOT NULL,
  `Emote` enum('Pain','Happy','Surprise','Angry','Blink') DEFAULT NULL,
  `WorldID` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_bots_npc`
--

INSERT INTO `tw_bots_npc` (`ID`, `BotID`, `PosX`, `PosY`, `GiveQuestID`, `DialogData`, `Function`, `Static`, `Emote`, `WorldID`) VALUES
(1, 7, 6472, 7569, NULL, '[{\"text\":\"Someone once told me friendship is magic. That\'s ridiculous. You can\'t turn people into frogs with friendship{OR}Hey..what\'s <bot_9> up to? Have you...have you talked to her, by chance?{OR}\",\"emote\":\"normal\"}\n]', -1, 0, '', 1),
(2, 8, 9973, 8561, NULL, '[{\"text\":\"This is a big upgrade from wiping that table all day.{OR}A lot of tenacity and a little bit of luck can go a long way...{OR}<bot_9> seems nice. I should bring her back with me.{OR}\",\"emote\":\"happy\"}\n]', -1, 0, 'Happy', 1),
(3, 9, 2791, 7345, NULL, '[{ \"text\": \"[l][le][re]I think you look better this way.{OR}Quit being such a baby! I’ve seen worse.{OR}I keep asking for wine, but all <bot_7> gives me are mugs of ale.\"}]', 0, 0, 'Happy', 1),
(4, 11, 665, 2257, 1, '[{\"text\":\"[l]The nightmare fatigues you greatly, your thoughts fractured and sluggish. You\'re relieved that the terrible dream was only just that, but you regret not being able to see it to its end.\"},{\"text\":\"[l]The people in your dream seem powerful and important, not at all like the people you know at the small fishing village. How did the dream end? You long to find out...\"},{\"text\":\"<player>, are you zoning out? I thought you said you were supposed to deliver some goods to <bot_10>. You\'re an official garrison member; I shouldn\'t need to remind you to do your job!\"},{\"text\":\"I like you a lot; don\'t let me down. The boss wants to discuss something with me, so go ahead and meed with the Chief\'s Wife.\"},{\"text\":\"[l]You are not sure why or when you fell asleep. Yesterday, you agreed to deliver some merchandise to the Chief\'s Wife. It looks as though you\'ve missed the delivery; you have to apologize to her.\",\"action_step\":1}]', -1, 1, 'Blink', 0),
(5, 14, 9417, 6833, NULL, NULL, -1, 1, 'Happy', 1),
(6, 15, 6280, 6417, NULL, NULL, -1, 1, '', 1),
(7, 9, 5612, 1009, NULL, '[{ \"text\": \"I think you look better this way.{OR}Quit being such a baby! I’ve seen worse.{OR}I keep asking for wine, but all <bot_7> gives me are mugs of ale.\"}]', 0, 0, 'Happy', 2),
(8, 26, 5779, 8369, NULL, NULL, 0, 0, 'Blink', 1),
(9, 9, 1075, 2161, NULL, '[{ \"text\": \"I think you look better this way.{OR}Quit being such a baby! I’ve seen worse.{OR}I keep asking for wine, but all <bot_7> gives me are mugs of ale.\"}]', 0, 1, 'Happy', 3),
(10, 11, 3367, 7505, NULL, NULL, -1, 1, 'Blink', 1),
(11, 39, 10958, 2513, NULL, NULL, -1, 1, 'Blink', 3),
(12, 42, 10115, 3665, NULL, NULL, -1, 0, 'Angry', 3),
(13, 9, 7438, 1553, NULL, '[{ \"text\": \"I think you look better this way.{OR}Quit being such a baby! I’ve seen worse.{OR}I keep asking for wine, but all <bot_7> gives me are mugs of ale.\"}]', 0, 1, 'Happy', 5),
(14, 7, 7226, 1425, NULL, '[{\"text\":\"Someone once told me friendship is magic. That\'s ridiculous. You can\'t turn people into frogs with friendship{OR}Hey..what\'s <bot_9> up to? Have you...have you talked to her, by chance?{OR}\",\"emote\":\"normal\"}\n]', -1, 0, 'Happy', 5),
(15, 26, 7038, 1169, NULL, NULL, -1, 0, 'Blink', 5),
(16, 47, 8585, 1873, 28, '[{\"text\":\"\\\"I understand how you feel, but the Town Mayor doesn\'t have time to solve your problem.\\\" Luther shrugs passively.\"},{\"text\":\"[l]Panic crosses your face. You ask Luther why the mayor can\'t help.\"},{\"text\":\"\\\"Not long ago, the Catseye Pirate Crew threatened the Town Mayor and demanded protection money. If she didn\'t pay up, they told her all of Helonia would suffer.\\\"\"},{\"text\":\"\\\"The deadline to pay them off is approaching, and we\'re still missing half the goods. The Mayor is scrambling to solve this crisis.\\\"\"},{\"text\":\"\\\"Hey, if it\'s really urgent, go and talk to her. The worst thing she can say is \'no.\'\\\"\",\"action_step\":1},{\"text\":\"\\\"If you need a Starlight Treasure Chest, you should look for the Town Mayor, Madeline. I cannot do it for you.\\\"\"}]', -1, 0, 'Blink', 5),
(17, 9, 519, 3633, NULL, '[{ \"text\": \"I think you look better this way.{OR}Quit being such a baby! I’ve seen worse.{OR}I keep asking for wine, but all <bot_34> gives me are mugs of ale.\"}]', 0, 0, 'Happy', 6),
(18, 51, 6551, 3505, NULL, NULL, -1, 0, 'Blink', 6),
(19, 50, 11502, 2193, NULL, NULL, -1, 1, 'Surprise', 5),
(20, 56, 3304, 1649, NULL, NULL, -1, 1, 'Blink', 5),
(21, 55, 547, 1073, NULL, NULL, -1, 1, 'Angry', 5),
(22, 57, 7709, 1969, NULL, NULL, -1, 1, 'Angry', 5),
(23, 9, 362, 1776, NULL, '[{ \"text\": \"I think you look better this way.{OR}Quit being such a baby! I’ve seen worse.{OR}I keep asking for wine, but all <bot_34> gives me are mugs of ale.\"}]', 0, 1, 'Happy', 7);

-- --------------------------------------------------------

--
-- Table structure for table `tw_bots_quest`
--

CREATE TABLE `tw_bots_quest` (
  `ID` int(11) NOT NULL,
  `BotID` int(11) NOT NULL DEFAULT -1,
  `QuestID` int(11) NOT NULL DEFAULT -1,
  `Step` int(11) NOT NULL DEFAULT 1,
  `WorldID` int(11) DEFAULT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `DialogData` longtext DEFAULT NULL,
  `EventData` longtext DEFAULT NULL,
  `RequiredItemID1` int(11) DEFAULT NULL,
  `RequiredItemID2` int(11) DEFAULT NULL,
  `RewardItemID1` int(11) DEFAULT NULL,
  `RewardItemID2` int(11) DEFAULT NULL,
  `RequiredDefeatMobID1` int(11) DEFAULT NULL,
  `RequiredDefeatMobID2` int(11) DEFAULT NULL,
  `Amount` varchar(64) NOT NULL DEFAULT '|0|0|0|0|0|0|',
  `InteractionType` int(11) DEFAULT NULL,
  `InteractionTemp` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_bots_quest`
--

INSERT INTO `tw_bots_quest` (`ID`, `BotID`, `QuestID`, `Step`, `WorldID`, `PosX`, `PosY`, `DialogData`, `EventData`, `RequiredItemID1`, `RequiredItemID2`, `RewardItemID1`, `RewardItemID2`, `RequiredDefeatMobID1`, `RequiredDefeatMobID2`, `Amount`, `InteractionType`, `InteractionTemp`) VALUES
(1, 10, 1, 1, 0, 28353, 2065, '[{\"text\":\"[re][le]It is now getting late. <player>, this boy is so...\"},{\"text\":\"\\\"<player>, have you forgotten your promise already?\\\"\\n\\n Sheila asks with a gentle smile.\",\"action_step\":1}]', '{\"chat\":[{\"text\":\"What was it?\"},{\"text\":\"You are disturbed by a strange dream.\"}]}', 92, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(2, 10, 2, 1, 1, 7897, 7921, '[{\"text\":\"[l]You apologize to <bot_10> and ask if there is a way to make it up to her.\"},{\"text\":\"\\\"Your apology is quite sincere. I forgive you. And I do have another task,\\\" <bot_10> says with a kind smile.\"},{\"text\":\"These boxes of jam just arrived from the port. Please help me deliver them to the merchants in the village.\"},{\"text\":\"Sheila gives a frown only an irritated mother can give.\\n\\n \\\"Also, ask them if they have seen my son, <bot_12>. I have no idea where he is.\\\"\",\"action_step\":1},{\"text\":\"I\'ve heard of rebellions, but <bot_12> has gone too far. It\'s late, and he hasn\'t come home yet. I fear he\'s getting spoiled, and we can\'t have that! He needs to learn a lesson.\"}]', NULL, NULL, NULL, 29, NULL, NULL, NULL, '|0|0|3|0|0|0|', NULL, NULL),
(3, 13, 2, 2, 1, 8141, 7089, '[{\"text\":\"This is so unusual. So you\'re helping <bot_10> deliver goods today? I suppose you are indeed a real garrison member. Keep up the good work.\",\"action_step\":1},{\"text\":\"From now on, everyone will treat you as an adult. Perhaps you\'ll travel the world, meeting maharajas and princesses. Or maybe you\'ll just live and work in this fishing port all your life. You know, either way.\"},{\"text\":\"Just remember this: in the outside world, you can buy different potions at various drugstores. You might find some other special items there, too.\"},{\"text\":\"[l]You ask <bot_13> if she has seen <bot_12>.\"},{\"text\":\"<bot_12> didn\'t come to the marketplace today. It\'s a bit odd; he usually stops by.\"}]', NULL, 29, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(4, 14, 2, 2, 1, 9255, 6801, '[{\"text\":\"\\\"<player>, would you like to buy a piece of <item_28>? We carry the armor in all sizes, and we... what is this?\\\"\\n\\n <bot_14> takes the jar of jam and examines it suspiciously.\",\"action_step\":1},{\"text\":\"\\\"Wait, I clearly told <bot_13> that I don\'t want any jam. It\'s too sweet. I like my jam salty and sour!\\\"\\n\\n <bot_14> huffs.\"},{\"text\":\"Well, let\'s forget about the jam. Why don\'t you treat yourself to some new armor? Being a garrison member in a fishing village is not exactly a dangerous job, but from time to time, you\'ll need to clear out some monsters, and you\'ll be happy to have steel between their claws and your heart!\"},{\"text\":\"[l]You ask <bot_14> if she has seen <bot_12>.\"},{\"text\":\"I saw him yesterday, but not today.\"}]', NULL, 29, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(5, 15, 2, 2, 1, 6561, 6449, '[{\"text\":\"\\\"Jam! At last! My toast has been going naked!\\\"\\n\\n <bot_15> pops open the jar, dips in his fingers, and slurps up the jelly eagerly.\",\"action_step\":1},{\"text\":\"\\\"Mmm! <bot_13> was right. The jam is perfectly sweet and tart. Would you like some? I promise my fingers are clean.\\\"\\n\\n <bot_15> follows your gaze to the newly produced weapons.\"},{\"text\":\"Impressive, right? These weapons all come from abroad! This one here, for instance, is the <item_3>, a symbol of power. And over there is the <item_5>.\"},{\"text\":\"I\'ll give you a tip, jelly kid. When you have time, just visit weapon shops in different places. You might find exceptionally good weaponry at reasonable prices!\"},{\"text\":\"[l]You ask <bot_15> if he has seen <bot_12>.\"},{\"text\":\"<bot_12>? No, I haven\'t seen him today.\"}]', NULL, 29, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(6, 10, 2, 3, 1, 7897, 7921, '[{\"text\":\"\\\"Have you delivered the jam? You\'ve truly grown; it\'s time for you to start saving your money.\\\"\\n\\n <bot_10> places some gleaming coins in your palm and closes your fingers around them.\"},{\"text\":\"Did any of the merchants see <bot_12>?\"},{\"text\":\"[l]You shake your head and tell her that none of the merchants have seen him today. Hearing this, <bot_10>\'s irritation starts to turn to worry.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(10, 10, 3, 1, 1, 7726, 7921, '[{\"text\":\"\\\"Hasn\'t anyone seen <bot_12>? Where has that boy gone?\\\"\\n\\n <bot_10> glances around anxiously.\"},{\"text\":\"\\\"He can\'t have gone too far...\\\"\\n\\n She hangs her head, looking despondently at the <item_30> in her hand.\"},{\"text\":\"I ought to go look for him. Could you bring this <item_30> to my husband, <bot_16>? When you see him, ask him to find you a suitable job.\",\"action_step\":1}]', NULL, NULL, NULL, 30, NULL, NULL, NULL, '|0|0|1|0|0|0|', NULL, NULL),
(11, 16, 3, 2, 1, 6558, 7569, '[{\"text\":\"[rs_17]I must report to you, as you are the Village Chief. The sea has begun to churn; the situation grows more unstable by the day. I can only guide the ships into the port with light signals, but...\"},{\"text\":\"<bot_16> frowns, pacing anxiously as he chats with <bot_17>, the lighthouse manager.\"},{\"text\":\"\\\"Got it. The monster population in the forest has skyrocketed, too. Things are getting rough around here.\\\"\\n\\n <bot_16> looks over at you, eyeing the <item_30>.\"},{\"text\":\"<player>, where is my wife, and why are you delivering my lunch?\",\"action_step\":1}]', NULL, 30, NULL, 38, NULL, NULL, NULL, '|1|0|1|0|0|0|', NULL, NULL),
(12, 17, 3, 2, 1, 6418, 7569, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(13, 16, 4, 1, 1, 6558, 7569, '[{\"text\":\"\\\"<bot_10> dotes on him. She spoils him. He can play alone once in a while, you know? Who knows, he might grow up to be a great conqueror - if he stops being so darn soft!\\\"\\n\\n <bot_16> chortles.\"},{\"text\":\"As for your work... well, I really don\'t have much for you to do.\"},{\"text\":\"[rs_17]<bot_16>, you said you needed someone to replace one of <bot_18>\'s... less enthusiastic workers. How could you forget so quickly?\"},{\"text\":\"I didn\'t forget. But working for <bot_18> is no picnic. Very few are willing to work backbreaking hours at the Logging Site, and <bot_18>\'s not exactly Mr. Personality.\"},{\"text\":\"<player>, would you like to try working at the Logging Site? The salary\'s not bad.\",\"action_step\":1},{\"text\":\"\\\"This boy <bot_19> is a fool! He only makes work harder for me! Unless we\'re short workers, I\'d fire him right away!\\\"\\n\\n <bot_18> grumbles to himself. It seems that working at the Logging Site is quite arduous.\"}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(14, 17, 4, 1, 1, 6428, 7569, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(15, 18, 4, 1, 1, 6480, 7569, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(16, 18, 4, 2, 2, 6256, 1105, '[{\"text\":\"Did <bot_16> ask you to come here? Excellent. I was just wondering what the Garrison Members in the village normally do during the day. They just hang about the forest. They might as well be here doing something productive for once.\"},{\"text\":\"Speaking of which, have you seen <bot_19> lately? He hasn\'t come to work in days!\"},{\"text\":\"[l]You shake your head and explain that you don\'t know him.\"},{\"text\":\"Well, there\'s no time to lose. you should just start working right away.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(17, 18, 5, 1, 2, 6256, 1105, '[{\"text\":\"Lately, numerous <bot_20>hops have migrated to the Logging Site. The naughty troublemakers make a mess all day long. Workmen complain constantly about picking up their, er, \'jelly beans.\'\"},{\"text\":\"We cannot let those rascally rabbits slow us down. If the wood is not delivered to the village fast enough, construction there will be delayed.\"},{\"text\":\"I\'ve a task for you. Drive the <bot_20>hops out of the Logging Site so the lumberjacks can focus on their work!\",\"action_step\":1},{\"text\":\"\\\"We have three days to deliver to the furniture shop. We cannot be late.\\\"\\n\\n <bot_18> snaps, scanning a list.\"},{\"text\":\"\\\"Not bad for a newbie.\\\"\\n\\n <bot_18> is clearly satisfied by the reduced number of <bot_20>hops.\"},{\"text\":\"This was originally <bot_19>\'s job, but I think you ought to take his place. You\'re more responsible than he ever was. Go to the Garrison Captain. I will speak to him.\"}]', NULL, NULL, NULL, NULL, NULL, 20, NULL, '|0|0|0|0|15|0|', NULL, NULL),
(18, 18, 6, 1, 2, 5971, 1041, '[{\"text\":\"\\\"It\'s about time to send the chopped wood back to the village\\\"\\n\\nCaine explains, scanning the sky.\"},{\"text\":\"You must help me finish off today\'s work if you want your proper salary.\"},{\"text\":\"You\'ll find a good deal of wood in the area. I need you to carry it to me.\"},{\"text\":\"<bot_22> typically pile up the chopped wood under another tree. Take a walk around the Logging Site. It shouldn\'t be hard to find the wood.\",\"action_step\":1},{\"text\":\"[l]You pass the collected lumber to Caine.\"},{\"text\":\"You move pretty fast. What about the others?\"}]', NULL, 31, NULL, NULL, NULL, NULL, NULL, '|15|0|0|0|0|0|', 2, NULL),
(19, 18, 7, 1, 2, 5971, 1041, '[{\"text\":\"What are those <bot_21> doing now? It\'s late, and they haven\'t brought the wood back yet.\"},{\"text\":\"<player>, everyone says I\'m too harsh. I suppose you can judge for yourself now. Only you have returned with the wood. What are all the others doing out there, picking each other\'s noses?\"},{\"text\":\"\\\"Take this hammer and search the site for anyone sleeping on the job. Give \'em a wake up call they won\'t forget!\\\"\\n\\n <bot_18> snarls, his face flushed with fury.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(20, 21, 7, 2, 2, 5246, 1073, '[{\"text\":\"[l]You started to wake up the <bot_21>.\"},{\"text\":\"I\'m awake, don\'t touch me.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(21, 21, 7, 2, 2, 5574, 1009, '[{\"text\":\"[l]You started to wake up the <bot_21>.\"},{\"text\":\"I\'m awake, don\'t touch me.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(22, 21, 7, 2, 2, 6507, 1137, '[{\"text\":\"[l]You started to wake up the <bot_21>.\"},{\"text\":\"I\'m awake, don\'t touch me.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(23, 21, 7, 2, 2, 3817, 1169, '[{\"text\":\"[l]You started to wake up the <bot_21>.\"},{\"text\":\"I\'m awake, don\'t touch me.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(24, 18, 7, 3, 2, 5727, 1009, '[{\"text\":\"These lazy fools are becoming more and more arrogant. It\'s time they take a pay cut! I doubt they\'ll dare slack off on the job again.\"},{\"text\":\"See for yourself. <player>, who started working here just today, has contributed more to our project in one day than you have in your career. Shame on you!\"},{\"text\":\"[rs_22]I was just taking a break. I didn\'t know I was going to fall asleep. It\'s not a big deal.\"},{\"text\":\"Not much of an excuse, you lazy fool! You only get half a day\'s pay today. End of discussion!\"},{\"text\":\"[rs_22]You\'re such a scrooge! Everyone needs a nap now and again!\"},{\"text\":\"\\\"If you argue again, I\'ll take away your breaks,\\\" <bot_18> growls, glaring.\"},{\"text\":\"[rs_22]Fine, fine. I know I made a mistake. I won\'t do it again.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(25, 22, 7, 3, 2, 5539, 1009, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(26, 18, 8, 1, 2, 5727, 1009, '[{\"text\":\"<player>, you have impressed me. I think I can recommend <bot_23> to you, you can make more money with it than working at the <world_1>.\"},{\"text\":\"[l]You nervously explain to <bot_18> that you know nothing about medicine, and you don\'t want to be a burden to the doctor.\"},{\"text\":\"Oh, c\'mon. It can\'t be that hard! You only go to school for, what, eleven, twelve years to become a doctor? No biggie. Just look at it as a learning experience.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(27, 23, 8, 2, 1, 2688, 7345, '[{\"text\":\"Haha, each of those lazy lumberjacks comes to me with the same complaints. Seems like this <bot_18> guy is pretty strict.\"},{\"text\":\"Hello, <player>, how are you?\"},{\"text\":\"[l]*You tell him how you\'re feeling today*\"},{\"text\":\"Mm, okay.\"},{\"text\":\"<bot_18> sent you to me, I know.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(28, 23, 9, 1, 1, 2688, 7345, '[{\"text\":\"At the moment, I can\'t find any physical problems. Maybe you\'re overworked; that can affect your brain, you know. You\'ve got the same symptoms as the other workmen.\"},{\"text\":\"[l]*You consider his reasoning, but it doesn\'t quite make sense. You explain to <bot_23> that perhaps your continuous nightmares have kept you from getting a proper night\'s sleep.*\"},{\"text\":\"I heard <bot_18> sent you here to help out. Luckily, I\'ve got the perfect project for you.\"},{\"text\":\"This should be a simple task for someone like you. Here, take this; it\'ll get your blood flowing.\"},{\"text\":\"You and the other lumberjacks need something - some sort of medicine to refresh your minds and bodies. Something to put some pep back in your step.\"},{\"text\":\"I need you to prepare some <item_32> for me. You can find <bot_24> nearby.\"},{\"text\":\"You need blood from <bot_24>s to make the refreshing medicine. Nothing\'s more invigorating than animal blood, am I right?\",\"action_step\":1},{\"text\":\"[l]*You present <bot_23> with the <item_32> you collected.*\"},{\"text\":\"*<bot_23> crows happily.*\"},{\"text\":\"I\'m going to officially name you as my assistant. When the time is right, I\'ll teach you everything I know! I bet you\'re a fast learner!\"}]', NULL, 32, NULL, NULL, NULL, NULL, NULL, '|10|0|0|0|0|0|', NULL, NULL),
(29, 23, 10, 1, 1, 2688, 7345, '[{\"text\":\"To begin with, approach <bot_14>. She has something for you that will help your task.\"},{\"text\":\"[l]Well.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(30, 14, 10, 2, 1, 9255, 6801, '[{\"text\":\"Oh yes, I was just waiting for you. <bot_23> asked me to give you this.\",\"action_step\":1},{\"text\":\"I don\'t know why, but apparently he wants you to get all kinds of herbs for him.\"},{\"text\":\"We used to have Aliya as an herbalist. I\'m relying on you.\"}]', NULL, NULL, NULL, 26, NULL, NULL, NULL, '|0|0|1|0|0|0|', NULL, NULL),
(31, 23, 10, 3, 1, 2688, 7345, '[{\"text\":\"\\\"I\'ve almost completely ground up these herbs. We should put some <item_33>s into the medicine and grind them together, too...\\\" \\n\\n<bot_23> suddenly pauses to examine the medicine.\"},{\"text\":\"\\\"I used up the mushrooms last time I prepared a medication. I haven\'t had time to seek them out again.\\\" \\n\\n<bot_23> scratches his head, anxious.\"},{\"text\":\"<player>, looks like I need to ask you to go gather some <item_33>s. They typically grow near tree trunks. Take a look around the area, and I\'m sure you\'ll find \'em.\\n\\n(You can find the location of ores, loot, and mobs in the Wiki tab in the voting).\",\"action_step\":1},{\"text\":\"These mushrooms may well be Skandia\'s most valuable resource. A good deal of caravans come to the port just to buy them. I hear they are also excellent in risotto.\"},{\"text\":\"[l]<bot_23> grins when you hand him the <item_33>s.\"},{\"text\":\"\\\"Perfect! These are Skandia\'s most unique mushrooms.\\\" \\n\\n<bot_23> examines them out one by one.\"},{\"text\":\"\\\"You are truly an excellent assistant,\\\" \\n\\n<bot_23> comments, patting you on the back.\"}]', NULL, 33, NULL, NULL, NULL, NULL, NULL, '|32|0|0|0|0|0|', NULL, NULL),
(32, 23, 11, 1, 1, 2688, 7345, '[{\"text\":\"Good! Put them together and give \'em a good grind. Then mix in this medicine, bottle it, and we\'re done! Easy, eh?\"},{\"text\":\"I\'ll let you try this bottle of <item_45>. Trust me, it\'ll make you feel better.\"},{\"text\":\"Wait until your mind stabilizes, then please send the potion to the lumberjacks.\\n\",\"action_step\":1}]', NULL, NULL, NULL, 45, NULL, NULL, NULL, '|0|0|3|0|0|0|', NULL, NULL),
(33, 22, 11, 2, 2, 5539, 1009, '[{\"text\":\"[l]<bot_23> asked me to give this to you\"},{\"text\":\"Oh this <item_45>, a useful thing. At least for 3 days it will be easier.\",\"action_step\":1}]', NULL, 45, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(34, 22, 11, 2, 1, 4861, 5489, '[{\"text\":\"[l]<bot_23> asked me to give this to you\"},{\"text\":\"Oh this <item_45>, a useful thing. At least for 3 days it will be easier.\",\"action_step\":1}]', NULL, 45, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(35, 22, 11, 2, 1, 8322, 6065, '[{\"text\":\"[l]<bot_23> asked me to give this to you\"},{\"text\":\"Oh this <item_45>, a useful thing. At least for 3 days it will be easier.\",\"action_step\":1}]', NULL, 45, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(36, 23, 11, 3, 1, 2688, 7345, '[{\"text\":\"\\\"Drink it up. It\'s the only way you\'ll have the energy to work tomorrow,\\\" <bot_23> says with a grin.\"},{\"text\":\"[l]Something strange is happening to your body. You can feel the potion pumping through your every vein, and you also feel rather nauseous. You quickly explain your symptoms to <bot_23>.\",\"action_step\":1},{\"text\":\"What?! that medicine shouldn\'t have had any side effects. Let me take a closer look.\"}]', '{\"chat\":[{\"text\":\"You drank the potion the gave you Doctor Cal\"},{\"text\":\"You began to feel very dizzy.\",\"broadcast\":1}],\"effect\":{\"name\":\"Dizziness\",\"seconds\":480}}', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(37, 23, 12, 1, 1, 2688, 7345, '[{\"text\":\"Why do you feel dizzy? Could you be allergic to the medicine? Here, look. How many fingers am I holding up?\"},{\"text\":\"[l]While Village <bot_23> is checking up on you, you start feeling nauseous.\"},{\"text\":\"Hey, <player>, what\'s wrong with you? Maybe we ought to induce vomiting...\"},{\"text\":\"[l]You take deep breaths to try to feel better, which seems to help. You start to feel lighter. As you fill your lungs with what... seems like more than just air.\"},{\"text\":\"\\\"Something\'s appeared next to you. And, um, it\'s getting brighter...\\\" <bot_23> tries his best to mask it, but you can tell he is worried.\"},{\"text\":\"[l]You turn and see a Light Orb floating next to you. It emits a faint blue light. You have a feeling that\'s what was causing you to feel ill...\"},{\"text\":\"[l]You immediately ask <bot_23> what\'s going on.\"},{\"text\":\"[l]As you speak, the Blue Light Orb appears as though it\'s listening to what you have to say, hanging onto every word, as it dances around you.\"},{\"text\":\"This... is just a bottle of <item_45>. It couldn\'t do something like this...\"},{\"text\":\"I don\'t understand. I\'ve never seen anyone react this way to my medicine. A possible allergy... no... impossible! I\'m just as interested in finding out what\'s going on as you are!\"},{\"text\":\"[l]A moment of awkward silence passes, and then... <bot_23> -- thinking carefully -- lets out a long breath.\"},{\"text\":\"All I can say is, <player>, um... don\'t worry... Just get yourself to the Statue Ruins right away. There, you\'ll find the Witch\'s Assistant, <bot_33>. Perhaps she can help.\",\"action_step\":1}]', '', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(38, 33, 12, 2, 3, 6144, 1489, '[{\"text\":\"\\\"Originally, there was some sort of connection between these Runic letters. I don\'t know why I didn\'t think about that before,\\\" <bot_33> muses, her head in her hands.\",\"action_step\":1},{\"text\":\"[l]You appear in front of <bot_33>. The look on her face betrays an overwhelming sense of horror.\"},{\"text\":\"\\\"You, too...\\\" <bot_33> murmurs to herself as she stares at the Light Orb.\"}]', '{\"chat\":[{\"text\":\"It seems that the mysterious Light Orb is\\n reacting to you and the people nearby. \\nThis perplexes you even more.\",\"broadcast\":1}]}', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(39, 33, 13, 1, 3, 6144, 1489, '[{\"text\":\"[l]You tell Flower everything that happened and beg for assistance.\"},{\"text\":\"\\\"If I can help you, I\'m more than willing to do so. I\'ve seen something like this before. There was a spirit - a Light Orb - following me, but...\\\"\"},{\"text\":\"[l]Hearing that the same thing had happened to Flower sparks hope in your heart. However, you notice that she is focused on the machine in your hands.\"},{\"text\":\"\\\"Something happened a while ago. Elena needed me to decipher the Runic letters on the statue. She said it was urgent, but she never explained why. However, her request has proven excessively difficult. The first time I translated the spell, it was stolen. Now the Fairies nearby won\'t let me anywhere near the Stone Statue.\\\"\"},{\"text\":\"\\\"First, I need you to help me get rid of those pesky Petal Fairies and Green Fairies. Only then can I freely search for clues about the stone statue.\\\"\",\"action_step\":1},{\"text\":\"\\\"These are all old runic letters, but whoever etched them upon the stone probably didn\'t want others to decipher his message easily. This must be the reason...\\\"\\n\"},{\"text\":\"[l]You tell Flower that you have cleared away the Fairies.\"},{\"text\":\"\\\"I\'m so glad you\'re here to help! Otherwise, I\'d never be able to decipher these runes.\\\"\"}]', '', NULL, NULL, NULL, NULL, 34, 35, '|0|0|0|0|12|12|', NULL, NULL),
(40, 33, 13, 2, 3, 6144, 1489, '[{\"text\":\"[l]Just as you are about to speak, a black shadow suddenly races over to you. Both you and Flower shriek in surprise.\",\"action_step\":1},{\"text\":\"[l]For a moment, you have no idea how to react.\"},{\"text\":\"\\\"Hey, Beebis, come!\\\" Flower tries to pull the ostrich closer, but it ambles away toward you.\"},{\"text\":\"\\\"This is Beebis, one of Elena\'s ostriches. He\'s a bit shy...\\\"\"},{\"text\":\"[rs_36]Flower calls out again, but Beebis pays no attention. It continues pacing around you.\"}]', '{\"chat\":[{\"text\":\"After a moment, you can see that the shadowy figure is actually an ostrich.\"},{\"text\":\"It doesn\'t seem to be afraid to get close to you.\"}]}', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(41, 36, 13, 2, 3, 5973, 1489, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(42, 33, 14, 1, 3, 5866, 1489, '[{\"text\":\"\\\"Ugh, forget it. I have far more important things to do.\\\" Flower sighs, giving up on Beebis. She returns to studying the runes on the Stone Statue.\\n\"},{\"text\":\"[l]You peer at the runes as Flower taps her feet impatiently.\"},{\"text\":\"[rs_36]Beebis continues to stay by your side, watching your every move. It seems very interested in the Light Orb.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(43, 36, 14, 1, 3, 5991, 1489, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(44, 33, 14, 2, 3, 5395, 1585, '[{\"text\":\"\\\"This is useless! There is no translation book for ancient spells. I can\'t possibly decipher all these. Heck, these runes aren\'t even complete! Some of them are mostly worn off.\\\"\\n\"},{\"text\":\"[l]You find small mounds all around the place. Broken pieces of slate are buried beneath them. You report your findings to Flower.\\n\"},{\"text\":\"\\\"Wow! Some fragments of the ruins must be buried under these mounds. Hurry; help me dig. The most important information is carved on these fragments.\\\"\\n\"},{\"text\":\"[l]You ask Flower what she plans to do about the ancient spell translations. Even if you collect all of the shards, she still might not be able to decipher them.\\n\"},{\"text\":\"\\\"I suppose you\'re right. I have two choices. I can defeat the Ruins Giant that stole the translation information... Or I can spend a few days retranslating the ancient spells in Navea.\\\"\\n\"},{\"text\":\"[l]It sounds like the first choice would be much quicker. You ask Flower where you could find the Ruins Giant.\\n\"},{\"text\":\"\\\"You want to take on that beastly thing? He dwells in the Ruins Cave not far from here, but you\'d better be careful.\\\"\\n\"},{\"text\":\"[l]You nod and prepare to set out. Beebis trots behind you eagerly.\\n\"},{\"text\":\"\\\"Beebis, come here. Don\'t get in <player>\'s way!\\\" Flower drags Beebis back.\",\"action_step\":1},{\"text\":\"\\\"How annoying. The most important parts have flaked off. Who knows what was predicted here?\\\"\"},{\"text\":\"\\\"Beebis, stop following <player>. You\'re only causing trouble!\\\" Flower shouts, frustrated.\"},{\"text\":\"[rs_36]Beebis takes a step back, then quickly catches back up with you.\"},{\"text\":\"[l]You hand everything you\'ve found, including the Relic Fragments, Flower. You also give her the secret translation information.\"},{\"text\":\"\\\"Well, at least we found most of the fragments. Let me see. This part should go here, and that one must be placed there...\\\"\"},{\"text\":\"\\\"Ah, it fits together like a puzzle! I can finally decipher the message inscribed in the statue!\\\" Flower claps gleefully.\"}]', NULL, 48, NULL, NULL, NULL, 37, NULL, '|4|0|0|0|1|0|', NULL, NULL),
(45, 36, 14, 2, 3, 5545, 1585, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(46, 33, 15, 1, 3, 6144, 1489, '[{\"text\":\"[l]Your heart races anxiously; you ask Flower about the message on the stone statue.\"},{\"text\":\"\\\"Do you really want to know?\\\" Flower looks at you, eyes welling with anxiety and fear.\"},{\"text\":\"\\\"It means that a hero who will save the world will be born here. But it also says... the hero could become a demon that could destroy the very fabric of our universe.\\\"\"},{\"text\":\"\\\"Not long ago, a teenaged boy named Brian came looking for Elena. Just like you, he also had a spirit with him.\\\"\"},{\"text\":\"\\\"He firmly believed he was cursed by an evil spell, so he wanted to ask Elena for help. But the next day...\\\"\"},{\"text\":\"\\\"Brian\'s personality completely changed. He shattered everything in Elena\'s place and snapped her staff in two... I\'m telling you this because you must stay vigilant.\\\"\"},{\"text\":\"\\\"It\'s clear that Beebis doesn\'t see you as a dangerous person.\\\"\"},{\"text\":\"[l]Worrying that you might end up like Brian, you tentatively ask what happened to him next.\"},{\"text\":\"\\\"Brian fled to the forest. The village garrison is investigating this case right now. Maybe Garrison Member Willie will have some clues.\\\"\\n\"},{\"text\":\"[l]You nod, planning to ask Willie directly.\",\"action_step\":1},{\"text\":\"[rs_36]When you turn to leave, Beebis grabs your sleeve with its beak and gently pulls you back.\"},{\"text\":\"\\\"Hmm, <player>, it looks like you\'ll have to take Beebis with you. Perhaps he can serve as your mount for now. Elena won\'t mind if you take him out for a trip, I\'m sure!\\\"\"}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(47, 36, 15, 1, 3, 5973, 1489, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(48, 11, 15, 2, 3, 2384, 2129, '[{\"text\":\"\\\"Even a small, quaint fishing village like this isn\'t really safe,\\\" Willie mutters sadly.\"},{\"text\":\"\\\"Good lord, <player>! You scared me to death! What are you doing here?!\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(49, 11, 16, 1, 3, 2384, 2129, '[{\"text\":\"\\\"Why are you looking for Brian, too? Do you know him?\\\"\"},{\"text\":\"[l]You shake your head fiercely. \\\"I have an urgent issue, and I must speak to him to clear things up.\\\"\"},{\"text\":\"\\\"You really want to know where he is? He\'s as crazy as the day is long; he\'s been attacking residents! A couple beans short of a chili, if you know what I mean. If you want to investigate, you can talk to Larry, a traveling merchant. He\'s the person Brian most recently attacked.\\\"\"},{\"text\":\"\\\"But before meeting with him, could you help me drive the Sunbirds out of here? They keep stealing our fruit!\\\"\",\"action_step\":1},{\"text\":\"[rs_38]\\\"Ugh, why does stuff like this always happen to me?\\\" he groans with a heavy sigh.\\n\"},{\"text\":\"[rs_38]\\\"Brian, Brian... oh, yeah, the crazy jerk who broke my wagon, right? He smacked me! With his hand!\\\" Larry whines.\"}]', NULL, NULL, NULL, NULL, NULL, 27, NULL, '|0|0|0|0|20|0|', NULL, NULL),
(50, 38, 16, 1, 3, 2198, 2129, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(51, 38, 17, 1, 3, 1376, 1617, '[{\"text\":\"\\\"What? You don\'t know? Well, what can I do? I need to be compensated for this!\\\" Larry roars, frustrated.\"},{\"text\":\"\\\"I\'m sorry. I know it\'s not your fault. I just need to calm down.\\\" He sighs again.\"},{\"text\":\"[l]You see goods scattered about and wonder if you should help him with the investigation.\"},{\"text\":\"\\\"You still want to assist me? Could you just help me bring those scattered boxes over here? Please?\\\"\",\"action_step\":1},{\"text\":\"You help shove the scattered crates back to Larry\'s side.\"},{\"text\":\"\\\"Luckily, we only lost one box of food,\\\" Larry grunts.\\n\"},{\"text\":\"Glad you\'re here to help. I couldn\'t move all this stuff on my own. I\'m Larry, by the way, and I\'m a traveling merchant.\\\" He sticks out his hand. You shake it gamely, then introduce yourself to him.\"}]', NULL, 49, NULL, NULL, NULL, NULL, NULL, '|8|0|0|0|0|0|', 2, NULL),
(52, 38, 18, 1, 3, 1376, 1617, '[{\"text\":\"\\\"<player>, I\'m afraid I need to ask you for another favor.\\\" Larry rummages through the scattered goods and fishes out a package.\"},{\"text\":\"\\\"I\'d like to ask you to send this package to my friend Bentley on my behalf. It\'s just some dried food he asked me to buy for him several days ago. Though the other items were destroyed, this will prevent him from going hungry, at least.\\\"\"},{\"text\":\"\\\"There\'s something in it for you, too. This friend of mine has been hunting here for ages, so he knows this forest like the back of his hand. Perhaps he can help you find Brian.\\\"\",\"action_step\":1}]', NULL, NULL, NULL, 50, NULL, NULL, NULL, '|0|0|1|0|0|0|', NULL, NULL),
(53, 39, 18, 2, 3, 10880, 2545, '[{\"text\":\"\\\"Well, what really happened to Brian,\\\" Bentley mutters, pressing down on a bleeding wound.\",\"action_step\":1},{\"text\":\"[l]You hand the package to Bentley and explain why you\'ve come. He winces as he listens to you; you quickly realize he has been badly injured.\"},{\"text\":\"\\\"Beebis? Why is Elena\'s house ostrich here with you?\\\" Bentley clutches his bleeding wound, staring into Beebis\'s beady eyes.\"},{\"text\":\"[l]You explain why Beebis is following you, then inquire about Bentley\'s injuries.\"},{\"text\":\"\\\"It\'s Brian. I just don\'t know what he\'s up to. I just saw him dragging a child deep into the forest. I approached him; I never thought he\'d attack me so viciously.\\\"\"},{\"text\":\"[rs_36]Bentley grinds his teeth, clearly in immense pain. Beebis rests his feathered head on your shoulder as though he, too, is concerned.\"}]', NULL, 50, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(54, 36, 18, 2, 3, 10701, 2545, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(55, 36, 19, 1, 3, 10701, 2545, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(56, 39, 19, 1, 3, 10880, 2545, '[{\"text\":\"\\\"This is a pretty deep wound. I\'m probably going to have to do something about it, eh?\\\" Bentley winces.\"},{\"text\":\"[l]You inspect the festering wound with concern, then ask Bentley what you can do to help him.\"},{\"text\":\"\\\"A bunch of Grass Sprouts reside in the forest. The slime they produce can stop the bleeding and help the wound heal. Please collect some slime and fluid from the sprouts.\\\"\"},{\"text\":\"\\\"Beebis, follow <player>.\\\" Bentley swallows hard, hiding his pain badly, and shoos you and Beebis away.\",\"action_step\":1},{\"text\":\"\\\"Gasp... gasp... cough, cough!\\\"\"},{\"text\":\"Bentley quickly mixes two liquids and applies the viscous solution to his laceration. The endless bleeding from the wound begins to slow. The pained look fades from Bentley\'s face.\"}]', NULL, 52, 53, NULL, NULL, NULL, NULL, '|4|4|0|0|0|0|', NULL, NULL),
(57, 39, 20, 1, 3, 10880, 2545, '[{\"text\":\"\\\"<player>, thank you. I feel much better...\\\" Bentley nods weakly.\"},{\"text\":\"\\\"You know you have a Light Orb following you, right? Or am I hallucinating?\\\"\"},{\"text\":\"[l]You feel utterly depressed after hearing that from Bentley. You both gaze at the Spirit warily. Beebis cocks his head, watching the Orb carefully.\"},{\"text\":\"[l]You explain to Bentley that you need to rid yourself of the spirit. However, you are still determined to find Brian and talk with him.\"},{\"text\":\"\\\"How will that fool help? I have a good friend who might be able to actually lend a hand.\\\" Bentley opens a small bag hanging around his waist. From inside the bag, he removes an amulet.\"},{\"text\":\"\\\"I\'m gonna lend you this amulet. It\'s a token of friendship between Elena and I. Go to Elena\'s house with this, and I know she\'ll come up with a way to help you.\\\"\",\"action_step\":1}]', NULL, NULL, NULL, 54, NULL, NULL, NULL, '|0|0|1|0|0|0|', NULL, NULL),
(58, 42, 20, 2, 3, 10115, 3665, '[{\"text\":\"\\\"There\'s nothing wrong with the content deciphered by Flower. So why on earth would the creator choose a small, insignificant place like Skandia?\\\"\"},{\"text\":\"\\\"Stop! You take one step forward, and you\'ll be sorry!\\\" Fire flashes through Elena\'s eyes as she prepares to cast magic, but her demeanor immediately softens when she sees the spirit next to you... and her beloved Beebis.\"},{\"text\":\"[l]You quickly explain that you need her help, handing her the amulet from Bentley.\",\"action_step\":1},{\"text\":\"\\\"You... you have a spirit. That boy Brian, he had one, too,\\\" Elena recalls coldly. \\\"At first, I wanted to help him, but his spirit transformed into a sinister black panther.\\\"\"},{\"text\":\"\\\"He and the panther ran into the house and attacked me, then broke my staff in two...\\\"\"}]', NULL, 54, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(59, 36, 20, 2, 3, 10342, 3665, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(60, 42, 21, 1, 3, 10115, 3665, '[{\"text\":\"[rs_36]Beebis quietly follows behind you.\"},{\"text\":\"Elena stares at you, suspiciously, before her gaze shifts to Bentley\'s token.\"},{\"text\":\"\\\"It\'s a real surprise. Beebis -- who is usually quite anxious around strangers -- seems to have a certain fondness for you. You must not be very dangerous.\\\" Elena arches an eyebrow, analyzing you.\"},{\"text\":\"[l]Sensing that she is softening toward you, you ask Elena if she can help you remove the spirit following you.\"},{\"text\":\"\\\"Any friend of Bentley\'s is a friend of mine. However, you are going to have to help me fix my staff before I can cast any such spell.\\\"\"},{\"text\":\"\\\"The staff lost all its energy when Brian attacked me. I need your help. You must help me power it up again.\\\" Elena removes three brightly colored gems from a bag.\"},{\"text\":\"\\\"This is the Water Gem, Tree Gem, and Magic Gem. You should go to the Wharf, Logging Site, and Ruins with the corresponding gems.\\\"\"},{\"text\":\"\\\"I will use teleportation magic to send you back to Port Skandia when you\'re ready. With Beebis\' help, you shouldn\'t have too much trouble.\\\"\",\"action_step\":1}]', '{\"chat\":[{\"text\":\"You need to use the services of Craftsman\"}],\"teleport\":{\"x\":10129,\"y\":8561,\"world_id\":1}}', NULL, NULL, 55, NULL, NULL, NULL, '|0|0|3|0|0|0|', NULL, NULL),
(61, 36, 21, 1, 3, 10342, 3665, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(62, 42, 21, 2, 3, 10115, 3665, '[{\"text\":\"\\\"Have all three gems recovered their power? We\'ve no time to lose. You should get this done as quickly as possible.\\\"\",\"action_step\":1},{\"text\":\"[l]You return to Elena and give her the three gems, each practically vibrating with energy.\"},{\"text\":\"[l]After Elena embeds the three gems in her staff, they begin to radiate dazzling rays of brilliant light.\"},{\"text\":\"\\\"Ah, they\'ve regained their power. Next, I will cast the exorcism spell on you. If it goes as planned, it will remove the Spirit that follows you.\\\"\"},{\"text\":\"\\\"However, if it fails, and the spirit takes you over, I will have no choice but to use my magic to defeat you. Do you understand?\\\"\"}]', NULL, 56, NULL, NULL, NULL, NULL, NULL, '|3|0|0|0|0|0|', NULL, NULL),
(63, 42, 22, 1, 3, 10115, 3665, '[{\"text\":\"\\\"<player>, are you ready? I have some special magic I can try on that spirit. As for the result, well, we\'ll just have to wait and see.\\\"\"},{\"text\":\"\\\"...\\\"\"},{\"text\":\"Elena waves her wand and recites an incantation you have never heard before.\"},{\"text\":\"[l]A warm mist fills the air. Elena\'s magic makes you feel as though you are floating, which makes sense, because when you open your eyes, you are hovering in midair.\"},{\"text\":\"[l]You try to relax as the winds whips around you, but you can\'t help but let out a small shriek of surprise.\",\"action_step\":1},{\"text\":\"\\\"<player>, are you okay? It might feel a little uncomfortable, but it shouldn\'t be any worse than a light fever. A little grogginess shouldn\'t be a problem for you, right?\\\"\"},{\"text\":\"\\\"<player>?!\\\"\"},{\"text\":\"Elena cries out when she sees the pain on your face and immediately stops reciting the incantation.\"},{\"text\":\"The light of the spell extinguishes; you collapse to the ground in pain.\"},{\"text\":\"[l]The spirit that had haunted you ominously changes its form into a creature you have never seen before.\"}]', '{\"chat\":[{\"text\":\"You want to answer, but you can\'t say a word. You try to resist the pressure of the whirling air; it seems to grow hotter and hotter with each passing second.\"},{\"text\":\"The air is suddenly violent, as though it is trying to swallow you whole. An electric pain shoots through your body.\"}],\"effect\":{\"name\":\"Suppression\",\"seconds\":30}}', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(64, 11, 23, 1, 3, 10441, 3665, '[{\"text\":\"[l]Willie and Elena\'s conversation makes you dizzy and you can hardly focus on their words. Trying to guide the subject away from the weird creature, you ask Willie where you can find Kane.\"},{\"text\":\"\\\"The Garrison Captain is currently in the north-eastern corner of the forest. Reports were saying that Brian was spotted hiding in a nearby cave.\\\"\"},{\"text\":\"\\\"So... so I guess I\'m done here. I, uh, gotta go then!\\\"\"},{\"text\":\"[rs_42]\\\"<player>, I think you should seek out Kane in a hurry. Here, let this ostrich take you there. Maybe we can figure out from Brian what kind of... creature this thing is.\\\"\"},{\"text\":\"[l]You nod and turn to go, yet the strange creature is following your every move!\\n\"},{\"text\":\"[rs_43]\\\"Please take me with you, you\'ll need my help!\\\"\"},{\"text\":\"[l]You sigh helplessly and allow the strange creature to follow you around. You have to focus on finding Brian; then you can figure out what to do with your strange possession situation.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(65, 43, 23, 1, 3, 10232, 3665, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(66, 42, 23, 1, 3, 10115, 3665, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(67, 44, 23, 2, 2, 5643, 785, '[{\"text\":\"\\\"Such confounded...\\\"\"},{\"text\":\"\\\"<player>, what are you doing here? It\'s dangerous out here, so just go on back to the village...\\\" Kane spots the creature next to you, staggering back with surprise.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(68, 44, 24, 1, 2, 5643, 785, '[{\"text\":\"\\\"<player>... you\'ve got a monster on your back, eh? Are you bound by a curse, too?\\\" Kane keeps one hand on his weapon as he sizes you up.\"},{\"text\":\"[l]Kane\'s words pain you; you have to find Brian and ask him to clarify the situation. You hope Kane can tell you where Brian is.\"},{\"text\":\"\\\"Brian took the Villager Chief\'s son, Joel. When we tried to save the boy, a black panther attacked us. They\'re hiding in the cave now.\\\"\"},{\"text\":\"[l]You tell Kane that the creature with you might have something to do with Brian, hoping he\'ll allow you into the cave to find Brian. You have to find a way to rescue Joel!\"},{\"text\":\"\\\"I don\'t know what to do. You two are both cursed by spirits, so perhaps you are bound together somehow... Hopefully, through Brian, you can save yourself.\\\"\",\"action_step\":1}]', '{\"chat\":[{\"text\":\"You need to complete a dungeon\"},{\"text\":\"You can enter in the callvotes menu\"}]}', NULL, NULL, NULL, NULL, 19, 28, '|0|0|0|0|1|5|', NULL, NULL),
(69, 16, 24, 2, 1, 6611, 7569, '[{\"text\":\"\\\"...\\\"\"},{\"text\":\"\\\"Joel, are you okay? Are you hurt?\\\" York examines the boy frantically, searching for wounds.\"},{\"text\":\"[rs_12]\\\"Dad, Brian... he\'s not... he\'s transformed... I was so scared...\\\"\"},{\"text\":\"\\\"Don\'t be afraid, son. Hush. Daddy\'s here. It will all be alright.\\\" York glances at you with unease. Will it really be okay?\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(70, 12, 24, 2, 1, 6411, 7569, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(71, 16, 25, 1, 1, 6611, 7569, '[{\"text\":\"\\\"<player>, I owe you big time... But, um, could you tell us exactly what happened?\\\"\"},{\"text\":\"[l]York stares at you as the other look on in silence. You will need a convincing explanation about everything that has happened.\",\"action_step\":1},{\"text\":\"[rs_10]\\\"...\\\"\"}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(72, 10, 25, 1, 1, 6196, 7569, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(73, 12, 25, 2, 1, 6411, 7569, '[{\"text\":\"\\\"Mom, Dad, why are you looking at <player> that way? This hero saved me from a serious baddie. Without my savior, I would never have seen you again.\\\"\"},{\"text\":\"[rs_16]\\\"Joel... Dad just thinks...\\\" York sighs. You see a tinge of fear in his eyes.\"},{\"text\":\"[rs_10]\\\"Joel, we know. But there are still a couple of things we have to figure out...\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(74, 16, 25, 2, 1, 6611, 7569, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(75, 10, 25, 2, 1, 6196, 7569, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(76, 42, 25, 3, 3, 10113, 3665, '[{\"text\":\"\\\"Are you okay? Did you find any clues from Brian?\\\" Elena looks at you anxiously.\"},{\"text\":\"[l]You tell Elena what Brian said and ask her if the same thing will happen to you. Will you become a bloodthirsty, savage monster, too?\"},{\"text\":\"\\\"I... I have no idea. I really am sorry. I just can\'t say.\\\" Elena sighs regrettably.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(77, 12, 25, 3, 3, 10419, 3665, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(78, 16, 25, 3, 3, 10245, 3665, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(79, 10, 25, 3, 3, 10362, 3665, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(80, 44, 25, 4, 3, 1790, 2161, '[{\"text\":\"\\\"Chief York! <player> shouldn\'t stay in Port Skandia. Nobody can guarantee this freak won\'t go crazy. For everyone\'s safety, <player> should leave.\\\"\"},{\"text\":\"[rs_16]\\\"...\\\"\"},{\"text\":\"[rs_10]\\\"Kane! Bite your tongue!\\\"\"},{\"text\":\"[rs_12]\\\"Dad! <player> is a good person! You\'re just going to send my hero away?\\\"\"},{\"text\":\"[rs_42]\\\"Kane, how could you say such a thing?!\\\"\"},{\"text\":\"\\\"Am I lying? You saw what became of Brian. How can I be sure the same won\'t happen to <player>? Anyone under the curse should leave for the good of our village!\\\"\",\"action_step\":1},{\"text\":\"[rs_10]\\\"<player>, we know you are a true warrior. If you have a problem, you can let us know. You needn\'t bear the entire burden yourself.\\\"\"},{\"text\":\"[l]Sheila rests a hand on your shoulder tenderly; you can\'t help but shed a single tear. All of the suffering and fear you have hidden deep within yourself suddenly leaps to the surface.\"},{\"text\":\"[rs_10]\\\"I think that\'s enough for the day. Everyone is exhausted. You\'ll still have to say something eventually, but for now, let\'s all get some rest.\\\"\"}]', '{\"chat\":[{\"text\":\"After some time!\"}],\"teleport\":{\"x\":6411,\"y\":7345,\"world_id\":1}}', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(81, 42, 25, 4, 3, 1905, 2161, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(82, 12, 25, 4, 3, 1664, 2161, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(83, 16, 25, 4, 3, 1555, 2161, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(84, 10, 25, 4, 3, 1600, 2161, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(85, 45, 26, 1, 1, 6430, 7345, '[{\"text\":\"\\\"If you need to talk...\\\" Since the spirit appeared, it seems that everyone wants to chat. However, after what happened with Brian, you are fearful of what the future will bring.\"},{\"text\":\"[l]You hang you head and release a soft sigh. <player> continues speaking as if it understands how you feel.\"},{\"text\":\"\\\"I\'m Otohime, an Eidolon. We exist everywhere you are and everywhere you are not, and I hide within the transforming power of your spirit.\\\"\"},{\"text\":\"\\\"They say we are the very origin of life, that we connect you with Gaia...\\\"\"},{\"text\":\"[l]Otohime\'s words render you speechless. Gaia is a name you\'ve heard in legends since childhood. At least it isn\'t more talk of an evil spirit or a curse.\"},{\"text\":\"\\\"Right now, I grow with the power of your spirit. But if your spirit gets weak and frail, darkness can devour you, just as it did Brian. You\'ll lose yourself; evil will distort your mind.\\\"\"},{\"text\":\"[l]If what the creature says is true, you could follow the same path as Brian, and because of that, you have no choice but to leave the village.\",\"action_step\":1}]', NULL, NULL, NULL, 57, NULL, NULL, NULL, '|0|0|1|0|0|0|', NULL, NULL),
(86, 10, 26, 2, 1, 7811, 7921, '[{\"text\":\"\\\"<player>, did you get a good night\'s sleep?\\\"\"},{\"text\":\"[l]You tell Sheila that you have decided to leave the village, and you plan to find a way to separate from your spirit, even if you have to travel to the ends of Earth.\"},{\"text\":\"\\\"I thought you might come to that conclusion. There\'s nothing I can really say now that you\'ve made up your mind.\\\"\",\"action_step\":1},{\"text\":\"\\\"It\'s just better this way...\\\"\"}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(87, 13, 26, 3, 1, 8141, 7089, '[{\"text\":\"\\\"You\'ve really decided to leave?\\\" Anita peers at you curiously.\"},{\"text\":\"[l]You nod.\"},{\"text\":\"\\\"Well... you\'re just a kid. You need to take care of yourself. If somebody bullies you, you come back and let me know.\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(88, 14, 26, 3, 1, 9255, 6801, '[{\"text\":\"\\\"Are you really leaving today? I thought you\'d stay for a few more days.\\\" There is an edge of sadness to Betsy\'s smile.\"},{\"text\":\"\\\"I truly believe you will succeed. You\'re different from Brian. I know it in my very soul.\\\" Betsy nods with affirmation.\"},{\"text\":\"\\\"Go ahead! Experience the brilliant journey of life and be sure to tell everyone about the wonders of Port Skandia! We could use some tourist money.\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL);
INSERT INTO `tw_bots_quest` (`ID`, `BotID`, `QuestID`, `Step`, `WorldID`, `PosX`, `PosY`, `DialogData`, `EventData`, `RequiredItemID1`, `RequiredItemID2`, `RewardItemID1`, `RewardItemID2`, `RequiredDefeatMobID1`, `RequiredDefeatMobID2`, `Amount`, `InteractionType`, `InteractionTemp`) VALUES
(89, 15, 26, 3, 1, 6561, 6449, '[{\"text\":\"\\\"<player>, I\'ve heard about your situation. What have you decided to do?\\\"\"},{\"text\":\"[l]You shrug helplessly and inform Corey that you are considering looking for someone to help you. Somebody out there has to have a solution to your terrible dilemma.\"},{\"text\":\"\\\"I see that the spirit has no intention of harming anyone. Perhaps, in time, it could become your companion.\\\" Corey smiles encouragingly at your Eidolon.\"},{\"text\":\"\\\"Take it easy, and be careful. Nothing too exciting ever happens in this tiny town. The world outside this place - well, now there\'s an adventure!\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(90, 10, 26, 4, 1, 7811, 7921, '[{\"text\":\"\\\"Have you met everyone you needed to meet? <player>, do you know where to go?\\\" Sheila asks sorrowfully.\"},{\"text\":\"[l]You nod, though you truly aren\'t sure. Maybe it would be better to go to a big city with lots of people to ask about the issue. Perhaps you\'ll find someone who knows how to separate the spirit from your body.\"},{\"text\":\"\\\"I don\'t know if this will help, but I once heard from a friend from Helonia about something called a Starlight Treasure Chest. Legend has it that an item within it can chase evil spirits away.\\\"\"},{\"text\":\"[l]Upon hearing that, you brighten a bit. The Helonia Coast is located to the northwest of Port Skandia. You decide to go there and investigate.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(91, 10, 27, 1, 1, 7811, 7921, '[{\"text\":\"\\\"Once you get to the Helonia Coast look for a merchant named Luther. Tell him that you\'re looking for a Starlight Treasure Chest.\\\"\"},{\"text\":\"[l]You nod. The news the Village Chief\'s wife gave you sparks new hope in your heart. Your journey now has a purpose.\"},{\"text\":\"\\\"Did you pack all your things? You\'ve got everything you need, right?\\\" Sheila asks fretfully.\"},{\"text\":\"[l]You take inventory of all your equipment and suddenly notice that Beebis has followed you the whole time.\"},{\"text\":\"[rs_36]Beebis seems tranquil, as though he is totally unaware of the tragedy that has taken place.\"},{\"text\":\"[l]You affectionately ruffle Beebis\'s feathers; it is time to say goodbye. You have only spent a short amount of time together, but with Beebis by your side, you feel you aren\'t alone.\"},{\"text\":\"[l]You tell Sheila that you feel much better now, and you prepare to hit the road.\"},{\"text\":\"\\\"Silly child! You\'re one of us: a citizen of Skandia. And Beebis is your companion now. We couldn\'t possibly just let you two leave without a proper goodbye!\\\"\"},{\"text\":\"\\\"Go quickly! Don\'t keep everyone waiting.\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(92, 36, 27, 1, 1, 7940, 7921, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(93, 47, 27, 2, 5, 8585, 1873, '[{\"text\":\"\\\"What do you want to buy? I can help you find it more quickly.\\\"\"},{\"text\":\"\\\"Welcome to Helonia. Are you here on business? Helonia\'s a wonderful place!\\\"\"},{\"text\":\"[l]You politely introduce yourself to Luther and mention the Starlight Treasure Chest. You also explain what Sheila had said.\",\"action_step\":1},{\"text\":\"\\\"Sheila told you to look for me?\\\" Luther scratches his head.\"},{\"text\":\"\\\"Well, I know about a Starlight Treasure Chest, but it\'s not mine. Madeline, Mayor of Helonia, has it.\\\"\"}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(94, 48, 28, 1, 5, 5558, 1617, '[{\"text\":\"Madeline paces tautly. She glances at you, but it is clear she doesn\'t want to talk.\"},{\"text\":\"[l]You force a friendly smile and greet Madeline, then ask about borrowing the treasure chest.\"},{\"text\":\"\\\"I have more important matter to attend to right now, and you\'re not a resident of this town. Why should I give you the Starlight Treasure Chest?\\\"\"},{\"text\":\"\\\"If there\'s nothing else I can help you with, you should probably go.\\\"\"},{\"text\":\"Madeline closes her eyes for a moment, sighing deeply. \\\"We\'re still missing half. What are we going to do? Why can\'t Selena understand?\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(95, 47, 28, 2, 5, 8585, 1873, '[{\"text\":\"\\\"You don\'t look too good. I guess she didn\'t have the time to solve your problem, eh?\\\"\"},{\"text\":\"\\\"But I did say I\'d try to help you, and I\'m a man of my word. I\'ll think of a way to get you out of your pickle.\\\" Luther strokes his chin, thinking.\"},{\"text\":\"\\\"Maybe...\\\" His eyes light up. You lean in eagerly.\"},{\"text\":\"\\\"This protection money issue is really putting pressure on the Town Mayor. If you can help her out, I bet she\'d be willing to help you, too.\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(96, 47, 29, 1, 5, 8585, 1873, '[{\"text\":\"\\\"As far as I know, the pirates are demanding, among other things, the fruit that grows on the Helonia Coast. It\'s quite famous. It\'s Selena\'s favorite.\\\" Luther blushes scarlet.\"},{\"text\":\"[eidolon]\\\"Who\'s Selena? And why does she make you so nervous?\\\" <eidolon> pipes up.\"},{\"text\":\"\\\"Um, Selena is the Town Mayor\'s daughter. We grew up together.\\\" Luther smoothes his hair, clearly flustered.\"},{\"text\":\"[eidolon]\\\"Your pulse has quickened. That\'s what happens when humans feel the emotion known as \'fear.\' Is this Selena a wretched monster of some kind?\\\"\"},{\"text\":\"\\\"No, not at all! Selena is a wonderful, beautiful girl. I... I\'ve actually always liked her...\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(97, 48, 29, 2, 5, 5558, 1617, '[{\"text\":\"\\\"I haven\'t the energy to think about anything else. Dealing with these Pirates is no easy task.\\\"\"},{\"text\":\"\\\"Helonia didn\'t produce enough fruit this year. I don\'t know what to do.\\\"\"},{\"text\":\"\\\"The problems seem insurmountable. Not only do we have a severe shortage, but the Tanuki outside the village keep disrupting the villagers with their incessant fighting.\\\"\",\"action_step\":1},{\"text\":\"[l]You place the pile of collected fruit in front of Madeline.\"},{\"text\":\"Madeline raises a surprised eyebrow. She cocks her head, staring at you.\"},{\"text\":\"[rs_36]Suddenly, Beebis snatches up a piece of fruit and gulps it down.\"},{\"text\":\"[eidolon]\\\"Hey! Stop that! That is your master\'s property,\\\" Snaps Alessa, moving the fruit out of Beebis\'s reach.\"},{\"text\":\"Madeline still looks utterly puzzled. \\\"Where did this fruit come from?\\\"\"},{\"text\":\"[l]You tell her about Luther; he hopes that the fruit gathered from the Tanuki will solve Helonia\'s dilemma.\"},{\"text\":\"\\\"That Luther is a smart one. If Selena were half as mature as he is, I wouldn\'t have to worry so much.\\\" Madeline sighs heavily.\"},{\"text\":\"\\\"And, um, thank you. I apologize for my rudeness. I just have way too much on my mind.\\\"\"}]', NULL, 67, NULL, NULL, NULL, 49, NULL, '|7|0|0|0|12|0|', NULL, NULL),
(98, 36, 29, 2, 5, 5428, 1617, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(99, 48, 30, 1, 5, 5558, 1617, '[{\"text\":\"[l]You ask Madeline if there is some way you can help.\"},{\"text\":\"Madeline sighs again and shakes her head. \\\"I just have to keep calm, even in the face of the pirates. If I panic, everyone else will, too.\\\"\"},{\"text\":\"\\\"My daughter Selena and I are fighting, too. We exchanged words, and she just up and left. I haven\'t heard from her since. I can\'t help but worry about her!\\\"\"},{\"text\":\"\\\"I can\'t think clearly lately. The Tanuki outside the town squawk and squeak constantly. \"},{\"text\":\"They just never shut up! I\'m begging you. Please get rid of them; I\'ve had a migraine for months!\\\"\",\"action_step\":1},{\"text\":\"\\\"I\'ll consider what you said, but I must deal with the pirates first. Find Luther if you need something.\\\"\"}]', NULL, NULL, NULL, NULL, NULL, 49, NULL, '|0|0|0|0|8|0|', NULL, NULL),
(100, 47, 30, 2, 5, 8585, 1873, '[{\"text\":\"\\\"The Tanuki outside the village are growing more rambunctious by the day. The Town Mayor is about to lose her mind. She must be under so much pressure. I wish Selena were a bit more understanding.\\\"\",\"action_step\":1},{\"text\":\"You explain to Luther that you\'ve already driven off the Tanuki, then you tell him what the Town Mayor said.\"},{\"text\":\"\\\"Thank you for your hard work, <player>. The Town Mayor said she might consider your Starlight Treasure Chest request. That\'s a huge win for you!\\\"\"},{\"text\":\"\\\"When I\'m done helping the Town Mayor inventory the supplies, I\'ll go looking for Selena. For now, I\'m sure the Mayor just appreciates the quiet.\\\"\"}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(101, 47, 31, 1, 5, 8585, 1873, '[{\"text\":\"[l]You ask Luther why the townsfolk don\'t just band together to resist the Pirates.\"},{\"text\":\"Luther releases a long sigh. \\\"The people here just want to live in peace. If they can solve the problem by throwing money at it, that\'s what they\'ll do.\\\"\"},{\"text\":\"\\\"But not everyone shares this viewpoint. If I remember correctly, the younger of the Fisherman Brothers, Braeden, doesn\'t want to hand over the cash or goods.\\\"\"},{\"text\":\"\\\"Perhaps you can find his older brother, Isaac, and speak to him. Maybe you can come up with a way to convince Braeden. We could really use Braeden\'s help collecting supplies.\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(102, 50, 31, 2, 5, 11502, 2193, '[{\"text\":\"[l]You relay Luther\'s words to Isaac and explain that you want to convince Braeden to hand over the goods.\"},{\"text\":\"\\\"I\'m worried about Braeden. His refusal to hand over the supplies might provoke the pirates.\\\" Isaac sighs.\"},{\"text\":\"\\\"My brother won\'t listen to me. There will always be more fish to catch; a few trout aren\'t worth his life. He just doesn\'t understand.\\\"\"},{\"text\":\"\\\"Well, I guess since you\'re willing to convince him, it\'s worth a try.\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(103, 51, 31, 3, 6, 6551, 3505, '[{\"text\":\"\\\"Can I help you?\\\"\"},{\"text\":\"[l]You explain to Braeden why you have come, but he seems increasingly annoyed.\"},{\"text\":\"\\\"Let me tell you something. I won\'t be threatened. I won\'t give those pirates a dime!\\\" Braeden seethes.\"},{\"text\":\"[l]You tell Braeden that you agree, but resisting will only alert the pirates to your plan.\"},{\"text\":\"Braeden stares at you a moment, thinking.\"},{\"text\":\"\\\"You know what? You\'re right. I didn\'t think of that. We can\'t just blatantly resist them.\\\"\"},{\"text\":\"\\\"I\'ll prepare the supplies for the pirates, but I have a few tricks up my sleeve.\\\"\"},{\"text\":\"[l]You nod, but insist that the entire town still needs to fully prepare if they hope to fight back.\",\"action_step\":1},{\"text\":\"\\\"You know, I just met you, but I think we\'re gonna get along just fine.\\\" Braeden grins.\"},{\"text\":\"\\\"Count me in. I\'ve got some ideas on how to take down the pirates. I\'ll prepare all the supplies before executing the plan.\"}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(104, 51, 32, 1, 6, 6551, 3505, '[{\"text\":\"\\\"I\'ll take all the fish that we\'ve caught recently to a nearby hut. Add that to the fish stock, and we should meet the quota.\\\"\"},{\"text\":\"\\\"It won\'t be so easy to get the fish to the pirates, though.\\\" Braeden bites his lips, thinking.\"},{\"text\":\"\\\"The pirates\' activity has forced Sun Crabs to gather near town. One of \'em is larger and stronger than the rest. The townsfolk call him Mobray the Giant Crab.\\\"\"},{\"text\":\"\\\"Unfortunately, the crabs are converging on the very place where I put all the fish.\\\"\"},{\"text\":\"[l]You tell Braeden that you will help him fend off the Crabs and bring back the catch.\"},{\"text\":\"\\\"The hut containing the fish is in the southern section of the eastern coast. Don\'t underestimate those Sun Crabs, though. Their pincers can snap straight through human bone.\\\"\",\"action_step\":1},{\"text\":\"\\\"After we get the catch back, we need to take inventory and add it to the payment. I don\'t want to give even one extra fish to those thieving cads.\\\"\"},{\"text\":\"\\\"The fish are stored in a hut south of the eastern coast. but watch out for the blasted Sun Crabs.\\\"\"},{\"text\":\"[l]Braeden grins widely as you present the catch to him.\"},{\"text\":\"\\\"I saw what you did on the coast!\\\" Braeden crows, his eyes full of awe.\"},{\"text\":\"\\\"I\'ve never seen someone with such skill. Every last Sun Crab scampered away, terrified. You even defeated Mobray!\\\"\"},{\"text\":\"\\\"I had no idea you had such battle prowess. You could teach me a thing or two, partner.\\\"\"}]', NULL, 68, NULL, NULL, NULL, 52, 53, '|1|0|0|0|12|1|', NULL, NULL),
(105, 51, 33, 1, 6, 6551, 3505, '[{\"text\":\"\\\"We\'ll prepare the supplies and carry out our plans against the Pirates simultaneously, so listen close.\\\"\"},{\"text\":\"\\\"I plan to lead the entire band of pirates to an open area, then use gunpowder to blast \'em to high heaven in one fell swoop.\\\"\"},{\"text\":\"\\\"I\'ve already told others in town, and we\'ve a good deal of gunpowder. We need to refine the powder to transform it into a powerful bomb.\\\"\"},{\"text\":\"[l]You ask how you can help.\"},{\"text\":\"\\\"We need this bomb to do some serious damage. Will you help collect materials for the bomb\'s outer casing?\\\"\"},{\"text\":\"\\\"Collect some of the nearby Fruit. Once we soak and dry \'em, we can use them as shrapnel. That\'ll give our bomb some extra punch.\\\"\"},{\"text\":\"\\\"I\'ll stay here and work on dividing up the gunpowder. We don\'t have a lot of time to prepare, so it\'s best if we divide and conquer!\\\"\"},{\"text\":\"\\\"We don\'t have much time. You must quickly find the Fruit. We can\'t do anything without \'em,\\\" Braeden urges eagerly.\",\"action_step\":1}]', NULL, 71, NULL, NULL, NULL, NULL, NULL, '|24|0|0|0|0|0|', 3, NULL),
(106, 51, 33, 2, 6, 6551, 3505, '[{\"text\":\"\\\"Now go soak them, but you have to do it right, contact Craftsman for that, he will help you.\\\"\",\"action_step\":1},{\"text\":\"[l]You hand the sopping wet Cleaned Fruit to Braeden.\"},{\"text\":\"\\\"Just you wait, you filthy pirates! You won\'t trifle with the good people of Helonia again!\\\" Braeden exclaims.\"},{\"text\":\"\\\"Okay, I\'ll dry out the Cleaned Fruit, then fill them with gunpowder. These will teach those cads a lesson they won\'t soon forget.\\\"\"},{\"text\":\"\\\"I\'ve considered the options carefully, but the best place for our plan is at the beach right in front of us.\\\"\"}]', NULL, 70, NULL, NULL, NULL, NULL, NULL, '|24|0|0|0|0|0|', NULL, NULL),
(107, 51, 34, 1, 6, 6551, 3505, '[{\"text\":\"\\\"If we want to set our bombs on the beach, I need you to help prepare some waterproof paint.\\\"\"},{\"text\":\"[l]You anxiously ask Braeden to be more specific.\"},{\"text\":\"\\\"West of Helonia, you\'ll find Helonia Woods. Forest Beetles live there, and their gooey bodily secretions cause water to bead up. The goo is used as a sealant on many ships.\\\"\"},{\"text\":\"\\\"The mist and fog are thick around the coast. If those bombs aren\'t waterproof, it could compromise the whole plan.\\\"\"},{\"text\":\"\\\"We\'ve gotta use the secretions to keep the gunpowder dry. Please help me collect the secretions of the Forest Beetles. I\'ll do my best to finish my current task by the time you get back.\\\"\",\"action_step\":1},{\"text\":\"\\\"I\'ve got to think everything through and lay out the plans completely. Maybe if I present the townsfolk with concrete ideas, I can tap into their suppressed rage toward the pirates,\\\" Braeden contemplates, pacing.\"},{\"text\":\"\\\"Oh! You\'ve brought a lot of Insect Fluid! Quick, let me take a look.\\\" Braeden snatches the vial of Insect Fluid out of your hands before you have a chance to respond.\"},{\"text\":\"\\\"I\'m almost done with my work here. Let me think about what to do next...\\\"\"}]', NULL, 72, NULL, NULL, NULL, 54, NULL, '|6|0|0|0|24|0|', NULL, NULL),
(108, 51, 35, 1, 6, 6551, 3505, '[{\"text\":\"Braeden busily tinkers with the Insect Body Fluid, using a delicate brush to seal the Cleaned Fruit.\"},{\"text\":\"[l]It appears that Braeden has already poured the gunpowder into the Cleaned Fruit peels. The bombs are almost fully prepared.\"},{\"text\":\"\\\"I know you\'re anxious to help out. I could use an extra set of hands to apply this sealant.\\\" Braeden lips stretch into a smirk.\"},{\"text\":\"\\\"Just apply the bug fluids to the peels of the Cleaned Fruit, and the bombshells will be complete!\\\"\",\"action_step\":1}]', NULL, NULL, NULL, 70, NULL, NULL, NULL, '|0|0|3|0|0|0|', NULL, NULL),
(109, 51, 35, 2, 6, 6551, 3505, '[{\"text\":\"\\\"Make sure you don\'t let the paint leak onto any part of the bomb, or we\'ll end up with a bunch of duds.\\\"\\n\\nYou need a Craftstman!\",\"action_step\":1},{\"text\":\"[l]Braeden flashes a big grin when you hand him the sealed bombs.\"},{\"text\":\"\\\"You are quite skilled. Nice attention to detail. Good job!\\\" Braeden gives you the thumbs up as he carefully looks over the bombs.\"},{\"text\":\"\\\"I just wish my brother could join the fight. Unfortunately, he just doesn\'t get it, so I have to hide my plans from him.\\\" Braeden bites his lip and looks into the distance with a sad expression.\"},{\"text\":\"\\\"Ah, but I digress! Worry not; your work will not be in vain. These bombs will make the pirates run for the hills.\\\"\"}]', NULL, 73, NULL, NULL, NULL, NULL, NULL, '|3|0|0|0|0|0|', NULL, NULL),
(110, 51, 36, 1, 6, 6551, 3505, '[{\"text\":\"\\\"Now onto the next step of my plan,\\\" Braeden exclaims.\"},{\"text\":\"\\\"There\'s no time to lose!\\\" Braeden brings out a few sheets of paper and begins scrawling on them.\"},{\"text\":\"\\\"This is the beginning of the next step of my plan. We should distribute these flyers to the townspeople. We\'ve got to incite the anger in them; they hate those dastardly pirates, but they\'re too afraid to fight back. Appeal to the townsfolk to stand together against the pirates.\\\"\"},{\"text\":\"\\\"Take these flyers and distribute them. Make sure that you don\'t lose them; they took me a long time to print up. I just know they\'ll get a good reaction out of the townsfolk.\\\"\",\"action_step\":1},{\"text\":\"[l]You tell Braeden that you will distribute the flyers, but you aren\'t sure if the townspeople will pass on the information as he hopes.\"},{\"text\":\"\\\"It shouldn\'t be a problem. Everyone loves some good gossip. We need as many people to fight against the pirates as possible. However, I\'m worried that my older brother will try to stop us when he hears the plan.\\\"\"},{\"text\":\"\\\"I think I\'m getting an idea. After you\'ve handed out the flyers, seek out my brother and find out if he\'s wise to our plan.\\\"\"}]', NULL, NULL, NULL, 74, NULL, NULL, NULL, '|0|0|3|0|0|0|', NULL, NULL),
(111, 50, 36, 2, 5, 846, 273, '[{\"text\":\"\\\"<player>, I\'m glad we can agree. It\'s not worth ordinary folk like us making enemies out of the pirates, who knows what they\'ll do if they\'re provoked.\\\"\"},{\"text\":\"\\\"Hold on, what are those papers you\'re holding? Do you need some help?\\\"\"},{\"text\":\"[l]You dismiss the offer of help with a wave of your hand. Looks like you\'ll have to distribute the flyers before getting any information out of Isaac.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(112, 57, 36, 3, 5, 7709, 1969, '[{\"text\":\"[l]You pass a flyer to an elderly townsman.\",\"action_step\":1},{\"text\":\"He raises a white eyebrow and reads the paper. \\\"I fully support fighting the pirates. I once got in a tussle with them in a pub when I was younger. I wasn\'t afraid of them then, and I\'m certainly not afraid of them now!\\\"\"}]', NULL, 74, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(113, 56, 36, 3, 5, 3304, 1649, '[{\"text\":\"[l]You pass a flyer to a young man.\",\"action_step\":1},{\"text\":\"He reads it with surprise, then smiles.\"},{\"text\":\"\\\"Finally, someone\'s taking some action around here! We\'ve put up with those pirates for far too long!\\\"\"},{\"text\":\"\\\"You can count on my help. And my friends will be on your side, too.\\\" The Helonian Man grins, excitement in his eyes.\"}]', NULL, 74, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(114, 55, 36, 3, 5, 547, 1073, '[{\"text\":\"[l]You shove a flyer into the hands of a middle-aged man.\",\"action_step\":1},{\"text\":\"\\\"Don\'t go looking for problems! I have a wife and kid at home. I\'m not going to pick a fight with a pirate, you fool!\\\" Clearly agitated, the man crumples the flyer into a ball.\"},{\"text\":\"\\\"Get out of here! Don\'t come near my house again. I will have no part in your foolish plan!\\\" He throws the paper to the ground, furious.\"}]', NULL, 74, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(115, 50, 36, 4, 5, 1564, 1841, '[{\"text\":\"\\\"We all know those pirates are vicious cads, but we\'re just normal people. We\'re not experts like you, <player>. We shouldn\'t start trouble.\\\"\"},{\"text\":\"\\\"Speaking of which, I really must thank you. I\'ve heard that, thanks to you, Braeden will hand over the supplies. I couldn\'t persuade him no matter how hard I tried.\\\"\"},{\"text\":\"\\\"Earlier, when I was fishing, I found something called a \'Secret Stone.\' They say such a stone could be very useful for adventurers. I want to give it to you as a way to express my gratitude, but...\"},{\"text\":\"[l]Noticing Isaac\'s hesitation, you ask him to tell you more.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(116, 50, 37, 1, 5, 1564, 1841, '[{\"text\":\"\\\"While fishing, I accidentally caught a stone in my net, and it totally shredded the mesh. I need that net! Without it, my livelihood is all but destroyed.\\\"\"},{\"text\":\"\\\"I\'m still not sure what to do. I could get the materials needed to make a new net in the forest, but I\'ve heard that the pirates frequent that area and tend to attack anyone who goes in alone.\\\"\"},{\"text\":\"[l]You ask Isaac what kinds of items he needs for the net, offering to help collect them.\"},{\"text\":\"\\\"That would be fantastic! You can get high-quality Thin Vines from the Spirit and Dryads in the Helonia Woods (Azuria). The vines can be woven into excellent nets.\\nThin Vine you can craft.\\\"\",\"action_step\":1},{\"text\":\"\\\"My foolish brother! I wonder if he took my words to heart. He needs to focus on fishing, not provoking pirates.\\\"\"},{\"text\":\"[l]You hand all the Thin Vines to Isaac.\"},{\"text\":\"\\\"Wow, look at all of these!\\\" he exclaims, clearly pleased.\"},{\"text\":\"\\\"Even without the whole pirate issue, it would\'ve taken Braeden and I much longer to collect the Thin Vines.\\\"\"},{\"text\":\"\\\"Thank you so much.\\\"\"}]', NULL, 79, NULL, NULL, NULL, 58, 59, '|2|0|0|0|10|10|', NULL, NULL),
(117, 50, 38, 1, 5, 1564, 1841, '[{\"text\":\"\\\"Before I leave, I must tell my brother something.\\\"\"},{\"text\":\"[l]You are momentarily taken aback. If Isaac finds Braeden, he\'ll definitely learn about the counterattack plan. Casually, you ask why he needs to find his brothers.\"},{\"text\":\"\\\"I fear that even Braeden collected so many Thin Vines, he doesn\'t have the parrots the pirates are after... and I\'ve still got the cages.\\\"\"},{\"text\":\"\\\"Pirates love parrots. What are you gonna do?\\\" Isaac shrugs.\"},{\"text\":\"\\\"I got to make sure he remembers, or everything he\'s done up to this point will be for naught.\\\"\"},{\"text\":\"[l]You anxiously stop Isaac in his tracks and tell him you\'ll help Braeden catch the parrots.\"},{\"text\":\"\\\"You\'re willing to help? Excellent! I don\'t think Braeden has it in him to catch the birds anyway.\\\"\"},{\"text\":\"\\\"Go to Helonia Woods. Catch the parrots. We fishermen mostly focus on, you know, fishing, so the pirates\' request isn\'t an easy one for us. And the parrots aren\'t exactly the friendliest birds ever...\\\"\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(118, 51, 38, 2, 6, 6551, 3505, '[{\"text\":\"\\\"Don\'t let a little mistake ruin the whole counterattack plan.\\\"\"},{\"text\":\"\\\"I feel like I\'m forgetting something... eh, I\'m sure it wasn\'t that important.\\\"\",\"action_step\":1},{\"text\":\"[l]You give Braeden the Wild Parrots and tell him what Isaac has said.\"},{\"text\":\"\\\"I knew I was forgetting something! I nearly let a bunch of stupid birds ruin the whole plan!\\\" Braeden groans, pulling at his hair.\"},{\"text\":\"\\\"I\'m lucky to have your help - and my brother\'s too. You caught those dastardly birds a lot faster than I could\'ve. Heck, even if I had a few days, I\'m not sure I\'d be able to catch \'em.\\\"\"}]', NULL, 90, NULL, NULL, NULL, NULL, NULL, '|5|0|0|0|0|0|', NULL, NULL),
(119, 51, 39, 1, 6, 6551, 3505, '[{\"text\":\"\\\"That was really close, but I\'ve got everything I need.\\\" Braeden smiles, flushed.\"},{\"text\":\"\\\"Can you help me get these items into town? The mayor will be happy to see \'em,\\\" Braeden grunts.\"},{\"text\":\"\\\"I still have to go to my brother\'s place to get new fishing nets, and I\'ve gotta make sure he\'s still in the dark about our plans. If you\'re ready, go ahead. I don\'t want to give the pirates a reason to start trouble.\\\"\",\"action_step\":1}]', NULL, NULL, NULL, 91, NULL, NULL, NULL, '|0|0|1|0|0|0|', NULL, NULL),
(120, 48, 39, 2, 7, 3870, 1553, '[{\"text\":\"\\\"We\'re missing Braeden\'s items. I can\'t believe he\'s not here yet!\\\"\",\"action_step\":1},{\"text\":\"[l]You place the box full of supplies next to the Town Mayor, explaining that these are the items Braeden is supposed to collect. Everything is accounted for within the box.\"},{\"text\":\"\\\"Wonderful!\\\" Her expression lifts, then suddenly darkens.\"},{\"text\":\"\\\"The pirates... they\'re here. I hope things go as planned,\\\" She murmurs, biting her lip.\"}]', NULL, 91, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL);

-- --------------------------------------------------------

--
-- Table structure for table `tw_crafts_list`
--

CREATE TABLE `tw_crafts_list` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) DEFAULT NULL,
  `ItemValue` int(11) NOT NULL,
  `RequiredItems` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL CHECK (json_valid(`RequiredItems`)),
  `Price` int(11) NOT NULL DEFAULT 100,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

--
-- Dumping data for table `tw_crafts_list`
--

INSERT INTO `tw_crafts_list` (`ID`, `ItemID`, `ItemValue`, `RequiredItems`, `Price`, `WorldID`) VALUES
(1, 39, 1, '{\"items\":[{\"id\":40,\"value\":40},{\"id\":33,\"value\":24}]}', 80, 1),
(2, 41, 1, '{\"items\":[{\"id\":40,\"value\":50},{\"id\":33,\"value\":12},{\"id\":32,\"value\":60}]}', 100, 1),
(3, 27, 1, '{\"items\":[{\"id\":44,\"value\":18}]}', 100, 1),
(4, 26, 1, '{\"items\":[{\"id\":44,\"value\":18}]}', 100, 1),
(5, 43, 1, '{\"items\":[{\"id\":27,\"value\":1}, {\"id\":40,\"value\":24}]}', 125, 1),
(6, 42, 1, '{\"items\":[{\"id\":26,\"value\":1}, {\"id\":40,\"value\":24}]}', 125, 1),
(7, 56, 1, '{\"items\":[{\"id\":55,\"value\":1}]}', 25, 1),
(8, 61, 1, '{\"items\":[{\"id\":52,\"value\":1},{\"id\":53,\"value\":1}]}', 25, 1),
(9, 60, 1, '{\"items\":[{\"id\":39,\"value\":1},{\"id\":61,\"value\":16}]}', 150, 1),
(10, 63, 1, '{\"items\":[{\"id\":27,\"value\":1}, {\"id\":43,\"value\":1}, {\"id\":61,\"value\":40}]}', 150, 1),
(11, 62, 1, '{\"items\":[{\"id\":26,\"value\":1}, {\"id\":42,\"value\":1}, {\"id\":61,\"value\":40}]}', 150, 1),
(12, 15, 3, '{\"items\":[{\"id\":46,\"value\":3}, {\"id\":47,\"value\":3}]}', 50, 1),
(13, 69, 1, '{\"items\":[{\"id\":60,\"value\":1}, {\"id\":67,\"value\":12}]}', 1000, 5),
(14, 70, 1, '{\"items\":[{\"id\":71,\"value\":1}]}', 100, 5),
(15, 73, 3, '{\"items\":[{\"id\":70,\"value\":3},{\"id\":72,\"value\":6}]}', 400, 5),
(16, 76, 1, '{\"items\":[{\"id\":60,\"value\":1}, {\"id\":72,\"value\":12}]}', 1000, 5),
(17, 77, 1, '{\"items\":[{\"id\":60,\"value\":1}, {\"id\":72,\"value\":6}, {\"id\":67,\"value\":6}]}', 1000, 5),
(18, 10, 1, '{\"items\":[{\"id\":61,\"value\":40}, {\"id\":72,\"value\":24}, {\"id\":32,\"value\":80}]}', 7500, 5),
(19, 13, 1, '{\"items\":[{\"id\":61,\"value\":40}, {\"id\":72,\"value\":24}, {\"id\":32,\"value\":80}]}', 7500, 5),
(20, 79, 1, '{\"items\":[{\"id\":78,\"value\":10}, {\"id\":72,\"value\":10}]}', 700, 5),
(21, 80, 1, '{\"items\":[{\"id\":79,\"value\":24}]}', 5000, 5),
(22, 83, 1, '{\"items\":[{\"id\":82,\"value\":3}]}', 800, 5),
(23, 85, 1, '{\"items\":[{\"id\":84,\"value\":24}, {\"id\":81,\"value\":16}]}', 1600, 5),
(24, 86, 1, '{\"items\":[{\"id\":84,\"value\":12}, {\"id\":81,\"value\":32}]}', 1600, 5),
(25, 87, 1, '{\"items\":[{\"id\":83,\"value\":12}, {\"id\":79,\"value\":8}]}', 1800, 5);

-- --------------------------------------------------------

--
-- Table structure for table `tw_dungeons`
--

CREATE TABLE `tw_dungeons` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL DEFAULT 'Unknown',
  `Level` int(11) NOT NULL DEFAULT 1,
  `DoorX` int(11) NOT NULL DEFAULT 0,
  `DoorY` int(11) NOT NULL DEFAULT 0,
  `RequiredQuestID` int(11) NOT NULL DEFAULT -1,
  `Story` tinyint(4) NOT NULL DEFAULT 0,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_dungeons`
--

INSERT INTO `tw_dungeons` (`ID`, `Name`, `Level`, `DoorX`, `DoorY`, `RequiredQuestID`, `Story`, `WorldID`) VALUES
(1, 'Abandoned mine', 10, 1105, 1521, -1, 1, 4);

-- --------------------------------------------------------

--
-- Table structure for table `tw_dungeons_door`
--

CREATE TABLE `tw_dungeons_door` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL DEFAULT 'Write here name dungeon',
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `BotID` int(11) NOT NULL,
  `DungeonID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_dungeons_door`
--

INSERT INTO `tw_dungeons_door` (`ID`, `Name`, `PosX`, `PosY`, `BotID`, `DungeonID`) VALUES
(1, 'AM 1', 4302, 1940, 28, 1),
(2, 'AM 2', 1808, 3600, 29, 1),
(3, 'AM 3', 750, 4850, 30, 1),
(4, 'AM 4', 2255, 4850, 32, 1);

-- --------------------------------------------------------

--
-- Table structure for table `tw_dungeons_records`
--

CREATE TABLE `tw_dungeons_records` (
  `ID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `DungeonID` int(11) NOT NULL,
  `Seconds` int(11) NOT NULL,
  `PassageHelp` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_guilds`
--

CREATE TABLE `tw_guilds` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT 'Member name',
  `UserID` int(11) DEFAULT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Experience` int(11) NOT NULL DEFAULT 0,
  `Bank` int(11) NOT NULL DEFAULT 0,
  `Score` int(11) NOT NULL DEFAULT 0,
  `AvailableSlots` int(11) NOT NULL DEFAULT 2,
  `ChairExperience` int(11) NOT NULL DEFAULT 1,
  `ChairMoney` int(11) NOT NULL DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_guilds_decorations`
--

CREATE TABLE `tw_guilds_decorations` (
  `ID` int(11) NOT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `HouseID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `WorldID` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_guilds_decorations`
--

INSERT INTO `tw_guilds_decorations` (`ID`, `PosX`, `PosY`, `HouseID`, `ItemID`, `WorldID`) VALUES
(4, 4319, 6283, 1, 11, 1),
(5, 4198, 6276, 1, 10, 1),
(6, 4202, 6351, 1, 12, 1);

-- --------------------------------------------------------

--
-- Table structure for table `tw_guilds_history`
--

CREATE TABLE `tw_guilds_history` (
  `ID` int(11) NOT NULL,
  `GuildID` int(11) NOT NULL,
  `Text` varchar(64) NOT NULL,
  `Time` datetime NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_guilds_houses`
--

CREATE TABLE `tw_guilds_houses` (
  `ID` int(11) NOT NULL,
  `GuildID` int(11) DEFAULT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `DoorX` int(11) NOT NULL,
  `DoorY` int(11) NOT NULL,
  `TextX` int(11) NOT NULL,
  `TextY` int(11) NOT NULL,
  `Price` int(11) NOT NULL DEFAULT 50000,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_guilds_houses`
--

INSERT INTO `tw_guilds_houses` (`ID`, `GuildID`, `PosX`, `PosY`, `DoorX`, `DoorY`, `TextX`, `TextY`, `Price`, `WorldID`) VALUES
(1, NULL, 4120, 6449, 4521, 6449, 4206, 6307, 42000, 1),
(2, NULL, 9504, 5713, 9187, 5713, 9486, 5612, 42000, 1),
(3, NULL, 3598, 1521, 3888, 1521, 3568, 1396, 56000, 5);

-- --------------------------------------------------------

--
-- Table structure for table `tw_guilds_invites`
--

CREATE TABLE `tw_guilds_invites` (
  `ID` int(11) NOT NULL,
  `GuildID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_guilds_ranks`
--

CREATE TABLE `tw_guilds_ranks` (
  `ID` int(11) NOT NULL,
  `Access` int(11) NOT NULL DEFAULT 3,
  `Name` varchar(32) NOT NULL DEFAULT 'Rank name',
  `GuildID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_houses`
--

CREATE TABLE `tw_houses` (
  `ID` int(11) NOT NULL,
  `UserID` int(11) DEFAULT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `DoorX` int(11) NOT NULL,
  `DoorY` int(11) NOT NULL,
  `Class` varchar(32) NOT NULL DEFAULT 'Class name',
  `Price` int(11) NOT NULL,
  `HouseBank` int(11) NOT NULL,
  `PlantID` int(11) NOT NULL,
  `PlantX` int(11) NOT NULL,
  `PlantY` int(11) NOT NULL,
  `AccessData` varchar(128) DEFAULT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_houses`
--

INSERT INTO `tw_houses` (`ID`, `UserID`, `PosX`, `PosY`, `DoorX`, `DoorY`, `Class`, `Price`, `HouseBank`, `PlantID`, `PlantX`, `PlantY`, `AccessData`, `WorldID`) VALUES
(1, NULL, 8001, 5297, 8294, 5297, 'Default', 21000, 0, 40, 7499, 5329, '38,33,53', 1),
(2, NULL, 8989, 7729, 8704, 7729, 'Default', 21000, 202, 40, 9466, 7761, '2', 1),
(3, NULL, 2046, 913, 1936, 913, 'Extra', 32000, 9074, 40, 2570, 982, '', 5),
(4, NULL, 2049, 593, 1936, 593, 'Extra', 32000, 0, 40, 2570, 444, NULL, 5),
(5, NULL, 1211, 913, 1330, 913, 'Extra', 32000, 0, 40, 707, 991, NULL, 5),
(6, NULL, 1222, 593, 1330, 593, 'Extra', 32000, 0, 40, 706, 444, NULL, 5);

-- --------------------------------------------------------

--
-- Table structure for table `tw_houses_decorations`
--

CREATE TABLE `tw_houses_decorations` (
  `ID` int(11) NOT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `HouseID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_houses_decorations`
--

INSERT INTO `tw_houses_decorations` (`ID`, `PosX`, `PosY`, `HouseID`, `ItemID`, `WorldID`) VALUES
(1, 7970, 5230, 1, 12, 1),
(2, 8173, 5176, 1, 12, 1),
(3, 8075, 5205, 1, 10, 1),
(5, 8987, 7647, 2, 13, 1),
(7, 9129, 7679, 2, 13, 1),
(8, 8939, 7627, 2, 13, 1),
(9, 8844, 7599, 2, 13, 1),
(10, 8971, 7620, 2, 13, 1),
(11, 8848, 7670, 2, 13, 1),
(12, 8788, 7678, 2, 13, 1),
(14, 8880, 7698, 2, 12, 1),
(15, 8866, 7636, 2, 12, 1),
(20, 8937, 7668, 2, 12, 1);

-- --------------------------------------------------------

--
-- Table structure for table `tw_items_list`
--

CREATE TABLE `tw_items_list` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT 'Item name',
  `Description` varchar(64) NOT NULL DEFAULT 'Item desc',
  `Type` int(11) DEFAULT -1,
  `Function` int(11) DEFAULT -1,
  `Data` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL CHECK (json_valid(`Data`)),
  `InitialPrice` int(11) NOT NULL DEFAULT 100,
  `Desynthesis` int(11) NOT NULL DEFAULT 100,
  `Attribute0` int(11) DEFAULT NULL,
  `Attribute1` int(11) DEFAULT NULL,
  `AttributeValue0` int(11) NOT NULL DEFAULT 0,
  `AttributeValue1` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_items_list`
--

INSERT INTO `tw_items_list` (`ID`, `Name`, `Description`, `Type`, `Function`, `Data`, `InitialPrice`, `Desynthesis`, `Attribute0`, `Attribute1`, `AttributeValue0`, `AttributeValue1`) VALUES
(1, 'Gold', 'Major currency', 0, -1, NULL, 0, 0, 16, NULL, 0, 0),
(2, 'Hammer', 'A normal hammer', 6, 0, NULL, 0, 0, 16, 6, 3, 3),
(3, 'Gun', 'Conventional weapon', 6, 1, NULL, 5, 0, 17, NULL, 1, 0),
(4, 'Shotgun', 'Conventional weapon', 6, 2, NULL, 5, 0, 18, NULL, 1, 0),
(5, 'Grenade', 'Conventional weapon', 6, 3, NULL, 5, 0, 19, NULL, 1, 0),
(6, 'Rifle', 'Conventional weapon', 6, 4, NULL, 5, 0, 20, NULL, 5, 0),
(7, 'Material', 'Required to improve weapons', 4, 13, NULL, 5, 0, NULL, NULL, 0, 0),
(8, 'Ticket guild', 'Command: /gcreate <name>', 4, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(9, 'Skill Point', 'Skill point', 0, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(10, 'Decoration Armor', 'Decoration for house!', 7, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(11, 'Decoration Heart Elite', 'Decoration for house!', 7, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(12, 'Decoration Ninja Elite', 'Decoration for house!', 7, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(13, 'Decoration Heart', 'Decoration for house!', 7, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(14, 'Potion mana regen', 'Regenerate +5%, 15sec every sec.\n', 8, 9, NULL, 5, 10, NULL, NULL, 0, 0),
(15, 'Tiny HP Potion', 'Recovers 7HP per second for 10 seconds.', 8, 9, NULL, 5, 10, NULL, NULL, 0, 0),
(16, 'Capsule survival experience', 'You got 10-50 experience survival', 1, 10, NULL, 5, 0, NULL, NULL, 0, 0),
(17, 'Little bag of gold', 'You got 10-50 gold', 1, 10, NULL, 5, 0, NULL, NULL, 0, 0),
(18, 'Potion resurrection', 'Resuscitates in the zone where you died!', 8, -1, NULL, 5, 10, NULL, NULL, 0, 0),
(19, 'Explosive module for gun', 'It happens sometimes', 3, 11, NULL, 5, 100, 17, NULL, 1, 0),
(20, 'Explosive module for shotgun', 'It happens sometimes', 3, 11, NULL, 5, 100, 18, NULL, 1, 0),
(21, 'Ticket reset class stats', 'Resets only class stats(Dps, Tank, Healer).', 1, 9, NULL, 5, 0, NULL, NULL, 0, 0),
(22, 'Mode PVP', 'Settings game.', 5, 11, NULL, 0, 0, NULL, NULL, 0, 0),
(23, 'Ticket reset weapon stats', 'Resets only ammo stats(Ammo).', 1, 9, NULL, 5, 0, NULL, NULL, 0, 0),
(24, 'Blessing for discount craft', 'Need dress it, -20% craft price', 8, 9, NULL, 5, 0, NULL, NULL, 0, 0),
(25, 'Show equipment description', 'Settings game.', 5, 11, NULL, 0, 0, NULL, NULL, 0, 0),
(26, 'Rusty Rake', 'The usual rusty rake.', 6, 6, NULL, 5, 30, 15, NULL, 3, 0),
(27, 'Rusty Pickaxe', 'The usual rusty pickaxe.', 6, 5, NULL, 5, 30, 14, NULL, 3, 0),
(28, 'Beast Tusk Light Armor (All)', 'Lightweight armor.', 6, 7, NULL, 5, 40, 8, NULL, 10, 0),
(29, 'Boxes of jam', 'A jam made of lots of fresh fruits.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(30, 'Sheila\'s Bento Box', 'A lunchbox made by Sheila.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(31, 'Wood', '...', 2, 12, NULL, 5, 1, NULL, NULL, 0, 0),
(32, 'Antelope blood', 'The blood extracted from Antelopes.', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(33, 'Wild Forest Mushroom', 'They can often be found under big trees.', 2, 12, NULL, 5, 1, NULL, NULL, 0, 0),
(34, 'Show critical damage', 'Settings game.', 5, 11, NULL, 0, 0, NULL, NULL, 0, 0),
(35, 'Althyk\'s Ring', 'Althyk\'s Ring is an item level 1.', 3, 11, NULL, 5, 30, 8, 10, 18, 22),
(36, 'Empyrean Ring', 'Empyrean Ring is an item level 1.', 3, 11, NULL, 5, 30, 8, 10, 28, 16),
(37, 'Ring of Fidelity', 'Ring of Fidelity is an item level 1.', 3, 11, NULL, 5, 30, 7, 4, 5, 1),
(38, 'Eversharp Ring', 'Eversharp Ring is an item level 1.', 3, 11, NULL, 5, 32, 8, NULL, 50, 0),
(39, 'Green Grass Armor (All)', 'Lightweight armor.', 6, 7, NULL, 5, 60, 8, NULL, 20, 0),
(40, 'Green grass', '...', 2, 12, NULL, 5, 1, NULL, NULL, 0, 0),
(41, 'Bloody Woven Flower', 'Soaked in blood', 3, 11, NULL, 5, 50, 10, 13, 40, 3),
(42, 'Grass Rake', 'The usual grass rake.', 6, 6, NULL, 5, 50, 15, NULL, 5, 0),
(43, 'Grass Pickaxe', 'The usual grass pickaxe.', 6, 5, NULL, 5, 50, 14, NULL, 5, 0),
(44, 'Fragile Iron', '...', 2, 13, NULL, 5, 1, NULL, NULL, 0, 0),
(45, 'Refreshing Potion', 'A potion that Village Doctor Cal made.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(46, 'Blue pollen', '...', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(47, 'Green pollen', '...', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(48, 'Relic Fragments', 'Fragments from the ruins.', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(49, 'Heavy Crate', 'A crate full of various items.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(50, 'Rations', 'These cloth-wrapped are heavier than they look.', 4, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(51, 'Torn handbag', 'Torn handbag with ammo', 3, 11, NULL, 5, 100, 13, NULL, 5, 0),
(52, 'Sticky Secretion', 'A sticky and slimy liquid.', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(53, 'Cooling Liquid', 'An ice-cold cooling liquid.', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(54, 'Bentley\'s Amulet', 'A somewhat shabby amulet that emits magic waves.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(55, 'Uncharged Gem', 'A mysterious gem that can absorb Magic.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(56, 'Charged Gem', 'A gem that has been recharged.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(57, 'Otohime', 'Eidolon Otohime', 6, 8, NULL, 5, 320, 22, NULL, 90, 0),
(58, 'Random Relics Box', 'Small chance of dropping a relics', 1, 10, '{\"random_box\":[{\"item_id\":16,\"value\":3,\"chance\":82},{\"item_id\":17,\"value\":2,\"chance\":81},{\"item_id\":44,\"value\":3,\"chance\":80},{\"item_id\":48,\"value\":3,\"chance\":60},{\"item_id\":19,\"value\":1,\"chance\":1},{\"item_id\":20,\"value\":1,\"chance\":1},{\"item_id\":59,\"value\":1,\"chance\":1}]}', 0, 0, NULL, NULL, 0, 0),
(59, 'Merrilee', 'Eidolon Merrilee', 6, 8, NULL, 0, 0, 22, NULL, 90, 0),
(60, 'Blue Light Armor (All)', 'Lightweight armor.', 6, 7, NULL, 5, 80, 8, NULL, 30, 0),
(61, 'Blue slime', '...', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(62, 'Blue Rake', 'The usual blue rake.', 6, 6, NULL, 5, 60, 15, NULL, 10, 0),
(63, 'Blue Pickaxe', 'The usual blue pickaxe.', 6, 5, NULL, 5, 60, 14, NULL, 10, 0),
(64, 'Poison Hook', 'Inflicts gradual damage.', 3, 11, NULL, 5, 80, 12, NULL, 20, 0),
(65, 'Explosive impulse hook', 'Inflicts gradual explode damage.', 3, 11, NULL, 5, 80, 11, NULL, 20, 0),
(66, 'Magic Spider Hook', 'It\'s sticky to the air.', 3, 11, NULL, 5, 80, 5, NULL, 20, 0),
(67, 'Fruit', 'This fruit gives out a sweet fragrance.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(68, 'Confiscated Material', 'The thick, heavy box', 2, -1, NULL, 5, 3, NULL, NULL, 0, 0),
(69, 'Bestial Light Armor (DoT)', 'Armor for Tank.', 6, 7, NULL, 5, 100, 8, 9, 40, 15),
(70, 'Cleaned Fruit', 'This fruit gives out a sweet fragrance.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(71, 'Hard Fruit', 'The fruit has just been picked from a tree and is caked in mud.', 2, 12, NULL, 5, 2, NULL, NULL, 0, 0),
(72, 'Insect Fluid', 'It\'s extremely sticky and has a slightly sour taste.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(73, 'Waterproof Bomb', 'Covered in waterproof paint and won\'t get wet.', 4, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(74, 'Flyer from Braeden', 'The flyer from Fisherman Braeden is covered in text.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(75, 'Eternal Sun Belt', 'Eternal Sun Belt is an item level 1.', 3, 11, NULL, 5, 30, 9, 5, 30, 52),
(76, 'Shadower Mantle (DoH)', 'Mantle for Healer.', 6, 7, NULL, 5, 100, 10, 11, 30, 15),
(77, 'Mercenary Armor (DoW)', 'Lightweight armor for DPS.', 6, 7, NULL, 5, 100, 4, 5, 2, 80),
(78, 'Vine', 'It\'s long, but quite firm.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(79, 'Thin Vine', 'It\'s thin and long, but quite firm.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(80, 'Dryad', 'Eidolon Dryad', 6, 8, NULL, 0, 0, 22, NULL, 100, 0),
(81, 'Crab Claw', 'Claw of crab.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(82, 'Broken Bone', 'Cracked bones with plentiful marrow.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(83, 'Sharp Bone', 'Sharp bone can be sold to merchants who need it.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(84, 'Large Petal', 'After a special drying process.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(85, 'Thorny Ring', 'Thorny Ring is an item level 1.', 3, 11, NULL, 5, 32, 12, 8, 32, 16),
(86, 'Thorny Necklace', 'Thorny Necklace is an item level 1.', 3, 11, NULL, 5, 32, 8, 10, 24, 12),
(87, 'Bone Armillae', 'Bone Armillae is an item level 1.', 3, 11, NULL, 5, 32, 4, NULL, 1, 0),
(88, 'Pig Queen', 'Eidolon Pig Queen', 6, 8, NULL, 0, 0, 22, 4, 150, 1),
(89, 'Eidolon Crystal', 'Required to improve eidolons', 4, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(90, 'Caged Parrot', 'Was it worth it?', 2, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(91, 'Box full of supplies', '....', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(92, 'The adventurer\'s clearance', 'Needed for permission to pass.', 4, -1, NULL, 0, 0, NULL, NULL, 0, 0);

-- --------------------------------------------------------

--
-- Table structure for table `tw_logics_worlds`
--

CREATE TABLE `tw_logics_worlds` (
  `ID` int(11) NOT NULL,
  `MobID` int(11) NOT NULL,
  `Mode` int(11) NOT NULL DEFAULT 0 COMMENT '(1,3) 0 up 1 left',
  `ParseInt` int(11) NOT NULL COMMENT '(2) health (3)itemid key',
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `WorldID` int(11) NOT NULL,
  `Comment` varchar(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Table structure for table `tw_positions_mining`
--

CREATE TABLE `tw_positions_mining` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Health` int(11) NOT NULL DEFAULT 100,
  `PositionX` int(11) NOT NULL,
  `PositionY` int(11) NOT NULL,
  `Distance` int(11) NOT NULL DEFAULT 300 COMMENT 'Range of unit spreading',
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

--
-- Dumping data for table `tw_positions_mining`
--

INSERT INTO `tw_positions_mining` (`ID`, `ItemID`, `Level`, `Health`, `PositionX`, `PositionY`, `Distance`, `WorldID`) VALUES
(1, 44, 1, 180, 4754, 564, 500, 2);

-- --------------------------------------------------------

--
-- Table structure for table `tw_positions_plant`
--

CREATE TABLE `tw_positions_plant` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Health` int(11) NOT NULL DEFAULT 100,
  `PositionX` int(11) NOT NULL,
  `PositionY` int(11) NOT NULL,
  `Distance` int(11) NOT NULL DEFAULT 300 COMMENT 'Range of unit spreading',
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

--
-- Dumping data for table `tw_positions_plant`
--

INSERT INTO `tw_positions_plant` (`ID`, `ItemID`, `Level`, `Health`, `PositionX`, `PositionY`, `Distance`, `WorldID`) VALUES
(1, 33, 1, 125, 4940, 1014, 400, 2),
(2, 71, 5, 350, 10680, 4100, 400, 6);

-- --------------------------------------------------------

--
-- Table structure for table `tw_quests_list`
--

CREATE TABLE `tw_quests_list` (
  `ID` int(11) NOT NULL,
  `Name` varchar(24) NOT NULL DEFAULT 'Quest name',
  `Money` int(11) NOT NULL,
  `Exp` int(11) NOT NULL,
  `StoryLine` varchar(24) NOT NULL DEFAULT 'Hero'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_quests_list`
--

INSERT INTO `tw_quests_list` (`ID`, `Name`, `Money`, `Exp`, `StoryLine`) VALUES
(1, 'Bad Dreams and Mornings', 30, 25, 'Ch1'),
(2, 'A Sticky Situation', 30, 25, 'Ch1'),
(3, 'Boxed Lunch', 35, 30, 'Ch1'),
(4, 'Looking for Work', 40, 40, 'Ch1'),
(5, 'What Big Teeth You Have', 40, 40, 'Ch1'),
(6, 'Log Lugger', 40, 40, 'Ch1'),
(7, 'The Lazy Lumberjack', 50, 50, 'Ch1'),
(8, 'Doctor Cal', 60, 60, 'Ch1'),
(9, 'Collect Antelope Blood', 60, 60, 'Ch1'),
(10, 'Mushroom Master', 60, 60, 'Ch1'),
(11, 'Just Sign This Waiver...', 60, 60, 'Ch1'),
(12, 'Seek Professional Help', 30, 25, 'Ch1'),
(13, 'Sprite Fight', 30, 25, 'Ch1'),
(14, 'Buried Secrets', 32, 28, 'Ch1'),
(15, 'A Dark Future', 35, 30, 'Ch1'),
(16, 'A Reliable Witness', 30, 25, 'Ch1'),
(17, 'The Unlucky Merchant', 30, 25, 'Ch1'),
(18, 'An Unexpected Clue', 30, 25, 'Ch1'),
(19, 'Emergency Aid', 30, 25, 'Ch1'),
(20, 'Witch in the Forest', 30, 25, 'Ch1'),
(21, 'Elena\'s New Staff', 30, 25, 'Ch1'),
(22, 'Relief and Despair', 30, 25, 'Ch1'),
(23, 'Brian\'s Song', 30, 25, 'Ch1'),
(24, 'Facing Destiny', 30, 25, 'Ch1'),
(25, 'A Story to Tell', 30, 25, 'Ch1'),
(26, 'A Fond Farewell', 30, 25, 'Ch1'),
(27, 'Heading to Helonia', 30, 25, 'Ch1'),
(28, 'The Pirates\' Threat', 30, 25, 'Ch2'),
(29, 'Assisting the Mayor', 30, 25, 'Ch2'),
(30, 'Annoying Noise', 30, 25, 'Ch2'),
(31, 'Conservative and Radical', 30, 25, 'Ch2'),
(32, 'Surface Submission', 30, 25, 'Ch2'),
(33, 'Battle Plan', 30, 25, 'Ch2'),
(34, 'Mysterious Paint', 30, 25, 'Ch2'),
(35, 'Ammo Preparation', 30, 25, 'Ch2'),
(36, 'Residents of Helonia', 30, 25, 'Ch2'),
(37, 'Fishing Net', 30, 25, 'Ch2'),
(38, 'A Noisy Gift', 30, 25, 'Ch2'),
(39, 'Greedy Pirates', 30, 25, 'Ch2');

-- --------------------------------------------------------

--
-- Table structure for table `tw_skills_list`
--

CREATE TABLE `tw_skills_list` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL,
  `Description` varchar(64) NOT NULL,
  `Type` int(11) NOT NULL DEFAULT 0 COMMENT '0-Improvements\r\n1-Healer\r\n2-Dps\r\n3-Tank',
  `BoostName` varchar(64) NOT NULL DEFAULT '''name''',
  `BoostValue` int(11) NOT NULL DEFAULT 1,
  `PercentageCost` int(11) NOT NULL DEFAULT 10,
  `PriceSP` int(11) NOT NULL,
  `MaxLevel` int(11) NOT NULL,
  `Passive` tinyint(1) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_skills_list`
--

INSERT INTO `tw_skills_list` (`ID`, `Name`, `Description`, `Type`, `BoostName`, `BoostValue`, `PercentageCost`, `PriceSP`, `MaxLevel`, `Passive`) VALUES
(1, 'Health turret', 'Creates turret a recovery health ', 1, '+ 10sec life span', 1, 25, 28, 6, 0),
(2, 'Sleepy Gravity', 'Magnet mobs to itself', 3, 'radius', 20, 25, 28, 10, 0),
(3, 'Craft Discount', 'Will give discount on the price of craft items', 0, '% discount gold for craft item', 1, 0, 28, 50, 1),
(4, 'Proficiency with weapons', 'You can perform an automatic fire', 0, 'can perform an auto fire with all types of weapons', 1, 0, 120, 1, 1),
(5, 'Blessing of God of war', 'The blessing restores ammo', 3, '% recovers ammo within a radius of 800', 25, 50, 28, 4, 0),
(6, 'Attack Teleport', 'An attacking teleport that deals damage to all mobs radius', 2, '% your strength', 25, 10, 100, 4, 0),
(7, 'Cure I', 'Restores HP all nearby target\'s.', 1, '% adds a health bonus', 3, 5, 10, 5, 0),
(8, 'Provoke', 'Aggresses mobs in case of weak aggression', 3, 'power of aggression', 150, 30, 40, 2, 0);

-- --------------------------------------------------------

--
-- Table structure for table `tw_voucher`
--

CREATE TABLE `tw_voucher` (
  `ID` int(11) NOT NULL,
  `Code` varchar(32) NOT NULL,
  `Data` text NOT NULL,
  `Multiple` tinyint(1) NOT NULL DEFAULT 0,
  `ValidUntil` bigint(20) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_voucher`
--

INSERT INTO `tw_voucher` (`ID`, `Code`, `Data`, `Multiple`, `ValidUntil`) VALUES
(1, 'START', '{\n	\"exp\": 1000,\n	\"items\": [\n		{\n			\"id\": 17,\n			\"value\": 10\n		},\n		{\n			\"id\": 21,\n			\"value\": 1\n		},\n		{\n			\"id\": 23,\n			\"value\": 1\n		},\n		{\n			\"id\": 9,\n			\"value\": 10\n		},\n		{\n			\"id\": 8,\n			\"value\": 1\n		},\n		{\n			\"id\": 4,\n			\"value\": 1\n		}\n	]\n}', 1, 1670205449),
(2, 'EIDOLON', '{\r\n	\"exp\": 0,\r\n	\"items\": [\r\n		{\r\n			\"id\": 59,\r\n			\"value\": 1\r\n		}\r\n	]\r\n}', 1, 1670205449);

-- --------------------------------------------------------

--
-- Table structure for table `tw_voucher_redeemed`
--

CREATE TABLE `tw_voucher_redeemed` (
  `ID` int(11) NOT NULL,
  `VoucherID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `TimeCreated` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_warehouses`
--

CREATE TABLE `tw_warehouses` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT '''Bussines name''',
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `Currency` int(11) NOT NULL DEFAULT 1,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_warehouses`
--

INSERT INTO `tw_warehouses` (`ID`, `Name`, `PosX`, `PosY`, `Currency`, `WorldID`) VALUES
(1, 'Betsy shop', 9437, 6833, 1, 1),
(2, 'Weapons from Correy', 6247, 6417, 1, 1),
(3, 'Relic items by Bentley', 10858, 2545, 48, 3),
(4, 'Variety by Luther', 8474, 1873, 1, 5);

-- --------------------------------------------------------

--
-- Table structure for table `tw_warehouse_items`
--

CREATE TABLE `tw_warehouse_items` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `ItemValue` int(11) NOT NULL,
  `RequiredItemID` int(11) NOT NULL DEFAULT 1,
  `Price` int(11) NOT NULL,
  `Enchant` int(11) NOT NULL DEFAULT 0,
  `WarehouseID` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_warehouse_items`
--

INSERT INTO `tw_warehouse_items` (`ID`, `ItemID`, `ItemValue`, `RequiredItemID`, `Price`, `Enchant`, `WarehouseID`) VALUES
(4, 3, 1, 1, 140, 0, 2),
(5, 4, 1, 1, 350, 0, 2),
(6, 5, 1, 1, 350, 0, 2),
(7, 6, 1, 1, 400, 0, 2),
(8, 28, 1, 1, 500, 0, 1),
(9, 35, 1, 1, 490, 0, 1),
(10, 36, 1, 1, 520, 0, 1),
(11, 37, 1, 1, 540, 0, 1),
(12, 51, 1, 48, 50, 0, 3),
(13, 58, 1, 48, 5, 0, 3),
(14, 15, 1, 1, 100, 0, 4),
(15, 75, 1, 1, 1600, 0, 4);

-- --------------------------------------------------------

--
-- Table structure for table `tw_world_swap`
--

CREATE TABLE `tw_world_swap` (
  `ID` int(11) NOT NULL,
  `WorldID` int(11) DEFAULT NULL,
  `PositionX` int(11) DEFAULT NULL,
  `PositionY` int(11) DEFAULT NULL,
  `TwoWorldID` int(11) DEFAULT NULL,
  `TwoPositionX` int(11) DEFAULT NULL,
  `TwoPositionY` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_world_swap`
--

INSERT INTO `tw_world_swap` (`ID`, `WorldID`, `PositionX`, `PositionY`, `TwoWorldID`, `TwoPositionX`, `TwoPositionY`) VALUES
(8, 0, 28832, 1920, 1, 6415, 7345),
(9, 1, 3607, 8105, 2, 6912, 991),
(10, 1, 13744, 6670, 3, 496, 2000),
(11, 3, 13488, 1838, 5, 195, 2055),
(12, 5, 207, 227, 6, 6527, 4605),
(13, 6, 9430, 1370, 7, 240, 1690);

--
-- Indexes for dumped tables
--

--
-- Indexes for table `enum_behavior_mobs`
--
ALTER TABLE `enum_behavior_mobs`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `Behavior` (`Behavior`);

--
-- Indexes for table `enum_effects_list`
--
ALTER TABLE `enum_effects_list`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `Name` (`Name`);

--
-- Indexes for table `enum_items_functional`
--
ALTER TABLE `enum_items_functional`
  ADD PRIMARY KEY (`FunctionID`);

--
-- Indexes for table `enum_items_types`
--
ALTER TABLE `enum_items_types`
  ADD PRIMARY KEY (`TypeID`);

--
-- Indexes for table `enum_quest_interactive`
--
ALTER TABLE `enum_quest_interactive`
  ADD KEY `ID` (`ID`);

--
-- Indexes for table `enum_worlds`
--
ALTER TABLE `enum_worlds`
  ADD PRIMARY KEY (`WorldID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `Name` (`Name`),
  ADD KEY `SafeZoneWorldID` (`RespawnWorld`),
  ADD KEY `WorldID_2` (`WorldID`),
  ADD KEY `RequiredQuestID` (`RequiredQuestID`);

--
-- Indexes for table `tw_accounts`
--
ALTER TABLE `tw_accounts`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `Password` (`Password`),
  ADD KEY `Username` (`Username`);

--
-- Indexes for table `tw_accounts_aethers`
--
ALTER TABLE `tw_accounts_aethers`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `TeleportID` (`AetherID`);

--
-- Indexes for table `tw_accounts_bans`
--
ALTER TABLE `tw_accounts_bans`
  ADD PRIMARY KEY (`Id`),
  ADD KEY `tw_accounts_bans_tw_accounts_ID_fk` (`AccountId`);

--
-- Indexes for table `tw_accounts_data`
--
ALTER TABLE `tw_accounts_data`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `Nick` (`Nick`),
  ADD KEY `MemberID` (`GuildID`),
  ADD KEY `DiscordID` (`DiscordID`),
  ADD KEY `tw_accounts_data_ibfk_3` (`WorldID`),
  ADD KEY `GuildRank` (`GuildRank`),
  ADD KEY `Level` (`Level`),
  ADD KEY `Exp` (`Exp`);

--
-- Indexes for table `tw_accounts_farming`
--
ALTER TABLE `tw_accounts_farming`
  ADD PRIMARY KEY (`UserID`),
  ADD UNIQUE KEY `AccountID` (`UserID`);

--
-- Indexes for table `tw_accounts_items`
--
ALTER TABLE `tw_accounts_items`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `ItemID` (`ItemID`);

--
-- Indexes for table `tw_accounts_mailbox`
--
ALTER TABLE `tw_accounts_mailbox`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `tw_accounts_inbox_ibfk_2` (`ItemID`);

--
-- Indexes for table `tw_accounts_mining`
--
ALTER TABLE `tw_accounts_mining`
  ADD PRIMARY KEY (`UserID`),
  ADD UNIQUE KEY `AccountID` (`UserID`);

--
-- Indexes for table `tw_accounts_quests`
--
ALTER TABLE `tw_accounts_quests`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD UNIQUE KEY `UK_tw_accounts_quests` (`QuestID`,`UserID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `tw_accounts_quests_ibfk_4` (`QuestID`);

--
-- Indexes for table `tw_accounts_skills`
--
ALTER TABLE `tw_accounts_skills`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `SkillID` (`SkillID`),
  ADD KEY `OwnerID` (`UserID`);

--
-- Indexes for table `tw_account_eidolon_enhancements`
--
ALTER TABLE `tw_account_eidolon_enhancements`
  ADD PRIMARY KEY (`ID`);

--
-- Indexes for table `tw_aethers`
--
ALTER TABLE `tw_aethers`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Indexes for table `tw_attributs`
--
ALTER TABLE `tw_attributs`
  ADD PRIMARY KEY (`ID`);

--
-- Indexes for table `tw_auction_items`
--
ALTER TABLE `tw_auction_items`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `Time` (`ValidUntil`),
  ADD KEY `Price` (`Price`);

--
-- Indexes for table `tw_bots_info`
--
ALTER TABLE `tw_bots_info`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `SlotWings` (`SlotArmor`),
  ADD KEY `SlotHammer` (`SlotHammer`),
  ADD KEY `SlotGun` (`SlotGun`),
  ADD KEY `tw_bots_world_ibfk_4` (`SlotShotgun`),
  ADD KEY `SlotGrenade` (`SlotGrenade`),
  ADD KEY `SlotRifle` (`SlotRifle`);

--
-- Indexes for table `tw_bots_mobs`
--
ALTER TABLE `tw_bots_mobs`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `MobID` (`BotID`),
  ADD KEY `it_drop_0` (`it_drop_0`),
  ADD KEY `it_drop_1` (`it_drop_1`),
  ADD KEY `it_drop_2` (`it_drop_2`),
  ADD KEY `it_drop_3` (`it_drop_3`),
  ADD KEY `it_drop_4` (`it_drop_4`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `Effect` (`Effect`),
  ADD KEY `Behavior` (`Behavior`);

--
-- Indexes for table `tw_bots_npc`
--
ALTER TABLE `tw_bots_npc`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `MobID` (`BotID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `tw_bots_npc_ibfk_3` (`Emote`),
  ADD KEY `tw_bots_npc_ibfk_5` (`GiveQuestID`);

--
-- Indexes for table `tw_bots_quest`
--
ALTER TABLE `tw_bots_quest`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `MobID` (`BotID`),
  ADD KEY `it_need_0` (`RequiredItemID1`),
  ADD KEY `tw_bots_quest_ibfk_3` (`RequiredItemID2`),
  ADD KEY `tw_bots_quest_ibfk_4` (`RewardItemID1`),
  ADD KEY `it_reward_1` (`RewardItemID2`),
  ADD KEY `QuestID` (`QuestID`),
  ADD KEY `tw_bots_quest_ibfk_6` (`RequiredDefeatMobID1`),
  ADD KEY `tw_bots_quest_ibfk_7` (`RequiredDefeatMobID2`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `interactive_type` (`InteractionType`);

--
-- Indexes for table `tw_crafts_list`
--
ALTER TABLE `tw_crafts_list`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `CraftIID` (`ItemID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Indexes for table `tw_dungeons`
--
ALTER TABLE `tw_dungeons`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Indexes for table `tw_dungeons_door`
--
ALTER TABLE `tw_dungeons_door`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_dungeons_door_ibfk_1` (`DungeonID`),
  ADD KEY `tw_dungeons_door_ibfk_2` (`BotID`);

--
-- Indexes for table `tw_dungeons_records`
--
ALTER TABLE `tw_dungeons_records`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_dungeons_records_ibfk_1` (`UserID`),
  ADD KEY `DungeonID` (`DungeonID`),
  ADD KEY `Seconds` (`Seconds`);

--
-- Indexes for table `tw_guilds`
--
ALTER TABLE `tw_guilds`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `Bank` (`Bank`),
  ADD KEY `Level` (`Level`),
  ADD KEY `Experience` (`Experience`);

--
-- Indexes for table `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_guilds_decorations_ibfk_2` (`ItemID`),
  ADD KEY `tw_guilds_decorations_ibfk_3` (`WorldID`),
  ADD KEY `HouseID` (`HouseID`);

--
-- Indexes for table `tw_guilds_history`
--
ALTER TABLE `tw_guilds_history`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `MemberID` (`GuildID`);

--
-- Indexes for table `tw_guilds_houses`
--
ALTER TABLE `tw_guilds_houses`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerMID` (`GuildID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Indexes for table `tw_guilds_invites`
--
ALTER TABLE `tw_guilds_invites`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `MemberID` (`GuildID`);

--
-- Indexes for table `tw_guilds_ranks`
--
ALTER TABLE `tw_guilds_ranks`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `MemberID` (`GuildID`);

--
-- Indexes for table `tw_houses`
--
ALTER TABLE `tw_houses`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `PlantID` (`PlantID`);

--
-- Indexes for table `tw_houses_decorations`
--
ALTER TABLE `tw_houses_decorations`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `HouseID` (`HouseID`),
  ADD KEY `DecoID` (`ItemID`);

--
-- Indexes for table `tw_items_list`
--
ALTER TABLE `tw_items_list`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ItemID` (`ID`),
  ADD KEY `ItemBonus` (`Attribute0`),
  ADD KEY `ItemID_2` (`ID`),
  ADD KEY `ItemType` (`Type`),
  ADD KEY `tw_items_list_ibfk_3` (`Function`),
  ADD KEY `tw_items_list_ibfk_5` (`Attribute1`);

--
-- Indexes for table `tw_logics_worlds`
--
ALTER TABLE `tw_logics_worlds`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `MobID` (`MobID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `ParseInt` (`ParseInt`);

--
-- Indexes for table `tw_positions_mining`
--
ALTER TABLE `tw_positions_mining`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Indexes for table `tw_positions_plant`
--
ALTER TABLE `tw_positions_plant`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Indexes for table `tw_quests_list`
--
ALTER TABLE `tw_quests_list`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`);

--
-- Indexes for table `tw_skills_list`
--
ALTER TABLE `tw_skills_list`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`);

--
-- Indexes for table `tw_voucher`
--
ALTER TABLE `tw_voucher`
  ADD PRIMARY KEY (`ID`);

--
-- Indexes for table `tw_voucher_redeemed`
--
ALTER TABLE `tw_voucher_redeemed`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_voucher_redeemed_ibfk_1` (`UserID`),
  ADD KEY `tw_voucher_redeemed_ibfk_2` (`VoucherID`);

--
-- Indexes for table `tw_warehouses`
--
ALTER TABLE `tw_warehouses`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `Currency` (`Currency`);

--
-- Indexes for table `tw_warehouse_items`
--
ALTER TABLE `tw_warehouse_items`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `StorageID` (`WarehouseID`),
  ADD KEY `NeedItem` (`RequiredItemID`),
  ADD KEY `Price` (`Price`);

--
-- Indexes for table `tw_world_swap`
--
ALTER TABLE `tw_world_swap`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `TwoWorldID` (`TwoWorldID`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `enum_behavior_mobs`
--
ALTER TABLE `enum_behavior_mobs`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- AUTO_INCREMENT for table `enum_effects_list`
--
ALTER TABLE `enum_effects_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- AUTO_INCREMENT for table `enum_items_functional`
--
ALTER TABLE `enum_items_functional`
  MODIFY `FunctionID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=15;

--
-- AUTO_INCREMENT for table `enum_items_types`
--
ALTER TABLE `enum_items_types`
  MODIFY `TypeID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=9;

--
-- AUTO_INCREMENT for table `tw_accounts`
--
ALTER TABLE `tw_accounts`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_accounts_aethers`
--
ALTER TABLE `tw_accounts_aethers`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_accounts_bans`
--
ALTER TABLE `tw_accounts_bans`
  MODIFY `Id` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_accounts_data`
--
ALTER TABLE `tw_accounts_data`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_accounts_items`
--
ALTER TABLE `tw_accounts_items`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_accounts_mailbox`
--
ALTER TABLE `tw_accounts_mailbox`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_accounts_quests`
--
ALTER TABLE `tw_accounts_quests`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_accounts_skills`
--
ALTER TABLE `tw_accounts_skills`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_account_eidolon_enhancements`
--
ALTER TABLE `tw_account_eidolon_enhancements`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_aethers`
--
ALTER TABLE `tw_aethers`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=9;

--
-- AUTO_INCREMENT for table `tw_auction_items`
--
ALTER TABLE `tw_auction_items`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_bots_info`
--
ALTER TABLE `tw_bots_info`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=62;

--
-- AUTO_INCREMENT for table `tw_bots_mobs`
--
ALTER TABLE `tw_bots_mobs`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=22;

--
-- AUTO_INCREMENT for table `tw_bots_npc`
--
ALTER TABLE `tw_bots_npc`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=24;

--
-- AUTO_INCREMENT for table `tw_bots_quest`
--
ALTER TABLE `tw_bots_quest`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=121;

--
-- AUTO_INCREMENT for table `tw_crafts_list`
--
ALTER TABLE `tw_crafts_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=26;

--
-- AUTO_INCREMENT for table `tw_dungeons`
--
ALTER TABLE `tw_dungeons`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT for table `tw_dungeons_door`
--
ALTER TABLE `tw_dungeons_door`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=5;

--
-- AUTO_INCREMENT for table `tw_dungeons_records`
--
ALTER TABLE `tw_dungeons_records`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_guilds`
--
ALTER TABLE `tw_guilds`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=11;

--
-- AUTO_INCREMENT for table `tw_guilds_history`
--
ALTER TABLE `tw_guilds_history`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_guilds_houses`
--
ALTER TABLE `tw_guilds_houses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- AUTO_INCREMENT for table `tw_guilds_invites`
--
ALTER TABLE `tw_guilds_invites`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_guilds_ranks`
--
ALTER TABLE `tw_guilds_ranks`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_houses`
--
ALTER TABLE `tw_houses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=7;

--
-- AUTO_INCREMENT for table `tw_houses_decorations`
--
ALTER TABLE `tw_houses_decorations`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=27;

--
-- AUTO_INCREMENT for table `tw_items_list`
--
ALTER TABLE `tw_items_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=93;

--
-- AUTO_INCREMENT for table `tw_logics_worlds`
--
ALTER TABLE `tw_logics_worlds`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_positions_mining`
--
ALTER TABLE `tw_positions_mining`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT for table `tw_positions_plant`
--
ALTER TABLE `tw_positions_plant`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;

--
-- AUTO_INCREMENT for table `tw_quests_list`
--
ALTER TABLE `tw_quests_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=40;

--
-- AUTO_INCREMENT for table `tw_skills_list`
--
ALTER TABLE `tw_skills_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=9;

--
-- AUTO_INCREMENT for table `tw_voucher`
--
ALTER TABLE `tw_voucher`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;

--
-- AUTO_INCREMENT for table `tw_voucher_redeemed`
--
ALTER TABLE `tw_voucher_redeemed`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- AUTO_INCREMENT for table `tw_warehouses`
--
ALTER TABLE `tw_warehouses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=5;

--
-- AUTO_INCREMENT for table `tw_warehouse_items`
--
ALTER TABLE `tw_warehouse_items`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=16;

--
-- AUTO_INCREMENT for table `tw_world_swap`
--
ALTER TABLE `tw_world_swap`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=14;

--
-- Constraints for dumped tables
--

--
-- Constraints for table `enum_worlds`
--
ALTER TABLE `enum_worlds`
  ADD CONSTRAINT `enum_worlds_ibfk_1` FOREIGN KEY (`RequiredQuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_accounts_aethers`
--
ALTER TABLE `tw_accounts_aethers`
  ADD CONSTRAINT `tw_accounts_aethers_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_aethers_ibfk_2` FOREIGN KEY (`AetherID`) REFERENCES `tw_aethers` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_accounts_bans`
--
ALTER TABLE `tw_accounts_bans`
  ADD CONSTRAINT `tw_accounts_bans_tw_accounts_ID_fk` FOREIGN KEY (`AccountId`) REFERENCES `tw_accounts` (`ID`);

--
-- Constraints for table `tw_accounts_data`
--
ALTER TABLE `tw_accounts_data`
  ADD CONSTRAINT `tw_accounts_data_ibfk_3` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_data_ibfk_4` FOREIGN KEY (`GuildRank`) REFERENCES `tw_guilds_ranks` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_data_ibfk_5` FOREIGN KEY (`ID`) REFERENCES `tw_accounts` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_accounts_farming`
--
ALTER TABLE `tw_accounts_farming`
  ADD CONSTRAINT `tw_accounts_farming_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_accounts_items`
--
ALTER TABLE `tw_accounts_items`
  ADD CONSTRAINT `tw_accounts_items_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_items_ibfk_2` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_accounts_mailbox`
--
ALTER TABLE `tw_accounts_mailbox`
  ADD CONSTRAINT `tw_accounts_mailbox_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_mailbox_ibfk_2` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_accounts_mining`
--
ALTER TABLE `tw_accounts_mining`
  ADD CONSTRAINT `tw_accounts_mining_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_accounts_quests`
--
ALTER TABLE `tw_accounts_quests`
  ADD CONSTRAINT `tw_accounts_quests_ibfk_3` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_quests_ibfk_4` FOREIGN KEY (`QuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_accounts_skills`
--
ALTER TABLE `tw_accounts_skills`
  ADD CONSTRAINT `tw_accounts_skills_ibfk_1` FOREIGN KEY (`SkillID`) REFERENCES `tw_skills_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_skills_ibfk_2` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_aethers`
--
ALTER TABLE `tw_aethers`
  ADD CONSTRAINT `tw_aethers_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_bots_info`
--
ALTER TABLE `tw_bots_info`
  ADD CONSTRAINT `tw_bots_info_ibfk_1` FOREIGN KEY (`SlotArmor`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_2` FOREIGN KEY (`SlotHammer`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_3` FOREIGN KEY (`SlotGun`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_4` FOREIGN KEY (`SlotShotgun`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_5` FOREIGN KEY (`SlotGrenade`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_6` FOREIGN KEY (`SlotRifle`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_bots_mobs`
--
ALTER TABLE `tw_bots_mobs`
  ADD CONSTRAINT `tw_bots_mobs_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_10` FOREIGN KEY (`it_drop_1`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_11` FOREIGN KEY (`it_drop_2`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_12` FOREIGN KEY (`it_drop_3`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_13` FOREIGN KEY (`it_drop_4`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_8` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE NO ACTION ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_9` FOREIGN KEY (`it_drop_0`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Constraints for table `tw_bots_npc`
--
ALTER TABLE `tw_bots_npc`
  ADD CONSTRAINT `tw_bots_npc_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_npc_ibfk_4` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_npc_ibfk_5` FOREIGN KEY (`GiveQuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_bots_quest`
--
ALTER TABLE `tw_bots_quest`
  ADD CONSTRAINT `tw_bots_quest_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_10` FOREIGN KEY (`InteractionType`) REFERENCES `enum_quest_interactive` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_2` FOREIGN KEY (`RequiredItemID1`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_3` FOREIGN KEY (`RequiredItemID2`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_4` FOREIGN KEY (`RewardItemID1`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_5` FOREIGN KEY (`RewardItemID2`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_6` FOREIGN KEY (`RequiredDefeatMobID1`) REFERENCES `tw_bots_info` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_7` FOREIGN KEY (`RequiredDefeatMobID2`) REFERENCES `tw_bots_info` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_8` FOREIGN KEY (`QuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_9` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Constraints for table `tw_crafts_list`
--
ALTER TABLE `tw_crafts_list`
  ADD CONSTRAINT `tw_crafts_list_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_crafts_list_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_dungeons`
--
ALTER TABLE `tw_dungeons`
  ADD CONSTRAINT `tw_dungeons_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE NO ACTION ON UPDATE CASCADE;

--
-- Constraints for table `tw_dungeons_door`
--
ALTER TABLE `tw_dungeons_door`
  ADD CONSTRAINT `tw_dungeons_door_ibfk_1` FOREIGN KEY (`DungeonID`) REFERENCES `tw_dungeons` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_dungeons_door_ibfk_2` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_dungeons_records`
--
ALTER TABLE `tw_dungeons_records`
  ADD CONSTRAINT `tw_dungeons_records_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_dungeons_records_ibfk_2` FOREIGN KEY (`DungeonID`) REFERENCES `tw_dungeons` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_guilds`
--
ALTER TABLE `tw_guilds`
  ADD CONSTRAINT `tw_guilds_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Constraints for table `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_2` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_3` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_4` FOREIGN KEY (`HouseID`) REFERENCES `tw_guilds_houses` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_guilds_history`
--
ALTER TABLE `tw_guilds_history`
  ADD CONSTRAINT `tw_guilds_history_ibfk_1` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_guilds_houses`
--
ALTER TABLE `tw_guilds_houses`
  ADD CONSTRAINT `tw_guilds_houses_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_houses_ibfk_2` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Constraints for table `tw_guilds_ranks`
--
ALTER TABLE `tw_guilds_ranks`
  ADD CONSTRAINT `tw_guilds_ranks_ibfk_1` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_houses`
--
ALTER TABLE `tw_houses`
  ADD CONSTRAINT `tw_houses_ibfk_1` FOREIGN KEY (`PlantID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_ibfk_3` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_houses_decorations`
--
ALTER TABLE `tw_houses_decorations`
  ADD CONSTRAINT `tw_houses_decorations_ibfk_1` FOREIGN KEY (`HouseID`) REFERENCES `tw_houses` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_decorations_ibfk_2` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_decorations_ibfk_3` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_items_list`
--
ALTER TABLE `tw_items_list`
  ADD CONSTRAINT `tw_items_list_ibfk_1` FOREIGN KEY (`Type`) REFERENCES `enum_items_types` (`TypeID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_items_list_ibfk_2` FOREIGN KEY (`Function`) REFERENCES `enum_items_functional` (`FunctionID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_items_list_ibfk_3` FOREIGN KEY (`Attribute0`) REFERENCES `tw_attributs` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_items_list_ibfk_4` FOREIGN KEY (`Attribute1`) REFERENCES `tw_attributs` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_logics_worlds`
--
ALTER TABLE `tw_logics_worlds`
  ADD CONSTRAINT `tw_logics_worlds_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_logics_worlds_ibfk_2` FOREIGN KEY (`ParseInt`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_positions_mining`
--
ALTER TABLE `tw_positions_mining`
  ADD CONSTRAINT `tw_positions_mining_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_positions_mining_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_positions_mining_ibfk_3` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_positions_mining_ibfk_4` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_positions_plant`
--
ALTER TABLE `tw_positions_plant`
  ADD CONSTRAINT `tw_positions_plant_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_positions_plant_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_voucher_redeemed`
--
ALTER TABLE `tw_voucher_redeemed`
  ADD CONSTRAINT `tw_voucher_redeemed_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_voucher_redeemed_ibfk_2` FOREIGN KEY (`VoucherID`) REFERENCES `tw_voucher` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_warehouses`
--
ALTER TABLE `tw_warehouses`
  ADD CONSTRAINT `tw_warehouses_ibfk_1` FOREIGN KEY (`Currency`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_warehouses_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`);

--
-- Constraints for table `tw_warehouse_items`
--
ALTER TABLE `tw_warehouse_items`
  ADD CONSTRAINT `tw_warehouse_items_ibfk_1` FOREIGN KEY (`WarehouseID`) REFERENCES `tw_warehouses` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_warehouse_items_ibfk_2` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_warehouse_items_ibfk_3` FOREIGN KEY (`RequiredItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_world_swap`
--
ALTER TABLE `tw_world_swap`
  ADD CONSTRAINT `tw_world_swap_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_world_swap_ibfk_2` FOREIGN KEY (`TwoWorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
