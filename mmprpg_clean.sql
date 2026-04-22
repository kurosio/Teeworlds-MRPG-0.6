-- phpMyAdmin SQL Dump
-- version 5.2.2
-- https://www.phpmyadmin.net/
--
-- Host: mariadb
-- Generation Time: Mar 19, 2026 at 07:05 PM
-- Server version: 10.11.14-MariaDB-ubu2204
-- PHP Version: 8.2.27

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
  `Name` varchar(16) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Dumping data for table `enum_effects_list`
--

INSERT INTO `enum_effects_list` (`ID`, `Name`) VALUES
(3, 'Fire'),
(2, 'Poison'),
(1, 'Slowdown');

-- --------------------------------------------------------

--
-- Table structure for table `enum_professions`
--

CREATE TABLE `enum_professions` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Dumping data for table `enum_professions`
--

INSERT INTO `enum_professions` (`ID`, `Name`) VALUES
(-1, 'Unclassed'),
(0, 'Tank'),
(1, 'DPS'),
(2, 'Healer'),
(3, 'Miner'),
(4, 'Farmer'),
(5, 'Fisherman'),
(6, 'Loader');

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts`
--

CREATE TABLE `tw_accounts` (
  `ID` int(11) NOT NULL,
  `Username` varchar(64) NOT NULL,
  `PinCode` varchar(16) DEFAULT NULL,
  `Password` varchar(64) NOT NULL,
  `PasswordSalt` varchar(64) DEFAULT NULL,
  `RegisterDate` varchar(64) NOT NULL,
  `LoginDate` varchar(64) NOT NULL DEFAULT 'First log in',
  `RegisteredIP` varchar(64) NOT NULL DEFAULT '0.0.0.0',
  `LoginIP` varchar(64) NOT NULL DEFAULT '0.0.0.0',
  `Language` varchar(8) NOT NULL DEFAULT 'en',
  `CountryISO` varchar(32) NOT NULL DEFAULT 'UN',
  `TimeoutCode` varchar(64) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_achievements`
--

CREATE TABLE `tw_accounts_achievements` (
  `AccountID` int(11) NOT NULL,
  `AchievementType` int(11) NOT NULL,
  `Criteria` int(11) NOT NULL,
  `Progress` int(11) NOT NULL DEFAULT 0,
  `CompletedLevel` int(11) NOT NULL DEFAULT 0,
  `UpdatedAt` timestamp NOT NULL DEFAULT current_timestamp() ON UPDATE current_timestamp()
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
  `Reason` varchar(512) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'No Reason Given'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_data`
--

CREATE TABLE `tw_accounts_data` (
  `ID` int(11) NOT NULL,
  `Nick` varchar(32) NOT NULL,
  `WorldID` int(11) DEFAULT NULL,
  `Rating` int(11) NOT NULL DEFAULT 0,
  `ProfessionID` int(11) NOT NULL DEFAULT -1,
  `Bank` varchar(512) DEFAULT '0',
  `CrimeScore` int(11) NOT NULL DEFAULT 0,
  `DailyStamp` bigint(20) NOT NULL DEFAULT 0,
  `WeekStamp` bigint(20) NOT NULL DEFAULT 0,
  `MonthStamp` bigint(20) NOT NULL DEFAULT 0,
  `EquippedSlots` longtext DEFAULT NULL,
  `Achievements` longtext NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

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
  `UserID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL,
  `Sender` varchar(32) NOT NULL DEFAULT '''Game''',
  `Description` varchar(64) NOT NULL,
  `AttachedItems` longtext DEFAULT NULL,
  `Readed` tinyint(1) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_professions`
--

CREATE TABLE `tw_accounts_professions` (
  `ID` int(11) NOT NULL,
  `ProfessionID` int(11) NOT NULL,
  `Data` longtext NOT NULL,
  `UserID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

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
  `UsedByEmoticon` int(11) DEFAULT -1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_accounts_skill_tree`
--

CREATE TABLE `tw_accounts_skill_tree` (
  `UserID` int(11) NOT NULL,
  `SkillID` int(11) NOT NULL,
  `LevelIndex` int(11) NOT NULL,
  `OptionIndex` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_achievements`
--

CREATE TABLE `tw_achievements` (
  `ID` int(11) NOT NULL,
  `Type` int(11) NOT NULL,
  `Criteria` int(11) NOT NULL,
  `Required` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL,
  `Reward` varchar(256) DEFAULT NULL,
  `AchievementPoint` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Dumping data for table `tw_achievements`
--

INSERT INTO `tw_achievements` (`ID`, `Type`, `Criteria`, `Required`, `Name`, `Reward`, `AchievementPoint`) VALUES
(1, 1, 0, 1, 'PVP 1.1 | The first one?', '{\r\n    \"exp\": 10\r\n}', 1),
(2, 1, 0, 10, 'PVP 1.2 | A rookie hit man', '{\n    \"exp\": 50,\n    \"items\": [\n        {\n            \"id\": 1,\n            \"value\": 150\n        }\n    ]\n}', 1),
(3, 1, 0, 25, 'PVP 1.3 | The silent killer?', '{\n    \"exp\": 100,\n    \"items\": [\n        {\n            \"id\": 1,\n            \"value\": 250\n        }\n    ]\n}', 1),
(4, 1, 0, 50, 'PVP 1.4 | Bounty hunter', '{\r\n    \"exp\": 200,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}', 1),
(5, 1, 0, 100, 'PVP 2.1 | The Butcher', '{\r\n    \"exp\": 400,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 450\r\n        }\r\n    ]\r\n}', 4),
(6, 1, 0, 200, 'PVP 2.2 | King of the battle', '{\r\n    \"exp\": 800,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1000\r\n        }\r\n    ]\r\n}', 4),
(7, 1, 0, 500, 'PVP 2.3 | Merciless tee', '{\r\n    \"exp\": 1300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2000\r\n        }\r\n    ]\r\n}', 5),
(8, 1, 0, 750, 'PVP 2.4 | The unstoppable', '{\r\n    \"exp\": 1800,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 3000\r\n        }\r\n    ]\r\n}', 6),
(9, 1, 0, 1000, 'PVP 2.5 | Lord of War', '{\r\n    \"exp\": 2000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 139,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}', 10),
(10, 1, 0, 5000, 'PVP 3.1 | God of destruction', '{\r\n    \"exp\": 2500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}', 30),
(11, 1, 0, 10000, 'PVP 3.2 | Predator and prey', '{\r\n    \"exp\": 5000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 10000\r\n        }\r\n    ]\r\n}', 40),
(12, 1, 0, 15000, 'PVP 3.3 | Scarred face', '{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 20000\r\n        }\r\n    ]\r\n}', 55),
(13, 1, 0, 20000, 'PVP 3.4 | Dominator', '{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 30000\r\n        }\r\n    ]\r\n}', 60),
(14, 1, 0, 50000, 'PVP 3.5 | Legend of the DM', '{\r\n    \"exp\": 30000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 140,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}', 99),
(16, 2, 0, 1, 'PVE 1.1 | Rookie in the MMORPG', '{\r\n    \"exp\": 10,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5\r\n        }\r\n    ]\r\n}', 1),
(17, 2, 0, 50, 'PVE 1.2 | First booty?', '{\r\n    \"exp\": 20,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 50\r\n        }\r\n    ]\r\n}', 1),
(18, 2, 0, 200, 'PVE 1.3 | Hunter\'s apprentice', '{\r\n    \"exp\": 60,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}', 1),
(19, 2, 0, 800, 'PVE 1.4 | Defender of Gridania', '{\r\n    \"exp\": 400,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1000\r\n        }\r\n    ]\r\n}', 1),
(20, 2, 0, 3200, 'PVE 1.5 | Who\'s in charge here?', '{\r\n    \"exp\": 800,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1500\r\n        }\r\n    ]\r\n}', 1),
(21, 2, 0, 9600, 'PVE 2.1 | Who\'s next?', '{\r\n    \"exp\": 1500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 4000\r\n        }\r\n    ]\r\n}', 5),
(22, 2, 0, 28800, 'PVE 2.3 | A slayer of evil', '{\r\n    \"exp\": 2500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 7000\r\n        }\r\n    ]\r\n}', 10),
(23, 2, 0, 86400, 'PVE 2.4 | Meat grinder', '{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 18000\r\n        }\r\n    ]\r\n}', 15),
(24, 2, 0, 259200, 'PVE 2.5 | The legendary traveler', '{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 24000\r\n        }\r\n    ]\r\n}', 30),
(25, 2, 0, 518400, 'PVE 3.1 | The eternal hunter', '{\r\n    \"exp\": 25000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 50000\r\n        }\r\n    ]\r\n}', 40),
(26, 2, 0, 1036800, 'PVE 3.2 | Who will survive?', '{\r\n    \"exp\": 50000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 75000\r\n        }\r\n    ]\r\n}', 55),
(27, 2, 0, 1555200, 'PVE 3.3 | A trail of blood', '{\r\n    \"exp\": 70000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100000\r\n        }\r\n    ]\r\n}', 70),
(28, 2, 0, 2177280, 'PVE 3.4 | Reaper of Souls', '{\r\n    \"exp\": 80000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100000\r\n        }\r\n    ]\r\n}', 80),
(29, 2, 0, 3265920, 'PVE 3.5 | Dungeon King', '{\r\n    \"exp\": 100000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 142,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}', 99),
(30, 4, 0, 1, 'Died 1.1 | First blood', '{\r\n    \"exp\": 10\r\n}', 1),
(31, 4, 0, 50, 'Died 1.2 | You\'re so stubborn', '{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}', 1),
(32, 4, 0, 200, 'Died 1.3 | Master of Respawn', '{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 150\r\n        }\r\n    ]\r\n}', 1),
(33, 4, 0, 400, 'Died 1.4 | Immortal... almost', '{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 200\r\n        }\r\n    ]\r\n}', 1),
(34, 4, 0, 1000, 'Died 1.5 | Unbreakable? joke', '{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 500\r\n        }\r\n    ]\r\n}', 1),
(35, 4, 0, 5000, 'Died 2.1 | Infinite loop', '{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1000\r\n        }\r\n    ]\r\n}', 5),
(36, 4, 0, 10000, 'Died 2.2 | Death is your ally', '{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}', 5),
(37, 4, 0, 50000, 'Died 2.3 | Death by boredom', '{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 10000\r\n        }\r\n    ]\r\n}', 25),
(38, 4, 0, 100000, 'Died 2.4 | Self-enemy', '{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 20000\r\n        }\r\n    ]\r\n}', 50),
(39, 4, 0, 200000, 'Died 2.5 | Beyond...', '{\n    \"items\": [\n        {\n            \"id\": 1,\n            \"value\": 50000\n        }\n    ]\n}', 99),
(40, 11, 0, 5, 'Tank 1. | Rookie in armor', '{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}', 1),
(41, 11, 0, 25, 'Tank 2. | Iron Guardian', '{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}', 3),
(42, 11, 0, 50, 'Tank 3. | Lockout Wizard', '{\n    \"exp\": 300,\n    \"items\": [\n        {\n            \"id\": 1,\n            \"value\": 600\n        }\n    ]\n}', 6),
(43, 11, 0, 100, 'Tank 4. | A fortress in armor', '{\r\n    \"exp\": 500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}', 12),
(44, 11, 0, 250, 'Tank 5. | Iron Garrison', '{\r\n    \"exp\": 1000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}', 24),
(45, 11, 0, 500, 'Tank 6. | An unconquered warrior', '{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}', 36),
(46, 11, 0, 1000, 'Tank 7. | Lord of Armor', '{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 143,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}', 48),
(47, 11, 1, 5, 'DPS 1. | A shadow in the night', '{\n    \"exp\": 100,\n    \"items\": [\n        {\n            \"id\": 1,\n            \"value\": 100\n        }\n    ]\n}', 1),
(48, 11, 1, 25, 'DPS 2. | A budding assassin', '{\n    \"exp\": 150,\n    \"items\": [\n        {\n            \"id\": 1,\n            \"value\": 300\n        }\n    ]\n}', 3),
(49, 11, 1, 50, 'DPS 3. | A ghost in the shadows', '{\n    \"exp\": 300,\n    \"items\": [\n        {\n            \"id\": 1,\n            \"value\": 600\n        }\n    ]\n}', 6),
(50, 11, 1, 100, 'DPS 4. | Blade of Fate', '{\n    \"exp\": 500,\n    \"items\": [\n        {\n            \"id\": 1,\n            \"value\": 800\n        }\n    ]\n}', 12),
(51, 11, 1, 250, 'DPS 5. | The Dark Avenger', '{\n    \"exp\": 1000,\n    \"items\": [\n        {\n            \"id\": 1,\n            \"value\": 5000\n        }\n    ]\n}', 24),
(52, 11, 1, 500, 'DPS 6. | The death stare', '{\n    \"exp\": 10000,\n    \"items\": [\n        {\n            \"id\": 1,\n            \"value\": 15000\n        }\n    ]\n}', 36),
(53, 11, 1, 1000, 'DPS 7. | Lord of DMG', '{\n    \"exp\": 15000,\n    \"items\": [\n        {\n            \"id\": 144,\n            \"value\": 1\n        }\n    ]\n}', 48),
(54, 11, 2, 5, 'Healer 1. | Light of Hope', '{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}', 1),
(55, 11, 2, 25, 'Healer 2. | Beginning healer', '{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}', 3),
(56, 11, 2, 50, 'Healer 3. | Guardian of Life', '{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 600\r\n        }\r\n    ]\r\n}', 6),
(57, 11, 2, 100, 'Healer 4. | Recovery Wizard', '{\r\n    \"exp\": 500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}', 12),
(58, 11, 2, 250, 'Healer 5. | Legend of Healing', '{\r\n    \"exp\": 1000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}', 24),
(59, 11, 2, 500, 'Healer 6. | Bright angel', '{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}', 36),
(60, 11, 2, 1000, 'Healer 7. | Lord of Life', '{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 145,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}', 48),
(61, 11, 3, 5, 'Miner 1. | Rookie and pickaxe', '{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}', 1),
(62, 11, 3, 25, 'Miner 2. | Ore Seeker', '{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}', 2),
(63, 11, 3, 50, 'Miner 3. | Ore-savvy', '{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 600\r\n        }\r\n    ]\r\n}', 4),
(64, 11, 3, 100, 'Miner 4. | Master of Mining', '{\r\n    \"exp\": 500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}', 8),
(65, 11, 3, 250, 'Miner 5. | The Digger of Depths', '{\r\n    \"exp\": 1000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}', 16),
(66, 11, 3, 500, 'Miner 6. | King of the mines', '{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}', 32),
(67, 11, 3, 1000, 'Miner 7. | Master of the deep', '{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 146,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}', 48),
(68, 11, 4, 5, 'Farmer 1. | A seed of hope', '{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}', 1),
(69, 11, 4, 25, 'Farmer 2. | Rookie farmer', '{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}', 2),
(70, 11, 4, 50, 'Farmer 3. | Green Thumb', '{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 600\r\n        }\r\n    ]\r\n}', 4),
(71, 11, 4, 100, 'Farmer 4. | Harvest Master', '{\r\n    \"exp\": 500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}', 8),
(72, 11, 4, 250, 'Farmer 5. | Lord of the beds', '{\r\n    \"exp\": 1000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}', 16),
(73, 11, 4, 500, 'Farmer 6. | Gatherer of Plenty', '{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}', 32),
(74, 11, 4, 1000, 'Farmer 7. | Harvest King', '{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 147,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}', 48),
(75, 5, 0, 1000, 'DAMAGE 1. | A light punch', '{\r\n    \"exp\": 10\r\n}', 1),
(76, 5, 0, 100000, 'DAMAGE 2. | Attempt to do damage', '{\r\n    \"exp\": 100\r\n}', 5),
(77, 5, 0, 1000000, 'DAMAGE 3. | A serious threat?', '{\r\n    \"exp\": 500\r\n}', 10),
(78, 5, 0, 10000000, 'DAMAGE 4. | Armageddon', '{\r\n    \"exp\": 1000\r\n}', 20),
(79, 5, 0, 100000000, 'DAMAGE 5. | Lord of Damage', '{\r\n    \"exp\": 5000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 148,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}', 40),
(80, 8, 1, 500, 'Gold 1. | The first coin', '{\r\n    \"exp\": 100\r\n}', 1),
(81, 8, 1, 5000, 'Gold 2. | An aspiring rich man', '{\r\n    \"exp\": 500\r\n}', 2),
(82, 8, 1, 100000, 'Gold 3. | A lover of glitter', '{\r\n    \"exp\": 1000\r\n}', 4),
(83, 8, 1, 1000000, 'Gold 4. | MMORPG Oligarch', '{\r\n    \"exp\": 5000\r\n}', 8),
(84, 8, 1, 10000000, 'Gold 5. | King of Gold', '{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 149,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}', 16),
(85, 6, 100, 1, 'Weapons | Pizdamet LOL?', '{\r\n    \"exp\": 200\r\n}', 1),
(86, 6, 103, 1, 'Weapons | A wall of laser. What?', '{\r\n    \"exp\": 200\r\n}', 1),
(87, 6, 102, 1, 'Weapons | Don\'t blow yourself up', '{\r\n    \"exp\": 200\r\n}', 1),
(88, 6, 99, 1, 'Weapons | A hammer on a leash', '{\r\n    \"exp\": 200\r\n}', 1),
(89, 7, 95, 1, 'Items | It\'s guild time', '{\r\n    \"exp\": 50\r\n}', 1),
(90, 6, 96, 1, 'Module | I wish I had a new skin', '{\r\n    \"exp\": 50\r\n}', 1),
(91, 6, 97, 1, 'Module | I\'m not in pain', '{\r\n    \"exp\": 100\r\n}', 1),
(92, 6, 64, 1, 'Module | What a poisonous tail', '{\r\n    \"exp\": 100\r\n}', 1),
(93, 6, 65, 1, 'Module | Jump and explode', '{\r\n    \"exp\": 100\r\n}', 1),
(94, 6, 66, 1, 'Module | I\'m a spider', '{\r\n    \"exp\": 100\r\n}', 1),
(95, 3, 31, 5000, 'Defeat | Chigoe Killer', '{\r\n    \"exp\": 200\r\n}', 1),
(96, 3, 32, 5000, 'Defeat | Anole Killer', '{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1500\r\n        }\r\n    ]\r\n}', 5),
(97, 3, 33, 5000, 'Defeat | Scrambler Killer', '{\r\n    \"exp\": 120,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1700\r\n        }\r\n    ]\r\n}', 5),
(98, 3, 34, 5000, 'Defeat | Mushroom Killer', '{\r\n    \"exp\": 110,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1600\r\n        }\r\n    ]\r\n}', 5),
(99, 3, 35, 5000, 'Defeat | Shadow shape Killer', '{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2000\r\n        }\r\n    ]\r\n}', 5),
(100, 3, 36, 5000, 'Defeat | Winged beast Killer', '{\r\n    \"exp\": 170,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2300\r\n        }\r\n    ]\r\n}', 5),
(101, 3, 38, 5000, 'Defeat | Spider Killer', '{\r\n    \"exp\": 180,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2400\r\n        }\r\n    ]\r\n}', 5),
(102, 3, 39, 5000, 'Defeat | Bear Killer', '{\r\n    \"exp\": 190,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2400\r\n        }\r\n    ]\r\n}', 5),
(103, 3, 40, 5000, 'Defeat | Orc Tee Killer', '{\r\n    \"exp\": 200,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2500\r\n        }\r\n    ]\r\n}', 5),
(104, 3, 41, 5000, 'Defeat | Dynamt ogre Killer', '{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 3000\r\n        }\r\n    ]\r\n}', 5),
(105, 3, 42, 5000, 'Defeat | Mad Wolf Killer', '{\r\n    \"exp\": 200,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2500\r\n        }\r\n    ]\r\n}', 5),
(106, 3, 43, 5000, 'Defeat | Doll Tee Killer', '{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 4000\r\n        }\r\n    ]\r\n}', 5),
(107, 3, 44, 5000, 'Defeat | Zombie Killer', '{\r\n    \"exp\": 5000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 6000\r\n        }\r\n    ]\r\n}', 5),
(109, 9, 2, 1, 'Craft | Hard-earned iron', '{\r\n    \"exp\": 100\r\n}', 1),
(110, 9, 46, 1, 'Craft | Stone vs stone', '{\r\n    \"exp\": 80\r\n}', 1),
(111, 9, 12, 1, 'Craft | The iron guy', '{\r\n    \"exp\": 200\r\n}', 1),
(112, 9, 15, 1, 'Craft | Hands-free work', '{\r\n    \"exp\": 80\r\n}', 1),
(113, 9, 18, 1, 'Craft | First pickaxe for ore', '{\r\n    \"exp\": 50\r\n}', 1),
(114, 9, 2, 500, 'Craft | Blacksmith', '{\r\n    \"exp\": 500\r\n}', 5),
(115, 9, 23, 500, 'Craft | Leatherworker', '{\r\n    \"exp\": 500\r\n}', 5),
(116, 9, 44, 1, 'Craft | Some ammunition', '{\r\n    \"exp\": 100\r\n}', 2),
(117, 9, 45, 1, 'Craft | Gunpowder and bullets', '{\r\n    \"exp\": 100\r\n}', 2),
(118, 9, 47, 1, 'Craft | Stone and earth', '{\r\n    \"exp\": 50\r\n}', 1),
(119, 7, 16, 50, 'Items | A capsule experience?', '{\r\n    \"exp\": 50\r\n}', 1),
(120, 7, 17, 50, 'Items | Golden find', '{\r\n    \"exp\": 50\r\n}', 1),
(121, 8, 17, 15, 'Items | I could use a drink', '{\r\n    \"exp\": 50\r\n}', 1),
(122, 6, 4, 1, 'Weapons | Whoo-hoo. Come here', '{\r\n    \"exp\": 30\r\n}', 1),
(123, 6, 5, 1, 'Weapons | It\'s gonna boom', '{\r\n    \"exp\": 40\r\n}', 1),
(124, 6, 6, 1, 'Weapons | Pew pew. I\'m a sniper', '{\r\n    \"exp\": 50\r\n}', 1),
(125, 8, 7, 50, 'Synth 1. | How do you get back', '{\r\n    \"exp\": 10\r\n}', 1),
(126, 8, 7, 300, 'Synth 2. | Disassembled. And?', '{\r\n    \"exp\": 60\r\n}', 2),
(127, 8, 7, 1000, 'Synth 3. | Disassembler', '{\r\n    \"exp\": 150\r\n}', 3),
(128, 8, 7, 10000, 'Synth 4. | Material Generator', '{\r\n    \"exp\": 500\r\n}', 4),
(129, 8, 7, 100000, 'Synth 5. |  Lord Desynthesizer', '{\r\n    \"exp\": 1500\r\n}', 5),
(130, 7, 8, 50, 'Deliverer 1. | I\'m a courier?', '{\r\n    \"exp\": 10\r\n}', 1),
(131, 7, 8, 5000, 'Deliverer 2. | New job', '{\r\n    \"exp\": 100\r\n}', 2),
(132, 7, 8, 50000, 'Deliverer 3. | Where\'s the tip?', '{\r\n    \"exp\": 500\r\n}', 3),
(133, 7, 8, 500000, 'Deliverer 4. | Who else but me?', '{\r\n    \"exp\": 500\r\n}', 4),
(134, 7, 8, 5000000, 'Deliverer 5. | Grocery delivery', '{\r\n    \"exp\": 2500\r\n}', 5),
(135, 11, 6, 5, 'Loader 1. | Its hard', '{\n    \"exp\": 100,\n    \"items\": [\n        {\n            \"id\": 1,\n            \"value\": 100\n        }\n    ]\n}', 1),
(136, 11, 6, 25, 'Loader 2. | Navigator', '{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}', 3),
(137, 11, 6, 50, 'Loader 3. | Postman', '{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 600\r\n        }\r\n    ]\r\n}', 6),
(138, 11, 6, 100, 'Loader 4. | Heavyweight', '{\r\n    \"exp\": 500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}', 12),
(139, 11, 6, 250, 'Loader 5. | Truck', '{\r\n    \"exp\": 1000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1500\r\n        }\r\n    ]\r\n}', 24),
(140, 11, 6, 500, 'Loader 6. | Tanker', '{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}', 24),
(141, 11, 2, 1000, 'Loader 7. | Lord Loader', '{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}', 24),
(142, 10, 5, 0, 'Quest 1 | Beginning', '{\r\n    \"exp\": 10\r\n}', 1),
(143, 10, 16, 0, 'Quest 2 | Hunter', '{\r\n    \"exp\": 100\r\n}', 1),
(144, 10, 19, 0, 'Quest 3 | The Avenger', '{\r\n    \"exp\": 100\r\n}', 1),
(146, 11, 5, 5, 'Fishermen 1. | So fishing', '{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}', 1),
(147, 11, 5, 25, 'Fishermen 2. | Waiting...', '{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}', 1),
(148, 11, 5, 50, 'Fishermen 3. | A fisherman will', '{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 600\r\n        }\r\n    ]\r\n}', 2),
(149, 11, 5, 100, 'Fishermen 4. | Hook up', '{\r\n    \"exp\": 500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}', 4),
(150, 11, 5, 250, 'Fishermen 5. | Water lover', '{\r\n    \"exp\": 1000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1500\r\n        }\r\n    ]\r\n}', 16),
(151, 11, 5, 500, 'Fishermen 6. | Fish in fish', '{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}', 32),
(152, 11, 5, 1000, 'Fishermen 7. | Sushi lover', '{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}', 48),
(153, 9, 97, 1, 'Craft | Time to kill', '{\r\n    \"exp\": 200\r\n}', 1),
(154, 9, 101, 1, 'Craft | The Light of Life', '{\r\n    \"exp\": 200\r\n}', 1),
(155, 9, 52, 1, 'Craft | Big hands', '{\r\n    \"exp\": 200\r\n}', 1);

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
(1, 'Gridania (Center)', 1, 19552, 4718),
(2, 'Apartaments 1 (Left)', 1, 13438, 1747),
(3, 'Apartaments 2  (Right)', 1, 25600, 2661),
(4, 'Swamp (Left)', 1, 8063, 3781),
(5, 'Dungeon (Right)', 1, 25476, 5843),
(6, 'Center of the forest', 6, 7870, 3931);

-- --------------------------------------------------------

--
-- Table structure for table `tw_attributes`
--

CREATE TABLE `tw_attributes` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL,
  `Price` int(11) NOT NULL,
  `Group` int(11) NOT NULL COMMENT '	0-tank. 1-healer. 2-dps. 3-weapon. 4-damage. 5-jobs. 6-others.'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_attributes`
--

INSERT INTO `tw_attributes` (`ID`, `Name`, `Price`, `Group`) VALUES
(1, 'DMG', 5, 4),
(2, 'Attack SPD', 1, 2),
(3, 'Crit DMG', 1, 4),
(4, 'Crit', 1, 2),
(5, 'HP', 1, 0),
(6, 'Lucky', 1, 0),
(7, 'MP', 1, 1),
(8, 'Vampirism', 1, 1),
(9, 'Ammo Regen', 1, 3),
(10, 'Ammo', 5, 3),
(11, 'Efficiency', 5, 5),
(12, 'Extraction', 5, 5),
(13, 'Hammer DMG', 5, 4),
(14, 'Gun DMG', 5, 4),
(15, 'Shotgun DMG', 5, 4),
(16, 'Grenade DMG', 5, 4),
(17, 'Rifle DMG', 5, 4),
(18, 'Lucky Drop', 1, 6),
(19, 'Eidolon PWR', 1, 6),
(20, 'Gold Capacity', 1, 6),
(21, 'Patience', 5, 5),
(22, 'Product Capacity', 1, 5);

-- --------------------------------------------------------

--
-- Table structure for table `tw_auction_slots`
--

CREATE TABLE `tw_auction_slots` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `Value` int(11) NOT NULL,
  `Enchant` int(11) NOT NULL DEFAULT 0,
  `Price` int(11) NOT NULL,
  `OwnerID` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_bots_info`
--

CREATE TABLE `tw_bots_info` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT 'Bot name',
  `JsonTeeInfo` varchar(128) NOT NULL DEFAULT '{ "skin": "default", 	"custom_color": 0, 	"color_body": 0, 	"color_feer": 0}',
  `EquippedModules` varchar(64) NOT NULL DEFAULT '0',
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

INSERT INTO `tw_bots_info` (`ID`, `Name`, `JsonTeeInfo`, `EquippedModules`, `SlotHammer`, `SlotGun`, `SlotShotgun`, `SlotGrenade`, `SlotRifle`, `SlotArmor`) VALUES
(1, 'Batya', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"chinese_by_whis\"}', '0', 2, 3, 4, 5, 6, NULL),
(2, 'Mamya', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"giraffe\"}', '0', 2, 3, 4, 5, 6, NULL),
(3, 'Bertennant', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"wartee\"}', '0', 2, 3, 4, 5, 6, NULL),
(4, 'Protector', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Soldier\"}', '90,91,35,36,37,20', 102, 151, 4, 100, 101, 84),
(5, 'Lead', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"TFT_Ninja\"}', '0', 2, 3, 4, 5, 6, NULL),
(6, 'Mother Miounne', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Hellokittygirl\"}', '0', 2, 3, 4, 5, 6, NULL),
(7, 'Madelle', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"cutee_glow\"}', '0', 2, 3, 4, 5, 6, NULL),
(8, 'Athelyna', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"robin_hood\"}', '0', 2, 3, 4, 5, 6, NULL),
(9, 'Jillian', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"swordl\"}', '0', 2, 3, 4, 5, 6, NULL),
(10, 'Nicia', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Chinswords\"}', '0', 2, 3, 4, 5, 6, NULL),
(11, 'Galfrid', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"conquistador\"}', '0', 2, 3, 4, 5, 6, NULL),
(12, 'Monranguin', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"eliteknight\"}', '0', 2, 3, 4, 5, 6, NULL),
(13, 'Pauline', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"fleur\"}', '0', 2, 3, 4, 5, 6, NULL),
(14, 'Tsubh Khamazom', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"fleurdelacour\"}', '0', 2, 3, 4, 5, 6, NULL),
(15, 'Alestan', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"link\"}', '0', 2, 3, 4, 5, 6, NULL),
(16, 'Keitha', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"lowpolybop\"}', '0', 2, 3, 4, 5, 6, NULL),
(17, 'Roseline', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"deadpool\"}', '0', 2, 3, 4, 5, 6, NULL),
(18, 'Osha Jaab', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"aragorn\"}', '0', 2, 3, 4, 5, 6, NULL),
(19, 'Theodore', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Evil Puffi\"}', '0', 2, 3, 4, 5, 6, NULL),
(20, 'Elmar', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"ronny\"}', '0', 2, 3, 4, 5, 6, NULL),
(21, 'Bernard', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"scary man\"}', '0', 2, 3, 4, 5, 6, NULL),
(22, 'Eylgar', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"TFT_Ninja\"}', '0', 2, 3, 4, 5, 6, NULL),
(23, 'Lothaire', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"XiaoHan\"}', '0', 2, 3, 4, 5, 6, NULL),
(24, 'Leonnie', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"xxmimi\"}', '0', 2, 3, 4, 5, 6, NULL),
(25, 'Armelle', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"zoro\"}', '0', 2, 3, 4, 5, 6, NULL),
(26, 'Jackson', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"cammo\"}', '0', 2, 3, 4, 5, 6, NULL),
(27, 'Luquelot', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Bouldy\"}', '0', 2, 3, 4, 5, 6, NULL),
(28, 'Lewin', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"P-Amazing\"}', '0', 2, 3, 4, 5, 6, NULL),
(29, 'Beatin', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Acid\"}', '0', 2, 3, 4, 5, 6, NULL),
(30, 'Kan-E-Senna', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"mofa_tee\"}', '0', 2, 3, 4, 5, 6, NULL),
(31, 'Chigoe', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"bao_fu for 26\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(32, 'Anole', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"Bob\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(33, 'Scrambler', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"bugofdoom\"}', '0', 2, 3, NULL, NULL, NULL, NULL),
(34, 'Mushroom', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"Goomba Mario\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(35, 'BlackShadow', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"blacktee\"}', '66', 2, NULL, NULL, NULL, 6, NULL),
(36, 'Wing Beast', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"demonlimekitty\"}', '66', 2, NULL, NULL, NULL, NULL, NULL),
(37, 'Janremi Black', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"ahl_War\"}', '0', 2, 3, 4, 5, 6, NULL),
(38, '[M] Spider', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"Spider_KZ\"}', '64,66', 2, NULL, NULL, NULL, NULL, NULL),
(39, 'Bear', '{ \"skin\": \"beardog\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(40, 'Orc Tee', '{ \"skin\": \"orc_tee\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}', '0', 2, NULL, 4, NULL, NULL, NULL),
(41, 'Dynamt ogre', '{ \"skin\": \"dynamite\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}', '65', 102, NULL, NULL, 5, NULL, NULL),
(42, 'Mad Wolf', '{ \"skin\": \"WolfTee\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(43, 'Doll Tee', '{ \"skin\": \"voodoo_tee\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(44, 'Zombie', '{ \"skin\": \"zombie\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(45, 'Spawn', '{ \"skin\": \"cloud_ball\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}', '66', 2, NULL, NULL, NULL, NULL, NULL),
(46, 'Slimtee', '{ \"skin\": \"A Slime Blob\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}', '66', 2, NULL, NULL, NULL, 103, NULL),
(47, '[L] Spider ', '{ \"skin\": \"Spider Small_KZ\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(48, '[B] Spider ', '{ \"skin\": \"spider_god\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}', '66,64', 2, NULL, NULL, NULL, 103, NULL),
(49, 'Fungaloid', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"mushroom\"}', '0', 2, 3, NULL, NULL, NULL, NULL),
(50, 'Horn Beast', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"TeeDevil\"}', '66', 2, NULL, NULL, NULL, NULL, NULL),
(51, 'Chameleon', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"Exotix\"}', '0', 2, NULL, NULL, 4, NULL, NULL),
(52, 'Swamp Ghost ', '{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"ghost_greensward\"}', '66,19', 2, 3, NULL, NULL, NULL, NULL),
(53, 'Ghost', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"ghost\"}', '66', 2, NULL, NULL, NULL, NULL, NULL),
(54, 'Guardian', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"roman\"}', '90,91,35,36,37,20', 99, 151, 4, 100, 150, 84),
(55, 'Pig', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"oink\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(56, 'Ogre', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"defatulYoda\"}', '0', 2, NULL, NULL, NULL, NULL, 28),
(57, 'HarryPotter', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"grosboule\"}', '0', 2, 3, 4, 5, 6, NULL),
(58, 'Knight', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Chinswords\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(59, 'Wraith', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"meta-knight\"}', '0', 2, NULL, 4, 5, NULL, NULL),
(60, 'Gloom', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"glow_greensward\"}', '0', 2, 3, NULL, 5, NULL, NULL),
(61, 'Sentry', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"aperbop\"}', '0', 2, 3, 4, 5, NULL, NULL),
(62, 'Cryst', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"AmethystCat\"}', '0', 2, 1, NULL, 5, 103, NULL),
(63, 'Amethyst', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"amethyst\"}', '0', 2, 3, 4, 5, 6, NULL),
(64, 'Phantom', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"glow_rayxer\"}', '0', 2, 3, NULL, 5, 6, NULL),
(65, 'Ossero', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"bedooin\"}', '0', 99, 3, 4, 100, 101, NULL),
(66, 'Pirate', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Pirate_Dashie\"}', '90,91', 2, NULL, 4, NULL, NULL, NULL),
(67, 'Skeletee', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"skeleton\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(68, 'ShadowCatsk', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Shadow Catsk\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(69, 'Octopus', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"octopus\"}', '316,319,320,66,90,91,97', 102, NULL, NULL, NULL, 324, NULL),
(70, 'Living Plant', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"fleur\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(71, 'Living Stone', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"SisyphusTee\"}', '90,91', 2, 3, NULL, NULL, NULL, NULL),
(72, 'MaterialTee', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Material_Circle\"}', '0', 99, NULL, NULL, NULL, NULL, NULL),
(73, 'Acorn', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Acorn\"}', '64,90,91', 2, NULL, NULL, 5, NULL, NULL),
(74, 'Bee', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"bee\"}', '19,66,90,91', 2, 1, NULL, NULL, NULL, NULL),
(75, 'Emerald', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"emerald\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(76, 'Crab', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Crab_KZ\"}', '64,66', 2, NULL, NULL, NULL, NULL, NULL),
(77, 'Eye', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"EyeBall\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1031, 'GrenadeBoy', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"default\"}', '133,53', 2, NULL, NULL, 5, NULL, NULL),
(1032, 'Machinegunner', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"default\"}', '133,53', 2, 3, NULL, NULL, NULL, NULL),
(1033, 'FlameBoss', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"default\"}', '20,53', 2, NULL, 4, NULL, NULL, NULL),
(1034, 'Pusher', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"default\"}', '53', 2, NULL, NULL, NULL, 101, NULL),
(1035, 'Targeter', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"default\"}', '133', 2, NULL, NULL, NULL, 150, NULL),
(1036, 'Aperbop', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"aperbop\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1037, 'Categer', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"cattoboi\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1038, 'Cheems', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Cheems_meme_KZ\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1039, 'Cutala', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"cutala\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1040, 'Cyan', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Cyan\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1041, 'Dartmonkey', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"dart_monkey\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1042, 'Demix', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Demix\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1043, 'Derp', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"derp\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1044, 'Small Dragon', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"dragon 2\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1045, 'Foks', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"foks\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1046, 'Fuzzball', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"fuzzball\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1047, 'Witch', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"IceWitch\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1048, 'Mauzi', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"mauzi\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1049, 'Muffet', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"muffet\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1050, 'Mantis', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Praying Mantis\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1051, 'Punster', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"punster\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1052, 'Fat frog', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"fat_frog\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(1053, 'Dage', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"M_dage\"}', '0', 2, NULL, NULL, NULL, NULL, NULL);

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
  `Debuffs` set('Slowdown','Poison','Fire') DEFAULT NULL,
  `Behavior` set('sleepy','slower','poisonous','neutral') DEFAULT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Power` int(11) NOT NULL DEFAULT 10,
  `Number` int(11) NOT NULL DEFAULT 1,
  `Respawn` int(11) NOT NULL DEFAULT 1,
  `Radius` int(11) NOT NULL DEFAULT 800,
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

INSERT INTO `tw_bots_mobs` (`ID`, `BotID`, `WorldID`, `PositionX`, `PositionY`, `Debuffs`, `Behavior`, `Level`, `Power`, `Number`, `Respawn`, `Radius`, `Boss`, `it_drop_0`, `it_drop_1`, `it_drop_2`, `it_drop_3`, `it_drop_4`, `it_drop_count`, `it_drop_chance`) VALUES
(1, 31, 1, 11296, 4384, 'Poison', NULL, 2, 10, 2, 5, 800, 0, 22, 107, NULL, NULL, NULL, '|1|1|0|0|0|', '|30|15|0|0|0|'),
(2, 32, 1, 10048, 4384, NULL, 'slower', 2, 10, 2, 5, 800, 0, 33, 107, NULL, NULL, NULL, '|1|1|0|0|0|', '|30|17|0|0|0|'),
(3, 33, 1, 8704, 4384, NULL, NULL, 3, 15, 4, 8, 800, 0, 105, 113, NULL, NULL, NULL, '|1|1|0|0|0|', '|10|10|0|0|0|'),
(4, 34, 1, 9888, 5728, NULL, NULL, 5, 25, 5, 12, 1152, 0, 112, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|30|0|0|0|0|'),
(5, 35, 1, 8512, 4416, 'Fire', 'sleepy', 4, 80, 1, 1100, 448, 1, 111, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|20|0|0|0|0|'),
(6, 49, 1, 9888, 5728, 'Poison', 'sleepy', 6, 120, 1, 1000, 1152, 1, 105, 134, NULL, NULL, NULL, '|15|1|0|0|0|', '|50|1|0|0|0|'),
(8, 46, 1, 9728, 4416, 'Slowdown', 'sleepy,poisonous', 3, 60, 1, 900, 640, 1, 137, 125, NULL, NULL, NULL, '|10|1|0|0|0|', '|5|15|0|0|0|'),
(9, 55, 1, 17255, 4319, NULL, 'sleepy,neutral', 1, 5, 4, 4, 1000, 0, 173, 186, 107, NULL, NULL, '|1|1|1|0|0|', '|20|15|10|0|0|'),
(10, 36, 1, 8352, 2656, NULL, NULL, 20, 100, 4, 12, 960, 0, 110, 107, 116, NULL, NULL, '|1|1|1|0|0|', '|30|10|30|0|0|'),
(11, 42, 1, 5856, 2944, NULL, NULL, 21, 105, 5, 13, 960, 0, 110, 107, NULL, NULL, NULL, '|2|1|0|0|0|', '|30|25|0|0|0|'),
(12, 38, 1, 7456, 6240, 'Poison', NULL, 20, 100, 3, 12, 1184, 0, 92, 105, NULL, NULL, NULL, '|5|3|0|0|0|', '|50|15|0|0|0|'),
(13, 47, 1, 7456, 6240, 'Poison', 'slower,poisonous', 21, 105, 5, 14, 1184, 0, 92, NULL, NULL, NULL, NULL, '|3|0|0|0|0|', '|30|0|0|0|0|'),
(14, 48, 1, 5632, 6048, 'Slowdown,Poison', 'sleepy,slower,poisonous', 21, 420, 1, 1500, 320, 1, 92, 105, 128, NULL, NULL, '|4|2|1|0|0|', '|70|50|100|0|0|'),
(15, 39, 1, 6400, 4352, NULL, 'slower', 21, 105, 4, 12, 1120, 0, 107, 110, NULL, NULL, NULL, '|5|4|0|0|0|', '|30|30|0|0|0|'),
(16, 50, 1, 8352, 2656, NULL, NULL, 21, 420, 1, 1800, 960, 1, 110, 116, 107, NULL, NULL, '|5|5|1|0|0|', '|50|40|30|0|0|'),
(17, 51, 1, 5856, 2944, 'Slowdown,Poison,Fire', 'sleepy,slower', 22, 440, 1, 2000, 320, 1, 110, 115, 111, NULL, NULL, '|10|20|15|0|0|', '|30|50|30|0|0|'),
(18, 52, 1, 5536, 4160, 'Slowdown', 'sleepy,slower', 22, 440, 1, 2300, 320, 1, 137, 115, NULL, 113, 111, '|8|5|0|4|40|', '|70|30|0|70|40|'),
(19, 40, 1, 3424, 3616, 'Slowdown', NULL, 40, 200, 5, 15, 1120, 0, 109, 50, NULL, NULL, NULL, '|2|1|0|0|0|', '|40|100|0|0|0|'),
(20, 41, 1, 3424, 3936, 'Slowdown,Fire', 'slower', 41, 820, 1, 3500, 320, 1, 127, 109, NULL, NULL, NULL, '|5|2|0|0|0|', '|30|100|0|0|0|'),
(21, 45, 1, 2624, 4960, NULL, NULL, 42, 210, 5, 13, 640, 0, 115, NULL, NULL, NULL, NULL, '|5|0|0|0|0|', '|10|0|0|0|0|'),
(22, 53, 1, 4032, 5824, NULL, NULL, 43, 215, 5, 13, 768, 0, 115, 113, NULL, NULL, NULL, '|5|1|0|0|0|', '|10|1|0|0|0|'),
(23, 56, 1, 2624, 6208, NULL, NULL, 44, 220, 5, 14, 512, 0, 114, 113, 109, 120, NULL, '|1|1|1|1|0|', '|60|20|30|1|0|'),
(24, 77, 1, 1248, 6176, NULL, 'sleepy,slower', 46, 230, 4, 12, 576, 0, NULL, NULL, 107, NULL, NULL, '|1|1|1|0|0|', '|20|15|10|0|0|'),
(25, 43, 1, 28224, 2944, NULL, 'sleepy', 60, 300, 5, 13, 448, 0, 111, 114, 125, NULL, NULL, '|5|1|1|0|0|', '|10|20|5|0|0|'),
(27, 58, 2, 3864, 0, NULL, 'slower', 12, 2, 15, 7, 800, 0, 221, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|8|0|0|0|0|'),
(28, 59, 2, 7409, 270, NULL, NULL, 13, 1, 10, 6, 800, 0, 221, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|10|0|0|0|0|'),
(29, 60, 2, 5337, 2764, NULL, NULL, 14, 1, 14, 7, 800, 0, 221, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|12|0|0|0|0|'),
(30, 62, 2, 7896, 1713, NULL, 'slower,poisonous', 40, 8, 2, 1, 800, 1, 221, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|14|0|0|0|0|'),
(31, 63, 2, 2382, 1587, NULL, 'slower', 16, 1, 12, 6, 800, 0, 221, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|'),
(32, 64, 2, 1993, 3601, NULL, NULL, 17, 3, 16, 8, 800, 0, 221, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|12|0|0|0|0|'),
(33, 65, 2, 7798, 3505, NULL, 'slower,poisonous', 80, 15, 1, 1, 400, 1, 221, 317, NULL, NULL, NULL, '|5|1|0|0|0|', '|100|20|0|0|0|'),
(34, 44, 1, 26400, 4576, 'Poison', 'slower', 61, 305, 2, 11, 416, 0, 114, 105, NULL, NULL, NULL, '|1|1|0|0|0|', '|40|10|0|0|0|'),
(35, 44, 1, 28192, 4608, 'Poison', 'slower', 61, 305, 2, 11, 320, 0, 114, 105, NULL, NULL, NULL, '|1|1|0|0|0|', '|40|10|0|0|0|'),
(36, 44, 1, 28064, 5760, 'Poison', 'slower', 61, 305, 2, 11, 320, 0, 114, 105, NULL, NULL, NULL, '|1|1|0|0|0|', '|40|10|0|0|0|'),
(37, 44, 1, 26400, 5728, 'Poison', 'slower', 61, 305, 2, 11, 320, 0, 114, 105, NULL, NULL, NULL, '|1|1|0|0|0|', '|40|10|0|0|0|'),
(38, 44, 1, 25376, 6432, 'Poison', 'slower', 61, 305, 2, 11, 320, 0, 114, 105, NULL, NULL, NULL, '|1|1|0|0|0|', '|40|10|0|0|0|'),
(39, 67, 1, 29088, 3744, 'Slowdown', NULL, 31, 305, 3, 12, 544, 0, 113, 125, NULL, NULL, NULL, '|5|1|0|0|0|', '|40|40|0|0|0|'),
(40, 67, 1, 27136, 3808, 'Slowdown', NULL, 31, 305, 3, 12, 448, 0, 113, 125, NULL, NULL, NULL, '|5|1|0|0|0|', '|40|40|0|0|0|'),
(41, 68, 1, 30528, 5664, NULL, 'slower', 32, 310, 5, 13, 960, 0, 111, NULL, NULL, NULL, NULL, '|5|0|0|0|0|', '|35|0|0|0|0|'),
(42, 71, 1, 20544, 4992, 'Slowdown', 'neutral', 1, 5, 2, 3, 960, 0, NULL, 173, 35, 54, NULL, '|1|2|1|1|0|', '|0|50|25|5|0|'),
(43, 71, 1, 22816, 5792, 'Slowdown', 'neutral', 1, 5, 2, 3, 800, 0, NULL, 173, 35, 54, NULL, '|1|2|1|1|0|', '|0|50|25|5|0|'),
(44, 71, 1, 6688, 1120, 'Slowdown', 'neutral', 1, 5, 5, 4, 1184, 0, NULL, 173, 35, 54, NULL, '|0|2|1|1|0|', '|0|50|25|5|0|'),
(45, 70, 1, 6432, 192, 'Poison', 'slower,neutral', 1, 5, 2, 3, 544, 0, NULL, 223, 67, NULL, NULL, '|0|1|5|0|0|', '|0|10|25|0|0|'),
(46, 70, 1, 9280, 1536, 'Poison', 'slower,neutral', 1, 5, 2, 3, 320, 0, NULL, 223, 67, NULL, NULL, '|0|1|5|0|0|', '|0|10|25|0|0|'),
(47, 70, 1, 14784, 2048, 'Poison', 'slower,neutral', 1, 5, 3, 4, 960, 0, NULL, 223, 67, NULL, NULL, '|0|1|5|0|0|', '|0|10|25|0|0|'),
(48, 70, 1, 27872, 928, 'Poison', 'slower,neutral', 1, 5, 3, 4, 576, 0, NULL, 223, 67, NULL, NULL, '|0|1|5|0|0|', '|0|10|25|0|0|'),
(49, 66, 1, 3168, 1248, NULL, 'sleepy', 5, 25, 8, 13, 1280, 0, NULL, 1, 307, NULL, NULL, '|0|50|1|0|0|', '|0|10|10|0|0|'),
(50, 72, 1, 19808, 2688, NULL, 'neutral', 1, 5, 3, 4, 320, 0, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|'),
(51, 72, 1, 22944, 2560, NULL, 'neutral', 1, 5, 5, 4, 640, 0, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|'),
(52, 73, 1, 25088, 3328, 'Poison', 'neutral', 1, 5, 3, 4, 480, 0, NULL, 223, 121, 122, NULL, '|0|1|1|0|0|', '|0|3|3|0|0|'),
(53, 73, 1, 12640, 2496, 'Poison', 'neutral', 1, 5, 3, 4, 320, 0, NULL, 223, 121, 122, NULL, '|0|1|1|0|0|', '|0|3|3|0|0|'),
(54, 74, 1, 15392, 672, 'Slowdown,Poison', 'slower,neutral', 1, 5, 3, 4, 384, 0, NULL, 105, NULL, NULL, NULL, '|0|1|0|0|0|', '|0|10|0|0|0|'),
(55, 74, 1, 20832, 928, 'Slowdown,Poison', 'slower,neutral', 1, 5, 3, 4, 448, 0, NULL, 105, NULL, NULL, NULL, '|0|1|0|0|0|', '|0|10|0|0|0|'),
(56, 75, 1, 24448, 4224, 'Slowdown', 'neutral', 1, 5, 7, 5, 640, 0, 136, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|1|0|0|0|0|'),
(57, 76, 1, 29504, 1984, NULL, 'slower', 1, 5, 3, 4, 320, 0, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|'),
(61, 69, 1, 1, 1, NULL, 'sleepy,slower', 1, 5, 5, 1, 1, 0, 173, 186, 107, NULL, NULL, '|1|1|1|0|0|', '|20|15|10|0|0|'),
(100, 32, 6, 2560, 5952, NULL, 'slower', 2, 10, 4, 5, 800, 0, NULL, 107, NULL, NULL, NULL, '|0|1|0|0|0|', '|0|17|0|0|0|'),
(101, 1036, 6, 1600, 4384, 'Poison', NULL, 3, 15, 4, 8, 1120, 0, 106, 107, 369, 370, NULL, '|1|1|1|1|0|', '|30|17|5|0.2|0|'),
(102, 1037, 6, 3616, 3296, NULL, NULL, 4, 20, 4, 9, 672, 0, 371, 372, NULL, NULL, NULL, '|1|1|0|0|0|', '|15|7|0|0|0|'),
(103, 1038, 6, 1088, 3040, NULL, NULL, 5, 100, 1, 120, 640, 1, 374, 107, NULL, NULL, NULL, '|2|15|0|0|0|', '|50|50|0|0|0|'),
(104, 1039, 6, 4864, 4704, NULL, NULL, 5, 35, 4, 11, 640, 0, 376, 107, NULL, NULL, NULL, '|1|1|0|0|0|', '|10|17|0|0|0|'),
(105, 1040, 6, 6304, 3744, NULL, NULL, 6, 30, 3, 10, 480, 0, 379, 107, NULL, NULL, NULL, '|1|1|0|0|0|', '|10|17|0|0|0|'),
(106, 1041, 6, 7808, 4992, NULL, NULL, 7, 35, 6, 12, 800, 0, 381, 107, 105, NULL, NULL, '|1|1|1|0|0|', '|10|17|13|0|0|'),
(107, 1042, 6, 7584, 2784, 'Slowdown', NULL, 8, 40, 8, 15, 640, 0, 111, 107, 113, NULL, NULL, '|2|1|1|0|0|', '|15|17|10|0|0|'),
(108, 1043, 6, 10880, 5152, NULL, NULL, 8, 40, 3, 10, 576, 0, 124, 107, 125, NULL, NULL, '|20|1|1|0|0|', '|15|17|5|0|0|'),
(109, 1044, 6, 9984, 3744, NULL, NULL, 9, 45, 4, 11, 512, 0, 116, 110, NULL, NULL, NULL, '|1|1|0|0|0|', '|10|15|0|0|0|'),
(110, 1045, 6, 13792, 5696, NULL, NULL, 10, 50, 4, 11, 640, 0, 110, 107, NULL, NULL, NULL, '|2|1|0|0|0|', '|10|17|0|0|0|'),
(111, 1046, 6, 10272, 2432, NULL, NULL, 11, 55, 6, 13, 960, 0, 387, 107, NULL, NULL, NULL, '|1|1|0|0|0|', '|30|17|0|0|0|'),
(112, 1047, 6, 13984, 1984, NULL, NULL, 12, 60, 7, 14, 960, 0, 389, 107, NULL, NULL, NULL, '|1|1|0|0|0|', '|10|17|0|0|0|'),
(113, 1048, 6, 3744, 1664, NULL, NULL, 13, 65, 3, 11, 512, 0, 372, 107, NULL, NULL, NULL, '|1|1|0|0|0|', '|30|17|0|0|0|'),
(114, 1049, 6, 4864, 128, NULL, NULL, 14, 70, 5, 13, 736, 0, 92, 107, 391, NULL, NULL, '|1|1|1|0|0|', '|5|17|30|0|0|'),
(115, 1050, 6, 10368, 448, NULL, NULL, 15, 75, 7, 15, 1600, 0, 393, 107, NULL, NULL, NULL, '|1|1|0|0|0|', '|15|17|0|0|0|'),
(116, 1051, 6, 6688, 1824, NULL, NULL, 16, 80, 3, 11, 320, 0, 116, 107, NULL, NULL, NULL, '|1|1|0|0|0|', '|35|17|0|0|0|'),
(117, 1052, 6, 5216, 1216, NULL, NULL, 17, 340, 1, 120, 128, 1, 106, 107, 325, NULL, NULL, '|1|1|1|0|0|', '|25|17|5|0|0|'),
(118, 1053, 6, 8864, 1856, NULL, NULL, 18, 360, 1, 120, 128, 1, 111, 107, 317, NULL, NULL, '|10|1|1|0|0|', '|50|17|5|0|0|'),
(120, 1, 1, 6176, 7360, NULL, NULL, 43, 322, 5, 50, 640, 0, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|');

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
(1, 1, 19278, 3953, NULL, '[{\"text\":\"If you found me, you must have forgotten something. Okay, I\'ll say it again.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The voting menu has all the important settings for you. There you can change the class. Pump up your skills that you have. Manage your inventory, such as throwing away items and disassemble them into materials and also enchant. Manage guilds, create groups to play together, In the settings you can disable settings and change the language or anything else.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In the crafting area: Required items quantity. Craft button. Immediately I tell you that crafting can be paid. In the window where the reason you can select the number of items to craft instead of clicking constantly\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In farming and mining zones. you gather resources. Which you can also use in crafting. These resources provide a stable economy. You mine, you can buy things. You can buy things with products. You can also carry products and get gold for it.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"There are homes you can buy them from. All functions in the voting menu. These are your houses and you can use them as you like. Bring your friends or be alone there, for example.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If you forgot what how to use skils or where to buy. Buy them here next to need skills to raise or pump them. The usual command to use skills /use_skill number\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Pump yourself up first, then go into battle!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', -1, 1, 'Blink', 1),
(3, 16, 15153, 1393, NULL, '[{\"action\":true,\"text\":\"Hey, stranger. Have you come to sell your harvest?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', -1, 1, NULL, 1),
(4, 26, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hi. Got the rock again, huh? All right, spill it while I\'m being nice.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', -1, 1, 'Surprise', 1),
(5, 6, 22609, 2961, NULL, '[{\"action\":true,\"text\":\"Hey, stranger. Help yourself to any beverage you want. Even the illegal stuff. Heehee.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', -1, 1, 'Happy', 1),
(6, 4, 12510, 4273, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(7, 4, 27183, 3121, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(8, 4, 27548, 3121, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(9, 4, 27418, 3121, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(10, 54, 18953, 4401, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(11, 54, 15243, 4369, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(12, 54, 16128, 2289, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(13, 54, 21378, 1041, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(14, 54, 23062, 2577, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(15, 54, 19136, 2769, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(16, 54, 13357, 3505, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(17, 54, 14034, 4017, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(18, 57, 17550, 3665, 58, '[{\"action\":true,\"text\":\"If you\'re a good citizen, your stay here will be shortened.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', -1, 1, 'Blink', 1),
(19, 54, 12628, 2545, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(20, 54, 8522, 1041, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(21, 54, 5335, 2097, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(22, 54, 16619, 1073, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(23, 54, 25760, 1777, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(24, 54, 28116, 2161, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1),
(25, 54, 19786, 5329, NULL, '[{\"action\":true,\"text\":\"Look, don\'t distract me. I\'m keeping order here.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', 2, 0, 'Angry', 1);

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
  `AutoFinish` set('Partial') DEFAULT NULL,
  `DialogData` longtext DEFAULT NULL,
  `ScenarioData` longtext DEFAULT NULL,
  `TasksData` longtext DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_bots_quest`
--

INSERT INTO `tw_bots_quest` (`ID`, `BotID`, `QuestID`, `Step`, `WorldID`, `PosX`, `PosY`, `AutoFinish`, `DialogData`, `ScenarioData`, `TasksData`) VALUES
(1, 1, 1, 1, 0, 817, 2193, 'Partial', '[{\"text\":\"Oh, it\'s good to see you <player>!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I have a little favor to ask of you. Do you see this <item_31>?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It contains important supplies for <bot_2> that he needs urgently.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Can I ask you to deliver it?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_recieve_objectives\":{\"steps\":[{\"action\":\"message\",\"chat\":\"You can use special items(door\'s), by hammer hit!\"},{\"action\":\"fix_cam\",\"delay\":100,\"position\":{\"x\":1404,\"y\":2193}},{\"action\":\"fix_cam\",\"delay\":100,\"position\":{\"x\":2002,\"y\":2065}}]},\"on_end\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":100,\"position\":{\"x\":2949,\"y\":2225}}]}}', '{\n    \"move_to\": [\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Take the crate\",\n            \"completion_text\": \"You picked up the box\",\n            \"x\": 2006,\n            \"y\": 2065,\n            \"mode\": \"move_press\",\n            \"cooldown\": 3,\n            \"pick_up_item\": {\n                \"id\": 31,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(2, 2, 1, 2, 0, 2949, 2225, NULL, '[{\"text\":\"Oh, hey! You look like you brought something.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Is it from <bot_1>\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Thank you so much! It\'s exactly what I\'ve been waiting for. You\'ve helped me more than you can imagine.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 31,\n            \"value\": 1\n        }\n    ]\n}'),
(3, 3, 2, 1, 1, 19550, 4305, 'Partial', '[{\"text\":\"Another green adventurer, I presume?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I thought as much. We cannot allow strangers to wander Gridania unchecked and untested.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Before you rush off and begin pestering every second citizen for work, I suggest you make yourself known at the Carline Canopy. That\'s the headquarters of the local Adventurers\' Guild, in case you were wondering.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"The Carline Canopy is the building you see behind me. Speak to Mother Miounne within, and she will take you in hand.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]\r\n', NULL, '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Find Miounne\",\n            \"x\": 22192,\n            \"y\": 3313,\n            \"mode\": \"move\",\n            \"cooldown\": 3\n        }\n    ]\n}'),
(4, 6, 2, 2, 1, 22989, 3281, NULL, '[{\"text\":\"Well, well, what have we here?\",\"side\":\"default\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"text\":\"A wide-eyed and wondering young adventurer, come to put your name down at the guild, I assume?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Actually...haven\'t you registered with us already? There\'s something strikingly familiar about you, but I can\'t for the life of me remember when or where we might have met.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Oh, I probably just have you confused with another adventurer─dozens of you come through here every day, after all. Now, where was I? Ah, yes.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Welcome. Miounne\'s my name, or Mother Miounne as most call me, and the Carline Canopy is my place.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As the head of the Adventurers\' Guild in Gridania, I have the honor of providing guidance to the fledgling heroes who pass through our gates.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"No matter your ambitions, the guild is here to help you attain them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In return, we expect you to fulfill your duties as an adventurer by assisting the people of Gridania. A fine deal, wouldn\'t you agree?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"To an outsider\'s eyes, all may seem well with our nation, but naught could be further from the truth. The people live in a state of constant apprehension.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Ixal and various gangs of common bandits provide an unending supply of trouble─trouble compounded by the ever-present threat of the Garlean Empire to the north. And that is to say nothing of the Calamity...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Even now, the wounds have barely begun to heal. Ah, but I speak of it as if you were there. Forgive me. Five years past, Eorzea was well-nigh laid to waste when a dread wyrm emerged from within the lesser moon, Dalamud, and rained fire upon the realm. It is this which people call “the Calamity.”\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Scarcely a square malm of the Twelveswood was spared the devastation. Yet despite the forest\'s extensive wounds, not a soul among us can recall precisely how it all happened.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I am well aware of how improbable that must sound to an outsider... It is improbable. But it\'s also true. For reasons we can ill explain, the facts surrounding the Calamity are shrouded in mystery. There are as many versions of events as there are people willing to recount them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Yet amidst the hazy recollections and conflicting accounts, all agree on one thing: that Eorzea was saved from certain doom by a band of valiant adventurers.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Whatever else we\'ve misremembered, none of us have forgotten the heroes who risked life and limb for the sake of the realm. And yet...whenever we try to say their names, the words die upon our lips.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And whenever we try to call their faces to mind, we see naught but silhouettes amidst a blinding glare.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Thus have these adventurers come to be known as “the Warriors of Light.”\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"...Ahem. Pray do not feel daunted by the deeds of legends. We do not ask that you become another Warrior of Light, only that you do what you can to assist the people of Gridania.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Great or small, every contribution counts. I trust you will play your part.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You have my gratitude!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"All that\'s left, then, is to conclude the business of registration. Here\'s a quill. Scrawl your name right there.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Oh, and I would appreciate it if you used your real name─there is a special place in the seventh hell for those who use “amusing” aliases.\",\"side\":\"default\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"text\":\"...Forename Surname, hm? And you\'re quite sure that isn\'t an amusing alias?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Very well. From this moment forward, you are a registered adventurer of Gridania, nation blessed of the elementals and the bounty of the Twelveswood. The guild expects great things from you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]\r\n', NULL, NULL),
(5, 5, 2, 3, 1, 22865, 3249, NULL, '[{\"action\":true,\"text\":\"Look at what just arrived─another godsdamned adventurer...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]\n', NULL, NULL),
(6, 6, 2, 3, 1, 22989, 3281, NULL, '[{\"text\":\"Don\'t you start with that. Adventurers are the very salve that Gridania needs.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"The Elder Seedseer herself bade us welcome them with open arms. Do you mean to disregard her will?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]\n', NULL, NULL),
(7, 5, 2, 4, 1, 22865, 3249, NULL, '[{\"text\":\"Ordinarily, the forest funguars that inhabit the Central Shroud are naught more than a nuisance.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"However, we have received reports that vast clouds of the creatures\' spores have rendered parts of the Twelveswood impassable, and ruined crops besides.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"We periodically cull the funguar population in order to prevent such occurrences, but the creatures have taken to spawning out of season, making it ever more difficult to keep their numbers in check.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Though this is indeed a troubling development, the Gods\' Quiver has more pressing concerns and can ill afford to waste time fighting funguars.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If you wish to prove yourself, go to the Central Shroud and exterminate six of the pests. Use caution and approach them one at a time, lest your adventuring career be cut disappointingly short.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Don\'t you start with that. Adventurers are the very salve that Gridania needs.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Elder Seedseer herself bade us welcome them with open arms. Do you mean to disregard her will?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Don\'t you start with that. Adventurers are the very salve that Gridania needs.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Elder Seedseer herself bade us welcome them with open arms. Do you mean to disregard her will?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Of course not! Lest you forget, it is my sworn duty to uphold the peace! Am I to blame if outsiders bring mistrust upon themselves?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You─adventurer! Mind that you do not cause any trouble here, or I shall personally cast you out of this realm and into the seventh hell.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Of course not! Lest you forget, it is my sworn duty to uphold the peace! Am I to blame if outsiders bring mistrust upon themselves?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"You─adventurer! Mind that you do not cause any trouble here, or I shall personally cast you out of this realm and into the seventh hell.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(8, 6, 2, 5, 1, 22989, 3281, NULL, '[{\"text\":\"Ahem. Pay that outburst no mind. He meant only to...counsel you. Suspicious characters have been prowling the Twelveswood of late, you see, and the Wood Wailers feel they cannot afford to take any chances.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As is often the way with folk who live in isolation, Gridanians are wont to mistrust things they do not well know, your good self included. Fear not, however─given a catalog of exemplary deeds, and no more than a handful of years, the locals will surely warm to you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"On behalf of my fellow citizens, I welcome you to Gridania. May you come to consider our nation as your own in time.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Now then, you may depend on old Mother Miounne to teach you a few things that every adventurer should know.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]\n', NULL, NULL),
(9, 6, 3, 1, 1, 22989, 3281, 'Partial', '[{\"text\":\"Let us begin at the beginning, shall we? Now that you are a formal member of the Adventurers\' Guild, we must be sure you have a firm grasp of the fundamentals of adventuring. To that end, I have three tasks I wish you to perform.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Your first task is to visit the aetheryte. This massive crystal stands in the middle of the aetheryte plaza, not far from the Carline Canopy.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As a device that enables instantaneous transportation, the aetheryte plays a key role in the life of the ever-wandering adventurer.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Once you have located the crystal, all you need do is touch its surface. A member of the Wood Wailers will be present to offer further instruction.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"false\",\n            \"name\": \"Visit the aetheryte\",\n            \"completion_text\": \"You have visited aetheryte\",\n            \"x\": 19551,\n            \"y\": 4694,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        }\n    ]\n}'),
(10, 6, 3, 2, 1, 22989, 3281, NULL, '[{\"text\":\"For your second task, you are to visit the Conjurers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"There is no better place to learn the arts of conjury. Speak with Madelle, and she will explain the benefits of joining the guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_end\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":100,\"position\":{\"x\":19475,\"y\":3217}}]}}', NULL),
(14, 7, 3, 3, 1, 19475, 3217, NULL, '[{\"text\":\"You seek the secrets of conjury, adventurer? Then search no longer, for you have found your way to the Conjurers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It is at Miounne\'s request that you have come? Then allow me to provide you with an overview of what it is to be a conjurer.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Conjury is the art of healing and purification. Its practitioners harness the power of nature, that they might bring about change in the form of spells.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Primitive magic such as that once wrought by individuals known as mages─meaning those with the ability to manipulate aether─has existed since the dawn of time.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It was not until some five centuries ago that conjury emerged from this shapeless agglomeration of spells and charms─an event which led to the founding of Gridania.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In those dark days, the elementals would not suffer man\'s presence in the Twelveswood, forcing our forebears to make their homes beneath the earth, in the great subterranean city of Gelmorra.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But their desire to settle in the Twelveswood continued to burn fiercely; time and again they sought to curry the elementals\' favor.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Unlike men and other creatures bound in temples of flesh, the elementals are beings of pure aether. Recognizing this, the mages of eld reasoned that their talent for aetheric manipulation might allow them to commune with these theretofore enigmatic entities.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It took five long decades, but our forebears finally succeeded. Their reward: the elementals\' permission to dwell in the Twelveswood. So it was that the nation of Gridania was born.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Since that time, the elementals have taught us to live as one with nature, speaking to all Gridanians through the Hearers─those mages who are able to commune with them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And for their intimacy with the elementals, the Hearers would go on to attain greater mastery over the forces of nature. Thus did they conceive the art of conjury.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I hope this has helped you gain a greater understanding of the Conjurers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Should you wish to delve further into the mysteries of conjury, then I urge you to consider joining our ranks.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I can begin your initiation whenever you desire. Call upon me when you are ready to take the first step.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(15, 6, 3, 4, 1, 22989, 3281, NULL, '[{\"text\":\"For your second task, you are to visit the Archers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"There is no better place to learn the arts of the bow. Speak with Athelyna, and she will explain the benefits of joining the guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(16, 8, 3, 5, 1, 19396, 3409, NULL, '[{\"text\":\"Greetings, friend. You have found your way to the Archers\' Guild. Do you seek to uncover the secrets of our art?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Ah, now that I think on it, you have the look of one who has received Mother Miounne\'s “gentle” instruction. Very well, I shall give you a brief introduction to archery and the Archers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The skills practiced by our archers allow them to gauge an enemy\'s weaknesses from afar, and turn the tide of a battle with a single, well-placed arrow. Should you join us, you will be taught to do the same.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Archery as practiced in Gridania was born of two distinct styles of bowmanship.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The first was devised by the longbow sentries of the Elezen who once ruled the lowlands, while the second belonged to the shortbow hunters of the formerly nomadic Miqo\'te. As you will doubtless be aware, both races ultimately came to call the Twelveswood home.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Though the two peoples began as rivals, they gradually learned to live together in harmony. During this time, they learned from one another, their two schools of archery intermingling to give birth to the art as it is known today.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"For a time, the bow was used primarily for hunting. But as the hunters vied with one another to prove who was the better shot, there emerged a group of archers whose ultimate goal lay not in the practical pursuit of prey, but in perfection.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Thus was the Archers\' Guild born from the ranks of the Trappers\' League.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It is the way of the guild to promote greatness in archery through friendly competition. And the results of our methods can be seen in the vaunted archers of the Gods\' Quiver, many of whom spent their formative years loosing arrows at the guild\'s practice butts.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I hope this gives you a better idea of who we are, and what we do here. Oho, did I see the spark of ambition flare within your eyes?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If you wish to draw a string with the finest archers in Eorzea, look no further than the Archers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Before you can enlist, however, you must gain the approval of the guildmaster. Once you are ready to proceed, speak with me again and we can begin seeing about your enrollment.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(17, 6, 3, 6, 1, 22989, 3281, NULL, '[{\"text\":\"For your second task, you are to visit the Lancers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"There is no better place to learn the arts of the polearm. Speak with Jillian, and she will explain the benefits of joining the guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(18, 9, 3, 7, 1, 19862, 3473, NULL, '[{\"text\":\"Welcome to the Lancers\' Guild. I see you brought your own spear.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If you seek to refine your skills with the polearm, then you have come to the right place.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Here at the Lancers\' Guild, spear wielders gather to train with one another, and further hone their abilities under the tutelage of our fine instructors.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"More than just an instrument of war, the spear is also a tool for hunting, and with game ever plentiful in the Twelveswood, the weapon has been the mainstay of the locals here since before the founding of Gridania.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"With the passing of time, our nation became a gathering place for spearmasters from across the realm─many eager to test their mettle against the famed might of our Wood Wailers.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And it was here in Gridania that their myriad fighting styles came into contact, eventually giving rise to the art taught here today.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"That spear technique could be formalized at all owed much to the founding of the Lancers\' Guild by Wood Wailer captain Mistalle nigh on a century past.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The tradition of accepting students from without as well as within Gridania\'s borders persists to this day, ensuring that the art of the polearm may not only survive, but also continue to evolve.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Well, there you have it. I hope this brief history of the guild has helped settle any doubts in your mind.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Lancers\' Guild is always eager to welcome new initiates.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"If you\'ve a will to enlist, speak with me again and we can begin the enrollment procedures.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(19, 6, 3, 8, 1, 22989, 3281, NULL, '[{\"text\":\"For your third and final task, I would have you visit the markets at the heart of Old Gridania\'s commercial district. There you shall find weapons and armor, and all the various items that an adventurer might need on his/her travels.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"There is, however, more to the markets than buying and selling goods. Speak with Parsemontret, and listen well to his counsel.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The master merchant can be...uncooperative at times, so be sure to offer him one of my famous eel pies. Like so many men, he is much more charitable when his stomach is full. Here, I made a batch not long ago.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You have your tasks. May Nophica guide your path.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Ah, and one more thing: should you happen to come across any citizens in need, don\'t be afraid to proffer a helping hand. I am certain they will be pleased to meet an adventurer in whom they can confide their woes.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Granted, the work they offer is unlikely to be of realm-shattering importance─but prove your worth and build a reputation, and in time folk will be more inclined to entrust you with matters of moment.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I also suggest that you lend an ear to the Smith here in the Canopy. The Smiths are trusted representatives of the Adventurers\' Guild, and are an invaluable source of advice for neophyte heroes seeking to attain greatness.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(20, 10, 3, 9, 1, 23127, 3281, NULL, '[{\"text\":\"Greetings, adventurer. I see you are faithfully following Mother Miounne\'s instructions.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Allow me to offer you a bit of instruction myself. I am Nicia of the Wood Wailers, and I know a thing or two about the aetheryte─yes, that big crystal right there.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Aetherytes are devices that tap into aetherial energies, and are primarily used as a means to travel swiftly from one place to another.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Return and Teleport─the most common transportation spells─make direct use of the aetherytes and their connection to the flow of aether.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And as these devices are found in almost every corner of Eorzea, any adventurer worthy of the name will wish to seek out and attune himself/herself to each one.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Truly, few things in this world are so useful to an intrepid explorer.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But you need not locate them all at once. Before rushing out into the wilds, I suggest you start with the aetherytes found here in Gridania.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Should you wish to learn more about the aetheryte or transportation magic, I am here to answer your questions.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(21, 6, 3, 10, 1, 22989, 3281, NULL, '[{\"text\":\"The conquering hero returns. You have completed my little tasks, I trust?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The three locations you visited will feature prominently in your life as an adventurer─it is best you grow familiar with them as soon as possible.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And you took the time to listen to the woes of the citizenry? I cannot emphasize enough how important it is to lend your talents to one and all, no matter how trivial the matter may seem.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I am thankful that you are an obliging sort,. It is adventurers like you who will win the hearts of the locals and pave the way for those who follow. I pray Gridania can rely on your aid in its struggles to come.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(22, 6, 4, 1, 1, 22989, 3281, 'Partial', '[{\"text\":\"<player>, have you visited the Bannock on your wanderings?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It is a training ground found just outside the city where the soldiers of the Order of the Twin Adder are drilled in swordplay and other martial matters.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I mention this because an acquaintance of mine -- a gentleman by the name of Galfrid -- is an instructor there, and I think you may be of use to him. Go and introduce yourself, and find out if there is anything you can do to help.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Mind you do not stray far from the path -- the Twelveswood is no place for merry strolls through the underbrush.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_recieve_objectives\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":100,\"position\":{\"x\":13660,\"y\":4369}}]}}', '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"The instructor at the Bannock is in need of a help\",\n            \"completion_text\": \"Talk to the instructor.\",\n            \"x\": 13660,\n            \"y\": 4369,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 3\n        }\n    ]\n}'),
(23, 11, 4, 2, 1, 13542, 4369, NULL, '[{\"text\":\"Greetings, <player>. Miounne sent word to expect you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"My name is Galfrid, and I am responsible for training our Twin Adder recruits.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I thank you for volunteering your assistance. The Twelveswood is much changed since the calamitous arrival of the Seventh Umbral Era five years ago.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The power of the elementals wanes, and the harmony of the forest gives way to chaos. A great abundance of life has been lost as the strong run rampant, stifling the weak and new-sprung.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Though it may not appear so to the eyes of an outsider, the Twelveswood is ailing -- its once rich variety a fading memory.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"For the citizens of Gridania, the restoration of the forest is a sacred duty. And it is my hope that adventurers such as you will offer to aid them in their struggle.\",\"side\":\"default\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"action\":true,\"text\":\"Listen to their requests, and do all that you can. May the elementals bless your endeavors, <player>.\",\"side\":\"default\",\"left_speaker_id\":-1,\"right_speaker_id\":0}]', NULL, NULL),
(24, 11, 5, 1, 1, 13542, 4369, NULL, '[{\"text\":\"I see you are eager to lend a hand, <player>. That is well. But I cannot in good conscience send you into the forest until I have established that your equipment is equal to the task.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It bears repeating that, in the five years since the dawn of the Seventh Umbral Era, many of the Twelveswood\'s creatures have transformed into vicious, bloodthirsty monsters. Venturing into the forest without the proper gear is tantamount to suicide.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I suggest you take some time to evaluate your equipment. Once you deem your armor to be of sufficient quality, present yourself to me for inspection.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"System: Equip your head, body, hands, legs, and feet with gear of item level 5 or above before returning to speak with Galfrid.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_recieve_objectives\":{\"steps\":[{\"action\":\"message\",\"chat\":\"Something you can buy at the store!\"},{\"action\":\"fix_cam\",\"delay\":200,\"position\":{\"x\":21063,\"y\":3867}},{\"action\":\"message\",\"chat\":\"And something you can create in the \'Craft guild\'.\"},{\"action\":\"fix_cam\",\"delay\":80,\"position\":{\"x\":21459,\"y\":4341}}]}}', '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 3,\r\n            \"value\": 1,\r\n            \"type\": \"show\"\r\n        },\r\n        {\r\n            \"id\": 4,\r\n            \"value\": 1,\r\n            \"type\": \"show\"\r\n        }\r\n    ]\r\n}'),
(25, 11, 5, 2, 1, 13542, 4369, NULL, '[{\"text\":\"Ready for inspection are we? Right, then! Eyes forward! Back straight!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Hmmm... Yes, I think you pass muster.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You would be surprised at how many young, promising soldiers get themselves killed by rushing off into the woods without first donning a decent set of armor.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Your equipment, however, should provide the required degree of protection. Consider yourself ready for duty, <player>.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(26, 11, 6, 1, 1, 13542, 4369, 'Partial', '[{\"text\":\"Ah, <player>. By your tireless efforts, you have proven yourself a friend to Gridania. I believe you can be trusted with sensitive intelligence.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I would assign you a mission of considerable import, yet the need for secrecy prevents me from disclosing its details until you have pledged your participation. I am authorized to tell you only that it concerns suspicious activity in the Twelveswood. Say that you will lend us your aid, and I shall proceed with the briefing.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Good. Time is of the essence, so listen well.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You will by now have heard that a suspicious individual has been seen prowling the Twelveswood.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And you may also be aware that Ixali activity has risen sharply in the region of late. What you may not know is that this increase coincided almost exactly with the first recorded sighting of the aforementioned individual.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Suspecting a connection, we tightened our surveillance in hopes of tracking down our unknown visitor. Alas, our quarry is proving to be exceedingly elusive -- almost as if he knows our movements ahead of time.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But where whole units have failed, a lone adventurer may yet succeed. Acting independently and covertly, you may be able to close in on our quarry unnoticed.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Fear and anxiety are beginning to take their toll upon the citizenry, <player>. For their sake, I ask that you aid us in this investigation.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You have my gratitude. With your help, I am hopeful we will shed light upon this mystery.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Begin your search at Lifemend Stump. It is there that the majority of the sightings took place.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Be forewarned: my people cannot offer you support, lest our quarry catch scent of our presence and evade us yet again. Proceed with caution.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"\\\"See what\'s dangerous in the forest\",\n            \"completion_text\": \"See what\'s dangerous in the forest\",\n            \"x\": 12278,\n            \"y\": 4273,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Galfrid asks you to investigate the Stump of Life.\",\n            \"completion_text\": \"You have removed the sword from Lifemend Stump. Take it back to the Bannock and show it to Galfrid.\",\n            \"x\": 9746,\n            \"y\": 4543,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"pick_up_item\": {\n                \"id\": 21,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(27, 11, 6, 2, 1, 13542, 4369, NULL, '[{\"text\":\"<player>! It is good to see you back!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"One of our patrols sent word that you had been spotted doing battle with enraged treants. I am relieved to find you none the worse for the experience.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But tell me, what were you able to discover at Lifemend Stump?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"A sword in the stump, and a dead Ixal? Hmmm...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I can say with absolute certainty that this blade is of Ixali origin. It is of a kind used exclusively in the beastmen\'s rituals.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Ixal rarely set foot in the Central Shroud, so tight is our guard over the area. What purpose could have driven them to take such a risk? I fear something is afoot...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"What\'s that? A dubious couple sporting peculiar spectacles? Hah hah hah!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"So you have finally been acquainted with Yda and Papalymo! Lay your suspicions to rest -- Gridania counts them among her staunchest allies. Both are scholars hailing from a distant land, and have been with us since before the Calamity.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Their garb may appear outlandish, and their exchanges baffling, but never once have they given us cause to doubt them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Indeed, they often assist the Gods\' Quiver and the Wood Wailers in their work -- much as I hope you will in the days to come, <player>.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Although our unknown visitor eludes us still, owing to your efforts, we have acquired important intelligence on the Ixali threat. You have my gratitude.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"We are fortunate indeed to have a capable adventurer such as you aiding us. I pray you will continue to serve the people of Gridania in whatever capacity you are able.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 21,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(28, 11, 7, 1, 1, 13542, 4369, NULL, '[{\"text\":\"<player>, injuries to several of my men have left me shorthanded, and I require a capable sort to complete their unfinished duty.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The task is simple: put down as many of the local chigoe population as necessary to acquire eight of their egg sacs.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Having done so, you are to deliver them to Monranguin at Gilbert\'s Spire. He will answer any queries you might have. Now, I have other business to attend to.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"The Order of the Twin Adder makes no distinction between newcome adventurers and forestborn Gridanians. Your worth as a soldier is measured by your dedication to the cause.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 22,\n            \"value\": 3,\n            \"type\": \"default\"\n        }\n    ],\n    \"defeat_bots\": [\n        {\n            \"id\": 31,\n            \"value\": 10\n        }\n    ]\n}'),
(29, 12, 7, 2, 1, 11872, 4017, NULL, '[{\"text\":\"Ah, you must be the adventurer standing in for our injured companions. Terribly unfortunate business, that.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It seems, however, that you had little trouble gathering the egg sacs in their stead. Excellent work. I shall have them sent over to the Trappers\' League immediately.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Lest you wonder, these egg sacs are not destined for the dinner table! Members of the Twin Adder and the Wood Wailers are assisting the league by collecting the samples they need to check for signs of sickness.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The chigoe, you see, is one of the few creatures capable of transmitting the disease known as the Creeping Death. Until relatively recently, any Hyur who contracted this ghastly illness would almost invariably perish.\",\"side\":\"author\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"text\":\"Indeed, a single outbreak once claimed the lives of a third of the Hyuran population here in Gridania. That was a long time ago, of course. With the medicines available to us now, the Creeping Death is not the killer it once was.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Even so, it is best to halt any new outbreaks before they occur. Thus we gather chigoe eggs on a regular basis in order to assist the Trappers\' League with their ongoing research. Your timely assistance is most appreciated.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(30, 12, 8, 1, 1, 11872, 4017, 'Partial', '[{\"text\":\"Such an embarrassing turn of events... I sent a recruit from the Bannock on a surveying expedition only for the craven to turn tail and flee at the first sign of trouble.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"This is not how we treat requests from the conjurers! And as if such a poor showing weren\'t bad enough, the lily-livered half-wit left behind the surveying equipment provided by Hearer Pauline herself!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"While I attempt to instill some backbone into this so-called “soldier,” would you mind recovering the survey gear and returning it to Hearer Pauline at Gabineaux\'s Bower?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"According to my recruit\'s tale of woe, there should be a set of survey records, a surveyor\'s rope, and two boxes of surveyor\'s instruments strewn about the interior of a cave to the south of here. <sigh> It\'s a wonder the damn fool didn\'t lose his boots... Ahem. Matron watch over you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Get to the designated spot\",\n            \"completion_text\": \"Find the lost items\",\n            \"x\": 9852,\n            \"y\": 5297,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Recover the survey records\",\n            \"completion_text\": \"You have received the survey notes\",\n            \"x\": 9883,\n            \"y\": 5745,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 23,\n                \"value\": 1\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Recover the boxes of instruments\",\n            \"completion_text\": \"You have received recover the boxes of instruments\",\n            \"x\": 9006,\n            \"y\": 5745,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 24,\n                \"value\": 1\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Recover the boxes of instruments\",\n            \"completion_text\": \"You have received recover the boxes of instruments\",\n            \"x\": 10830,\n            \"y\": 5329,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 24,\n                \"value\": 1\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Recover the surveyor\'s rope\",\n            \"completion_text\": \"You have received recover the surveyor\'s rope\",\n            \"x\": 10129,\n            \"y\": 6033,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 32,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(31, 13, 8, 2, 1, 14154, 3153, NULL, '[{\"text\":\"Yes, may I assist you with some matter?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Oh? But this is the equipment I left with the soldiers of the Bannock...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Fled at the first sign of danger, you say? I see... Well, all is not lost: it appears the recruit managed to complete the surveying assignment. The records are actually quite detailed.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"With the changes wrought by the Calamity, I thought it wise to send the Order of the Twin Adder on a number of expeditions to map the region\'s topography. As fortune would have it, the officers saw these tasks as an excellent opportunity to train inexperienced soldiers.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"We can no longer rely on our past knowledge of the Twelveswood. If we are to survive these troubled times, we must reacquaint ourselves with our surroundings, that we may better discern the threats we face. Stay vigilant, adventurer\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 23,\n            \"value\": 1,\n            \"type\": \"default\"\n        },\n        {\n            \"id\": 24,\n            \"value\": 2,\n            \"type\": \"default\"\n        },\n        {\n            \"id\": 32,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(32, 13, 9, 1, 1, 14154, 3153, 'Partial', '[{\"text\":\"I hesitate to make such a dangerous, request, but might you assist us in thinning the number of anoles on Naked Rock?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In our efforts to commune with the elementals, we conjurers often find ourselves in the area. Of late, however, our meditations have all too frequently been interrupted by unprovoked anole attacks. Truly, the beasts grow more aggressive by the day.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Their numbers have continued to grow since the advent of the Seventh Umbral Era, you see, forcing packs of the scalekin to come down from the mountains in search of food. If you could slay a handful of the beasts, that should lessen their need to hunt and also serve as a warning to the anoles to remain within their territory.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"But I am afraid you must do more than thin the existing population. If we are to truly break this spiraling growth, then we must also target their future offspring. Bring me one of their eggs, and you will have played your part in returning balance to this area of the forest.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 33,\n            \"value\": 10,\n            \"type\": \"default\"\n        }\n    ],\n    \"defeat_bots\": [\n        {\n            \"id\": 32,\n            \"value\": 40\n        }\n    ]\n}'),
(33, 13, 9, 2, 1, 14154, 3153, NULL, '[{\"text\":\"Ah, you have returned. Now might my brothers and sisters continue their meditations undisturbed. You have my thanks.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As for the egg, may I ask you to deliver it to Tsubh Khamazom at the Bannock?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Anole eggs are both large and filled with nutrients - the perfect meal for a soldier. She will be more than a little pleased to see you, I should imagine.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(34, 14, 9, 3, 1, 13754, 4369, NULL, '[{\"text\":\"Who goes there! Oh, Adventurer, it\'s you. Hm? Another delivery?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I\'m not certain I should be the one to - By Nophica, that\'s an anole egg! The troops will be glad indeed to see one of these at table! And you say Hearer Pauline sent you on this errand?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I hear the anoles are more numerous than ever, yet you appear to have managed the task with your skin intact. Your skill and bravery continue to amaze me, Adventurer,\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(35, 11, 10, 1, 1, 13542, 4369, NULL, '[{\"text\":\"Adventurer! Thank the gods you\'ve come!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"We have a potential crisis on our hands, and I would appreciate your assistance. Will you hear me out?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Listen well, for we haven\'t much time.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"To the southeast of here lies a dungeon known as Spirithold. It was all but destroyed during the Calamity.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Desiring to offer the ruins back to the forest, a Hearer ventured inside to carry out the Rite of Returning.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Alas, it would seem something has gone awry. Word arrived just moments ago that the Hearer and his guards have been attacked by a towering shadow. Aye, you heard me true - a shadow.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"On any other day, I would dispatch my best Quivermen to provide support, but I sent them to repel an Ixali incursion in the West Shroud nary a bell ago.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The timing of these events cannot be mere coincidence. I fear the Ixal somehow caught wind of our plans, and are attempting to disrupt the rite in an effort to weaken the bond between man and elemental.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"They must not be allowed to succeed. And so I bid you go to Spirithold and do whatever is necessary to resolve the situation. Please, say you will help us.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I knew you would not let me down. You will have all the support I can muster.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Now, we are racing against time, so you had best make haste.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(36, 15, 10, 2, 1, 12668, 4305, 'Partial', '[{\"text\":\"Who goes there? An adventurer, is it?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Here at Instructor Galfrid\'s request, you say? Thank the Matron!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Doubtless you already know this, but a towering shadow manifested without warning and attacked the Hearer in the midst of the rite.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Most of his party has been successfully evacuated, but five remain unaccounted for.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Please find them, and see them out of harm\'s way.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Get to the battle site\",\n            \"x\": 12337,\n            \"y\": 4273,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing Hearer\",\n            \"completion_text\": \"You saved the Hearer.\",\n            \"x\": 11806,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 11388,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 10925,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 10592,\n            \"y\": 4530,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 10301,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 9822,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 9537,\n            \"y\": 4433,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        }\n    ]\n}'),
(37, 15, 10, 3, 1, 12668, 4305, NULL, '[{\"text\":\"All this happened inside Spirithold? Twelve preserve us...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Who was that masked mage, and by what dark ambition is he driven? SO many things shrouded in mystery...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Well, I shan\'t find any answers on my own. I must needs discuss this with Galfrid. The matter warrants a full investigation, if I am any judge, and that shall certainly be my recommendation.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Your courage has saved many lives this day, adventurer. For this you have my deepest gratitude.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Pray return to Gridania and seek out Miounne.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I understand she wishes to thank you for your efforts on our behalf.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(38, 6, 11, 1, 1, 22989, 3281, NULL, '[{\"text\":\"How is my favorite fresh-faced adventurer? Oh, do not scowl so—I speak out of habit. You\'ve come a long way since you first walked through my door, and I\'ll not deny it.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As a matter of fact, I think it\'s about time you made yourself useful at Bentbranch Meadows in the Central Shroud.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Unlike the recruits you see at the Bannock, the men and women out at Bentbranch are fully occupied with their own work. As such, I imagine there are more than a few who would welcome the assistance of a rapidly maturing adventurer like yourself.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Bentbranch is also home to a very usefully situated aetheryte. It is, in other words, the perfect place for you to begin the next stage of your journey as an adventurer.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And what better time than the present!? Leave the city via the Blue Badger Gate, and continue to the southwest until you come to a bridge.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Cross it, and when you spy an aetheryte in the distance, you may congratulate yourself on having successfully found Bentbranch Meadows.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If you so fancy, you may also make use of the chocobo porter service, doubtless the safest way to get to your destination. Chocobokeep Cingur should not hesitate to lend his birds to a capable adventurer like you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Oh, and be sure to introduce yourself to Keitha, the head chocobo wrangler, when you arrive.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(39, 16, 11, 2, 1, 15153, 1393, NULL, '[{\"text\":\"You must be the \'venturer Miounne sent word about. I\'m Keitha, head wrangler \'round these parts.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I\'ve heard many and more things about you—good things, lest you worry.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"When the Elder Seedseer bade us welcome \'venturers, \'tis fair to say we had our doubts—till hardworkin\' folks like yourself set about provin\' us wrong, that is. Consider me a convert!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Anyroad, you\'ve a mind to help out at the ranch, have you? Good. We could always do with a hand or two to keep the place runnin\' smoothly.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As a matter of fact, I\'ve a task right here that wants doin\'.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Let me know when you\'re ready to get busy. Oh, and you come highly recommended, so don\'t go lettin\' no one down, eh?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(40, 16, 12, 1, 1, 15153, 1393, NULL, '[{\"text\":\"Seven hells! Some bastard Qiqirn has gone and broken one of me chocobo eggs!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The filthy little thief was busy lootin\' the barn when a guard startled it, promptin\' the damn thing to drop the egg it was clutchin\' and run. Some of the lads gave chase, bless \'em, but when three more of the vermin appeared, me lot had no choice but to turn back.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Somethin\' has to be done about those Qiqirn...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Would you mind headin\' over to the Matron\'s Lethe and havin\' a word with a soldier named Roseline for me? The ratmen nest in her neck of the woods, see... She\'ll know what to do.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(41, 17, 12, 2, 1, 12180, 3185, 'Partial', '[{\"text\":\"You\'re here on Keitha\'s behalf? Hm? I see. Broke one of her eggs, you say? And there were four of the creatures?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Qiqirn are a nuisance at the best of times, but we must now add trespass and chocobo murder to their list of transgressions... They have forced our hand. Our retribution must needs be swift and decisive.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And who better than you to deliver it, adventurer? Find the lair to the west of here and make an example of exactly four Qiqirn scramblers. We can send no clearer message.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"defeat_bots\": [\n        {\n            \"id\": 33,\n            \"value\": 20\n        }\n    ]\n}'),
(42, 17, 12, 3, 1, 12180, 3185, NULL, '[{\"text\":\"The deed is done? Good. A grim task, but a necessary one.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"You have shown them the folly of inciting the wrath of those who consort with adventurers. Perhaps now the Qiqirn will think twice before giving in to their larcenous proclivities.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(43, 18, 13, 1, 1, 12692, 3121, 'Partial', '[{\"text\":\"Ho there, adventurer. You seem light on your feet. Fancy a quick skip along the root of the heavenspillar here? I need someone to pick off a blue trumpet or two.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I know what you\'re thinking: it\'s naught more than a mushroom, so why the commotion? I\'ll tell you why. You allow that fungal menace to multiply, and within a moon they\'ll be covering the whole damn root and rotting the wood clear through.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Just watch your step while you\'re up there, though—the diremites on the ground won\'t waste any time adding insult to falling injuries.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Once you\'re done, head up to the top end of the root and report to Theodore. He\'ll be glad to hear someone\'s taken care of one of his more dreaded chores.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"defeat_bots\": [\n        {\n            \"id\": 34,\n            \"value\": 25\n        }\n    ]\n}'),
(44, 19, 13, 2, 1, 12449, 3153, NULL, '[{\"text\":\"Oh, you\'ve cleared the root of blue trumpets? Wonderful! To be quite honest, I have this teeny-tiny problem with heights. <sigh> No, this is not my ideal posting, but we all do what we must.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Keeping the root passable is an important job, you see. It\'s one of the few ways folk can traverse the Central Shroud since the Calamity all but split the area in twain.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"It is not, perhaps, the easiest pathway to walk, but there are those who believe the will of the Matron Herself caused this tendril of a heavenspillar to remain thus suspended, that it might serve the forest\'s people. I\'m rather fond of the notion, myself.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(45, 19, 14, 1, 1, 12449, 3153, NULL, '[{\"text\":\"It is—regrettably—my duty to stand watch over the road from here to Bentbranch Meadows.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The ranch has become a veritable institution of Gridania, so any threats to its continued operation are taken quite seriously by the Wood Wailers.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Which reminds me—mayhap it was my imagination, but I believe I caught a glimpse of some shadowy fellow not too long ago. Would you mind passing word to Roseline down below? I would go myself but, well...it\'s hard enough marshaling the courage to walk the root for my shift...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(46, 17, 14, 2, 1, 12180, 3185, NULL, '[{\"text\":\"A shadowy fellow? Hmmm...now that you mention it, I may have seen something.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I dismissed it as a trick of the light before, but I thought I saw a shadow in the forest to the north. Still, I suspect it is nothing more than a Qiqirn thief on the run.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"If you must sate your curiosity, by all means investigate. Should you actually find something of note, I would like very much to see it.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 44,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ],\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Search for signs of the shadowy figure\",\n            \"completion_text\": \"A leather bag fell out of the shadows, take it to Roseline.\",\n            \"x\": 10297,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 35,\n                \"attribute_power\": 100\n            },\n            \"pick_up_item\": {\n                \"id\": 44,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(47, 17, 14, 3, 1, 12180, 3185, NULL, '[{\"text\":\"Hmmm? Have you found something?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Remnants of a campfire and a forgotten bag...this could belong to any adventurer or traveler. And inside we have...a chocobo grooming brush and roseling oil?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But wait—why would a traveler make camp here, when it would be far safer to beg the hospitality of Bentbranch Meadows?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Bugger me, I think this belongs to that stranger said to be meddling with the chocobos! Thank you, adventurer. We have been lax in our duties, but no longer—I swear we will find this shadow.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(48, 17, 15, 1, 1, 12180, 3185, NULL, '[{\"text\":\"We cannot allow other sentries to dismiss similar sightings—they must know what we have learned. To that end, I\'ve prepared this letter containing everything we know about our mysterious stranger.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I need you to show it to my comrades throughout the Shroud. Once each sentry has committed the details to memory, have them write their name at the bottom for confirmation.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Start with Elmar at the Bannock, then find Bernard at the eastern gates of Bentbranch Meadows. They ought to relay the information to the others.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Finally, make sure Eylgar sees the letter. He stands watch within the stables, so if this stranger\'s aim is to harm the chocobos, Eylgar may have to personally put an end to it.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(49, 20, 15, 2, 1, 13903, 4337, NULL, '[{\"text\":\"You don\'t look like you\'re here for training...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"A shadowy figure... Understood. I\'ll pass word to the recruits as well as the sentries.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Let me just make my mark...there, that should do.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"If you follow the road south, you\'ll find Bernard by the bridge to Bentbranch. Godsspeed, adventurer.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(50, 21, 15, 3, 1, 16370, 1617, NULL, '[{\"text\":\"You have business with me, adventurer?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Roseline is right to be cautious. For this stranger to venture so close, yet to go to such great lengths to remain undetected, is highly suspicious. They clearly have designs on Bentbranch.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"We appreciate the help. I should write my name here, yes?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And...here, take it. You\'ll find Eylgar in the stables, past the aetheryte.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(51, 22, 15, 4, 1, 16608, 1073, NULL, '[{\"text\":\"Mind the birds, adventurer. They get nervous around strangers.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"What\'s this? A shadowy stranger near the Matron\'s Lethe... You\'ve already shown this to Bernard and Elmar, I see. Good, good.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Then all that\'s left is to inform the staff here. Not everyone here is a Wood Wailer, true, but even our stableboys wouldn\'t hesitate to take up arms to defend these chocobos.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(52, 22, 16, 1, 1, 16629, 1073, 'Partial', '[{\"text\":\"You\'re quite the compassionate adventurer, by the sound of it. Well far be it from me to look a gift chocobo in the mouth─I have need of a capable [GENDER]/woman like yourself.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"We received a peddler at the gates the other day─an excitable Lalafell that was sweating and swearing that he had been attacked by large winged beasts. Yet other than hornets, I know of no flying creatures in this region.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If there\'s any truth to what he said, it might prove problematic for other travelers. Follow the road south and see if you can find any evidence to support his claim.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And, should you find something, kindly tell Lothaire to patrol his area sometime instead of just standing beneath the spire and staring at the godsdamned road. In those words.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Search for signs of the winged beasts 1/3\",\n            \"completion_text\": \"You searched the area and found feathers scattered on the ground.\",\n            \"x\": 16605,\n            \"y\": 1073,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 2\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Search for signs of the winged beasts 2/3\",\n            \"completion_text\": \"You noticed scratch marks on the trees. It appears the winged beasts have been here.\",\n            \"x\": 18361,\n            \"y\": 785,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 2\n        },\n        {\n            \"step\": 3,\n            \"navigator\": \"true\",\n            \"name\": \"Search for signs of the winged beasts 3/3\",\n            \"completion_text\": \"You found a nest.\",\n            \"x\": 15152,\n            \"y\": 721,\n            \"mode\": \"move_press\",\n            \"cooldown\": 2\n        },\n        {\n            \"step\": 4,\n            \"navigator\": \"true\",\n            \"name\": \"Protect yourself from the winged beast\",\n            \"x\": 15209,\n            \"y\": 421,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 36,\n                \"attribute_power\": 60\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Protect yourself from the winged beast\",\n            \"x\": 15584,\n            \"y\": 753,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 36,\n                \"attribute_power\": 60\n            }\n        }\n    ]\n}'),
(53, 23, 17, 1, 1, 18489, 785, NULL, '[{\"text\":\"I have another task for you, adventurer. I need you to head to the Hedgetree to the southwest of here and speak with Hearer Leonnie.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"After tending to the Hedgetree, the Hearer was scheduled to board a boat from the Mirror Planks... Well, the vessel\'s departure time has come and gone, but there is still no sign of her.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Knowing how absorbed the Hearer becomes in her work, I am not unduly concerned. Armelle, however, was responsible for organizing Leonnie\'s transportation and is likely wondering if her wayward passenger is ever going to arrive. Perhaps a gentle reminder is in order?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(54, 24, 17, 2, 1, 17456, 1617, NULL, '[{\"text\":\"Yes, what troubles you, my de─? Ah. Yes. The boat. I had quite forgotten.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Hm? Oh, my task with the Hedgetree is well and finished, but the elementals murmur of a malevolent presence in the vicinity of the Tam-Tara Deepcroft.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I must abide a while longer, that I might better divine the source of the elementals\' distress. Please inform Armelle that I shall be late in arriving.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(55, 25, 17, 3, 1, 20421, 1041, NULL, '[{\"text\":\"You bear a message from Hearer Leonnie? An evil presence in the Deepcroft? That does sound grave, indeed.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I shall organize a vessel for a later time, then. Perhaps a bell from now? Two? Better make it three, just to be safe. Thank you for your trouble.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(56, 25, 18, 1, 1, 20421, 1041, 'Partial', '[{\"text\":\"Might you assist me with another matter, <player>? A wagon that departed from Quarrymill was overturned on the road when some large, ill-tempered forest beast chose that moment to defend its territory.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"By Nophica\'s grace, the driver escaped without serious injury, but the wagon\'s cargo was not so fortunate. With none willing to risk another encounter with the creature, I can only assume the goods remain strewn across the ground where the incident occurred.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Simply follow the road here to the south and you should come across the wreckage. Salvage what cargo you can, and deliver it to Keitha at Bentbranch Meadows if you would be so kind.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Find a place\",\n            \"completion_text\": \"I think there\'s a lost cargo around here somewhere\",\n            \"x\": 10059,\n            \"y\": 4497,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Salvage cargo\",\n            \"completion_text\": \"You picked up cargo.\",\n            \"x\": 8810,\n            \"y\": 4529,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 45,\n                \"value\": 1\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Salvage cargo\",\n            \"completion_text\": \"You picked up cargo.\",\n            \"x\": 8049,\n            \"y\": 4689,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 45,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(57, 16, 18, 2, 1, 15153, 1393, NULL, '[{\"text\":\"You have a delivery for me?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Ah, this\'s the shipment I was waitin\' on from Quarrymill. I heard the wagon ran afoul o\' some great monstrosity just up the path from the Mirror Planks, but I see you\'ve managed to scrape together a few bits and pieces.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Seems I can\'t get through two bells these days without hearin\' some new tale o\' horrors in the Deepcroft or bandit cutthroats prowlin\' the woods hereabouts. Makes me wonder if me chocobos are safe at night, it does. If we\'re ever in need of a \'venturer\'s skills, I hope you\'ll be around to lend a hand.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 45,\n            \"value\": 2,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(58, 27, 19, 1, 1, 15681, 1489, 'Partial', '[{\"text\":\"No! Oh, please gods, no! Leia\'s egg!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"<player>, you must help me! I stepped out of the stables for but a moment, and when I returned it was gone!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I— What was gone? An egg! Sorry? You are sure the chocobos will lay another!? Gah! You do not understand—the egg is extremely valuable! I must find it!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"You will help me, won\'t you, <player>? Oh, thank you, thank you!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Someone must have made off with it. There can be no other explanation. I shall scour every ilm of the stable once more just to be sure. While I do so, I should be very grateful if you would ask the others if they noticed anything unusual.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Question the people of Bentbranch Meadows\",\n            \"x\": 16581,\n            \"y\": 1073,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 15\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Question the people of Bentbranch Meadows\",\n            \"x\": 15528,\n            \"y\": 1425,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 15\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Question the people of Bentbranch Meadows\",\n            \"completion_text\": \"You\'ve done a survey of the residents of Bentbranch Meadows.\",\n            \"x\": 17531,\n            \"y\": 1617,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 15\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Defeat Janremi Blackheart\",\n            \"completion_text\": \"You defeated Janremi Blackheart. Return Leia\'s egg to Luquelot.\",\n            \"x\": 21929,\n            \"y\": 3357,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 37,\n                \"attribute_power\": 80\n            },\n            \"pick_up_item\": {\n                \"id\": 46,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(59, 27, 19, 2, 1, 15453, 1425, NULL, '[{\"text\":\"Leia\'s egg is irreplaceable. I cannot well express to you the depth of my gratitude.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And yet, I find that I am still troubled... While this whole regrettable episode unfolded, I bore witness to a sight that greatly concerned me..\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 46,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(60, 27, 20, 1, 1, 15681, 1489, NULL, '[{\"text\":\"My thanks for your kindness earlier. I hate to impose again, but I have need of your assistance in another matter─one of grave import, I fear.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Of late, I have noticed that Ixali dirigibles have been appearing over the Twelveswood with increasing regularity. The frequency, however, does not bother me near so much as where they choose to fly─the patch of sky directly above the Guardian Tree.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The tree is a sacred site, or so I was given to believe when first I came to Gridania, and thus I naturally assumed that the elementals would not suffer the Ixal to profane it. Yet the birdmen have been coming and going as they please, with nary a sign of protest from the guardians of the Twelveswood.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And it was that which set me to thinking... Ever since the appearance of the much-talked-about “suspicious individual,” many and more strange things have been occurring in the forest. Could it be that he did something to the elementals?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In case it proved useful, I have committed the details of my sightings to parchment, and would ask that you deliver the document to Mother Miounne.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Please make all haste─I have an irrepressible feeling that something terrible is about to happen.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"reward_items\": [\n        {\n            \"id\": 47,\n            \"value\": 1\n        }\n    ]\n}'),
(61, 6, 20, 2, 1, 22989, 3281, NULL, '[{\"text\":\"I haven\'t the slightest inkling what this could be about...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Mother Miounne scans the letter, and her eyes widen.\",\"side\":\"author\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"text\":\"Gods be good...\",\"side\":\"author\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"text\":\"I have a mission for you. Suffice it to say, it is urgent.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I fear it may also prove dangerous, however, so you must be prepared.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Make what arrangements you can, and report back to me the moment you are ready.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 47,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(62, 6, 21, 1, 1, 22989, 3281, NULL, '[{\"text\":\"Time is of the essence, so I shall speak plain.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Based on Luquelot\'s observations, the Ixal have designs on the Guardian Tree, and they mean to act soon.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The tree is the oldest living thing in this ancient forest, and it is held sacred by every forestborn Gridanian.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Should it come to any harm, the elementals would fly into a rage beyond pacifying. I dread to think of the chaos that would ensue.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"There isn\'t much time. We must act quickly.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"<player>, please see this letter to the hands of Bowlord Lewin, at the Seat of the First Bow in Quiver\'s Hold. Should the need arise, pray put yourself wholly at the man\'s disposal. I strongly suspect he will need all the able-bodied souls he can muster.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"The fate of Gridania hangs in the balance. Go swiftly, <player>.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"reward_items\": [\n        {\n            \"id\": 47,\n            \"value\": 1\n        }\n    ]\n}'),
(63, 28, 21, 2, 1, 13738, 3217, NULL, '[{\"text\":\"So you are <player>, the adventurer of whom I have heard so much. I understand you wish words with me.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Miounne has information on the Ixal, you say? Speak freely─you have both my ears.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"...Matron forfend! They mean to defile the Guardian Tree?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Before Nophica, I swear those filthy birdmen will not touch it─nay, not so much as a single leaf!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Twelve help me! How can it be that neither the Wood Wailers nor the Gods\' Quiver caught wind of this?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I cannot help but think this plot bears the mark of the masked devil who has eluded us for so long. We must be wary─this incursion may be more than it seems.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Ah, would that the Warriors of Light were still with us...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"But this is no time for such idle thoughts. I thank you for delivering this message. You may assure Miounne that I will dispatch a unit of my best men to investi─\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 47,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(64, 28, 22, 1, 1, 13738, 3217, NULL, '[{\"text\":\"None in Gridania can doubt your worth, <player>...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But to receive such praise from the Elder Seedseer herself!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And nor is that the half of it! She chose you to play the role of Emissary, for gods\' sakes! You! An outsider! Do you have any idea what this means!?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"<sigh> But of course you don\'t. You are not forestborn...which is rather the point. Mistake me not, I think you worthy, but your selection is all but unprecedented. And I\'ll wager you have not the faintest inkling what is required of you...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"There are preparations that the Emissary must needs complete ahead of time. I suggest you consult Miounne regarding the matter—she is overseeing the arrangements for the event.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Now, you had best get going—the ceremony cannot commence without the Emissary.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Oh, and, <player>...don\'t make a hash of this!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(65, 6, 22, 2, 1, 22989, 3281, NULL, '[{\"text\":\"Well, well, if it isn\'t the Emissary himself/herself! Had I known you were coming, I would have baked a pie!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You truly are full of surprises, <player>. Next you\'ll be telling me you\'re one of the Warriors of Light, back from a half-decade long holiday!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But let\'s speak of preparations. As you doubtless already know, Greenbliss is an age-old ceremony for strengthening the bond between man and elemental. These days, though, the name also refers to the festival at large.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In the ceremony, the Emissary serves as a conduit—a bridge between the people and the guardians of the Twelveswood. Suffice it to say, it is no small responsibility—nor does the Elder Seedseer choose mankind\'s representative on a whim.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Seldom in history have non-forestborn been chosen for the role—which should give you an idea of the magnitude of the honor being accorded you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"For your part, you are required to wear a ceremonial artifact, which is presently in the keeping of Timbermaster Beatin.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Go to the Carpenters\' Guild and collect it from the man, then return to me for further instructions.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(66, 29, 22, 3, 1, 19278, 3409, NULL, '[{\"text\":\"So you are the Emissary-to-be. Miounne sent word that you would be coming to collect the ceremonial artifact.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The good news is that it\'s ready—painstakingly crafted by these very hands, and from the rarest of materials.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Take it, along with this warning: get so much as a scratch on the thing, and I shall make an unceremonial artifact out of you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"reward_items\": [\n        {\n            \"id\": 48,\n            \"value\": 1\n        }\n    ]\n}'),
(67, 6, 22, 4, 1, 22989, 3281, NULL, '[{\"text\":\"Back from your trip to the Carpenters\' Guild? Let\'s see what you have in that box.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Ahhh...this is by far the finest Monoa mask I have ever laid eyes upon. The timbermaster has truly outdone himself this time.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In case he didn\'t mention, the mask is crafted from consecrated lumber rendered up by the Guardian Tree, solely for use in the ceremony. In other words, it is priceless—Mother bids you to handle it with care.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And with that, your preparations are complete. The venue should just about be in order as well. If you have any questions, now\'s the time to ask them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"reward_items\": [\r\n        {\r\n            \"id\": 49,\r\n            \"value\": 1\r\n        }\r\n    ],\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 48,\r\n            \"value\": 1,\r\n            \"type\": \"show\"\r\n        }\r\n    ]\r\n}'),
(68, 6, 23, 1, 1, 22989, 3281, NULL, '[{\"text\":\"Now that you have the Monoa mask, all that\'s left is to participate in the ceremony.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Perchance you are feeling nervous, but never fear—despite all the pomp surrounding the role, there really is nothing to being Emissary.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"All you are required to do is wear the mask, stand up straight, and look dignified. The more involved aspects of the proceedings will be handled by others. Simple, no?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The ceremony will be held at Mih Khetto\'s Amphitheatre. When you are ready, make yourself known to the caretaker there—a woman named Estaine.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Oh, and be sure to wear your mask or she may not recognize you. Now, off you go, <player>, and good luck!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 49,\r\n            \"value\": 1,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(69, 30, 23, 2, 1, 12526, 2545, NULL, '[{\"text\":\"I have looked forward to your coming. But tell me, are you recovered?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I am most glad of that. Now, I hope you will not doubt the earnestness of my concern...but I would ask a favor of you. Nor can I deny that I summoned you here in part with this in mind. Know, however, that I proceed only upon the understanding that you are rested and well.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, ''),
(70, 8, 25, 1, 1, 19396, 3409, 'Partial', '[{\"action\":true,\"text\":\"Ah, there you are. I hear you\'re looking for a little adventure. Well, if you\'re really up for it, I\'ve got something for you. But I\'m warning you, it\'s no laughing matter. It\'s about Les. I\'m short on supplies again. Bring 50 anole carcasses and 50 chigoe carcasses and their eggs, 25 apiece.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 22,\n            \"value\": 25,\n            \"type\": \"default\"\n        },\n        {\n            \"id\": 33,\n            \"value\": 25,\n            \"type\": \"default\"\n        }\n    ],\n    \"defeat_bots\": [\n        {\n            \"id\": 31,\n            \"value\": 50\n        },\n        {\n            \"id\": 32,\n            \"value\": 50\n        }\n    ]\n}'),
(71, 8, 26, 1, 1, 19396, 3409, 'Partial', '[{\"action\":true,\"text\":\"I really need feathers for my arrows. Bring me at least 50 of them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 116,\n            \"value\": 50,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(72, 8, 27, 1, 1, 19396, 3409, 'Partial', '[{\"action\":true,\"text\":\"Hi. Here\'s the thing. I can\'t handle a flying beast infestation on my own. If we don\'t do something now, it\'ll be too late. Why don\'t we reduce their population?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"defeat_bots\": [\n        {\n            \"id\": 36,\n            \"value\": 100\n        },\n        {\n            \"id\": 50,\n            \"value\": 15\n        }\n    ]\n}'),
(73, 8, 28, 1, 1, 19396, 3409, 'Partial', '[{\"action\":true,\"text\":\"Stranger, we all need your help. We\'re under attack by flying beasts. Please help us.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"move_to\": [\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Repel an attack\",\r\n            \"x\": 16224,\r\n            \"y\": 1600,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 36,\r\n                \"attribute_power\": 60\r\n            }\r\n        },\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Repel an attack\",\r\n            \"x\": 17248,\r\n            \"y\": 1120,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 36,\r\n                \"attribute_power\": 60\r\n            }\r\n        },\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Repel an attack\",\r\n            \"x\": 22432,\r\n            \"y\": 1919,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 36,\r\n                \"attribute_power\": 60\r\n            }\r\n        },\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Repel an attack\",\r\n            \"x\": 22816,\r\n            \"y\": 2048,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 36,\r\n                \"attribute_power\": 60\r\n            }\r\n        },\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Repel an attack\",\r\n            \"x\": 17655,\r\n            \"y\": 4497,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 36,\r\n                \"attribute_power\": 60,\r\n                \"world_id\": 1\r\n            }\r\n        },\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Repel an attack\",\r\n            \"x\": 25696,\r\n            \"y\": 3296,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 36,\r\n                \"attribute_power\": 60\r\n            }\r\n        }\r\n    ]\r\n}'),
(74, 8, 29, 1, 1, 19396, 3409, 'Partial', '[{\"action\":true,\"text\":\"Hi. I need iron, as always. Not many people are helping. Bring me 25 bars of iron. I really need them to make arrows.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 72,\n            \"value\": 25,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(75, 9, 30, 1, 1, 19862, 3473, 'Partial', '[{\"action\":true,\"text\":\"Hello, stranger. Do you like to hunt? We should hunt wild animals. I think 50 carcasses of each critter will be enough.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"defeat_bots\": [\n        {\n            \"id\": 39,\n            \"value\": 50\n        },\n        {\n            \"id\": 42,\n            \"value\": 50\n        }\n    ]\n}'),
(76, 9, 31, 1, 1, 19862, 3473, 'Partial', '[{\"action\":true,\"text\":\"Hi. Did you hear there\'s a ghost living in the swamps? Get rid of him. Only for sure. They say it keeps showing up. But try it anyway.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"defeat_bots\": [\r\n        {\r\n            \"id\": 52,\r\n            \"value\": 10\r\n        }\r\n    ]\r\n}'),
(77, 9, 32, 1, 1, 19862, 3473, 'Partial', '[{\"action\":true,\"text\":\"Hi. There\'s an orc settlement on the left in the steppe. You know what scares me? There\'s an ogre that just showed up and he\'s got dynamite in his head, you know? Okay, one, but they keep coming. You should visit them like this. I hope you come back with some ogre trophies.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"defeat_bots\": [\r\n        {\r\n            \"id\": 41,\r\n            \"value\": 10\r\n        }\r\n    ]\r\n}'),
(78, 9, 33, 1, 1, 19862, 3473, 'Partial', '[{\"action\":true,\"text\":\"Aapphpphpph. Hello stranger. You smell that foul odor. It\'s the mushrooms. They\'re getting big and plentiful. Please get rid of them. I think there\'s some kind of magic involved. Good luck, traveler.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"defeat_bots\": [\n        {\n            \"id\": 49,\n            \"value\": 5\n        },\n        {\n            \"id\": 34,\n            \"value\": 80\n        }\n    ]\n}'),
(79, 9, 34, 1, 1, 19862, 3473, 'Partial', '[{\"action\":true,\"text\":\"Hi. My guys need good armor and a great hammer. Bring them to me. I owe you one\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 82,\r\n            \"value\": 1,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(80, 7, 35, 1, 1, 19475, 3217, 'Partial', '[{\"action\":true,\"text\":\"Hello stranger. I\'m studying the fragments of darkness, if you could bring them to me.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 111,\n            \"value\": 50,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(81, 7, 36, 1, 1, 19475, 3217, 'Partial', '[{\"action\":true,\"text\":\"Hey, stranger. I want you to see how our aethers works. Find this location in the swamps.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"To get to aether\",\n            \"x\": 8063,\n            \"y\": 3781,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        }\n    ]\n}'),
(82, 7, 37, 1, 1, 19475, 3217, 'Partial', '[{\"action\":true,\"text\":\"Hello, stranger. Between the prairie and the plain, there\'s some kind of creature. It changes color. It\'s very frightening to the people of Gridania. You can reduce their numbers. Destroy 25 of the creatures.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"defeat_bots\": [\r\n        {\r\n            \"id\": 51,\r\n            \"value\": 10\r\n        }\r\n    ]\r\n}'),
(83, 7, 38, 1, 1, 19475, 3217, 'Partial', '[{\"action\":true,\"text\":\"Hi. I\'m still learning the elements of dark creatures and substances. But there are still a lot of kinda dead and kinda not dead dolls walking around near the cemetery. You can make the world a more peaceful place. Destroy 50 dolls\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"defeat_bots\": [\n        {\n            \"id\": 43,\n            \"value\": 50\n        }\n    ]\n}'),
(84, 7, 39, 1, 1, 19475, 3217, 'Partial', '[{\"action\":true,\"text\":\"Hi. There\'s a dungeon behind the cemetery. I don\'t know where it came from, but there\'s too many zombies. Can you figure it out? I\'d appreciate it if you could kill 50 zombies.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"defeat_bots\": [\n        {\n            \"id\": 44,\n            \"value\": 50\n        }\n    ]\n}'),
(85, 8, 40, 1, 1, 19396, 3409, 'Partial', '[{\"text\":\"We periodically exterminate populations of forest dwellers and borderlands to prevent such accidents, but these creatures have begun spawning out of season, and so it is becoming increasingly difficult to control their numbers.If you want to prove yourself, go to the forest and border zones and eliminate 20 pests each in all zones.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Be careful and approach them one at a time so your adventurer career doesn\'t get cut short.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"defeat_bots\": [\n        {\n            \"id\": 31,\n            \"value\": 20\n        },\n        {\n            \"id\": 32,\n            \"value\": 20\n        },\n        {\n            \"id\": 33,\n            \"value\": 20\n        },\n        {\n            \"id\": 34,\n            \"value\": 20\n        },\n        {\n            \"id\": 36,\n            \"value\": 20\n        },\n        {\n            \"id\": 42,\n            \"value\": 20\n        }\n    ]\n}'),
(86, 1, 1, 3, 1, 19548, 4305, NULL, '[{\"text\":\"Wait, you left so quickly, I\'d listen now if I were you. Just listen. This world is unlike any other, it\'s very exciting and full of possibilities. So, step one! You need to open the menu where you normally vote. It\'s called the Voting Panel. \",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"This is a very important panel, because you will interact with it most often. Your first task is to select the player\'s class. How to do this: 1. Go to the voting menu2. Click on upgrades and professions3. There will be a section at the top. Select a class. Choose the one you like.\",\"side\":\"thoughts\",\"left_speaker_id\":0,\"right_speaker_id\":0},{\"action\":true,\"text\":\"4. Don\'t miss the most important part! Familiarize yourself with the other buttons. Give it a try, you won\'t lose anything.5.  The most important thing!!!! when you familiarize yourself. The heart or  shield next to you is the navigator. It will help you always find your way\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_end\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":21125,\"y\":4433}}]}}', NULL),
(87, 1, 1, 4, 1, 21125, 4433, NULL, '[{\"text\":\"Where to start, actually. Remember we talked about the voting panel, we always interact through it, like if it\'s different stores, houses, craft places, etc. There are so many different interactions.And so click on the panel. At the top will be information about it, also pay attention to it.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"At the bottom are all the crafting items. They are divided into groups. To craft something, you need to click on the item and it will say:Required items: item - quantity.And then the graph of crafting (gold). Click on it. And we create an item. To avoid wasting time, you can specify the number of items in the field where you specify the reason.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_recieve_objectives\":{\"steps\":[{\"action\":\"message\",\"chat\":\"You can craft some item\'s here!\"},{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":21540,\"y\":4351}}]},\"on_end\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":22405,\"y\":4721}}]}}', NULL),
(88, 1, 1, 5, 1, 22356, 4721, NULL, '[{\"action\":true,\"text\":\"So we\'re the ones with the ore. I think you already know how to process it. This is for your practice.  We have 5 stones to mine. It\'s not that hard, but it\'ll be a lot of fun afterwards. Oh yes when you mine or farm you can get different items than just 1.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_recieve_objectives\":{\"steps\":[{\"action\":\"message\",\"chat\":\"In this point\'s you can mining ores!\"},{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":22587,\"y\":4721}},{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":23278,\"y\":4497}}]},\"on_end\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":21986,\"y\":6001}}]}}', '{\n    \"required_items\": [\n        {\n            \"id\": 173,\n            \"value\": 5,\n            \"type\": \"show\"\n        }\n    ]\n}'),
(89, 1, 1, 6, 1, 21986, 6001, 'Partial', '[{\"text\":\"Okay. This is one of the stores where you can sell certain things. Give him the stones. Remember how we talked through our voting menu. Oh, I almost forgot. We have a little bit of an economy here. That is, each player contributes to the economy, he mines ore and the store in turn makes products from it. And these products you can transfer to other stores where you can buy something and get gold for it! Work as a courier and for money. \",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Unfortunately, without products you can\'t buy anything, alas. And so your task is to sell 5 stones. Try it. I\'ll go somewhere else and you catch up with me. Follow your heart navigator . But sell first!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_recieve_objectives\":{\"steps\":[{\"action\":\"message\",\"chat\":\"Here you can sell ores!\"},{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":21678,\"y\":6033}}]},\"on_end\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":18254,\"y\":3057}}]}}', '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Go to the Store\",\n            \"x\": 21769,\n            \"y\": 6065,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 1,\n            \"completion_text\": \"Sell the stones\"\n        }\n    ]\n}'),
(90, 1, 1, 7, 1, 18254, 3057, NULL, '[{\"text\":\"I think you sold the stones and didn\'t keep them in your pockets. Heh-heh-heh. Remember we talked about classes?Well, look, each class has its own abilities. And then there\'s general abilities. I guess if you\'re interested, you can look it up and click on it. So to make a long story short. You pump your skills and then you can do a bind on a certain button with a certain skill that you pump and you can quickly use it during the battle. Use it! Useful thing\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"In the meantime, I\'ll move on. Study\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_recieve_objectives\":{\"steps\":[{\"action\":\"message\",\"chat\":\"Here you can learn your first skill!\"},{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":17927,\"y\":2965}}]},\"on_end\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":17345,\"y\":5265}}]}}', NULL),
(91, 1, 1, 8, 1, 17345, 5265, NULL, '[{\"action\":true,\"text\":\"It\'s a quiet neighborhood. You can buy a house here. Or buy a guild house if you are a guild leader. You can decorate your house, add friends, open doors, grow crops in your house. There are certain places where you can buy a house. Look for them, and you will find them. Let\'s move on\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_recieve_objectives\":{\"steps\":[{\"action\":\"message\",\"chat\":\"Here you got your first house!\"},{\"action\":\"fix_cam\",\"delay\":80,\"position\":{\"x\":17958,\"y\":5297}},{\"action\":\"fix_cam\",\"delay\":80,\"position\":{\"x\":16868,\"y\":5265}},{\"action\":\"fix_cam\",\"delay\":80,\"position\":{\"x\":18749,\"y\":5073}},{\"action\":\"fix_cam\",\"delay\":80,\"position\":{\"x\":19242,\"y\":5585}}]},\"on_end\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":16980,\"y\":2385}}]}}', NULL),
(92, 1, 1, 9, 1, 16980, 2385, NULL, '[{\"action\":true,\"text\":\"It\'s a bank. This is where you can deposit your money. It\'s important. First of all, you can lose some of it during battles. You need to enter the territory of the bank and press the respawn button. After you do this, a window will open where you will need to point the cursor at the tabs: Deposit / Withdraw. Try if you have sold stones put the money in the bank. And I\'ll move on.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_recieve_objectives\":{\"steps\":[{\"action\":\"message\",\"chat\":\"Here you can use bank managing!\"},{\"action\":\"fix_cam\",\"delay\":80,\"position\":{\"x\":17644,\"y\":2321}}]},\"on_end\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":16051,\"y\":2289}}]}}', NULL),
(93, 1, 1, 10, 1, 16051, 2289, NULL, '[{\"action\":true,\"text\":\"And this is our farm. This is where the plants are grown. This is where you can harvest your crops and sell them. Let me show you\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_recieve_objectives\":{\"steps\":[{\"action\":\"message\",\"chat\":\"Here you can planting!\"},{\"action\":\"fix_cam\",\"delay\":100,\"position\":{\"x\":15584,\"y\":2321}},{\"action\":\"fix_cam\",\"delay\":100,\"position\":{\"x\":13962,\"y\":2321}}]},\"on_end\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":150,\"position\":{\"x\":15352,\"y\":1425}}]}}', NULL),
(94, 1, 1, 11, 1, 15352, 1425, NULL, '[{\"action\":true,\"text\":\"Well, that\'s the last part. This is where you can sell your harvest. These crops are also processed into products like in the miner\'s store. Don\'t forget to carry food if you want to buy something. And don\'t forget to upgrade your skills, if you remember where. There you can pump the efficiency of the miner and farmer. And that\'s not all I told you! Find Bertennant. He will tell you. If anything, you can always find me\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(95, 16, 42, 1, 1, 15153, 1393, 'Partial', '[{\"action\":true,\"text\":\"Hi. This is a difficult time in our community. I was wondering if I could ask you to harvest some of the stock. Bring me 500 wheat. I will reward you\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 70,\n            \"value\": 500,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(96, 16, 43, 1, 1, 15153, 1393, 'Partial', '[{\"action\":true,\"text\":\"Hi. My ranch needs building materials. Can you bring me 500 stones?  That\'d be great. Thanks.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 173,\n            \"value\": 500,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(97, 16, 44, 1, 1, 15153, 1393, 'Partial', '[{\"action\":true,\"text\":\"Hi. As you know, raw food doesn\'t taste very good. Get me some 200 coal so I can cook over a fire. Uh-huh. Berries on a coal fire.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 50,\n            \"value\": 200,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(98, 16, 45, 1, 1, 15153, 1393, 'Partial', '[{\"action\":true,\"text\":\"Hi. You already know we have a lot of breakdowns. Iron ingots can help us fix or replace our tools. Can you please bring me 100 iron ignot?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 72,\r\n            \"value\": 100,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(99, 16, 46, 1, 1, 15153, 1393, 'Partial', '[{\"action\":true,\"text\":\"Hello, my friend. I want berries. Yeah. Bring me 250 berries. I\'d love it if you\'d do that for me.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 67,\r\n            \"value\": 250,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(100, 16, 47, 1, 1, 15153, 1393, NULL, '[{\"action\":true,\"text\":\"Uh-huh. I need help cleaning up. You come to my ranch a lot.  Help me, please.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Clean up the ranch\",\n            \"x\": 15057,\n            \"y\": 1425,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 20,\n            \"completion_text\": \"You cleared this place out\",\n            \"pick_up_item\": {\n                \"id\": 92,\n                \"value\": 1\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Clean up the ranch\",\n            \"x\": 14944,\n            \"y\": 1169,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 20,\n            \"completion_text\": \"You cleared this place out\",\n            \"pick_up_item\": {\n                \"id\": 92,\n                \"value\": 1\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Clean up the ranch\",\n            \"x\": 14899,\n            \"y\": 1361,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 20,\n            \"completion_text\": \"You cleared this place out\",\n            \"pick_up_item\": {\n                \"id\": 92,\n                \"value\": 1\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Clean up the ranch\",\n            \"x\": 15443,\n            \"y\": 1425,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 20,\n            \"completion_text\": \"You cleared this place out\"\n        }\n    ]\n}'),
(101, 16, 48, 1, 1, 15153, 1393, 'Partial', '[{\"action\":true,\"text\":\"Hey, stranger. I have an assignment for you to investigate the entire area. There might be some wild plants somewhere. We\'ll need that information in the future.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Scout the area\",\n            \"x\": 9962,\n            \"y\": 1777,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 5\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Scout the area\",\n            \"x\": 6329,\n            \"y\": 433,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 5\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Scout the area\",\n            \"x\": 27729,\n            \"y\": 1137,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 5\n        }\n    ]\n}'),
(102, 16, 49, 1, 1, 15153, 1393, 'Partial', '[{\"action\":true,\"text\":\"Hey, I need your help. Plant crops on all the farms\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Plant a crop\",\n            \"x\": 15590,\n            \"y\": 2321,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 60,\n            \"completion_text\": \"You planted a crop\"\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Plant a crop\",\n            \"x\": 15169,\n            \"y\": 2289,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 60,\n            \"completion_text\": \"You planted a crop\"\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Plant a crop\",\n            \"x\": 14751,\n            \"y\": 2257,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 60,\n            \"completion_text\": \"You planted a crop\"\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Plant a crop\",\n            \"x\": 14325,\n            \"y\": 2289,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 60,\n            \"completion_text\": \"You planted a crop\"\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Plant a crop\",\n            \"x\": 13959,\n            \"y\": 2321,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 60,\n            \"completion_text\": \"You planted a crop\"\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Plant a crop\",\n            \"x\": 9693,\n            \"y\": 1681,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 60,\n            \"completion_text\": \"You planted a crop\"\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Plant a crop\",\n            \"x\": 9279,\n            \"y\": 1649,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 60,\n            \"completion_text\": \"You planted a crop\"\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Plant a crop\",\n            \"x\": 8878,\n            \"y\": 1617,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 60,\n            \"completion_text\": \"You planted a crop\"\n        }\n    ]\n}'),
(103, 16, 50, 1, 1, 15153, 1393, 'Partial', '[{\"action\":true,\"text\":\"Hi, I want to check out our fields and beds and see if you can harvest some crops.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"required_items\": [\n        {\n            \"id\": 70,\n            \"value\": 50,\n            \"type\": \"default\"\n        }\n    ],\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Come to the farm with the plantings\",\n            \"x\": 15573,\n            \"y\": 2321,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"completion_text\": \"You\'ve come to the farm. Collect 50 wheat\"\n        }\n    ]\n}'),
(104, 16, 51, 1, 1, 15153, 1393, 'Partial', '[{\"action\":true,\"text\":\"Hello, damn pests. Can you get rid of them or we\'ll be out of crops.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Pest control\",\n            \"x\": 15170,\n            \"y\": 2289,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 33,\n                \"attribute_power\": 30,\n                \"world_id\": 1\n            }\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Pest control\",\n            \"x\": 15589,\n            \"y\": 2321,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 33,\n                \"attribute_power\": 30,\n                \"world_id\": 1\n            }\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Pest control\",\n            \"x\": 14730,\n            \"y\": 2257,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 33,\n                \"attribute_power\": 30,\n                \"world_id\": 1\n            }\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Pest control\",\n            \"x\": 14317,\n            \"y\": 2289,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 33,\n                \"attribute_power\": 30,\n                \"world_id\": 1\n            }\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Pest control\",\n            \"x\": 13956,\n            \"y\": 2321,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 33,\n                \"attribute_power\": 30,\n                \"world_id\": 1\n            }\n        }\n    ]\n}'),
(105, 26, 52, 1, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hello, damn pests. Can you get rid of them or we\'ll be out of crops.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Reconnaissance\",\n            \"x\": 20369,\n            \"y\": 6385,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Reconnaissance\",\n            \"x\": 6584,\n            \"y\": 1233,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Reconnaissance\",\n            \"x\": 9755,\n            \"y\": 5745,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        }\n    ]\n}'),
(106, 26, 53, 1, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hi, I\'m short a stone. Bring me 50 stones.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 173,\r\n            \"value\": 50,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(107, 26, 54, 1, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hey my miners need some food. Bring 25 corn.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 58,\r\n            \"value\": 25,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(108, 26, 55, 1, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hello. We have a contract with a partner, but we are running out of time. We need to find 300 ores of iron ore. Can you bring it over?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 54,\r\n            \"value\": 300,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(109, 26, 56, 1, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hello. There are mushrooms growing at one of our mines outside of town. We need to reduce their population.  \",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"defeat_bots\": [\r\n        {\r\n            \"id\": 34,\r\n            \"value\": 50\r\n        }\r\n    ]\r\n}'),
(110, 6, 57, 1, 1, 22609, 2961, '', '[{\"action\":true,\"text\":\"There\'s no easier task. Bring me 100 pig meat\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 186,\r\n            \"value\": 100,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(111, 57, 58, 1, 1, 17550, 3665, 'Partial', '[{\"text\":\"To do that, do some work for me.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"You ready for your feat? Good! Because the state of our toilets and showers requires heroic intervention. Go in there and restore them to their former glory. or at least clean.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', '{\"on_recieve_objectives\":{\"steps\":[{\"action\":\"fix_cam\",\"delay\":70,\"position\":{\"x\":17919,\"y\":3889}}]}}', '{\n    \"reward_items\": [\n        {\n            \"id\": 256,\n            \"value\": 1\n        }\n    ],\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Cleaning sink\",\n            \"x\": 17715,\n            \"y\": 3889,\n            \"mode\": \"move_press\",\n            \"cooldown\": 10,\n            \"completion_text\": \"You cleaned the sink.\"\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Cleaning sink\",\n            \"x\": 17775,\n            \"y\": 3889,\n            \"mode\": \"move_press\",\n            \"cooldown\": 10,\n            \"completion_text\": \"You cleaned the sink.\"\n        },\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Cleaning sink\",\n            \"x\": 17838,\n            \"y\": 3889,\n            \"mode\": \"move_press\",\n            \"cooldown\": 10,\n            \"completion_text\": \"You cleaned the sink.\"\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Cleaning toilet\",\n            \"x\": 17969,\n            \"y\": 3857,\n            \"mode\": \"move_press\",\n            \"cooldown\": 10,\n            \"completion_text\": \"You cleaned the toilet.\"\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Cleaning toilet\",\n            \"x\": 18097,\n            \"y\": 3857,\n            \"mode\": \"move_press\",\n            \"cooldown\": 10,\n            \"completion_text\": \"You cleaned the toilet.\"\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Cleaning toilet\",\n            \"x\": 18225,\n            \"y\": 3857,\n            \"mode\": \"move_press\",\n            \"cooldown\": 10,\n            \"completion_text\": \"You cleaned the toilet.\"\n        }\n    ]\n}'),
(112, 26, 59, 1, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hi. There are places that need to be explored for new ores can you help?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Explore the place\",\r\n            \"x\": 7587,\r\n            \"y\": 1777,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 50\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Explore the place\",\r\n            \"x\": 23857,\r\n            \"y\": 5009,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 50\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Explore the place\",\r\n            \"x\": 23761,\r\n            \"y\": 5649,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 50\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Explore the place\",\r\n            \"x\": 22322,\r\n            \"y\": 5489,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 50\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Explore the place\",\r\n            \"x\": 9263,\r\n            \"y\": 5457,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\"\r\n        }\r\n    ]\r\n}'),
(113, 26, 60, 1, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hi need to find some items. They have value to someone. Bring them in, please.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Find items\",\r\n            \"x\": 6336,\r\n            \"y\": 433,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_follow_press\",\r\n            \"cooldown\": 15,\r\n            \"interactive\": {\r\n                \"x\": 6336,\r\n                \"y\": 433\r\n            }\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Find items\",\r\n            \"x\": 23272,\r\n            \"y\": 5713,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_follow_press\",\r\n            \"cooldown\": 15,\r\n            \"interactive\": {\r\n                \"x\": 23272,\r\n                \"y\": 5713\r\n            }\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Find items\",\r\n            \"x\": 29742,\r\n            \"y\": 2193,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_follow_press\",\r\n            \"cooldown\": 15,\r\n            \"interactive\": {\r\n                \"x\": 29742,\r\n                \"y\": 2193\r\n            }\r\n        }\r\n    ]\r\n}'),
(114, 26, 61, 1, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hi. Some of the mines are blocked. I need help clearing them out.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Clear the wreckage\",\r\n            \"x\": 23793,\r\n            \"y\": 5041,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 120\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Clear the wreckage\",\r\n            \"x\": 23761,\r\n            \"y\": 5649,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 120\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Clear the wreckage\",\r\n            \"x\": 22318,\r\n            \"y\": 5489,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 120\r\n        }\r\n    ]\r\n}'),
(115, 26, 62, 1, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hi. There\'s a spider\'s nest nearby. You should see it.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"move_to\": [\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Explore the spider\'s nest\",\r\n            \"x\": 7194,\r\n            \"y\": 6065,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 30\r\n        }\r\n    ]\r\n}'),
(116, 26, 63, 1, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hi I am in need of rare ore. Bring me 100 titanium ore.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 55,\r\n            \"value\": 100,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(117, 26, 64, 1, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hey, I\'ve got a great assignment for you. Bring me some coal! Lots of coal.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 50,\r\n            \"value\": 1000,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(118, 6, 65, 1, 1, 22609, 2961, '', '[{\"action\":true,\"text\":\"Hi. Few people need fish, but I know what they\'re for. Bring it to me. Here\'s the usual list\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 175,\r\n            \"value\": 500,\r\n            \"type\": \"default\"\r\n        },\r\n        {\r\n            \"id\": 176,\r\n            \"value\": 50,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(119, 6, 66, 1, 1, 22609, 2961, '', '[{\"action\":true,\"text\":\"Hi I think you\'re going to have to buy a house soon. Take a walk around the neighborhood and see if you can find something useful.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Walking\",\r\n            \"x\": 18736,\r\n            \"y\": 5457,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"cooldown\": 20\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Walking\",\r\n            \"x\": 13751,\r\n            \"y\": 1105,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"cooldown\": 20\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Walking\",\r\n            \"x\": 24094,\r\n            \"y\": 2001,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"cooldown\": 20\r\n        }\r\n    ]\r\n}'),
(120, 6, 67, 1, 1, 22609, 2961, '', '[{\"action\":true,\"text\":\"Hi. I\'ve always wanted to see the flying islands. Would you be able to see for me what\'s out there\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Explore the island\",\r\n            \"x\": 724,\r\n            \"y\": 465,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"cooldown\": 20\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Explore the island\",\r\n            \"x\": 31257,\r\n            \"y\": 465,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"cooldown\": 20\r\n        }\r\n    ]\r\n}'),
(121, 6, 67, 2, 1, 22609, 2961, '', '[{\"action\":true,\"text\":\"You were there, weren\'t you? I can tell from you that you liked it. Must be a lot of rare plants, huh?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, NULL),
(122, 6, 68, 1, 1, 22609, 2961, '', '[{\"action\":true,\"text\":\"Hi. Listen, here\'s the deal. Check out the auction. No one\'s selling my beer?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"View the auction\",\r\n            \"x\": 20622,\r\n            \"y\": 1617,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"cooldown\": 300\r\n        }\r\n    ]\r\n}'),
(123, 6, 68, 2, 1, 22609, 2961, '', '[{\"action\":true,\"text\":\"You\'re taking a long time. I see you\'re trying to say you didn\'t find anything. That\'s fine.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, ''),
(124, 6, 69, 1, 1, 22609, 2961, '', '[{\"action\":true,\"text\":\"Hi. No one\'s taking care of my trees. Will you help me? I need to water them. Please, please, please.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Water the trees\",\r\n            \"x\": 12177,\r\n            \"y\": 3185,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_follow_press\",\r\n            \"cooldown\": 150,\r\n            \"interactive\": {\r\n                \"x\": 12177,\r\n                \"y\": 3105\r\n            }\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Water the trees\",\r\n            \"x\": 17459,\r\n            \"y\": 1617,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_follow_press\",\r\n            \"cooldown\": 120,\r\n            \"interactive\": {\r\n                \"x\": 17459,\r\n                \"y\": 1557\r\n            }\r\n        },\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Water the trees\",\r\n            \"x\": 6345,\r\n            \"y\": 433,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_follow_press\",\r\n            \"cooldown\": 120,\r\n            \"interactive\": {\r\n                \"x\": 6345,\r\n                \"y\": 373\r\n            }\r\n        }\r\n    ]\r\n}'),
(125, 6, 70, 1, 1, 22609, 2961, '', '[{\"action\":true,\"text\":\"Hello, adventurer! I would need you to deliver a package to Jackson, the miner. He needs these healing potions because, you know, mining is hard, haha! Would you mind helping me out? Of course, I can pay you!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Pick up potions\",\r\n            \"x\": 22702,\r\n            \"y\": 2897,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 15\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Pick up potions\",\r\n            \"x\": 23296,\r\n            \"y\": 3313,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 15\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Pick up potions\",\r\n            \"x\": 22576,\r\n            \"y\": 3313,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\"\r\n        }\r\n    ]\r\n}'),
(126, 26, 70, 2, 1, 21678, 6033, NULL, '[{\"action\":true,\"text\":\"Hi. I think you are from Mother Miounne. Thank you very much.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, ''),
(127, 7, 71, 1, 1, 19475, 3217, 'Partial', '[{\"action\":true,\"text\":\"Muahaha! What a great day, adventurer. I need magic matter in order to make my special concoctions, but it seems like I ran out of those. Would you mind lending me some help?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 115,\r\n            \"value\": 20,\r\n            \"type\": \"default\"\r\n        }\r\n    ],\r\n    \"defeat_bots\": [\r\n        {\r\n            \"id\": 45,\r\n            \"value\": 5\r\n        }\r\n    ]\r\n}'),
(128, 9, 72, 1, 1, 19862, 3473, 'Partial', '[{\"action\":true,\"text\":\"The King of the sea has come back! You must  confront this giant Octopus at once, adventurer. This monster has caused way too much harm in the past, and this time, we must fend it off again! Fortunately, it hasn\'t emerged from the sea yet. You must not allow it to reach the land, or else this whole city will be in danger!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]', NULL, '{\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Kill Octopus\",\r\n            \"x\": 891,\r\n            \"y\": 2458,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 69,\r\n                \"attribute_power\": 500,\r\n                \"world_id\": 1\r\n            },\r\n            \"pick_up_item\": {\r\n                \"id\": 7,\r\n                \"value\": 50\r\n            }\r\n        }\r\n    ]\r\n}');

-- --------------------------------------------------------

--
-- Table structure for table `tw_crafts_list`
--

CREATE TABLE `tw_crafts_list` (
  `ID` int(11) NOT NULL,
  `GroupName` varchar(256) NOT NULL,
  `ItemID` int(11) DEFAULT NULL,
  `ItemValue` int(11) NOT NULL,
  `RequiredItems` varchar(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  `Price` int(11) NOT NULL DEFAULT 100,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

--
-- Dumping data for table `tw_crafts_list`
--

INSERT INTO `tw_crafts_list` (`ID`, `GroupName`, `ItemID`, `ItemValue`, `RequiredItems`, `Price`, `WorldID`) VALUES
(1, 'First start', 41, 1, '[40/1]', 0, 0),
(2, 'Resources:Mine', 36, 1, '[50/20],[35/20]', 30, 1),
(3, 'Resources:Mine', 72, 1, '[50/40],[54/20]', 80, 1),
(4, 'Resources:Mine', 74, 1, '[50/150],[56/100]', 550, 1),
(12, 'Armor:Tank', 82, 1, '[50/200],[72/24],[108/8],[37/1],[341/1]', 15900, 1),
(13, 'Armor:Tank', 83, 1, '[50/250],[73/30],[108/25],[82/1],[341/1]', 31690, 1),
(14, 'Armor:Tank', 84, 1, '[50/300],[74/50],[108/25],[83/1],[341/1]', 72325, 1),
(15, 'Tools:Rake', 26, 1, '[50/180],[72/12],[108/5],[39/1],[340/1]', 6004, 1),
(16, 'Tools:Rake', 42, 1, '[50/180],[73/12],[26/1],[340/1]', 10230, 1),
(17, 'Tools:Rake', 62, 1, '[50/250],[74/24],[42/1],[340/1]', 30070, 1),
(18, 'Tools:Pickaxe', 27, 1, '[50/200],[72/15],[108/5],[39/1],[339/1]', 6264, 1),
(19, 'Tools:Pickaxe', 43, 1, '[50/180],[73/12],[27/1],[339/1]', 11365, 1),
(20, 'Tools:Pickaxe', 63, 1, '[50/250],[74/12],[43/1],[339/1]', 23470, 1),
(23, 'Resources:Special', 108, 1, '[107/5]', 50, 1),
(32, 'Resources:Mine', 73, 1, '[50/100],[55/60]', 280, 1),
(40, 'Armor:Tank', 37, 1, '[50/100],[36/24],[28/1],[341/1]', 7820, 1),
(41, 'Tools:Rake', 38, 1, '[50/80],[36/12],[340/1]', 1440, 1),
(42, 'Tools:Pickaxe', 39, 1, '[50/100],[36/15],[339/1]', 1550, 1),
(43, 'Tools:Rod', 174, 1, '[72/50],[233/15],[274/1],[343/1]', 18880, 1),
(44, 'Modules:Ammo', 60, 1, '[72/1],[108/1],[152/1]', 5200, 1),
(45, 'Modules:Ammo Regen', 69, 1, '[72/5],[152/1],[50/1000]', 6400, 1),
(46, 'Tools:Pickaxe', 76, 1, '[173/30],[107/2]', 60, 1),
(47, 'Tools:Rake', 75, 1, '[173/30],[107/2]', 60, 1),
(48, 'Other:Title', 187, 1, '[186/500],[29/100]', 55500, 1),
(49, 'Modules:Lucky Drop', 81, 1, '[186/1000],[107/100],[152/1]', 17500, 1),
(50, 'Modules:DMG', 126, 1, '[110/200],[113/150],[111/25],[109/150],[152/5]', 42900, 1),
(51, 'Armor:Tank', 28, 1, '[108/25],[341/1]', 8000, 1),
(52, 'Tools:Gloves', 189, 1, '[50/180],[72/10],[108/5],[188/1],[342/1]', 6820, 1),
(53, 'Tools:Gloves', 190, 1, '[50/200],[73/15],[189/1],[342/1]', 11775, 1),
(54, 'Tools:Gloves', 188, 1, '[50/80],[36/10],[192/1],[342/1]', 3890, 1),
(55, 'Tools:Gloves', 191, 1, '[50/250],[74/20],[190/1],[342/1]', 34970, 1),
(56, 'Tools:Gloves', 192, 1, '[108/15],[342/1]', 3800, 1),
(60, 'Weapons:SpecialWeapons', 99, 1, '[2/1],[315/10],[115/100],[214/500],[7/999999]', 12004393, 1),
(61, 'Weapons:SpecialWeapons', 101, 1, '[6/1],[314/10],[215/500],[7/999999]', 12006793, 1),
(62, 'Weapons:SpecialWeapons', 102, 1, '[2/1],[315/10],[127/100],[216/500],[7/999999]', 12012293, 1),
(63, 'Weapons:SpecialWeapons', 103, 1, '[6/1],[314/10],[217/500],[7/999999]', 12006793, 1),
(64, 'Weapons:SpecialWeapons', 151, 1, '[3/1],[311/10],[218/500],[7/999999]', 12002393, 1),
(65, 'Weapons:SpecialWeapons', 100, 1, '[5/1],[313/10],[227/100],[127/100],[219/500],[7/999999]', 12117293, 1),
(66, 'Weapons:SpecialWeapons', 150, 1, '[6/1],[314/10],[220/500],[7/999999]', 12006793, 1),
(67, 'Resources:Special', 106, 1, '[105/25]', 325, 1),
(68, 'Resources:Special', 104, 1, '[92/10]', 220, 1),
(69, 'Resources:Special', 127, 1, '[123/25],[50/100]', 975, 1),
(70, 'Resources:Special', 120, 1, '[104/200]', 32800, 1),
(71, 'Resources:Special', 115, 1, '[121/70]', 490, 1),
(73, 'Tools:Rod', 222, 1, '[7/50],[107/10],[343/1]', 3500, 1),
(74, 'Resources:Farm', 226, 1, '[223/50]', 300, 1),
(75, 'Resources:Farm', 227, 1, '[224/100]', 600, 1),
(76, 'Modules:Product Capacity', 228, 1, '[226/150],[152/1]', 83750, 1),
(77, 'Modules:Efficiency', 119, 1, '[226/100],[108/100],[152/1]', 69500, 1),
(78, 'Other:Drafts', 214, 1, '[281/100],[315/50],[7/9999999]', 70074993, 1),
(79, 'Other:Drafts', 215, 1, '[115/999999],[7/9999999]', 101999961, 1),
(80, 'Other:Drafts', 216, 1, '[7/9999999]', 69999993, 1),
(81, 'Other:Drafts', 217, 1, '[115/999999],[7/9999999]', 101999961, 1),
(82, 'Other:Drafts', 218, 1, '[115/999999],[7/9999999]', 101999961, 1),
(83, 'Other:Drafts', 219, 1, '[127/999999],[7/9999999]', 180999882, 1),
(84, 'Other:Drafts', 220, 1, '[115/999999],[7/9999999]', 101999961, 1),
(85, 'Modules:Lucky Drop', 135, 1, '[226/100],[7/5000],[152/1]', 92500, 1),
(86, 'Modules:Hammer DMG', 229, 1, '[115/50],[111/400],[73/50],[152/3]', 39400, 1),
(87, 'Modules:Attack SPD', 230, 1, '[115/50],[111/400],[127/10],[134/1],[138/5000],[152/5]', 43495, 1),
(88, 'Modules:Rifle DMG', 231, 1, '[372/100],[136/5],[152/2]', 16450, 1),
(89, 'Modules:Patience', 232, 1, '[108/200],[104/50],[233/50],[58/500],[61/500],[68/250],[71/250],[152/1]', 49750, 1),
(90, 'Resources:Farm', 134, 1, '[122/250],[121/500]', 5250, 1),
(91, 'Other:Special', 89, 1, '[134/100],[115/500]', 214500, 1),
(92, 'Resources:Special', 138, 5, '[113/5]', 180, 1),
(93, 'Resources:Special', 138, 10, '[125/1]', 117, 1),
(94, 'Modules:DMG', 87, 1, '[113/100],[125/50],[138/100],[108/50],[104/25],[152/5]', 44650, 1),
(95, 'Armor:Dps', 234, 1, '[108/13],[186/100],[341/1]', 7660, 1),
(96, 'Armor:Dps', 235, 1, '[36/13],[22/80],[33/80],[341/1],[234/1]', 9950, 1),
(97, 'Armor:Dps', 236, 1, '[72/12],[116/50],[393/25],[387/50],[341/1],[235/1]', 20510, 1),
(98, 'Armor:Dps', 237, 1, '[73/15],[110/150],[106/50],[116/150],[341/1],[236/1]', 46150, 1),
(99, 'Armor:Healer', 238, 1, '[108/13],[70/100],[58/100],[61/100],[341/1]', 7160, 1),
(100, 'Armor:Healer', 239, 1, '[36/12],[61/400],[67/200],[68/100],[108/25],[238/1],[341/1]', 12860, 1),
(101, 'Armor:Healer', 240, 1, '[72/12],[71/400],[108/40],[239/1],[341/1]', 20160, 1),
(102, 'Armor:Healer', 241, 1, '[73/15],[226/40],[108/50],[240/1],[341/1]', 56200, 1),
(104, 'Modules:MP', 242, 1, '[121/70],[115/10],[108/50],[152/1]', 11810, 1),
(105, 'Modules:Crit', 243, 1, '[115/50],[111/400],[125/100],[152/2]', 32100, 1),
(106, 'Resources:Special', 233, 1, '[175/14],[176/8]', 126, 1),
(108, 'Modules:Attack SPD', 259, 1, '[233/100],[152/2]', 24600, 1),
(109, 'Modules:HP', 260, 1, '[137/800]', 35200, 1),
(110, 'Modules:Vampirism', 51, 1, '[111/800],[121/150],[122/120],[152/3]', 34490, 1),
(111, 'Other:Title', 261, 1, '[70/500],[29/100]', 50500, 1),
(112, 'Other:Title', 262, 1, '[35/250],[29/100]', 50500, 1),
(113, 'Modules:HP', 263, 1, '[108/50],[152/1]', 11000, 1),
(115, 'Potion:HP', 267, 1, '[376/10],[122/50],[266/2],[115/75],[138/500],[112/25],[106/5]', 24415, 1),
(116, 'Potion:HP', 268, 1, '[376/20],[122/100],[267/2],[115/150],[138/1000],[112/50],[106/10]', 108830, 1),
(117, 'Potion:MP', 272, 1, '[371/10],[121/50],[271/2],[115/75],[138/500],[112/25],[106/5]', 24055, 1),
(118, 'Potion:MP', 273, 1, '[371/20],[121/100],[272/2],[115/150],[138/1000],[112/50],[106/10]', 108110, 1),
(119, 'Tools:Rod', 274, 1, '[36/50],[233/15],[222/1],[343/1]', 7140, 1),
(120, 'Tools:Rod', 275, 1, '[73/50],[233/15],[174/1],[343/1]', 34130, 1),
(121, 'Tools:Rod', 276, 1, '[74/50],[233/15],[275/1],[343/1]', 75880, 1),
(123, 'Resources:Mine', 281, 1, '[50/200],[277/100]', 700, 1),
(124, 'Resources:Mine', 282, 1, '[50/300],[278/100]', 900, 1),
(125, 'Resources:Mine', 283, 1, '[50/400],[279/100]', 1100, 1),
(126, 'Resources:Mine', 284, 1, '[50/500],[280/100]', 1300, 1),
(127, 'Modules:Attack SPD', 285, 1, '[230/1],[386/1],[281/500],[152/1]', 562400, 1),
(128, 'Modules:HP', 286, 1, '[7/99999],[281/1000],[152/3]', 1414993, 1),
(129, 'Armor:Dps', 288, 1, '[7/9999999],[74/50],[341/1]', 70032493, 1),
(130, 'Armor:Healer', 289, 1, '[7/9999999],[74/50],[341/1]', 70032493, 1),
(131, 'Armor:Tank', 287, 1, '[7/9999999],[281/100],[341/1]', 70074993, 1),
(132, 'Armor:Dps', 290, 1, '[7/9999999],[281/50],[341/1]', 70039993, 1),
(133, 'Armor:Healer', 291, 1, '[7/9999999],[281/50],[341/1]', 70039993, 1),
(134, 'Modules:Lucky Drop', 292, 1, '[7/999999],[282/1000]', 7899993, 1),
(135, 'Modules:MP', 293, 1, '[7/999999],[282/1000],[152/3]', 7914993, 1),
(136, 'Armor:Tank', 294, 1, '[7/9999999],[282/150],[341/1]', 70139993, 1),
(137, 'Armor:Dps', 295, 1, '[7/9999999],[282/75],[341/1]', 70072493, 1),
(138, 'Armor:Healer', 296, 1, '[7/9999999],[282/75],[341/1]', 70072493, 1),
(139, 'Modules:Ammo Regen', 297, 1, '[7/9999999],[283/1000],[152/5]', 71124993, 1),
(140, 'Modules:HP', 298, 1, '[7/99999],[283/1000],[152/3]', 1814993, 1),
(141, 'Armor:Tank', 299, 1, '[7/9999999],[283/75],[341/1]', 70087493, 1),
(142, 'Armor:Dps', 300, 1, '[7/9999999],[379/500],[283/100],[341/1]', 70153493, 1),
(143, 'Armor:Healer', 301, 1, '[7/9999999],[283/100],[341/1]', 70114993, 1),
(144, 'Modules:Crit', 302, 1, '[7/99999],[284/1000],[152/5]', 2024993, 1),
(145, 'Modules:Ammo', 303, 1, '[7/999999],[284/1000],[152/5]', 8324993, 1),
(146, 'Armor:Tank', 304, 1, '[7/9999999],[284/300],[341/1]', 70394993, 1),
(147, 'Armor:Dps', 305, 1, '[7/9999999],[284/150],[341/1]', 70199993, 1),
(148, 'Armor:Healer', 306, 1, '[7/9999999],[284/150],[341/1]', 70199993, 1),
(149, 'Resources:Special', 7, 10, '[124/1000]', 9000, 1),
(150, 'Resources:Special', 311, 1, '[3/1]', 400, 1),
(151, 'Resources:Special', 312, 1, '[4/1]', 700, 1),
(152, 'Resources:Special', 313, 1, '[5/1]', 1200, 1),
(153, 'Resources:Special', 314, 1, '[6/1]', 1800, 1),
(154, 'Resources:Special', 315, 1, '[2/1]', 200, 1),
(155, 'Modules:Active Modules', 316, 1, '[315/10],[2/1],[317/100],[152/1]', 66000, 1),
(156, 'Modules:Active Modules', 320, 1, '[318/1],[319/1],[7/10000],[152/1]', 145000, 1),
(159, 'Modules:Active Modules', 64, 1, '[370/25],[325/100],[117/100],[106/100],[152/5]', 594600, 1),
(160, 'Modules:Active Modules', 65, 1, '[326/100],[127/100],[106/100],[152/5]', 1037600, 1),
(161, 'Modules:Active Modules', 66, 1, '[327/100],[117/100],[128/100],[152/3]', 1180500, 1),
(162, 'Modules:Active Modules', 19, 1, '[328/100],[127/1000],[311/50],[152/5]', 1146000, 1),
(163, 'Modules:Active Modules', 20, 1, '[328/100],[127/1000],[312/50],[152/10]', 1180500, 1),
(164, 'Modules:Active Modules', 323, 1, '[70/10000],[58/10000],[186/10000],[72/25],[104/50],[152/1]', 155200, 1),
(165, 'Modules:Gold Capacity', 331, 1, '[108/100],[117/500],[330/1],[152/2]', 774875, 1),
(166, 'Modules:Gold Capacity', 334, 1, '[108/1000],[104/250],[333/1],[17/10000],[152/5]', 321000, 1),
(167, 'Helmet:Tank', 349, 1, '[108/13],[341/1],[365/1]', 9060, 1),
(168, 'Helmet:Tank', 350, 1, '[50/50],[36/12],[349/1],[365/1]', 3910, 1),
(169, 'Helmet:Tank', 351, 1, '[50/100],[72/12],[108/4],[350/1],[365/1]', 7540, 1),
(170, 'Helmet:Tank', 352, 1, '[50/125],[73/15],[108/13],[351/1],[365/1]', 18385, 1),
(171, 'Helmet:Tank', 353, 1, '[50/150],[74/25],[108/13],[352/1],[365/1]', 37960, 1),
(172, 'Helmet:Dps', 354, 1, '[108/7],[186/50],[365/1]', 3890, 1),
(173, 'Helmet:Dps', 355, 1, '[36/7],[22/40],[33/40],[365/1],[354/1]', 4990, 1),
(174, 'Helmet:Dps', 356, 1, '[72/12],[116/50],[393/25],[387/50],[365/1],[355/1]', 14510, 1),
(175, 'Helmet:Dps', 357, 1, '[73/7],[110/75],[106/25],[116/75],[365/1],[356/1]', 22935, 1),
(176, 'Helmet:Healer', 358, 1, '[108/7],[70/50],[58/50],[61/50],[365/1]', 3640, 1),
(177, 'Helmet:Healer', 359, 1, '[36/6],[61/200],[67/100],[68/50],[108/13],[358/1],[365/1]', 6490, 1),
(178, 'Helmet:Healer', 360, 1, '[72/6],[71/200],[108/20],[359/1],[365/1]', 12580, 1),
(179, 'Helmet:Healer', 361, 1, '[73/7],[226/20],[108/25],[360/1],[365/1]', 30460, 1),
(180, 'Modules:Eidolon PWR', 373, 1, '[371/10],[223/100],[152/1]', 5930, 1),
(181, 'Modules:MP', 375, 1, '[374/100],[173/10000],[152/1]', 23400, 1),
(182, 'Food / Boosters', 377, 1, '[376/10],[371/10]', 1020, 1),
(183, 'Modules:Eidolon PWR', 378, 1, '[376/100],[173/10000],[104/500],[108/100],[226/100],[152/1]', 168400, 1),
(184, 'Food / Boosters', 380, 1, '[379/10],[371/20]', 1430, 1),
(185, 'Modules:Hammer DMG', 382, 1, '[381/100],[152/1]', 11000, 1),
(186, 'Modules:Crit', 383, 1, '[381/100],[106/100],[152/1]', 12500, 1),
(187, 'Modules:Lucky', 385, 1, '[81/1],[384/100],[152/1]', 17000, 1),
(188, 'Modules:Crit', 386, 1, '[110/500],[169/20],[152/1]', 162500, 1),
(189, 'Resources:Special', 104, 5, '[387/15]', 570, 1),
(190, 'Resources:Eidolons', 390, 1, '[389/100],[136/100],[111/100],[115/100]', 21500, 1),
(191, 'Food / Boosters', 392, 1, '[391/50]', 2650, 1),
(192, 'Modules:HP', 394, 1, '[393/500],[152/1]', 45000, 1),
(193, 'Modules:Patience', 395, 1, '[307/5],[233/20],[36/5],[115/2],[152/1]', 8314, 1),
(194, 'Food / Boosters', 396, 1, '[71/50],[70/500]', 800, 1),
(195, 'Modules:Patience', 397, 1, '[104/150],[152/1]', 29600, 1),
(196, 'Modules:Extraction', 398, 1, '[104/150],[7/350],[152/1]', 32050, 1),
(197, 'Modules:Active Modules', 404, 1, '[388/1],[403/1],[152/5]', 120000, 1);

-- --------------------------------------------------------

--
-- Table structure for table `tw_dungeons`
--

CREATE TABLE `tw_dungeons` (
  `ID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `DoorX` int(11) NOT NULL DEFAULT 0,
  `DoorY` int(11) NOT NULL DEFAULT 0,
  `Scenario` longtext DEFAULT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_dungeons`
--

INSERT INTO `tw_dungeons` (`ID`, `Level`, `DoorX`, `DoorY`, `Scenario`, `WorldID`) VALUES
(1, 1, 1105, 529, '{\n    \"editor_data\": {\n        \"groups\": []\n    },\n    \"steps\": [\n        {\n            \"id\": \"welcome_start\",\n            \"components\": [\n                {\n                    \"type\": \"message\",\n                    \"mode\": \"chat\",\n                    \"text\": \"Выбери правильное действие и дверь откроется!\"\n                },\n                {\n                    \"type\": \"door_control\",\n                    \"action\": \"create\",\n                    \"position\": {\n                        \"x\": 1140,\n                        \"y\": 529\n                    }\n                },\n                {\n                    \"type\": \"follow_camera\",\n                    \"position\": {\n                        \"x\": 1140,\n                        \"y\": 529\n                    },\n                    \"delay\": 300\n                }\n            ]\n        },\n        {\n            \"id\": \"message_condition_1\",\n            \"msg_info\": \"Выбери действие и введи его в чат (hello! или reposka)!\",\n            \"completion_logic\": \"any_of\",\n            \"components\": [\n                {\n                    \"type\": \"use_chat_code\",\n                    \"code\": \"hello!\",\n                    \"next_step_id\": \"fail_message_condition_1\"\n                },\n                {\n                    \"type\": \"use_chat_code\",\n                    \"code\": \"reposka\",\n                    \"next_step_id\": \"succes_message_condition_1\"\n                }\n            ]\n        },\n        {\n            \"id\": \"fail_message_condition_1\",\n            \"delay\": 300,\n            \"components\": [\n                {\n                    \"type\": \"message\",\n                    \"next_step_id\": \"message_condition_1\",\n                    \"mode\": \"chat\",\n                    \"text\": \"Ошибка повтори по новой....\"\n                }\n            ]\n        },\n        {\n            \"id\": \"succes_message_condition_1\",\n            \"components\": [\n                {\n                    \"type\": \"door_control\",\n                    \"action\": \"remove\"\n                },\n                {\n                    \"type\": \"follow_camera\",\n                    \"position\": {\n                        \"x\": 1140,\n                        \"y\": 529\n                    },\n                    \"delay\": 300\n                }\n            ]\n        },\n        {\n            \"id\": \"defeat_mobs_start\",\n            \"completion_logic\": \"any_of\",\n            \"msg_info\": \"Убейте мобов при этом не убив не одного Living Stone!\",\n            \"components\": [\n                {\n                    \"type\": \"defeat_mobs\",\n                    \"position\": {\n                        \"x\": 1497,\n                        \"y\": 529\n                    },\n                    \"radius\": 200,\n                    \"mobs\": [\n                        {\n                            \"mob_id\": 40,\n                            \"count\": 5,\n                            \"level\": 3\n                        }\n                    ],\n                    \"next_step_id\": \"success_defeat_start\"\n                },\n                {\n                    \"type\": \"defeat_mobs\",\n                    \"mode\": \"wave\",\n                    \"kill_target\": 1,\n                    \"position\": {\n                        \"x\": 1497,\n                        \"y\": 529\n                    },\n                    \"radius\": 200,\n                    \"mobs\": [\n                        {\n                            \"mob_id\": 42,\n                            \"count\": 2,\n                            \"level\": 3\n                        }\n                    ],\n                    \"next_step_id\": \"fail_defeat_start\"\n                }\n            ]\n        },\n        {\n            \"id\": \"fail_defeat_start\",\n            \"delay\": 300,\n            \"components\": [\n                {\n                    \"type\": \"message\",\n                    \"next_step_id\": \"defeat_mobs_start\",\n                    \"mode\": \"chat\",\n                    \"text\": \"Испытание провалено...... Через несколько секунд начнется заного!\"\n                }\n            ]\n        },\n        {\n            \"id\": \"success_defeat_start\",\n            \"delay\": 300,\n            \"components\": [\n                {\n                    \"type\": \"message\",\n                    \"mode\": \"chat\",\n                    \"text\": \"Испытание пройдено...... \"\n                }\n            ]\n        },\n        {\n            \"id\": \"gffsaa\",\n            \"components\": [\n                {\n                    \"type\": \"message\",\n                    \"text\": \"asdasdas\",\n                    \"mode\": \"full\"\n                },\n                {\n                    \"type\": \"message\",\n                    \"text\": \"weqweqweq\",\n                    \"mode\": \"full\",\n                    \"delay\": 255\n                },\n                {\n                    \"type\": \"message\",\n                    \"text\": \"Новое сообщение\",\n                    \"mode\": \"full\",\n                    \"delay\": 350\n                },\n                {\n                    \"type\": \"teleport\",\n                    \"position\": {\n                        \"x\": 1497,\n                        \"y\": 529\n                    },\n                    \"delay\": 350\n                }\n            ],\n            \"completion_logic\": \"sequential\"\n        }\n    ]\n}', 2);

-- --------------------------------------------------------

--
-- Table structure for table `tw_dungeons_door`
--

CREATE TABLE `tw_dungeons_door` (
  `ID` int(11) NOT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `BotID` int(11) NOT NULL,
  `DungeonID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Dumping data for table `tw_dungeons_door`
--

INSERT INTO `tw_dungeons_door` (`ID`, `PosX`, `PosY`, `BotID`, `DungeonID`) VALUES
(1, 5215, 529, 58, 1),
(2, 6298, 1105, 59, 1),
(3, 7910, 465, 59, 1),
(4, 6222, 2769, 60, 1),
(5, 8483, 1873, 62, 1),
(6, 4489, 2833, 62, 1),
(7, 784, 2737, 64, 1),
(8, 3687, 1937, 63, 1);

-- --------------------------------------------------------

--
-- Table structure for table `tw_dungeons_records`
--

CREATE TABLE `tw_dungeons_records` (
  `ID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `DungeonID` int(11) NOT NULL,
  `Time` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_groups`
--

CREATE TABLE `tw_groups` (
  `ID` int(11) NOT NULL,
  `OwnerUID` int(11) NOT NULL,
  `AccountIDs` varchar(64) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_guilds`
--

CREATE TABLE `tw_guilds` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT 'Member name',
  `Members` varchar(512) DEFAULT NULL,
  `DefaultRankID` int(11) DEFAULT NULL,
  `LeaderUID` int(11) DEFAULT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Exp` bigint(11) NOT NULL DEFAULT 0,
  `Bank` longtext NOT NULL DEFAULT '0',
  `Score` int(11) NOT NULL DEFAULT 0,
  `LogFlag` bigint(11) NOT NULL DEFAULT 0,
  `AvailableSlots` int(11) NOT NULL DEFAULT 2,
  `ChairLevel` int(11) NOT NULL DEFAULT 1,
  `DoorHealth` int(11) NOT NULL DEFAULT 1,
  `DecorationSlots` int(11) NOT NULL DEFAULT 5
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
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

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
  `InitialFee` int(11) NOT NULL DEFAULT 0,
  `RentDays` int(11) NOT NULL DEFAULT 3,
  `Doors` longtext DEFAULT NULL,
  `Farmzones` longtext DEFAULT NULL,
  `Properties` longtext DEFAULT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_guilds_houses`
--

INSERT INTO `tw_guilds_houses` (`ID`, `GuildID`, `InitialFee`, `RentDays`, `Doors`, `Farmzones`, `Properties`, `WorldID`) VALUES
(1, NULL, 10000, 3, '[\r\n    {\r\n        \"name\": \"Front Door\",\r\n        \"position\": {\"x\": 17827, \"y\": 1617}\r\n    },\r\n    {\r\n        \"name\": \"Back Door\",\r\n        \"position\": {\"x\": 19769, \"y\": 2257}\r\n    },\r\n    {\r\n        \"name\": \"Farm zone\",\r\n        \"position\": {\"x\": 18326, \"y\": 1905}\r\n    },\r\n    {\r\n        \"name\": \"Farm zone\",\r\n        \"position\": {\"x\": 19228, \"y\": 2257}\r\n    }\r\n]', '[\n    {\n        \"items\": \"121,122\",\n        \"name\": \"FarmZone 2\",\n        \"position\": {\n            \"x\": 18127.0,\n            \"y\": 1905.0\n        },\n        \"radius\": 224\n    },\n    {\n        \"items\": \"224,223\",\n        \"name\": \"FarmZone 1\",\n        \"position\": {\n            \"x\": 19027.0,\n            \"y\": 2257.0\n        },\n        \"radius\": 224\n    }\n]', '{\r\n    \"position\": {\"x\": 18720, \"y\": 1568},\r\n    \"text_position\": {\"x\": 19456, \"y\": 1648},\r\n    \"radius\": 1376\r\n}', 1),
(2, NULL, 9000, 6, '[\n    {\n        \"name\": \"Door\",\n        \"position\": {\"x\": 15686, \"y\": 3313}\n    },\n    {\n        \"name\": \"Bedroom\",\n        \"position\": {\"x\": 15246, \"y\": 2961}\n    },\n    {\n        \"name\": \"Farm zone\",\n        \"position\": {\"x\": 14888, \"y\": 3313}\n    }\n]', '[\n    {\n        \"items\": \"224\",\n        \"name\": \"FarmZone 1\",\n        \"position\": {\n            \"x\": 14708.0,\n            \"y\": 3313.0\n        },\n        \"radius\": 192\n    }\n]', '{\r\n    \"position\": {\"x\": 15200, \"y\": 3008},\r\n    \"text_position\": {\"x\": 15663, \"y\": 2576},\r\n    \"radius\": 864\r\n}', 1),
(3, NULL, 8000, 0, '[\r\n    {\r\n        \"name\": \"Front Door\",\r\n        \"position\": {\"x\": 12408, \"y\": 849}\r\n    },\r\n    {\r\n        \"name\": \"Back Door\",\r\n        \"position\": {\"x\": 10660, \"y\": 945}\r\n    },\r\n    {\r\n        \"name\": \"Farm zone\",\r\n        \"position\": {\"x\": 11200, \"y\": 928}\r\n    }\r\n]', '[\n    {\n        \"items\": \"223\",\n        \"name\": \"FarmZone\",\n        \"position\": {\n            \"x\": 11744.0,\n            \"y\": 928.0\n        },\n        \"radius\": 256\n    }\n]', '{\r\n    \"position\": {\"x\": 11520, \"y\": 544},\r\n    \"text_position\": {\"x\": 11528, \"y\": 1040},\r\n    \"radius\": 1120\r\n}', 1),
(4, NULL, 7000, 6, '[\r\n    {\r\n        \"name\": \"Front Door\",\r\n        \"position\": {\"x\": 16244, \"y\": 5073}\r\n    },\r\n    {\r\n        \"name\": \"Farm zone\",\r\n        \"position\": {\"x\": 16832, \"y\": 6208}\r\n    }\r\n]', '[\n    {\n        \"items\": \"\",\n        \"name\": \"FarmZone\",\n        \"position\": {\n            \"x\": 16928.0,\n            \"y\": 4768.0\n        },\n        \"radius\": 128\n    }\n]', '{\r\n    \"position\": {\"x\": 16352, \"y\": 4704},\r\n    \"text_position\": {\"x\": 16016, \"y\": 5185},\r\n    \"radius\": 736\r\n}', 1),
(5, NULL, 6000, 6, '[\r\n    {\r\n        \"name\": \"Front Door\",\r\n        \"position\": {\"x\": 26242, \"y\": 2449}\r\n    },\r\n    {\r\n        \"name\": \"Farm zone\",\r\n        \"position\": {\"x\": 26432, \"y\": 2208}\r\n    }\r\n]', '[\n    {\n        \"name\": \"FarmZone\",\n        \"position\": {\n            \"x\": 26368,\n            \"y\": 2208\n        },\n        \"items\": \"70\",\n        \"radius\": 96\n    }\n]', '{\r\n    \"position\": {\"x\": 26592, \"y\": 2304},\r\n    \"text_position\": {\"x\": 26627, \"y\": 2642},\r\n    \"radius\": 512\r\n}', 1),
(6, NULL, 5000, 6, '[\r\n    {\r\n        \"name\": \"Front Door\",\r\n        \"position\": {\"x\": 27766, \"y\": 1873}\r\n    },\r\n    {\r\n        \"name\": \"Farm zone\",\r\n        \"position\": {\"x\": 27872, \"y\": 1536}\r\n    }\r\n]', '[\n    {\n        \"name\": \"FarmZone\",\n        \"position\": {\n            \"x\": 27968,\n            \"y\": 1536\n        },\n        \"items\": \"70\",\n        \"radius\": 96\n    }\n]', '{\r\n    \"position\": {\"x\": 27712, \"y\": 1536},\r\n    \"text_position\": {\"x\": 28067, \"y\": 1248},\r\n    \"radius\": 840\r\n}', 1);

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
  `Rights` int(11) NOT NULL DEFAULT 3,
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
  `InitialFee` int(11) NOT NULL,
  `RentDays` int(11) NOT NULL DEFAULT 3,
  `Class` varchar(32) NOT NULL DEFAULT 'Class name',
  `Bank` varchar(128) NOT NULL DEFAULT '0',
  `Doors` longtext DEFAULT NULL,
  `Farmzones` longtext DEFAULT NULL,
  `Properties` longtext DEFAULT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_houses`
--

INSERT INTO `tw_houses` (`ID`, `UserID`, `InitialFee`, `RentDays`, `Class`, `Bank`, `Doors`, `Farmzones`, `Properties`, `WorldID`) VALUES
(1, NULL, 10000, 0, 'House 1', '0', '[\n    {\n        \"name\": \"Main door\",\n        \"position\": {\"x\": 17736, \"y\": 5329}\n    }\n]', NULL, '{\r\n    \"position\": {\"x\": 17933, \"y\": 5265},\r\n    \"text_position\": {\"x\": 17936, \"y\": 5182}\r\n}', 1),
(2, NULL, 15000, 0, 'House 2', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 18538, \"y\": 5105}\r\n    }\r\n]', NULL, '{\r\n    \"position\": {\"x\": 18734, \"y\": 5073},\r\n    \"text_position\": {\"x\": 18734, \"y\": 4947}\r\n}', 1),
(3, NULL, 15000, 0, 'House 3', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 19021, \"y\": 5617}\r\n    }\r\n]', NULL, '{\r\n    \"position\": {\"x\": 19214, \"y\": 5585},\r\n    \"text_position\": {\"x\": 19214, \"y\": 5458}\r\n}', 1),
(4, NULL, 10000, 3, 'House 4', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 18216, \"y\": 16065}\r\n    }\r\n]', NULL, '{\r\n    \"position\": {\"x\": 18414, \"y\": 6033},\r\n    \"text_position\": {\"x\": 18414, \"y\": 5902}\r\n}', 1),
(5, NULL, 7000, 0, 'House 5', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 17815, \"y\": 5841}\r\n    }\r\n]', NULL, '{\r\n    \"position\": {\"x\": 17617, \"y\": 5809},\r\n    \"text_position\": {\"x\": 17617, \"y\": 5684}\r\n}', 1),
(6, NULL, 7000, 3, 'House 6', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 17015, \"y\": 6161}\r\n    }\r\n]', NULL, '{\r\n    \"position\": {\"x\": 16817, \"y\": 6129},\r\n    \"text_position\": {\"x\": 16817, \"y\": 6000}\r\n}', 1),
(7, NULL, 5000, 0, 'House 7', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 16239, \"y\": 5649}\r\n    }\r\n]', NULL, '{\r\n    \"position\": {\"x\": 16049, \"y\": 5617},\r\n    \"text_position\": {\"x\": 16049, \"y\": 5488}\r\n}', 1),
(8, NULL, 5000, 3, 'House 8', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 16617, \"y\": 5297}\r\n    }\r\n]', NULL, '{\r\n    \"position\": {\"x\": 16846, \"y\": 5265},\r\n    \"text_position\": {\"x\": 16846, \"y\": 5140}\r\n}', 1),
(9, NULL, 30000, 0, 'Left apartments 1', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 13201, \"y\": 913}\r\n    },\r\n    {\r\n        \"name\": \"Back door\",\r\n        \"position\": {\"x\": 14322, \"y\": 913}\r\n    }\r\n]', '[\n    {\n        \"items\": \"223\",\n        \"name\": \"Vegetable garden\",\n        \"position\": {\n            \"x\": 13754.0,\n            \"y\": 945.0\n        },\n        \"radius\": 192\n    }\n]', '{\r\n    \"position\": {\"x\": 13169, \"y\": 945},\r\n    \"text_position\": {\"x\": 13759, \"y\": 881}\r\n}', 1),
(10, NULL, 25000, 1, 'Left apartments 2', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 13202, \"y\": 753}\r\n    },\r\n    {\r\n        \"name\": \"Back door\",\r\n        \"position\": {\"x\": 14322, \"y\": 753}\r\n    }\r\n]', '[\n    {\n        \"items\": \"71\",\n        \"name\": \"Vegetable garden\",\n        \"position\": {\n            \"x\": 13754.0,\n            \"y\": 785.0\n        },\n        \"radius\": 192\n    }\n]', '{\r\n    \"position\": {\"x\": 13169, \"y\": 785},\r\n    \"text_position\": {\"x\": 13759, \"y\": 721}\r\n}', 1),
(11, NULL, 20000, 0, 'Left apartments 3', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 13202, \"y\": 593}\r\n    },\r\n    {\r\n        \"name\": \"Back door\",\r\n        \"position\": {\"x\": 14325, \"y\": 593}\r\n    }\r\n]', '[\n    {\n        \"items\": \"224\",\n        \"name\": \"Vegetable garden\",\n        \"position\": {\n            \"x\": 13754.0,\n            \"y\": 625.0\n        },\n        \"radius\": 192\n    }\n]', '{\r\n    \"position\": {\"x\": 13169, \"y\": 625},\r\n    \"text_position\": {\"x\": 13759, \"y\": 561}\r\n}', 1),
(12, NULL, 15000, 0, 'Left apartments 4', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 13225, \"y\": 433}\r\n    },\r\n    {\r\n        \"name\": \"Back door\",\r\n        \"position\": {\"x\": 14302, \"y\": 433}\r\n    }\r\n]', '[\n    {\n        \"items\": \"121,122\",\n        \"name\": \"Vegetable garden\",\n        \"position\": {\n            \"x\": 13754.0,\n            \"y\": 465.0\n        },\n        \"radius\": 192\n    }\n]', '{\r\n    \"position\": {\"x\": 13169, \"y\": 465},\r\n    \"text_position\": {\"x\": 13759, \"y\": 401}\r\n}', 1),
(13, NULL, 10000, 3, 'Left apartments 5', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 13236, \"y\": 273}\r\n    },\r\n    {\r\n        \"name\": \"Back door\",\r\n        \"position\": {\"x\": 14292, \"y\": 273}\r\n    }\r\n]', '[\n    {\n        \"items\": \"225\",\n        \"name\": \"Vegetable garden\",\n        \"position\": {\n            \"x\": 13754.0,\n            \"y\": 305.0\n        },\n        \"radius\": 192\n    }\n]', '{\r\n    \"position\": {\"x\": 13201, \"y\": 305},\r\n    \"text_position\": {\"x\": 13759, \"y\": 229}\r\n}', 1),
(19, NULL, 15000, 0, '(R/L) Apartments 1', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 23439, \"y\": 1745}\r\n    }\r\n]', '[\r\n    {\r\n        \"items\": \"58\",\r\n        \"name\": \"Vegetable garden\",\r\n        \"position\": {\r\n            \"x\": 23559,\r\n            \"y\": 1553\r\n        },\r\n        \"radius\": 192\r\n    }\r\n]', '{\r\n    \"position\": {\"x\": 23404, \"y\": 1745},\r\n    \"text_position\": {\"x\": 23729, \"y\": 1486}\r\n}', 1),
(20, NULL, 15000, 0, '(R/L) Apartments 2', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 23439, \"y\": 1329}\r\n    }\r\n]', '[\r\n    {\r\n        \"items\": \"58\",\r\n        \"name\": \"Vegetable garden\",\r\n        \"position\": {\r\n            \"x\": 23559,\r\n            \"y\": 1137\r\n        },\r\n        \"radius\": 192\r\n    }\r\n]', '{\r\n    \"position\": {\"x\": 23404, \"y\": 1329},\r\n    \"text_position\": {\"x\": 23729, \"y\": 1070}\r\n}', 1),
(21, NULL, 15000, 0, '(R/L) Apartments 3', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 23439, \"y\": 913}\r\n    }\r\n]', '[\r\n    {\r\n        \"items\": \"58\",\r\n        \"name\": \"Vegetable garden\",\r\n        \"position\": {\r\n            \"x\": 23559,\r\n            \"y\": 721\r\n        },\r\n        \"radius\": 192\r\n    }\r\n]', '{\r\n    \"position\": {\"x\": 23404, \"y\": 913},\r\n    \"text_position\": {\"x\": 23729, \"y\": 654}\r\n}', 1),
(22, NULL, 15000, 0, '(R/L) Apartments 4', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 23439, \"y\": 497}\r\n    }\r\n]', '[\r\n    {\r\n        \"items\": \"58\",\r\n        \"name\": \"Vegetable garden\",\r\n        \"position\": {\r\n            \"x\": 23559,\r\n            \"y\": 305\r\n        },\r\n        \"radius\": 192\r\n    }\r\n]', '{\r\n    \"position\": {\"x\": 23404, \"y\": 497},\r\n    \"text_position\": {\"x\": 23729, \"y\": 238}\r\n}', 1),
(23, NULL, 15000, 0, '(R/R) Apartments 1', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 24748, \"y\": 1745}\r\n    }\r\n]', '[\r\n    {\r\n        \"items\": \"225\",\r\n        \"name\": \"Vegetable garden\",\r\n        \"position\": {\r\n            \"x\": 24646,\r\n            \"y\": 1553\r\n        },\r\n        \"radius\": 192\r\n    }\r\n]', '{\r\n    \"position\": {\"x\": 24795, \"y\": 1745},\r\n    \"text_position\": {\"x\": 24462, \"y\": 1486}\r\n}', 1),
(24, NULL, 15000, 3, '(R/R) Apartments 2', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 24748, \"y\": 1329}\r\n    }\r\n]', '[\r\n    {\r\n        \"items\": \"225\",\r\n        \"name\": \"Vegetable garden\",\r\n        \"position\": {\r\n            \"x\": 24646,\r\n            \"y\": 1137\r\n        },\r\n        \"radius\": 192\r\n    }\r\n]', '{\r\n    \"position\": {\"x\": 24795, \"y\": 1329},\r\n    \"text_position\": {\"x\": 24462, \"y\": 1070}\r\n}', 1),
(25, NULL, 15000, 3, '(R/R) Apartments 3', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 24748, \"y\": 913}\r\n    }\r\n]', '[\r\n    {\r\n        \"items\": \"225\",\r\n        \"name\": \"Vegetable garden\",\r\n        \"position\": {\r\n            \"x\": 24646,\r\n            \"y\": 721\r\n        },\r\n        \"radius\": 192\r\n    }\r\n]', '{\r\n    \"position\": {\"x\": 24795, \"y\": 913},\r\n    \"text_position\": {\"x\": 24462, \"y\": 654}\r\n}', 1),
(26, NULL, 15000, 3, '(R/R) Apartments 4', '0', '[\r\n    {\r\n        \"name\": \"Main door\",\r\n        \"position\": {\"x\": 24752, \"y\": 497}\r\n    }\r\n]', '[\r\n    {\r\n        \"items\": \"225\",\r\n        \"name\": \"Vegetable garden\",\r\n        \"position\": {\r\n            \"x\": 24646,\r\n            \"y\": 305\r\n        },\r\n        \"radius\": 192\r\n    }\r\n]', '{\r\n    \"position\": {\"x\": 24795, \"y\": 497},\r\n    \"text_position\": {\"x\": 24462, \"y\": 238}\r\n}', 1);

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

-- --------------------------------------------------------

--
-- Table structure for table `tw_items_list`
--

CREATE TABLE `tw_items_list` (
  `Comment` varchar(256) DEFAULT NULL,
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT 'Item name',
  `Description` varchar(64) NOT NULL DEFAULT 'Item desc',
  `Group` enum('Other','Usable','Resource','Quest','Settings','Equipment','Decoration','Potion','Currency') NOT NULL DEFAULT 'Quest',
  `Type` enum('Default','Equip hammer','Equip gun','Equip shotgun','Equip grenade','Equip rifle','Equip pickaxe','Equip rake','Equip fishrod','Equip gloves','Equip armor (tank)','Equip armor (dps)','Equip armor (healer)','Equip helmet (tank)','Equip helmet (dps)','Equip helmet (healer)','Equip eidolon','Equip title','Equip potion HP','Equip potion MP','Single use x1','Multiple use x99','Resource harvestable','Resource mineable','Resource fishes') NOT NULL DEFAULT 'Default',
  `Flags` set('Can''t droppable','Can''t tradeable') DEFAULT NULL,
  `ScenarioData` longtext DEFAULT NULL,
  `ScenarioMode` enum('universal','world') NOT NULL DEFAULT 'universal',
  `InitialPrice` int(11) DEFAULT NULL,
  `RequiresProducts` int(11) DEFAULT NULL,
  `AT1` int(11) DEFAULT NULL,
  `AT2` int(11) DEFAULT NULL,
  `ATValue1` int(11) DEFAULT NULL,
  `ATValue2` int(11) DEFAULT NULL,
  `Data` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL CHECK (json_valid(`Data`))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_items_list`
--

INSERT INTO `tw_items_list` (`Comment`, `ID`, `Name`, `Description`, `Group`, `Type`, `Flags`, `ScenarioData`, `InitialPrice`, `RequiresProducts`, `AT1`, `AT2`, `ATValue1`, `ATValue2`, `Data`) VALUES
(NULL, 1, 'Gold', 'Major currency', 'Currency', 'Default', NULL, NULL, 7, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 2, 'Hammer', 'A normal hammer', 'Equipment', 'Equip hammer', NULL, NULL, 200, NULL, 13, 3, 3, 5, NULL),
(NULL, 3, 'Gun', 'Conventional weapon', 'Equipment', 'Equip gun', NULL, NULL, 400, NULL, 14, NULL, 1, NULL, NULL),
(NULL, 4, 'Shotgun', 'Conventional weapon', 'Equipment', 'Equip shotgun', NULL, NULL, 700, NULL, 15, NULL, 1, NULL, NULL),
(NULL, 5, 'Grenade', 'Conventional weapon', 'Equipment', 'Equip grenade', NULL, NULL, 1200, NULL, 16, NULL, 3, NULL, NULL),
(NULL, 6, 'Rifle', 'Conventional weapon', 'Equipment', 'Equip rifle', NULL, NULL, 1800, NULL, 17, NULL, 5, NULL, NULL),
(NULL, 7, 'Material', 'Required to improve weapons', 'Currency', 'Default', NULL, NULL, 7, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 8, 'Product', 'Required to shop', 'Currency', 'Default', NULL, NULL, 7, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 9, 'Skill point', 'Skill point', 'Currency', 'Default', NULL, NULL, 5000, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 10, 'Achievement point', 'Achievement Point', 'Currency', 'Default', NULL, NULL, 1000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 11, 'Pickup Shotgun', 'Decoration for house!', 'Decoration', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 12, 'Pickup Grenade', 'Decoration for house!', 'Decoration', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 13, 'Pickup Mana', 'Decoration for house!', 'Decoration', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 14, 'CHANGE SHIT', 'CHANGE SHIT', 'Other', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 15, 'CHANGE SHIT', 'CHANGE SHIT', 'Other', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 16, 'Capsule survival experience', 'You got 10-50 class experience', 'Usable', 'Multiple use x99', NULL, NULL, 350, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 17, 'Little bag of gold', 'You got 10-50 gold', 'Usable', 'Multiple use x99', NULL, NULL, 8, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 18, 'Pickup Health', 'Decoration for house!', 'Decoration', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 19, 'Explosive for gun', 'Explosive bullets (gun)', 'Equipment', 'Default', NULL, NULL, 50000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 20, 'Explosive for shotgun', 'Explosive bullets (shotgun)', 'Equipment', 'Default', NULL, NULL, 70000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 21, 'Sword', 'A regular sword', 'Quest', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 22, 'Chigoe egg', 'The egg that Chigoe lays', 'Resource', 'Default', NULL, NULL, 16, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 23, 'Survey records', 'Strange incomprehensible records', 'Quest', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 24, 'Toolbox', 'A drawer full of tools', 'Quest', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 25, 'Show equipment description', 'Settings game.', 'Settings', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 26, 'Iron rake', 'The usual iron rake', 'Equipment', 'Equip rake', NULL, NULL, 5690, NULL, 12, NULL, 5, NULL, NULL),
(NULL, 27, 'Iron pickaxe', 'The usual iron pickaxe', 'Equipment', 'Equip pickaxe', NULL, NULL, 6825, NULL, 11, NULL, 5, NULL, NULL),
('Power 20', 28, 'Leather armor', 'Lightweight armor', 'Equipment', 'Equip armor (tank)', NULL, NULL, 2000, NULL, 5, NULL, 24, 6, NULL),
(NULL, 29, 'Activity coin', 'Coins that are given out for activity', 'Currency', 'Default', NULL, NULL, 500, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 30, 'Rookie', 'Rookie title', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 31, 'Crate', 'Crate', 'Quest', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 32, 'Surveyor\'s rope', 'Ordinary surveyor\'s rope.', 'Quest', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 33, 'Anole egg', 'It\'s a strange-looking and disgusting egg.', 'Resource', 'Default', NULL, NULL, 16, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 34, 'Show critical damage', 'Settings game.', 'Settings', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 35, 'Copper ore', 'Copper ore', 'Resource', 'Resource mineable', NULL, NULL, 2, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 36, 'Copper ingot', 'Processed copper', 'Resource', 'Default', NULL, NULL, 60, NULL, NULL, NULL, NULL, NULL, NULL),
('Power 50', 37, 'Copper armor', 'Armor made of copper', 'Equipment', 'Equip armor (tank)', NULL, NULL, 7820, NULL, 5, 7, 65, 15, NULL),
(NULL, 38, 'Copper rake', 'The usual copper rake', 'Equipment', 'Equip rake', NULL, NULL, 1440, NULL, 12, NULL, 2, NULL, NULL),
(NULL, 39, 'Copper pickaxe', 'The usual copper pickaxe', 'Equipment', 'Equip pickaxe', NULL, NULL, 3264, NULL, 11, NULL, 2, NULL, NULL),
(NULL, 40, 'Green grass', 'It looks like ordinary green grass.', 'Resource', 'Resource harvestable', NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 41, 'Treated green grass', 'Compressed processed grass', 'Quest', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 42, 'Titanium rake', 'The usual titanium rake.', 'Equipment', 'Equip rake', NULL, NULL, 15620, NULL, 12, NULL, 8, NULL, NULL),
(NULL, 43, 'Titanium pickaxe', 'The usual titanium pickaxe.', 'Equipment', 'Equip pickaxe', NULL, NULL, 15620, NULL, 11, NULL, 8, NULL, NULL),
(NULL, 44, 'Leather bag', 'Just leather bag.', 'Quest', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 45, 'Cargo', 'Very valuable cargo.', 'Quest', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 46, 'Leia\'s egg', 'The egg that Leia needs', 'Quest', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 47, 'Letter ', 'Stamped letter', 'Quest', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 48, 'Artifact', 'A magical thing', 'Quest', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 49, 'Monoa mask', 'Its a beautiful mask', 'Equipment', 'Default', NULL, NULL, NULL, NULL, 5, 7, 5, 5, NULL),
(NULL, 50, 'Coal', 'Black fuel', 'Resource', 'Resource mineable', NULL, NULL, 1, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 51, 'Vampire ring', 'The ring that draws out life', 'Equipment', 'Default', NULL, NULL, 32904, NULL, 8, NULL, 134, NULL, NULL),
('Не реализ', 52, 'Vampire armor', 'Armor that sucks the life', 'Equipment', 'Default', NULL, NULL, NULL, NULL, 5, 8, 50, 50, NULL),
(NULL, 53, 'AmmoModule', 'AmmoModuleNPC', 'Equipment', 'Default', NULL, NULL, NULL, NULL, 10, 9, 1000, 1000, NULL),
(NULL, 54, 'Iron ore', 'Ordinary iron ore', 'Resource', 'Resource mineable', NULL, NULL, 2, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 55, 'Titanium ore', 'Solid titanium ore', 'Resource', 'Resource mineable', NULL, NULL, 3, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 56, 'Adamantite ore', 'The strongest adamantite ore', 'Resource', 'Resource mineable', NULL, NULL, 4, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 57, 'EIDOL #1', '', 'Equipment', 'Equip eidolon', NULL, NULL, NULL, NULL, 19, NULL, 25, NULL, NULL),
(NULL, 58, 'Corn', 'Farm-grown Corn', 'Resource', 'Resource harvestable', NULL, NULL, 2, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 59, 'EIDOL #2', '', 'Equipment', 'Equip eidolon', NULL, NULL, NULL, NULL, 19, NULL, 25, NULL, NULL),
(NULL, 60, 'Bullet belt', 'Belt of the sharpshooter', 'Equipment', 'Default', NULL, NULL, 12545, NULL, 10, NULL, 5, NULL, NULL),
(NULL, 61, 'Tomato', 'Farm-grown Tomato', 'Resource', 'Resource harvestable', NULL, NULL, 3, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 62, 'Adamantium rake', 'The usual adamantium rake.', 'Equipment', 'Equip rake', NULL, NULL, 50000, NULL, 12, NULL, 12, NULL, NULL),
(NULL, 63, 'Adamantium pickaxe', 'The usual adamantium pickaxe.', 'Equipment', 'Equip pickaxe', NULL, NULL, 44250, NULL, 11, NULL, 12, NULL, NULL),
(NULL, 64, 'Poison hook', 'Inflicts gradual damage.', 'Equipment', 'Default', NULL, NULL, 999999, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 65, 'Explosive impulse hook', 'Inflicts gradual explode damage.', 'Equipment', 'Default', NULL, NULL, 9999999, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 66, 'Magic spider hook', 'It\'s sticky to the air.', 'Equipment', 'Default', NULL, NULL, 999999, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 67, 'Strawberries', 'Farm-grown Strawberries', 'Resource', 'Resource harvestable', NULL, NULL, 4, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 68, 'Cabbage', 'Farm-grown Cabbage', 'Resource', 'Resource harvestable', NULL, NULL, 5, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 69, 'Bullet generator', 'Spits out lead on command', 'Equipment', 'Default', NULL, NULL, 17775, NULL, 9, NULL, 10, NULL, NULL),
(NULL, 70, 'Wheat', 'Farm-grown Wheat', 'Resource', 'Resource harvestable', NULL, NULL, 1, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 71, 'Pumpkin', 'Farm-grown Pumpkin', 'Resource', 'Resource harvestable', NULL, NULL, 6, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 72, 'Iron ingot', 'Iron ingot', 'Resource', 'Default', NULL, NULL, 80, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 73, 'Titanium ingot', 'Titanium ingot', 'Resource', 'Default', NULL, NULL, 280, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 74, 'Adamantite ingot', 'Adamantite ingot', 'Resource', 'Default', NULL, NULL, 550, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 75, 'Stone rake', 'The usual stone rake.', 'Equipment', 'Equip rake', NULL, NULL, 5, NULL, 12, NULL, 1, NULL, NULL),
(NULL, 76, 'Stone pickaxe', 'The usual stone pickaxe.', 'Equipment', 'Equip pickaxe', NULL, NULL, 5, NULL, 11, NULL, 1, NULL, NULL),
(NULL, 77, 'Kit start', 'Novice kit', 'Usable', 'Multiple use x99', NULL, NULL, 1500, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"random_box\": [\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 3,\r\n      \"chance\": 29.41\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 50,\r\n      \"chance\": 35.29\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 150,\r\n      \"chance\": 17.65\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 300,\r\n      \"chance\": 8.63\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 700,\r\n      \"chance\": 1.96\r\n    },\r\n    {\r\n      \"item_id\": 76,\r\n      \"value\": 1,\r\n      \"chance\": 10.0\r\n    },\r\n    {\r\n      \"item_id\": 75,\r\n      \"value\": 1,\r\n      \"chance\": 10.0\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 5,\r\n      \"chance\": 68.97\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 20,\r\n      \"chance\": 27.59\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 50,\r\n      \"chance\": 3.45\r\n    },\r\n    {\r\n      \"item_id\": 28,\r\n      \"value\": 1,\r\n      \"chance\": 25.0\r\n    },\r\n    {\r\n      \"item_id\": 238,\r\n      \"value\": 1,\r\n      \"chance\": 25.0\r\n    },\r\n    {\r\n      \"item_id\": 234,\r\n      \"value\": 1,\r\n      \"chance\": 25.0\r\n    },\r\n    {\r\n      \"item_id\": 222,\r\n      \"value\": 1,\r\n      \"chance\": 10.0\r\n    }\r\n  ]\r\n}'),
('Botmodule', 78, 'VampireModule', 'VampireModuleNPC', 'Equipment', 'Default', NULL, NULL, NULL, NULL, 8, NULL, 10000, NULL, NULL),
('Не реализ', 79, 'EIDOL Box', 'EIDOL Box', 'Usable', 'Single use x1', NULL, 'EIDOL Box', NULL, NULL, NULL, NULL, NULL, NULL, '{\n  \"random_box\": [\n    {\n      \"item_id\": 57,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 59,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 80,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 88,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 100,\n      \"chance\": 70\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 500,\n      \"chance\": 50\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 5000,\n      \"chance\": 20\n    }\n  ]\n}'),
('Не реализ', 80, 'EIDOL #3', '', 'Equipment', 'Equip eidolon', NULL, NULL, NULL, NULL, 19, NULL, 25, NULL, NULL),
(NULL, 81, 'Pigs tail', 'A strangely lucky pig\'s tail', 'Equipment', 'Default', NULL, NULL, 9000, NULL, 18, NULL, 100, NULL, NULL),
('Power 100', 82, 'Iron armor', 'Iron armor', 'Equipment', 'Equip armor (tank)', NULL, NULL, 15040, NULL, 5, 7, 120, 30, NULL),
('Power 150', 83, 'Titanium armor', 'Titanium armor', 'Equipment', 'Equip armor (tank)', NULL, NULL, 36525, NULL, 5, 7, 180, 45, NULL),
('Power 200', 84, 'Adamantium armor', 'Adamantium armor', 'Equipment', 'Equip armor (tank)', NULL, NULL, 81925, NULL, 5, 7, 240, 60, NULL),
(NULL, 85, 'Thorny Ring', 'A ring of sharp, cursed briars', 'Equipment', 'Default', NULL, NULL, 99999, NULL, 9, NULL, 10, NULL, NULL),
(NULL, 86, 'Thorny Necklace', 'Choker of blood-seeking thorns', 'Equipment', 'Default', NULL, NULL, 999999, NULL, 5, 7, 10, 10, NULL),
(NULL, 87, 'Bone Armillae', 'Armbands carved from monster bone', 'Equipment', 'Default', NULL, NULL, 7850, NULL, 1, NULL, 1, NULL, NULL),
('Не реализ', 88, 'EIDOL #4', '', 'Equipment', 'Equip eidolon', NULL, NULL, NULL, NULL, 19, NULL, 25, NULL, NULL),
(NULL, 89, 'Eidolon Crystal', 'Required to improve eidolons', 'Currency', 'Default', NULL, NULL, 4616400, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 90, 'Ammo Box', 'A box that magically refills', 'Equipment', 'Default', NULL, NULL, 5000, NULL, 10, NULL, 20, NULL, NULL),
(NULL, 91, 'Ammo Generator', 'Generates ammunition', 'Equipment', 'Default', NULL, NULL, 5000, NULL, 9, NULL, 26, NULL, NULL),
(NULL, 92, 'Web', 'A spider\'s web', 'Resource', 'Default', NULL, NULL, 22, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 93, 'Show quest star navigation', 'Settings game.', 'Settings', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 94, 'Pickup Laser', 'Decoration for house!', 'Decoration', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 95, 'Ticket guild', 'Command: /gcreate <name>', 'Other', 'Default', NULL, NULL, 100000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 96, 'Customizer', 'Customizer for personal skins', 'Equipment', 'Default', NULL, NULL, 5000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 97, 'Damage Equalizer', 'Disabled self dmg.', 'Equipment', 'Default', NULL, NULL, 80000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 98, 'Show detail gain messages', 'Settings game.', 'Settings', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 99, 'Hammer Lamp', 'Hammer Lamp', 'Equipment', 'Equip hammer', NULL, NULL, 9999999, NULL, 13, 3, 5, 5, NULL),
('Не реализ', 100, 'Pizdamet', 'Pizdamet', 'Equipment', 'Equip grenade', NULL, NULL, 9999999, NULL, 16, NULL, 2, NULL, NULL),
('Не реализ', 101, 'Wall Pusher', 'Plazma wall', 'Equipment', 'Equip rifle', NULL, NULL, 999999, NULL, 17, NULL, 3, NULL, NULL),
('Не реализ', 102, 'Hammer Blast', 'Hammer Blast', 'Equipment', 'Equip hammer', NULL, NULL, 999999, NULL, 13, 3, 3, 5, NULL),
('Не реализ', 103, 'Magnetic Pulse', 'Conventional weapon', 'Equipment', 'Equip rifle', NULL, NULL, 999999, NULL, 17, NULL, 4, NULL, NULL),
(NULL, 104, 'Thread', 'Thread', 'Resource', 'Default', NULL, NULL, 164, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 105, 'Weak poison', 'A little weak poison', 'Resource', 'Default', NULL, NULL, 13, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 106, 'Poison', 'Concentrated poison', 'Resource', 'Default', NULL, NULL, 15, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 107, 'Untreated Leather', 'Untreated Leather', 'Resource', 'Default', NULL, NULL, 5, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 108, 'Leather', 'Treated leather', 'Resource', 'Default', NULL, NULL, 50, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 109, 'Teeth', 'The teeth', 'Resource', 'Default', NULL, NULL, 33, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 110, 'Claws ', 'The claws of the beast', 'Resource', 'Default', NULL, NULL, 35, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 111, 'Fragments of Darkness', 'Very dark fragments', 'Resource', 'Default', NULL, NULL, 22, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 112, 'Mushroom extract', 'Mushroom substance', 'Resource', 'Default', NULL, NULL, 16, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 113, 'Bone', 'Someone\'s bone', 'Resource', 'Default', NULL, NULL, 36, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 114, 'Rotten meat', 'Meat that\'s gone bad', 'Resource', 'Default', NULL, NULL, 55, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 115, 'Magic Matter', 'The matter that radiates magic', 'Resource', 'Default', NULL, NULL, 32, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 116, 'Feather', 'The feather of the beast', 'Resource', 'Default', NULL, NULL, 73, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 117, 'Hook parts', 'Hook parts', 'Resource', 'Default', NULL, NULL, 1500, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 118, 'A ring of greed', 'A cursed ring that desires gold', 'Equipment', 'Default', NULL, NULL, NULL, NULL, 20, NULL, 500, NULL, NULL),
(NULL, 119, 'Miner tools', 'Sturdy tools to unearth gems', 'Equipment', 'Default', NULL, NULL, 72750, NULL, 11, NULL, 3, NULL, NULL),
('Мусор?', 120, 'Banner', 'One of the items to create a guild', 'Resource', 'Default', NULL, NULL, 1468, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 121, 'Mana Flower', 'Magic pours out of this flower', 'Resource', 'Resource harvestable', NULL, NULL, 7, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 122, 'Life Flower', 'Life pours out of this flower', 'Resource', 'Resource harvestable', NULL, NULL, 7, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 123, 'Explosives', 'A lot of explosive substances', 'Resource', 'Default', NULL, NULL, 35, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 124, 'Trash', 'Trash', 'Resource', 'Default', NULL, NULL, 9, 1, NULL, NULL, NULL, NULL, NULL),
(NULL, 125, 'Skull', 'Someone\'s skull', 'Resource', 'Default', NULL, NULL, 117, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 126, 'Rage amulet', 'The amulet of rage', 'Equipment', 'Default', NULL, NULL, 28750, NULL, 1, NULL, 1, NULL, NULL),
(NULL, 127, 'Explosive Powder', 'Very explosive', 'Resource', 'Default', NULL, NULL, 111, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 128, 'Spider Egg', 'Spider Egg', 'Resource', 'Default', NULL, NULL, 155, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 129, 'Wooden Crate', 'Wooden Crate', 'Usable', 'Single use x1', NULL, NULL, 99999, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 130, 'Iron crate', 'Iron crate', 'Usable', 'Single use x1', NULL, NULL, 999999, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 131, 'Golden Crate', 'Golden Crate', 'Usable', 'Single use x1', NULL, NULL, 9999999, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 132, 'Beer', 'A little mana boost', 'Usable', 'Single use x1', NULL, NULL, 1000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 2,\r\n    \"duration_minutes\": 15,\r\n    \"type\": 4\r\n  }\r\n}'),
('BotModule', 133, 'SpeedModule', 'SpeedModuleNPC', 'Equipment', 'Default', NULL, NULL, NULL, NULL, 2, NULL, 1000, NULL, NULL),
(NULL, 134, 'Life crystal', 'The life-giving crystal', 'Resource', 'Default', NULL, NULL, 1985, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 135, 'Hoarder bag', 'A bag that is bigger inside', 'Equipment', 'Default', NULL, NULL, 87500, NULL, 6, 18, 200, 134, NULL),
(NULL, 136, 'Aether crystal', 'Aether crystal', 'Resource', 'Default', NULL, NULL, 70, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 137, 'Slime', 'A piece of slime', 'Resource', 'Default', NULL, NULL, 44, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 138, 'Bone meal', 'Bone meal', 'Resource', 'Default', NULL, NULL, 1, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 139, 'WarLord', 'You already have combat experience', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 1, NULL, 2, NULL, NULL),
(NULL, 140, 'Legend of the DM', 'You\'ve been through the whole ordeal of murder.', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 1, 2, 1, 150, NULL),
(NULL, 141, 'The Legendary Traveler', 'You already know and feel this place', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 5, NULL, 100, NULL, NULL),
(NULL, 142, 'Dungeon King', 'You\'re a pain to the dark world', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 5, 7, 100, 100, NULL),
(NULL, 143, 'Tank Lord', 'The armor is a part of you.', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 5, 6, 50, 1655, NULL),
(NULL, 144, 'DMG Lord', 'One touch, destruction', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 1, 2, 2, 75, NULL),
(NULL, 145, 'The Lord Healer', 'The earth before you blooms', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 5, 7, 50, 150, NULL),
(NULL, 146, 'Lord of the Ore', 'You make everything out of nothing', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 18, 11, 1500, 5, NULL),
(NULL, 147, 'Harvest King', 'You know nature better than anyone', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 12, 18, 5, 1500, NULL),
(NULL, 148, 'The Overlord of Damage', 'Nothing but nothing', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 1, 3, 5, 5, NULL),
(NULL, 149, 'King of Gold', 'You\'re respected. And they give you a discount', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 20, NULL, 25000, NULL, NULL),
('Не реализ', 150, 'Tracked Plazma', 'Tracked Plazma', 'Equipment', 'Equip rifle', NULL, NULL, 999999, NULL, 17, NULL, 6, NULL, NULL),
('Не реализ', 151, 'Gun Pulse', 'Gun Pulse', 'Equipment', 'Equip gun', NULL, NULL, NULL, NULL, 14, NULL, 3, NULL, NULL),
(NULL, 152, 'Module parts', 'Assembly components', 'Resource', 'Default', NULL, NULL, 5000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 153, 'Weapons parts', 'Need it for a gun?', 'Resource', 'Default', NULL, NULL, 500, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 167, 'Ancient remains', 'The ancient remains of someone', 'Resource', 'Default', NULL, NULL, 1000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 168, 'Ancient coin', 'The ancient coin', 'Other', 'Default', NULL, NULL, 5000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 169, 'Ancient adornment', 'The ancient adornment', 'Other', 'Default', NULL, NULL, 7000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 170, 'Ancient relic', 'The ancient relic', 'Other', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 171, 'Ancient component', 'The ancient component', 'Other', 'Default', NULL, NULL, 15000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 172, 'Ancient artifact', 'The ancient artifact', 'Other', 'Default', NULL, NULL, 50000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 173, 'Stone', 'Just stone', 'Resource', 'Resource mineable', NULL, NULL, 1, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 174, 'Iron fish rod', 'The usual fishing rod.', 'Equipment', 'Equip fishrod', NULL, NULL, 14940, NULL, 21, NULL, 5, NULL, NULL),
(NULL, 175, 'Crucian Carp', 'Common river fish', 'Resource', 'Resource fishes', NULL, NULL, 5, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 176, 'Northern Pike', 'Aggressive freshwater', 'Resource', 'Resource fishes', NULL, NULL, 7, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 177, 'Perch', 'Sleek and swift swimmer', 'Resource', 'Resource fishes', NULL, NULL, 10, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 178, 'Zander', 'Popular sport catch', 'Resource', 'Resource fishes', NULL, NULL, 12, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 179, 'Bream', 'Smooth, tasty fish', 'Resource', 'Resource fishes', NULL, NULL, 15, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 180, 'Catfish', 'Large, hard-to-catch fish', 'Resource', 'Resource fishes', NULL, NULL, 20, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 181, 'Trout', 'Vibrant, agile swimmer', 'Resource', 'Resource fishes', NULL, NULL, 22, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 182, 'Salmon', 'Premium, rich in flavor', 'Resource', 'Resource fishes', NULL, NULL, 25, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 183, 'Sturgeon', 'Ancient, prized catch', 'Resource', 'Resource fishes', NULL, NULL, 28, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 184, 'Swordfish', 'Dramatic giant hunter', 'Resource', 'Resource fishes', NULL, NULL, 30, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 185, 'Magnet for items', 'Magnetizes items meant for you', 'Equipment', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 186, 'Pig meat', 'The meat of a pig? Old version', 'Resource', 'Default', NULL, NULL, 11, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 187, 'Scavenger', 'Title for DPS', 'Equipment', 'Equip title', NULL, NULL, 7000, NULL, 2, 4, 50, 50, NULL),
(NULL, 188, 'Copper gloves', 'The usual copper loader gloves', 'Equipment', 'Equip gloves', NULL, NULL, 3240, NULL, 22, NULL, 15, NULL, NULL),
(NULL, 189, 'Iron gloves', 'The usual iron loader gloves', 'Equipment', 'Equip gloves', NULL, NULL, 5375, NULL, 22, NULL, 25, NULL, NULL),
(NULL, 190, 'Titanium gloves', 'The usual titanium loader gloves', 'Equipment', 'Equip gloves', NULL, NULL, 21720, NULL, 22, NULL, 50, NULL, NULL),
(NULL, 191, 'Adamantium gloves', 'The usual adamantium loader gloves', 'Equipment', 'Equip gloves', NULL, NULL, 37850, NULL, 22, NULL, 100, NULL, NULL),
(NULL, 192, 'Leather gloves', 'The usual leather loader gloves', 'Equipment', 'Equip gloves', NULL, NULL, 1510, NULL, 22, NULL, 10, NULL, NULL),
(NULL, 193, 'HP 2% 30m', 'Boost HP 2% 30m', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 2,\r\n    \"duration_minutes\": 30,\r\n    \"type\": 3\r\n  }\r\n}'),
(NULL, 194, 'MP 2% 30m', 'Boost MP 2% 30m', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 2,\r\n    \"duration_minutes\": 30,\r\n    \"type\": 4\r\n  }\r\n}'),
(NULL, 195, 'EXP 2% 30m', 'Boost EXP 2% 30m', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 2,\r\n    \"duration_minutes\": 15,\r\n    \"type\": 1\r\n  }\r\n}'),
(NULL, 196, 'GOLD 2% 30m', 'Boost GOLD 2% 30m', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 2,\r\n    \"duration_minutes\": 30,\r\n    \"type\": 2\r\n  }\r\n}'),
(NULL, 197, 'HP 5% 10m', 'Boost HP 5% 10m', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 5,\r\n    \"duration_minutes\": 10,\r\n    \"type\": 3\r\n  }\r\n}'),
(NULL, 198, 'EXP 5% 10m', 'Boost EXP 5% 10m', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 5,\r\n    \"duration_minutes\": 10,\r\n    \"type\": 1\r\n  }\r\n}'),
(NULL, 199, 'GOLD 5% 10m', 'Boost GOLD 5% 10m', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 5,\r\n    \"duration_minutes\": 10,\r\n    \"type\": 2\r\n  }\r\n}'),
(NULL, 200, 'HP 5% 2d', 'Boost HP 5% 2d', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 5,\r\n    \"duration_days\": 2,\r\n    \"type\": 3\r\n  }\r\n}'),
(NULL, 201, 'MP 5% 2d', 'Boost MP 5% 2d', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 5,\r\n    \"duration_days\": 2,\r\n    \"type\": 4\r\n  }\r\n}'),
(NULL, 202, 'EXP 5% 2d', 'Boost EXP 5% 2d', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 5,\r\n    \"duration_days\": 2,\r\n    \"type\": 1\r\n  }\r\n}'),
(NULL, 203, 'GOLD 5% 2d', 'Boost GOLD 5% 2d', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 5,\r\n    \"duration_days\": 2,\r\n    \"type\": 2\r\n  }\r\n}'),
(NULL, 204, 'HP 50% 2m', 'Boost HP 50% 2m', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 50,\r\n    \"duration_minutes\": 2,\r\n    \"type\": 3\r\n  }\r\n}'),
(NULL, 205, 'MP 50% 2m', 'Boost MP 50% 2m', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\n  \"bonus\": {\n    \"amount\": 50,\n    \"duration_minutes\": 2,\n    \"type\": 4\n  }\n}'),
(NULL, 206, 'EXP 50% 2m', 'Boost EXP 50% 2m', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 50,\r\n    \"duration_minutes\": 2,\r\n    \"type\": 1\r\n  }\r\n}'),
(NULL, 207, 'GOLD 50% 2m', 'Boost GOLD 50% 2m', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 50,\r\n    \"duration_minutes\": 2,\r\n    \"type\": 2\r\n  }\r\n}'),
(NULL, 208, 'Kill Gun', 'Improved regular gun', 'Equipment', 'Equip gun', NULL, NULL, 21000, NULL, 14, 6, 4, 1, NULL),
(NULL, 209, 'Burst shotgun', 'Improved burst shotgun', 'Equipment', 'Equip shotgun', NULL, NULL, 70000, NULL, 15, 18, 2, 1, NULL),
(NULL, 210, 'Injury grenade', 'Improved burst grenade', 'Equipment', 'Equip grenade', NULL, NULL, 140000, NULL, 16, 7, 6, 3, NULL),
(NULL, 211, 'Laser damager', 'Improved burst laser', 'Equipment', 'Equip rifle', NULL, NULL, 280000, NULL, 17, 3, 8, 1, NULL),
(NULL, 212, 'Decoration box', 'Decor house (HP/MP)', 'Usable', 'Multiple use x99', NULL, NULL, 1500, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"random_box\": [\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 3,\r\n      \"chance\": 29.41\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 50,\r\n      \"chance\": 35.29\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 150,\r\n      \"chance\": 17.65\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 300,\r\n      \"chance\": 8.63\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 700,\r\n      \"chance\": 1.96\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 5,\r\n      \"chance\": 68.97\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 20,\r\n      \"chance\": 27.59\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 50,\r\n      \"chance\": 3.45\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 13,\r\n      \"chance\": 2\r\n    },\r\n    {\r\n      \"item_id\": 18,\r\n      \"value\": 1,\r\n      \"chance\": 1\r\n    },\r\n    {\r\n      \"item_id\": 13,\r\n      \"value\": 1,\r\n      \"chance\": 1\r\n    }\r\n  ]\r\n}'),
(NULL, 213, 'Decoration chest', 'Decor house (weapons)', 'Usable', 'Multiple use x99', NULL, NULL, 1500, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"random_box\": [\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 3,\r\n      \"chance\": 29.41\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 50,\r\n      \"chance\": 35.29\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 150,\r\n      \"chance\": 17.65\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 300,\r\n      \"chance\": 8.63\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 700,\r\n      \"chance\": 1.96\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 5,\r\n      \"chance\": 68.97\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 20,\r\n      \"chance\": 27.59\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 50,\r\n      \"chance\": 3.45\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 13,\r\n      \"chance\": 2\r\n    },\r\n    {\r\n      \"item_id\": 11,\r\n      \"value\": 1,\r\n      \"chance\": 2\r\n    },\r\n    {\r\n      \"item_id\": 12,\r\n      \"value\": 1,\r\n      \"chance\": 1.5\r\n    },\r\n    {\r\n      \"item_id\": 94,\r\n      \"value\": 1,\r\n      \"chance\": 1\r\n    }\r\n  ]\r\n}'),
(NULL, 214, 'Draft Hammer Lamp', 'Draft Hammer Lamp', 'Other', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 215, 'Draft Wall Pusher', 'Draft Wall Pusher', 'Other', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 216, 'Draft Hammer Blast', 'Draft Hammer Blast', 'Other', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 217, 'Draft Magnetic Pulse', 'Draft Magnetic Pulse', 'Other', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 218, 'Draft Gun Pulse', 'Draft Gun Pulse', 'Other', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 219, 'Draft Pizdamet', 'Draft Pizdamet', 'Other', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 220, 'Draft Tracked Plazma', 'Draft Tracked Plazma', 'Other', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 221, 'Draft box', 'Draft box', 'Usable', 'Multiple use x99', NULL, NULL, 20, NULL, NULL, NULL, NULL, NULL, '{\n    \"random_box\": [\n        {\n            \"item_id\": 1,\n            \"value\": 3,\n            \"chance\": 29.41\n        },\n        {\n            \"item_id\": 1,\n            \"value\": 50,\n            \"chance\": 35.29\n        },\n        {\n            \"item_id\": 1,\n            \"value\": 150,\n            \"chance\": 17.65\n        },\n        {\n            \"item_id\": 1,\n            \"value\": 300,\n            \"chance\": 8.63\n        },\n        {\n            \"item_id\": 1,\n            \"value\": 700,\n            \"chance\": 1.96\n        },\n        {\n            \"item_id\": 7,\n            \"value\": 5,\n            \"chance\": 68.97\n        },\n        {\n            \"item_id\": 7,\n            \"value\": 20,\n            \"chance\": 27.59\n        },\n        {\n            \"item_id\": 7,\n            \"value\": 50,\n            \"chance\": 3.45\n        },\n        {\n            \"item_id\": 7,\n            \"value\": 13,\n            \"chance\": 2\n        },\n        {\n            \"item_id\": 214,\n            \"value\": 1,\n            \"chance\": 1\n        },\n        {\n            \"item_id\": 215,\n            \"value\": 1,\n            \"chance\": 1\n        },\n        {\n            \"item_id\": 216,\n            \"value\": 1,\n            \"chance\": 1\n        },\n        {\n            \"item_id\": 217,\n            \"value\": 1,\n            \"chance\": 1\n        },\n        {\n            \"item_id\": 218,\n            \"value\": 1,\n            \"chance\": 1\n        },\n        {\n            \"item_id\": 219,\n            \"value\": 1,\n            \"chance\": 1\n        },\n        {\n            \"item_id\": 220,\n            \"value\": 1,\n            \"chance\": 1\n        },\n        {\n            \"item_id\": 403,\n            \"value\": 1,\n            \"chance\": 0.05\n        }\n    ]\n}'),
(NULL, 222, 'Homemade rod', 'The usual homemade rod', 'Equipment', 'Equip fishrod', NULL, NULL, 450, NULL, 21, NULL, 1, NULL, NULL),
(NULL, 223, 'Сotton', 'Farm-grown cotton', 'Resource', 'Resource harvestable', NULL, NULL, 6, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 224, 'Sunflower', 'Farm-grown sunflower', 'Resource', 'Resource harvestable', NULL, NULL, 6, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 225, 'Black truffle', 'Farm-grown black truffle', 'Resource', 'Resource harvestable', NULL, NULL, 7, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 226, 'Сloth', 'Cloth', 'Resource', 'Default', NULL, NULL, 525, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 227, 'Oil', 'Vegetable oil', 'Resource', 'Default', NULL, NULL, 1010, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 228, 'Small bag', 'A small pouch for holding coins', 'Equipment', 'Default', NULL, NULL, 36750, NULL, 22, NULL, 50, NULL, NULL),
(NULL, 229, 'Hand-to-hand ring', 'A ring that empowers your fists', 'Equipment', 'Default', NULL, NULL, 188000, NULL, 13, NULL, 2, NULL, NULL),
(NULL, 230, 'Quick amulet', 'An amulet that hastens your step', 'Equipment', 'Default', NULL, NULL, 204900, NULL, 2, NULL, 75, NULL, NULL),
(NULL, 231, 'Laser sight', 'A sight for unerring accuracy', 'Equipment', 'Default', NULL, NULL, NULL, NULL, 17, NULL, 1, NULL, NULL),
(NULL, 232, 'Fishing box', 'A box with every lure you need', 'Equipment', 'Default', NULL, NULL, 43750, NULL, 21, NULL, 2, NULL, NULL),
(NULL, 233, 'Fish scales', 'Fish scales', 'Resource', 'Default', NULL, NULL, 146, NULL, NULL, NULL, NULL, NULL, NULL),
('Power 20', 234, 'Meat armor', 'Meat armor', 'Equipment', 'Equip armor (dps)', NULL, NULL, 2000, NULL, 5, 7, 23, 10, NULL),
('Power 50', 235, 'Warrior armor', 'Warrior armor', 'Equipment', 'Equip armor (dps)', NULL, NULL, 7000, NULL, 5, 7, 56, 25, NULL),
('Power 100', 236, 'Hunter armor', 'Hunter armor', 'Equipment', 'Equip armor (dps)', NULL, NULL, 20000, NULL, 5, 7, 112, 50, NULL),
('Power 150', 237, 'Bersek armor', 'Bersek armor', 'Equipment', 'Equip armor (dps)', NULL, NULL, 40000, NULL, 5, 7, 168, 75, NULL),
('Power 20', 238, 'Plant armor', 'Plant armor', 'Equipment', 'Equip armor (healer)', NULL, NULL, 2000, NULL, 5, 7, 16, 10, NULL),
('Power 50', 239, 'Priests robes', 'Priests robes', 'Equipment', 'Equip armor (healer)', NULL, NULL, 7000, NULL, 5, 7, 40, 24, NULL),
('Power 100', 240, 'Cloak of life', 'Cloak of life', 'Equipment', 'Equip armor (healer)', NULL, NULL, 20000, NULL, 5, 7, 80, 48, NULL),
('Power 150', 241, 'Healer cloak', 'Healer cloak', 'Equipment', 'Equip armor (healer)', NULL, NULL, 40000, NULL, 5, 7, 120, 72, NULL),
(NULL, 242, 'Magic bandage', 'A bandage that seals any wound', 'Equipment', 'Default', NULL, NULL, 14290, NULL, 7, NULL, 25, NULL, NULL),
(NULL, 243, 'Rage belt', 'A belt that fuels your battle fury', 'Equipment', 'Default', NULL, NULL, 151000, NULL, 4, NULL, 50, NULL, NULL),
(NULL, 244, 'Gridans', 'Gridans coins', 'Currency', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 245, 'Warrior', 'Warrior', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 1, NULL, 1, NULL, NULL),
(NULL, 246, 'Guner', 'Guner', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 14, NULL, 1, NULL, NULL),
(NULL, 247, 'ShotGuner', 'ShotGuner', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 15, NULL, 1, NULL, NULL),
(NULL, 248, 'Grenader', 'Grenader', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 16, NULL, 1, NULL, NULL),
(NULL, 249, 'Lasertee', 'Lasertee', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 17, NULL, 1, NULL, NULL),
(NULL, 250, 'Lucker', 'Lucker', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 6, NULL, 1000, NULL, NULL),
(NULL, 251, 'Mugger', 'Mugger', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 20, NULL, 2500, NULL, NULL),
(NULL, 252, 'Fisherman', 'Fisherman', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 21, NULL, 3, NULL, NULL),
(NULL, 253, 'Miner', 'Miner', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 11, NULL, 3, NULL, NULL),
(NULL, 254, 'Farmer', 'Farmer', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 12, NULL, 3, NULL, NULL),
(NULL, 255, 'Murderer', 'Murderer', 'Equipment', 'Equip title', NULL, NULL, NULL, NULL, 4, NULL, 3500, NULL, NULL),
(NULL, 256, 'Pack of cigarettes', 'Reduces jail time', 'Other', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 257, 'Booster box', 'Booster box', 'Usable', 'Multiple use x99', NULL, NULL, 1500, NULL, NULL, NULL, NULL, NULL, '{\n  \"random_box\": [\n    {\n      \"item_id\": 193,\n      \"value\": 1,\n      \"chance\": 60\n    },\n    {\n      \"item_id\": 194,\n      \"value\": 1,\n      \"chance\": 60\n    },\n    {\n      \"item_id\": 195,\n      \"value\": 1,\n      \"chance\": 60\n    },\n    {\n      \"item_id\": 196,\n      \"value\": 1,\n      \"chance\": 60\n    },\n    {\n      \"item_id\": 197,\n      \"value\": 1,\n      \"chance\": 30\n    },\n    {\n      \"item_id\": 198,\n      \"value\": 1,\n      \"chance\": 30\n    },\n    {\n      \"item_id\": 199,\n      \"value\": 1,\n      \"chance\": 30\n    },\n    {\n      \"item_id\": 200,\n      \"value\": 1,\n      \"chance\": 10\n    },\n    {\n      \"item_id\": 201,\n      \"value\": 1,\n      \"chance\": 10\n    },\n    {\n      \"item_id\": 202,\n      \"value\": 1,\n      \"chance\": 10\n    },\n    {\n      \"item_id\": 203,\n      \"value\": 1,\n      \"chance\": 10\n    },\n    {\n      \"item_id\": 204,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 205,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 206,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 207,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 1000,\n      \"chance\": 20\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 500,\n      \"chance\": 40\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 250,\n      \"chance\": 70\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 1000,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 500,\n      \"chance\": 20\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 100,\n      \"chance\": 40\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 50,\n      \"chance\": 70\n    }\n  ]\n}'),
(NULL, 258, 'Title box', 'Title box', 'Usable', 'Multiple use x99', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, '{\n  \"random_box\": [\n    {\n      \"item_id\": 245,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 246,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 247,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 248,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 249,\n      \"value\": 1,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 250,\n      \"value\": 1,\n      \"chance\": 1\n    },\n    {\n      \"item_id\": 251,\n      \"value\": 1,\n      \"chance\": 1\n    },\n    {\n      \"item_id\": 252,\n      \"value\": 1,\n      \"chance\": 1\n    },\n    {\n      \"item_id\": 253,\n      \"value\": 1,\n      \"chance\": 1\n    },\n    {\n      \"item_id\": 254,\n      \"value\": 1,\n      \"chance\": 1\n    },\n    {\n      \"item_id\": 255,\n      \"value\": 1,\n      \"chance\": 1\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 1000,\n      \"chance\": 20\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 500,\n      \"chance\": 40\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 250,\n      \"chance\": 70\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 1000,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 500,\n      \"chance\": 20\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 100,\n      \"chance\": 40\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 50,\n      \"chance\": 70\n    }\n  ]\n}'),
(NULL, 259, 'Scaly armbands', 'Armbands of hardened dragonscale', 'Equipment', 'Default', NULL, NULL, 15300, NULL, 2, NULL, 25, NULL, NULL),
(NULL, 260, 'Slime lining', 'Oozing, acid-proof inner layer', 'Equipment', 'Default', NULL, NULL, 22100, NULL, 5, NULL, 25, NULL, NULL),
(NULL, 261, 'Fiber', 'Title for Healer', 'Equipment', 'Equip title', NULL, NULL, 7000, NULL, 5, 7, 7, 1, NULL),
(NULL, 262, 'Rust', 'Title for Tank', 'Equipment', 'Equip title', NULL, NULL, 7000, NULL, 5, NULL, 8, NULL, NULL),
(NULL, 263, 'Leather shoulderpads', 'Tough, reinforced shoulder guards', 'Equipment', 'Default', NULL, NULL, 5750, NULL, 5, NULL, 10, NULL, NULL),
(NULL, 264, 'HP Potion I', 'Restores health', 'Potion', 'Equip potion HP', NULL, NULL, 1000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 1,\r\n    \"lifetime\": 10,\r\n    \"recast\": 50\r\n    }\r\n}'),
(NULL, 265, 'HP Potion II', 'Restores health', 'Potion', 'Equip potion HP', NULL, NULL, 5000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 3,\r\n    \"lifetime\": 10,\r\n    \"recast\": 50\r\n    }\r\n}'),
(NULL, 266, 'HP Potion III', 'Restores health', 'Potion', 'Equip potion HP', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10,\r\n    \"recast\": 50\r\n    }\r\n}'),
(NULL, 267, 'HP Potion IV', 'Restores health', 'Potion', 'Equip potion HP', NULL, NULL, 50000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 10,\r\n    \"lifetime\": 10,\r\n    \"recast\": 50\r\n    }\r\n}'),
(NULL, 268, 'HP Potion V', 'Restores health', 'Potion', 'Equip potion HP', NULL, NULL, 100000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 20,\r\n    \"lifetime\": 10,\r\n    \"recast\": 50\r\n    }\r\n}'),
(NULL, 269, 'Mana Potion I', 'Restores mana', 'Potion', 'Equip potion MP', NULL, NULL, 1000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny MP\",\r\n    \"value\": 1,\r\n    \"lifetime\": 10,\r\n    \"recast\": 50\r\n    }\r\n}'),
(NULL, 270, 'Mana Potion II', 'Restores mana', 'Potion', 'Equip potion MP', NULL, NULL, 5000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny MP\",\r\n    \"value\": 3,\r\n    \"lifetime\": 10,\r\n    \"recast\": 50\r\n    }\r\n}'),
(NULL, 271, 'Mana Potion III', 'Restores mana', 'Potion', 'Equip potion MP', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny MP\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10,\r\n    \"recast\": 50\r\n    }\r\n}'),
(NULL, 272, 'Mana Potion IV', 'Restores mana', 'Potion', 'Equip potion MP', NULL, NULL, 50000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny MP\",\r\n    \"value\": 10,\r\n    \"lifetime\": 10,\r\n    \"recast\": 50\r\n    }\r\n}'),
(NULL, 273, 'Mana Potion V', 'Restores mana', 'Potion', 'Equip potion MP', NULL, NULL, 100000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny MP\",\r\n    \"value\": 20,\r\n    \"lifetime\": 10,\r\n    \"recast\": 50\r\n    }\r\n}'),
(NULL, 274, 'Copper rod', 'The usual Copper rod', 'Equipment', 'Equip fishrod', NULL, NULL, 9690, NULL, 21, NULL, 3, NULL, NULL),
(NULL, 275, 'Titanium rod', 'The usual Titanium rod', 'Equipment', 'Equip fishrod', NULL, NULL, 43190, NULL, 21, NULL, 7, NULL, NULL),
(NULL, 276, 'Adamantite rod', 'The usual Adamantite rod', 'Equipment', 'Equip fishrod', NULL, NULL, 64690, NULL, 21, NULL, 10, NULL, NULL),
(NULL, 277, 'Moonsteel ore', 'Moonsteel ore', 'Resource', 'Resource mineable', NULL, NULL, 5, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 278, 'Sunbronze ore', 'Sunbronze ore', 'Resource', 'Resource mineable', NULL, NULL, 6, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 279, 'Aquaforged ore', 'Aquaforged ore', 'Resource', 'Resource mineable', NULL, NULL, 7, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 280, 'Elementium ore', 'Elementium ore', 'Resource', 'Resource mineable', NULL, NULL, 8, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 281, 'Moonsteel ingot', 'Moonsteel ingot', 'Resource', 'Default', NULL, NULL, 700, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 282, 'Sunbronze ingot', 'Sunbronze ingot', 'Resource', 'Default', NULL, NULL, 900, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 283, 'Aquaforged ingot', 'Aquaforged ingot', 'Resource', 'Default', NULL, NULL, 1100, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 284, 'Elementium ingot', 'Elementium ingot', 'Resource', 'Default', NULL, NULL, 1300, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 285, 'Crescent Edge', 'A blade shaped like the new moon', 'Equipment', 'Default', NULL, NULL, 99999999, NULL, 2, 4, 100, 500, NULL),
(NULL, 286, 'Moonward Plates', 'Armor that wards off moonlight', 'Equipment', 'Default', NULL, NULL, 9999999, NULL, 5, 7, 80, 45, NULL),
(NULL, 287, 'Moonsteal armor', 'Moonsteal armor', 'Equipment', 'Equip armor (tank)', NULL, NULL, 999999, NULL, 5, 7, 150, 30, NULL),
(NULL, 288, 'Adamager armor', 'Adamager armor', 'Equipment', 'Equip armor (dps)', NULL, NULL, 999999, NULL, 5, 7, 90, 60, NULL),
(NULL, 289, 'Adamler Mantle', 'Adamler Mantle', 'Equipment', 'Equip armor (healer)', NULL, NULL, 999999, NULL, 5, 7, 75, 75, NULL),
(NULL, 290, 'Lunar armor', 'Lunar armor', 'Equipment', 'Equip armor (dps)', NULL, NULL, 999999, NULL, 5, 7, 108, 72, NULL),
(NULL, 291, 'Darkmoon armor', 'Darkmoon armor', 'Equipment', 'Equip armor (healer)', NULL, NULL, 999999, NULL, 5, 7, 90, 90, NULL),
(NULL, 292, 'Ring of the Sun', 'A ring that burns with solar fire', 'Equipment', 'Default', NULL, NULL, 9999999, NULL, 1, 18, 1, 1500, NULL),
(NULL, 293, 'Codex of Sunlight', 'A tome filled with holy light', 'Equipment', 'Default', NULL, NULL, 9999999, NULL, 7, NULL, 150, NULL, NULL),
(NULL, 294, 'Sunbronze armor', 'Sunbronze armor', 'Equipment', 'Equip armor (tank)', NULL, NULL, 999999, NULL, 5, 7, 175, 35, NULL),
(NULL, 295, 'Lightcrit armor', 'Lightcrit armor', 'Equipment', 'Equip armor (dps)', NULL, NULL, 999999, NULL, 5, 7, 126, 84, NULL),
(NULL, 296, 'Bright armor', 'Bright armor', 'Equipment', 'Equip armor (healer)', NULL, NULL, 999999, NULL, 5, 7, 105, 105, NULL),
(NULL, 297, 'Hydro-generator', 'Generates ammo from air', 'Equipment', 'Default', NULL, NULL, 9999999, NULL, 9, NULL, 300, NULL, NULL),
(NULL, 298, 'Amphora of Life', 'A jar that holds healing nectar', 'Equipment', 'Default', NULL, NULL, 9999999, NULL, 5, NULL, 160, NULL, NULL),
(NULL, 299, 'Aquaforged armor', 'Aquaforged armor', 'Equipment', 'Equip armor (tank)', NULL, NULL, 999999, NULL, 6, 7, 200, 40, NULL),
(NULL, 300, 'StormWave armor', 'StormWave armor', 'Equipment', 'Equip armor (dps)', NULL, NULL, 999999, NULL, 5, 7, 144, 96, NULL),
(NULL, 301, 'Armor of Life', 'Armor of Life', 'Equipment', 'Equip armor (healer)', NULL, NULL, 999999, NULL, 5, 7, 120, 120, NULL),
(NULL, 302, 'Elementium Accumulative', 'Stores raw elemental power', 'Equipment', 'Default', NULL, NULL, 9999999, NULL, 3, 4, 1, 4000, NULL),
(NULL, 303, 'Elementium ammo', 'Ammo charged with raw elements', 'Equipment', 'Default', NULL, NULL, 9999999, NULL, 10, NULL, 100, NULL, NULL),
(NULL, 304, 'Elementium armor', 'Elementium armor', 'Equipment', 'Equip armor (tank)', NULL, NULL, 999999, NULL, 5, 7, 225, 45, NULL),
(NULL, 305, 'Absorbing armor', 'Absorbing armor', 'Equipment', 'Equip armor (dps)', NULL, NULL, 999999, NULL, 5, 7, 162, 108, NULL),
(NULL, 306, 'Adaptive armor', 'Adaptive armor', 'Equipment', 'Equip armor (healer)', NULL, NULL, 999999, NULL, 5, 7, 135, 135, NULL),
(NULL, 307, 'Pearls', 'Pearls', 'Resource', 'Default', NULL, NULL, 36, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 308, 'Weapons box', 'Weapons box', 'Usable', 'Multiple use x99', NULL, NULL, 5000, 500, NULL, NULL, NULL, NULL, '{\n  \"random_box\": [\n    {\n      \"item_id\": 2,\n      \"value\": 1,\n      \"chance\": 80\n    },\n    {\n      \"item_id\": 3,\n      \"value\": 1,\n      \"chance\": 75\n    },\n    {\n      \"item_id\": 4,\n      \"value\": 1,\n      \"chance\": 70\n    },\n    {\n      \"item_id\": 5,\n      \"value\": 1,\n      \"chance\": 65\n    },\n    {\n      \"item_id\": 6,\n      \"value\": 1,\n      \"chance\": 60\n    },\n    {\n      \"item_id\": 2,\n      \"value\": 1,\n      \"chance\": 80\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 1000,\n      \"chance\": 20\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 500,\n      \"chance\": 40\n    },\n    {\n      \"item_id\": 1,\n      \"value\": 250,\n      \"chance\": 70\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 250,\n      \"chance\": 5\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 120,\n      \"chance\": 20\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 50,\n      \"chance\": 40\n    },\n    {\n      \"item_id\": 7,\n      \"value\": 12,\n      \"chance\": 70\n    }\n  ]\n}'),
(NULL, 309, 'Module box', 'Module box', 'Usable', 'Multiple use x99', NULL, NULL, 7000, 750, NULL, NULL, NULL, NULL, '{\r\n  \"random_box\": [\r\n    {\r\n      \"item_id\": 90,\r\n      \"value\": 1,\r\n      \"chance\": 6\r\n    },\r\n    {\r\n      \"item_id\": 91,\r\n      \"value\": 1,\r\n      \"chance\": 6\r\n    },\r\n    {\r\n      \"item_id\": 87,\r\n      \"value\": 1,\r\n      \"chance\": 0.5\r\n    },\r\n    {\r\n      \"item_id\": 60,\r\n      \"value\": 1,\r\n      \"chance\": 5\r\n    },\r\n    {\r\n      \"item_id\": 242,\r\n      \"value\": 1,\r\n      \"chance\": 4\r\n    },\r\n    {\r\n      \"item_id\": 259,\r\n      \"value\": 1,\r\n      \"chance\": 3\r\n    },\r\n    {\r\n      \"item_id\": 69,\r\n      \"value\": 1,\r\n      \"chance\": 2\r\n    },\r\n    {\r\n      \"item_id\": 260,\r\n      \"value\": 1,\r\n      \"chance\": 1\r\n    },\r\n    {\r\n      \"item_id\": 126,\r\n      \"value\": 1,\r\n      \"chance\": 0.5\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 1000,\r\n      \"chance\": 20\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 500,\r\n      \"chance\": 40\r\n    },\r\n    {\r\n      \"item_id\": 1,\r\n      \"value\": 250,\r\n      \"chance\": 70\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 250,\r\n      \"chance\": 5\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 125,\r\n      \"chance\": 20\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 25,\r\n      \"chance\": 40\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 12,\r\n      \"chance\": 70\r\n    }\r\n  ]\r\n}'),
(NULL, 310, 'Material Box', 'Material Box', 'Usable', 'Multiple use x99', NULL, NULL, 1000, 250, NULL, NULL, NULL, NULL, '{\r\n  \"random_box\": [\r\n    {\r\n      \"item_id\": 124,\r\n      \"value\": 1,\r\n      \"chance\": 100\r\n    },\r\n    {\r\n      \"item_id\": 124,\r\n      \"value\": 5,\r\n      \"chance\": 80\r\n    },\r\n    {\r\n      \"item_id\": 124,\r\n      \"value\": 10,\r\n      \"chance\": 70\r\n    },\r\n    {\r\n      \"item_id\": 124,\r\n      \"value\": 25,\r\n      \"chance\": 100\r\n    },\r\n    {\r\n      \"item_id\": 124,\r\n      \"value\": 50,\r\n      \"chance\": 60\r\n    },\r\n    {\r\n      \"item_id\": 124,\r\n      \"value\": 100,\r\n      \"chance\": 50\r\n    },\r\n    {\r\n      \"item_id\": 124,\r\n      \"value\": 200,\r\n      \"chance\": 25\r\n    },\r\n    {\r\n      \"item_id\": 124,\r\n      \"value\": 500,\r\n      \"chance\": 10\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 1,\r\n      \"chance\": 80\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 5,\r\n      \"chance\": 60\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 10,\r\n      \"chance\": 50\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 25,\r\n      \"chance\": 50\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 50,\r\n      \"chance\": 40\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 100,\r\n      \"chance\": 20\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 200,\r\n      \"chance\": 10\r\n    },\r\n    {\r\n      \"item_id\": 7,\r\n      \"value\": 500,\r\n      \"chance\": 5\r\n    }\r\n  ]\r\n}'),
(NULL, 311, 'Gun parts', 'Gun parts', 'Resource', 'Default', NULL, NULL, 200, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 312, 'Shotgun parts', 'Shotgun parts', 'Resource', 'Default', NULL, NULL, 390, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 313, 'Grenade parts', 'Grenade parts', 'Resource', 'Default', NULL, NULL, 400, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 314, 'Laser parts', 'Laser parts', 'Resource', 'Default', NULL, NULL, 500, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 315, 'Hammer parts', 'Hammer parts', 'Resource', 'Default', NULL, NULL, 100, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 316, 'Basic Hammer+', 'Increases the radius of the hit', 'Equipment', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 317, 'Draft Hammer+', 'Draft Hammer+', 'Other', 'Default', NULL, NULL, 598, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 318, 'Breathing Reed', 'Double the time underwater', 'Equipment', 'Default', NULL, NULL, 45000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 319, 'Life Preserver', 'Keeps player afloat', 'Equipment', 'Default', NULL, NULL, 25000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 320, 'Diver\'s kit', 'Can\'t drown, keeps player afloat.', 'Equipment', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 321, 'Sunken Fishing Crate', 'Sunken Fishing Crate', 'Usable', 'Multiple use x99', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n	\"random_box\": [\r\n		{\r\n			\"item_id\": 319,\r\n			\"value\": 1,\r\n			\"chance\": 0.5\r\n		},\r\n		{\r\n			\"item_id\": 323,\r\n			\"value\": 1,\r\n			\"chance\": 1\r\n		},\r\n		{\r\n			\"item_id\": 124,\r\n			\"value\": 1,\r\n			\"chance\": 30\r\n		},\r\n		{\r\n			\"item_id\": 124,\r\n			\"value\": 2,\r\n			\"chance\": 15\r\n		},\r\n		{\r\n			\"item_id\": 175,\r\n			\"value\": 3,\r\n			\"chance\": 25\r\n		},\r\n		{\r\n			\"item_id\": 176,\r\n			\"value\": 2,\r\n			\"chance\": 20\r\n		},\r\n		{\r\n			\"item_id\": 177,\r\n			\"value\": 1,\r\n			\"chance\": 15\r\n		},\r\n		{\r\n			\"item_id\": 1,\r\n			\"value\": 25,\r\n			\"chance\": 40\r\n		},\r\n		{\r\n			\"item_id\": 1,\r\n			\"value\": 75,\r\n			\"chance\": 30\r\n		},\r\n		{\r\n			\"item_id\": 7,\r\n			\"value\": 10,\r\n			\"chance\": 40\r\n		},\r\n		{\r\n			\"item_id\": 7,\r\n			\"value\": 25,\r\n			\"chance\": 30\r\n		},\r\n		{\r\n			\"item_id\": 233,\r\n			\"value\": 1,\r\n			\"chance\": 10\r\n		}\r\n	]\r\n}'),
(NULL, 322, 'Old Captain\'s Chest', 'Old Captain\'s Chest', 'Usable', 'Multiple use x99', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\n	\"random_box\": [\n		{\n			\"item_id\": 318,\n			\"value\": 1,\n			\"chance\": 0.5\n		},\n		{\n			\"item_id\": 323,\n			\"value\": 1,\n			\"chance\": 1\n		},\n		{\n			\"item_id\": 168,\n			\"value\": 1,\n			\"chance\": 8\n		},\n		{\n			\"item_id\": 169,\n			\"value\": 1,\n			\"chance\": 5\n		},\n		{\n			\"item_id\": 307,\n			\"value\": 3,\n			\"chance\": 15\n		},\n		{\n			\"item_id\": 1,\n			\"value\": 50,\n			\"chance\": 30\n		},\n		{\n			\"item_id\": 1,\n			\"value\": 100,\n			\"chance\": 10\n		},\n		{\n			\"item_id\": 7,\n			\"value\": 30,\n			\"chance\": 25\n		},\n		{\n			\"item_id\": 7,\n			\"value\": 50,\n			\"chance\": 10\n		},\n		{\n			\"item_id\": 113,\n			\"value\": 2,\n			\"chance\": 20\n		}\n	]\n}'),
('не реализ', 323, 'Fish bait', 'Increases chance of hooking by 30%', 'Equipment', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 324, 'Tesla serpent', 'Creates lightning', 'Equipment', 'Equip rifle', NULL, NULL, NULL, NULL, 17, NULL, 4, NULL, NULL),
(NULL, 325, 'Draft Poison Hook', 'Draft Poison Hook', 'Other', 'Default', NULL, NULL, 2931, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 326, 'Draft Explosion Hook', 'Draft Explosion Hook', 'Other', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 327, 'Draft Spider Hook', 'Draft Spider Hook', 'Other', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 328, 'Draft Explosion Mod', 'Draft Explosion Mod', 'Other', 'Default', NULL, NULL, 10000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 329, '(S) Money Bag', 'Small money bag', 'Equipment', 'Default', NULL, NULL, 1070, NULL, 20, NULL, 500, NULL, NULL),
(NULL, 330, '(M) Money Bag', 'Medium money bag', 'Equipment', 'Default', NULL, NULL, 2875, NULL, 20, NULL, 1000, NULL, NULL),
(NULL, 331, '(B) Money Bag', 'Big money bag', 'Equipment', 'Default', NULL, NULL, 13950, NULL, 20, NULL, 5000, NULL, NULL),
(NULL, 332, '(S) Backpack gold', 'Small backpack gold', 'Equipment', 'Default', NULL, NULL, NULL, NULL, 20, NULL, 10000, NULL, NULL),
(NULL, 333, '(M) Backpack gold', 'Medium backpack gold', 'Equipment', 'Default', NULL, NULL, 55000, NULL, 20, NULL, 50000, NULL, NULL),
(NULL, 334, '(B) Backpack gold', 'Big backpack gold', 'Equipment', 'Default', NULL, NULL, NULL, NULL, 20, NULL, 100000, NULL, NULL),
(NULL, 335, 'Mod Gun 1', 'Increased damage for gun', 'Equipment', 'Default', NULL, NULL, 50000, NULL, 14, NULL, 1, NULL, NULL),
(NULL, 336, 'Mod Shotgun 1', 'Increased damage for shotgun', 'Equipment', 'Default', NULL, NULL, 70000, NULL, 15, NULL, 1, NULL, NULL),
(NULL, 337, 'Mod Grenade 1', 'Increased damage for grenade', 'Equipment', 'Default', NULL, NULL, 100000, NULL, 16, NULL, 1, NULL, NULL),
(NULL, 338, 'Mod Laser 1', 'Increased damage for laser', 'Equipment', 'Default', NULL, NULL, 150000, NULL, 17, NULL, 1, NULL, NULL),
(NULL, 339, 'Pickaxe template', 'Used to craft a pickaxe', 'Resource', 'Default', NULL, NULL, 1000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 340, 'Rake template', 'Used to craft a Rake', 'Resource', 'Default', NULL, NULL, 1000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 341, 'Armor template', 'Used to craft a Armor', 'Resource', 'Default', NULL, NULL, 5000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 342, 'Gloves template', 'Used to craft a Gloves', 'Resource', 'Default', NULL, NULL, 2000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 343, 'Rod template', 'Used to craft a Rod', 'Resource', 'Default', NULL, NULL, 3000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 344, 'Heart of health', 'A gem that pulses with vitality', 'Equipment', 'Default', NULL, NULL, 25000, NULL, 5, NULL, 5, NULL, NULL),
(NULL, 345, 'Mana Shield', 'A barrier of pure arcane force', 'Equipment', 'Default', NULL, NULL, 25000, NULL, 7, NULL, 5, NULL, NULL),
(NULL, 346, 'Chigoe paw', 'A lucky charm from a sand flea', 'Equipment', 'Default', NULL, NULL, 80000, NULL, 18, NULL, 10, NULL, NULL),
(NULL, 347, 'Belt with pockets', 'A belt with many hidden pouches', 'Equipment', 'Default', NULL, NULL, 70000, NULL, 22, NULL, 20, NULL, NULL),
(NULL, 348, 'Mini accelerator', 'Device that grants a burst of regen', 'Equipment', 'Default', NULL, NULL, 100000, NULL, 2, NULL, 15, NULL, NULL),
('Power 20', 349, 'Leather hat', 'Leather hat', 'Equipment', 'Equip helmet (tank)', NULL, NULL, 1000, NULL, 6, NULL, 53, NULL, NULL),
('Power 50', 350, 'Copper Helmet', 'Copper helmet', 'Equipment', 'Equip helmet (tank)', NULL, NULL, 3500, NULL, 6, NULL, 133, NULL, NULL),
('Power 100', 351, 'Iron Helmet', 'Iron helmet', 'Equipment', 'Equip helmet (tank)', NULL, NULL, 10000, NULL, 6, NULL, 266, NULL, NULL),
('Power 150', 352, 'Titanium Helmet', 'Titanium helmet', 'Equipment', 'Equip helmet (tank)', NULL, NULL, 20000, NULL, 6, NULL, 400, NULL, NULL),
('Power 200', 353, 'Adamantium Helmet', 'Adamantium helmet', 'Equipment', 'Equip helmet (tank)', NULL, NULL, 40000, NULL, 6, NULL, 533, NULL, NULL),
('Power 20', 354, 'Meat Hood', 'Meat hood', 'Equipment', 'Equip helmet (dps)', NULL, NULL, 1000, NULL, 2, 4, 4, 30, NULL),
('Power 50', 355, 'Warrior Mask', 'Warrior Mask', 'Equipment', 'Equip helmet (dps)', NULL, NULL, 3500, NULL, 2, 4, 10, 80, NULL),
('Power 100', 356, 'Hunter Helmet', 'Hunter helmet', 'Equipment', 'Equip helmet (dps)', NULL, NULL, 10000, NULL, 2, 4, 20, 150, NULL),
('Power 150', 357, 'Bersek Helmet', 'Bersek helmet', 'Equipment', 'Equip helmet (dps)', NULL, NULL, 20000, NULL, 2, 4, 30, 220, NULL),
('Power 20', 358, 'Plant Mask', 'Plant mask', 'Equipment', 'Equip helmet (healer)', NULL, NULL, 1000, NULL, 8, NULL, 40, NULL, NULL),
('Power 50', 359, 'Priest Hood', 'Priest hood', 'Equipment', 'Equip helmet (healer)', NULL, NULL, 3500, NULL, 8, NULL, 100, NULL, NULL),
('Power 100', 360, 'Wreath of Life', 'Wreath of life', 'Equipment', 'Equip helmet (healer)', NULL, NULL, 10000, NULL, 8, NULL, 200, NULL, NULL),
('Power 150', 361, 'Healer Hood', 'Healer hood', 'Equipment', 'Equip helmet (healer)', NULL, NULL, 20000, NULL, 8, NULL, 300, NULL, NULL),
(NULL, 362, 'Gingerbread of exp', 'A little EXP boost', 'Usable', 'Single use x1', NULL, NULL, 1000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 2,\r\n    \"duration_minutes\": 15,\r\n    \"type\": 1\r\n  }\r\n}'),
(NULL, 363, 'Meat kebabs', 'A little HP boost', 'Usable', 'Single use x1', NULL, NULL, 1000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 2,\r\n    \"duration_minutes\": 15,\r\n    \"type\": 3\r\n  }\r\n}'),
(NULL, 364, 'Golden apple salad', 'A little GOLD boost', 'Usable', 'Single use x1', NULL, NULL, 1000, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 2,\r\n    \"duration_minutes\": 15,\r\n    \"type\": 2\r\n  }\r\n}'),
(NULL, 365, 'Helmet template', 'Used to craft a Helmet', 'Resource', 'Default', NULL, NULL, 2500, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 366, 'Ammo belt', 'Belt of bullets', 'Equipment', 'Default', NULL, NULL, 2500, NULL, 10, NULL, 10, NULL, NULL),
(NULL, 367, 'Small box of ammo', 'Box of bullets', 'Equipment', 'Default', NULL, NULL, 1250, NULL, 10, NULL, 5, NULL, NULL),
(NULL, 368, 'Ammo backpack', 'Backpack of bullets', 'Equipment', 'Default', NULL, NULL, 10000, NULL, 10, NULL, 50, NULL, NULL),
(NULL, 369, 'Reactive enzyme', 'Reactive enzyme', 'Resource', 'Default', NULL, NULL, 62, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 370, 'Heart of acid', 'Heart of acid', 'Resource', 'Default', NULL, NULL, 5000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 371, 'Catnip', 'Cats really like it', 'Resource', 'Default', NULL, NULL, 33, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 372, 'Cats eye', 'Cats eye', 'Resource', 'Default', NULL, NULL, 61, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 373, 'Purr Bag', 'A bag that calms anxious minds', 'Equipment', 'Default', NULL, NULL, 1024, NULL, 5, 19, 25, 12, NULL),
(NULL, 374, 'Meme Fragment', 'Part of something bigger. The spirit of memes is felt.', 'Resource', 'Default', NULL, NULL, 84, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 375, 'Wisdom Stone', 'It is said to contain a bit of Cheems wisdom', 'Equipment', 'Default', NULL, NULL, 1200, NULL, 7, NULL, NULL, 35, NULL),
(NULL, 376, 'Eucalyptus Leaf', 'Eucalyptus Leaf', 'Resource', 'Default', NULL, NULL, 69, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 377, 'Boosted Tea', 'Boosted Tea', 'Usable', 'Single use x1', NULL, NULL, 135, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 4,\r\n    \"duration_minutes\": 10,\r\n    \"type\": 4\r\n  }\r\n}'),
(NULL, 378, 'Protective Totem', 'A small idol that wards off evil', 'Equipment', 'Default', NULL, NULL, 1600, NULL, 5, 19, 30, 50, NULL),
(NULL, 379, 'Cyan Droplet', 'Cyan Droplet', 'Resource', 'Default', NULL, NULL, 77, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 380, 'Energy Booster', 'Energy Booster', 'Usable', 'Single use x1', NULL, NULL, 170, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 4,\r\n    \"duration_minutes\": 15,\r\n    \"type\": 1\r\n  }\r\n}'),
(NULL, 381, 'Coconut shell', 'Coconut shell', 'Resource', 'Default', NULL, NULL, 60, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 382, 'Shell Hammer', 'A hammer made of a giant shell', 'Equipment', 'Default', NULL, NULL, 1250, NULL, 13, NULL, 1, NULL, NULL),
(NULL, 383, 'Improved bullet tips', 'Rounds that can pierce any armor', 'Equipment', 'Default', NULL, NULL, 1250, NULL, 4, NULL, 200, NULL, NULL),
(NULL, 384, 'Dragon scales', 'Dragon scales', 'Resource', 'Default', NULL, NULL, 30, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 385, 'Golden Dragon Scale', 'A scale that resists fire and magic', 'Equipment', 'Default', NULL, NULL, 30, NULL, 6, NULL, 334, NULL, NULL),
(NULL, 386, 'Amulet Fox Luck', 'A charm that grants foxy cunning', 'Equipment', 'Default', NULL, NULL, 2500, NULL, 4, NULL, 400, NULL, NULL),
(NULL, 387, 'Fluff', 'Fluff', 'Resource', 'Default', NULL, NULL, 38, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 388, 'Ring of Return Lightning', 'Chance to reflect harm as lightning', 'Equipment', 'Default', NULL, NULL, 50000, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 389, 'Witch Dust', 'A shimmering powder with mystical powers', 'Resource', 'Default', NULL, NULL, 91, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 390, 'Summoning Orb', 'Element for summoning', 'Resource', 'Default', NULL, NULL, 1250, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 391, 'Tea Leaf', 'Tea Leaf', 'Resource', 'Default', NULL, NULL, 53, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 392, 'Tea', 'Simple tea', 'Usable', 'Single use x1', NULL, NULL, 100, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 4,\r\n    \"duration_minutes\": 10,\r\n    \"type\": 1\r\n  }\r\n}'),
(NULL, 393, 'Chitin', 'Chitin', 'Resource', 'Default', NULL, NULL, 80, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 394, 'Chitinous shield', 'A shield of insectoid plates', 'Equipment', 'Default', NULL, NULL, 1800, NULL, 5, NULL, 80, NULL, NULL),
(NULL, 395, 'Anglers Ring', 'A ring that lures fish to bite', 'Equipment', 'Default', NULL, NULL, 9170, NULL, 21, NULL, 1, NULL, NULL),
(NULL, 396, 'Pumpkin Pie', 'Pumpkin Pie', 'Usable', 'Single use x1', NULL, NULL, 250, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 5,\r\n    \"duration_minutes\": 5,\r\n    \"type\": 4\r\n  }\r\n}'),
(NULL, 397, 'Fish net', 'A net to catch both fish and foe', 'Equipment', 'Default', NULL, NULL, 8300, NULL, 21, NULL, 1, NULL, NULL),
(NULL, 398, 'Harvest basket', 'A basket that never overflows', 'Equipment', 'Default', NULL, NULL, 2500, NULL, 12, NULL, 1, NULL, NULL),
(NULL, 399, '(T) Tome of \"Upgr-Reset\"', 'Skill reset for tank', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 400, '(D) Tome of \"Upgr-Reset\"', 'Skill reset for DPS', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 401, '(H) Tome of \"Upgr-Reset\"', 'Skill reset for Healer', 'Usable', 'Single use x1', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 402, 'Show only function (modules)', 'Setting game', 'Settings', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 403, 'Ring of Giving Lightning', 'Chance to imbue attacks with lightning', 'Equipment', 'Default', NULL, NULL, 45000, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 404, 'Ring of Perfect Lightning', 'Chance to retaliate and attack with lightning', 'Equipment', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
('Не реализ', 405, 'Tesla Inductive Coil', 'Increases electro damage by 25%', 'Equipment', 'Default', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
(NULL, 406, 'GOLD 50% 3d', 'Boost GOLD 50% 3d', 'Usable', 'Single use x1', 'Can\'t droppable,Can\'t tradeable', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\r\n  \"bonus\": {\r\n    \"amount\": 50,\r\n    \"duration_days\": 3,\r\n    \"type\": 2\r\n  }\r\n}'),
(NULL, 407, 'EXP 50% 3d', 'Boost EXP 50% 3d', 'Usable', 'Single use x1', 'Can\'t droppable,Can\'t tradeable', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '{\n  \"bonus\": {\n    \"amount\": 50,\n    \"duration_days\": 3,\n    \"type\": 1\n  }\n}');

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
-- Table structure for table `tw_quests_board_list`
--

CREATE TABLE `tw_quests_board_list` (
  `ID` int(11) NOT NULL,
  `QuestID` int(11) NOT NULL,
  `DailyBoardID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Dumping data for table `tw_quests_board_list`
--

INSERT INTO `tw_quests_board_list` (`ID`, `QuestID`, `DailyBoardID`) VALUES
(1, 25, 1),
(6, 30, 1),
(11, 35, 1),
(16, 40, 1),
(17, 42, 3),
(18, 43, 3),
(19, 44, 3),
(20, 45, 3),
(21, 46, 3),
(22, 47, 3),
(23, 48, 3),
(24, 49, 3),
(25, 50, 3),
(26, 51, 3),
(27, 52, 2),
(28, 53, 2),
(29, 54, 2),
(30, 55, 2),
(31, 56, 2),
(32, 57, 1),
(33, 62, 2),
(34, 59, 2),
(35, 60, 2),
(36, 61, 2),
(38, 63, 2),
(39, 64, 2),
(40, 65, 1),
(41, 66, 1),
(42, 67, 1),
(43, 68, 1),
(44, 69, 1),
(45, 70, 1),
(46, 71, 1),
(47, 72, 1);

-- --------------------------------------------------------

--
-- Table structure for table `tw_quests_list`
--

CREATE TABLE `tw_quests_list` (
  `ID` int(11) NOT NULL,
  `NextQuestID` int(11) DEFAULT NULL,
  `Name` varchar(24) NOT NULL DEFAULT 'Quest name',
  `Money` int(11) NOT NULL,
  `Exp` int(11) NOT NULL,
  `Flags` set('Type main','Type side','Type daily','Type weekly','Type repeatable','Can''t refuse','No activity point','Tutorial') NOT NULL DEFAULT 'Type main'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_quests_list`
--

INSERT INTO `tw_quests_list` (`ID`, `NextQuestID`, `Name`, `Money`, `Exp`, `Flags`) VALUES
(1, 2, 'First quest', 0, 0, 'Type main,Can\'t refuse','Tutorial'),
(2, 3, 'Coming to Gridania', 5, 10, 'Type main,Can\'t refuse'),
(3, 4, 'Close to Home', 10, 50, 'Type main,Can\'t refuse'),
(4, 5, 'To the Bannock', 10, 10, 'Type main,Can\'t refuse'),
(5, 6, 'Passing Muster', 0, 10, 'Type main,Can\'t refuse'),
(6, 7, 'Chasing Shadows', 50, 100, 'Type main,Can\'t refuse'),
(7, 8, 'Eggs over Queasy', 100, 80, 'Type main,Can\'t refuse'),
(8, 9, 'Surveying the Damage', 200, 300, 'Type main,Can\'t refuse'),
(9, 10, 'A Soldier\'s Breakfast', 500, 300, 'Type main,Can\'t refuse'),
(10, 11, 'Spirithold Broken', 10, 20, 'Type main,Can\'t refuse'),
(11, 12, 'On to Bentbranch', 50, 150, 'Type main,Can\'t refuse'),
(12, 13, 'You Shall Not Trespass', 60, 170, 'Type main,Can\'t refuse'),
(13, 14, 'Don\'t Look Down', 0, 0, 'Type main,Can\'t refuse'),
(14, 15, 'Darkness of the Forest', 250, 250, 'Type main,Can\'t refuse'),
(15, 16, 'Threat Level Elevated', 25, 50, 'Type main,Can\'t refuse'),
(16, 17, 'Migrant Marauders', 500, 700, 'Type main,Can\'t refuse'),
(17, 18, 'A Hearer Is Often Late', 25, 50, 'Type main,Can\'t refuse'),
(18, 19, 'Salvaging the Scene', 800, 300, 'Type main,Can\'t refuse'),
(19, 20, 'Leia\'s Legacy', 400, 800, 'Type main,Can\'t refuse'),
(20, 21, 'Dread Is in the Air', 50, 30, 'Type main,Can\'t refuse'),
(21, 22, 'To Guard a Guardian', 100, 50, 'Type main,Can\'t refuse'),
(22, 23, 'Festive Endeavors', 50, 50, 'Type main,Can\'t refuse'),
(23, 24, 'Renewing the Covenant', 20, 50, 'Type main,Can\'t refuse'),
(24, NULL, 'The Gridanian Envoy', 0, 0, 'Type main,Can\'t refuse'),
(25, 26, 'Archer | Need eggs!', 50, 50, 'Type weekly'),
(26, 27, 'Archer | The feathers?', 50, 50, 'Type weekly'),
(27, 28, 'Archer | Beast mortality', 50, 50, 'Type weekly'),
(28, 29, 'Archer | Protect us', 50, 50, 'Type weekly'),
(29, NULL, 'Archer | Iron tip', 50, 50, 'Type weekly'),
(30, 31, 'Lancer | Wild hunt', 50, 50, 'Type weekly'),
(31, 32, 'Lancer | Ghost vs', 50, 50, 'Type weekly'),
(32, 33, 'Lancer | Dynamite hunt', 50, 50, 'Type weekly'),
(33, 34, 'Lancer | Hate mushrooms', 50, 50, 'Type weekly'),
(34, NULL, 'Lancer | Armor boy', 50, 50, 'Type weekly'),
(35, 36, 'Wizard | Dark shards', 50, 50, 'Type weekly'),
(36, 37, 'Wizard | Teleport power', 50, 50, 'Type weekly'),
(37, 38, 'Wizard | Illusion tee?', 50, 50, 'Type weekly'),
(38, 39, 'Wizard | Puppeteer?', 50, 50, 'Type weekly'),
(39, NULL, 'Wizard | Zombie killer', 50, 50, 'Type weekly'),
(40, NULL, 'D | Population Control', 50, 50, 'Type daily'),
(42, NULL, 'Farmer | Need more wheat', 200, 20, 'Type weekly'),
(43, NULL, 'Farmer | And the stone?', 500, 150, 'Type weekly'),
(44, NULL, 'Farmer | Bring coal', 300, 50, 'Type weekly'),
(45, NULL, 'Farmer | Need for iron', 25000, 300, 'Type weekly'),
(46, NULL, 'Farmer | I want berries', 600, 200, 'Type weekly'),
(47, NULL, 'Farmer | Put it in order', 10, 30, 'Type daily'),
(48, NULL, 'Farmer | Reconnaissance', 10, 10, 'Type side'),
(49, NULL, 'Farmer | Plant a crop', 50, 10, 'Type weekly'),
(50, NULL, 'Farmer | Harvest', 10, 10, 'Type side'),
(51, NULL, 'Farmer | Pests', 50, 100, 'Type daily'),
(52, NULL, 'Miner | Reconnaissance', 30, 10, 'Type side'),
(53, NULL, 'Miner | Rock stone', 80, 10, 'Type side'),
(54, NULL, 'Miner | Need food', 100, 10, 'Type side'),
(55, NULL, 'Miner | Iron contract', 300, 50, 'Type weekly'),
(56, NULL, 'Miner | Annihilation', 120, 20, 'Type daily'),
(57, NULL, 'R | Meat fever', 105, 20, 'Type repeatable'),
(58, NULL, 'Jail | Correctional', 0, 0, 'Type repeatable,No activity point'),
(59, NULL, 'Miner | Excavations', 100, 50, 'Type daily'),
(60, NULL, 'Miner | Find items', 30, 60, 'Type weekly'),
(61, NULL, 'Miner | Wreckage', 30, 20, 'Type daily'),
(62, NULL, 'Miner | Spider hive', 100, 150, 'Type weekly'),
(63, NULL, 'Miner | My ore', 200, 50, 'Type weekly'),
(64, NULL, 'Miner | Black guy', 50, 80, 'Type weekly'),
(65, NULL, 'R | Fish is in fashion', 100, 100, 'Type repeatable'),
(66, NULL, 'S | Visit homes', 100, 50, 'Type side'),
(67, NULL, 'S | Sky islands', 50, 150, 'Type side'),
(68, NULL, 'S | Auction, huh?', 50, 10, 'Type side'),
(69, NULL, 'D | Trees of life', 20, 10, 'Type daily'),
(70, NULL, 'W | Miners parcel', 250, 30, 'Type weekly'),
(71, NULL, 'Wizard | Magic day', 500, 30, 'Type daily'),
(72, NULL, 'Lancer | Dangerous sea', 1500, 30, 'Type weekly');

-- --------------------------------------------------------

--
-- Table structure for table `tw_quest_boards`
--

CREATE TABLE `tw_quest_boards` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Dumping data for table `tw_quest_boards`
--

INSERT INTO `tw_quest_boards` (`ID`, `Name`, `PosX`, `PosY`, `WorldID`) VALUES
(1, 'Guild Tasks', 19665, 3185, 1),
(2, 'Miner Tasks', 22432, 5856, 1),
(3, 'Farmer Tasks', 15200, 1056, 1);

-- --------------------------------------------------------

--
-- Table structure for table `tw_skills_list`
--

CREATE TABLE `tw_skills_list` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL,
  `Description` varchar(64) NOT NULL,
  `ManaCostPct` int(11) NOT NULL DEFAULT 10,
  `PriceSP` int(11) NOT NULL,
  `Passive` tinyint(1) NOT NULL DEFAULT 0,
  `ProfessionID` int(11) NOT NULL DEFAULT -1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_skills_list`
--

INSERT INTO `tw_skills_list` (`ID`, `Name`, `Description`, `ManaCostPct`, `PriceSP`, `Passive`, `ProfessionID`) VALUES
(1, 'Health turret', 'Creates turret a recovery health ', 25, 28, 0, 2),
(2, 'Sleepy Gravity', 'Magnet mobs to itself', 25, 28, 0, 0),
(3, 'Master craftsman', 'Improvements that enable you to become a master', 0, 25, 1, -1),
(4, 'Master of weapons', 'Improving weapon proficiency', 0, 10, 1, -1),
(5, 'Blessing of God of war', 'The blessing restores ammo', 50, 28, 0, 1),
(6, 'Attack Teleport', 'An attacking teleport that deals damage to all mobs radius', 10, 100, 0, 1),
(7, 'Cure', 'Restores HP all nearby target\'s.', 5, 10, 0, -1),
(8, 'Provoke', 'Aggresses mobs in case of weak aggression', 30, 40, 0, 0),
(9, 'Last Stand', 'Enters mana damage dampening mode', 25, 40, 0, 0),
(10, 'Magic Bow', 'Entering magic bow mode', 30, 40, 0, 2),
(11, 'Healing aura', 'Creates an aura that restores health.', 30, 80, 0, 2),
(12, 'Flame Wall', 'Slows down and deals damage.', 30, 80, 0, 1),
(13, 'Healing Rift', 'Attacks and heals.', 50, 200, 0, 2);

-- --------------------------------------------------------

--
-- Table structure for table `tw_skills_tree`
--

CREATE TABLE `tw_skills_tree` (
  `ID` int(11) NOT NULL,
  `SkillID` int(11) NOT NULL,
  `LevelIndex` int(11) NOT NULL,
  `OptionIndex` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL,
  `Description` varchar(255) NOT NULL,
  `ModType` int(11) NOT NULL,
  `ModValue` int(11) NOT NULL,
  `PriceSP` int(11) DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Dumping data for table `tw_skills_tree`
--

INSERT INTO `tw_skills_tree` (`ID`, `SkillID`, `LevelIndex`, `OptionIndex`, `Name`, `Description`, `ModType`, `ModValue`, `PriceSP`) VALUES
(1, 3, 1, 1, 'Discount', 'Increases the discount', 5, 1, 30),
(2, 3, 2, 1, 'Discount', 'Increases the discount', 5, 3, 50),
(3, 3, 3, 1, 'Discount', 'Increases the discount', 5, 10, 80),
(4, 3, 3, 2, 'Extra item', 'Chance to get extra item', 10, 5, 80),
(5, 3, 4, 1, 'Discount', 'Increases the discount', 5, 20, 120),
(6, 3, 4, 2, 'Extra item', 'Chance to get extra item', 10, 10, 120),
(7, 1, 1, 1, 'Cast click', 'Reduces number of cast click', 3, 2, 20),
(8, 1, 1, 2, 'Lifetime', 'Increase lifetime', 4, 1, 20),
(9, 1, 2, 1, 'Cast click', 'Reduces number of cast click', 3, 3, 40),
(10, 1, 2, 2, 'Healing', 'Increase healing', 5, 5, 40),
(11, 1, 3, 1, 'Lifetime', 'Increase lifetime', 4, 3, 60),
(12, 1, 3, 2, 'Healing', 'Increase healing', 5, 10, 40),
(13, 1, 4, 1, 'Lifetime', 'Increase lifetime', 4, 4, 120),
(14, 1, 4, 2, 'Healing', 'Increase healing', 5, 15, 120),
(15, 1, 4, 3, 'Cast click', 'Reduces number of cast click', 3, 6, 120),
(16, 2, 1, 1, 'Radius', 'Increase radius', 2, 20, 20),
(17, 2, 2, 1, 'Lifetime', 'Increase lifetime', 4, 2, 50),
(18, 2, 2, 2, 'Radius', 'Increase radius', 2, 40, 50),
(19, 2, 3, 1, 'Lifetime', 'Increase lifetime', 4, 4, 100),
(20, 2, 3, 2, 'Radius', 'Increase radius', 2, 80, 100),
(21, 5, 1, 1, 'Radius', 'Increase radius', 2, 20, 20),
(22, 5, 2, 1, 'Radius', 'Increase radius', 2, 100, 50),
(23, 5, 2, 2, 'Restore ammo', 'Increase restored ammo', 5, 10, 50),
(24, 5, 3, 1, 'Radius', 'Increase radius', 2, 200, 120),
(25, 5, 3, 2, 'Restore ammo', 'Increase restored ammo', 5, 20, 120),
(26, 6, 1, 1, 'Combo', 'Unlock combo attack teleport', 11, 1, 80),
(27, 6, 2, 1, 'Stun', 'Attacks has Stun effect', 12, 1, 120),
(28, 6, 2, 2, 'Fire', 'Attacks has Fire effect', 13, 1, 120),
(29, 6, 2, 3, 'Health', 'Attacks can restore health', 14, 1, 120),
(30, 7, 1, 1, 'Ally', 'Restores health to allies as well', 15, 1, 50),
(31, 7, 2, 1, 'Radius', 'Increase radius', 2, 100, 60),
(32, 7, 2, 2, 'Healing', 'Increase healing', 5, 20, 60),
(33, 8, 1, 1, 'Radius', 'Increase radius', 2, 100, 60),
(34, 8, 1, 2, 'Aggression', 'Increase aggression', 6, 50, 60),
(35, 8, 2, 1, 'Radius', 'Increase radius', 2, 200, 100),
(36, 8, 2, 2, 'Aggression', 'Increase aggression', 6, 100, 100),
(37, 8, 1, 2, 'Aggression', 'Increase aggression', 6, 50, 60),
(38, 9, 1, 1, 'Mana cost', 'Reduces mana cost', 1, 5, 40),
(40, 10, 1, 1, 'Shots', 'Increases the number of shots', 6, 1, 20),
(41, 10, 1, 2, 'Radius of damage', 'Increases radius of damage', 2, 40, 20),
(42, 10, 2, 1, 'Shots', 'Increases the number of shots', 6, 2, 50),
(43, 10, 2, 2, 'Radius of damage', 'Increases radius of damage', 2, 80, 50),
(44, 11, 1, 1, 'Cast click', 'Reduces number of cast click', 3, 2, 20),
(45, 11, 1, 2, 'Lifetime', 'Increase lifetime', 4, 1, 20),
(46, 11, 2, 1, 'Cast click', 'Reduces number of cast click', 3, 3, 40),
(47, 11, 2, 2, 'Healing', 'Increase healing', 5, 5, 40),
(48, 11, 3, 1, 'Lifetime', 'Increase lifetime', 4, 3, 60),
(49, 11, 3, 2, 'Healing', 'Increase healing', 5, 10, 40),
(50, 11, 4, 1, 'Lifetime', 'Increase lifetime', 4, 4, 120),
(51, 11, 4, 2, 'Healing', 'Increase healing', 5, 15, 120),
(52, 11, 4, 3, 'Cast click', 'Reduces number of cast click', 3, 6, 120),
(53, 13, 1, 1, 'Cast click', 'Reduces number of cast click', 3, 2, 20),
(54, 13, 1, 2, 'Lifetime', 'Increase lifetime', 4, 1, 20),
(55, 13, 2, 1, 'Cast click', 'Reduces number of cast click', 3, 3, 40),
(56, 13, 2, 2, 'Healing', 'Increase healing', 5, 5, 40),
(57, 13, 3, 1, 'Lifetime', 'Increase lifetime', 4, 3, 60),
(58, 13, 3, 2, 'Healing', 'Increase healing', 5, 10, 40),
(59, 13, 4, 1, 'Lifetime', 'Increase lifetime', 4, 4, 120),
(60, 13, 4, 2, 'Healing', 'Increase healing', 5, 15, 120),
(61, 13, 4, 3, 'Cast click', 'Reduces number of cast click', 3, 6, 120),
(62, 4, 1, 1, 'Auto fire', 'Allows auto-attack to be used', 16, 1, 100);

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
  `Type` set('buying','selling','storage') NOT NULL DEFAULT 'buying',
  `Trades` longtext DEFAULT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `StorageData` longtext DEFAULT NULL,
  `Currency` int(11) NOT NULL DEFAULT 1,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `tw_warehouses`
--

INSERT INTO `tw_warehouses` (`ID`, `Name`, `Type`, `Trades`, `PosX`, `PosY`, `StorageData`, `Currency`, `WorldID`) VALUES
(1, 'Weapons Shop', 'buying,storage', '*Weapons:Basic\r\n[2/1(200)]\r\n[3/1(400)]\r\n[4/1(780)]\r\n[5/1(800)]\r\n[6/1(1000)]\r\n\r\n*Modules:Active\r\n[97/1(80000)]\r\n\r\n*Modules:Ammo\r\n[367/1(50000)]\r\n[366/1(100000)]\r\n[90/1(250000)]\r\n[368/1(500000)]\r\n\r\n*Modules:Weapons Mods\r\n[335/1(50000)]\r\n[336/1(70000)]\r\n[337/1(100000)]\r\n[338/1(250000)]\r\n\r\n*Resources\r\n[153/1(500)]\r\n[117/1(1500)]\r\n[152/1(5000)]\r\n[123/1(2500)]', 21003, 3883, '{\"position\":{\"x\":21067.0,\"y\":3606.0},\"value\":\"26\"}', 1, 1),
(2, 'Tavern', 'buying,storage', '*Food / Boosters\r\n[132/1(1000)]\r\n[362/1(5000)]\r\n[363/1(1500)]\r\n[364/1(2500)]\r\n\r\n*Potions:HP-related\r\n[COLLECTION(Potion:Equip potion HP)]\r\n\r\n*Potions:MP-related\r\n[COLLECTION(Potion:Equip potion MP)]', 22528, 2880, '{\"position\":{\"x\":23053.0,\"y\":2970.0},\"value\":\"589\"}', 1, 1),
(3, 'Mining Store', 'selling,storage', '*Resources:Mine\r\n[173/1(1/1)]\r\n[50/1(2/1)]\r\n[35/1(3/1)]\r\n[54/1(4/1)]\r\n[55/1(5/1)]\r\n[56/1(5/1)]\r\n[277/1(6/1)]\r\n[278/1(7/1)]\r\n[279/1(8/1)]\r\n[280/1(9/1)]\r\n\r\n*Resources:Rare\r\n[167/1(500/20)]\r\n[168/1(2500/20)]\r\n[169/1(3500/20)]\r\n[170/1(5000/20)]\r\n[171/1(7500/20)]\r\n[172/1(25000/20)]\r\n\r\n*Pickaxe\r\n[COLLECTION(Equipment:Equip pickaxe)]', 21728, 5984, '{\"position\":{\"x\":21895.0,\"y\":5616.0},\"value\":\"2585\"}', 1, 1),
(4, 'Farm Store', 'selling,storage', '*Farming\r\n[70/1(1/1)]\r\n[58/1(2/1)]\r\n[61/1(3/1)]\r\n[67/1(4/1)]\r\n[68/1(5/1)]\r\n[71/1(6/1)]\r\n[223/1(6/1)]\r\n[224/1(6/1)]\r\n[225/1(7/1)]\r\n[121/1(7/1)]\r\n[122/1(7/1)]\r\n\r\n*Mobs\r\n[186/5(1/1)]\r\n[22/5(10/1)]\r\n[33/5(15/1)]\r\n[137/5(80/1)]\r\n[128/5(80/1)]\r\n\r\n*Rake\r\n[COLLECTION(Equipment:Equip rake)]', 15040, 1280, '{\"position\":{\"x\":15696.0,\"y\":1137.0},\"value\":\"3853755\"}', 1, 1),
(9, 'Fish Seller', 'selling,storage', '*Resources\r\n[COLLECTION(Resource:Resource fishes)]\r\n\r\n*Fishrod\r\n[COLLECTION(Equipment:Equip fishrod)]', 28476, 2161, '{\"position\":{\"x\":28476.0,\"y\":1991.0},\"value\":\"8983\"}', 1, 1),
(10, 'Activity Shop', 'buying', '*Usable:Items\n[16/1(10)]\n[17/1(10)]\n\n*Usable:Boxes\n[77/1(50)]\n[213/1(100)]\n[257/1(180)]\n[258/1(250)]\n\n*Weapons:Improved Basic\n[208/1(11700)]\n[209/1(18000)]\n[210/1(25000)]\n[211/1(40000)]\n\n*Modules:Active\n[96/1(10000)]\n\n*Other\n[95/1(5000)]\n[399/1(1000)]\n[400/1(1000)]\n[401/1(1000)]', 16960, 3697, NULL, 29, 1),
(11, 'Achievement Shop', 'buying', '*Miscellaneous\n[79/1(99915)]', 27034, 1585, NULL, 10, 1),
(12, 'Donate Shop', 'buying', '*Food / Boosters\n[193/1(5)]\n[194/1(5)]\n[195/1(5)]\n[196/1(5)]\n[197/1(5)]\n[198/1(5)]\n[199/1(5)]\n[200/1(5)]\n[201/1(5)]\n[202/1(6)]\n[203/1(6)]\n[204/1(6)]\n[205/1(6)]\n[206/1(7)]\n[207/1(7)]\n\n*Modules:Active\n[19/1(50)]\n[20/1(55)]\n[185/1(80)]\n[96/1(80)]\n\n*Modules:Universal\n[49/1(1)]\n[51/1(10)]\n[52/1(50)]\n[85/1(10)]\n[86/1(10)]\n[87/1(10)]\n\n*Weapons:Improved Basic\n[53/1(60)]\n\n*Weapons:Special\n[99/1(50)]\n[100/1(70)]\n[101/1(60)]\n[102/1(50)]\n[103/1(55)]\n[150/1(60)]\n[151/1(60)]\n[324/1(65)]\n\n*Usable\n[95/1(5)]', 20706, 2225, NULL, 244, 1),
(13, 'Shop', 'buying,storage', '*Usable\r\n[77/1(1000)]\r\n[310/1(1000)]\r\n[308/1(5000)]\r\n[309/1(7000)]\r\n[257/1(50000)]\r\n\r\n*Modules:Gold\r\n[329/1(1500)]\r\n[330/1(15000)]\r\n[333/1(1000000)]\r\n[332/1(500000)]\r\n\r\n*Modules:HP\r\n[344/1(50000)]\r\n\r\n*Modules:MP\r\n[345/1(50000)]\r\n\r\n*Modules:Lucky Drop\r\n[346/1(160000)]\r\n\r\n*Modules:Product Capacity\r\n[347/1(140000)]\r\n\r\n*Modules:Attack SPD\r\n[348/1(200000)]\r\n\r\n*Resources:Templates\r\n[339/1(1000)]\r\n[340/1(1000)]\r\n[341/1(5000)]\r\n[342/1(2000)]\r\n[343/1(3000)]\r\n[365/1(2500)]\r\n\r\n*Resources:Other\r\n[108/1(900)]\r\n[124/1(50)]', 11168, 3840, '{\"position\":{\"x\":11300.0,\"y\":3600.0},\"value\":\"37\"}', 1, 1);

-- --------------------------------------------------------

--
-- Table structure for table `tw_worlds`
--

CREATE TABLE `tw_worlds` (
  `ID` int(11) NOT NULL,
  `Name` varchar(256) NOT NULL,
  `Path` varchar(256) NOT NULL,
  `Type` enum('default','dungeon','tutorial','deep_dungeon','treasure_dungeon','pvp','mini_games','rhythm') NOT NULL,
  `Flags` set('rating_system','crime_score','lost_gold_death','spawn_full_mana','allowed_pvp') DEFAULT NULL,
  `RespawnWorldID` int(11) NOT NULL,
  `JailWorldID` int(11) NOT NULL,
  `RequiredLevel` int(11) NOT NULL DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Dumping data for table `tw_worlds`
--

INSERT INTO `tw_worlds` (`ID`, `Name`, `Path`, `Type`, `Flags`, `RespawnWorldID`, `JailWorldID`, `RequiredLevel`) VALUES
(0, 'Tutorilishe', 'tutorial.map', 'tutorial', NULL, 0, 0, 1),
(1, 'Gridania', 'main.map', 'default', 'rating_system,crime_score,lost_gold_death,allowed_pvp', 1, 1, 1),
(2, 'Resonance (TEST)', 'dungeons/resonance.map', 'dungeon', 'spawn_full_mana', 2, 1, 1),
(3, 'Default - dm1 (TEST)', 'pvp/dm1-mmorpg.map', 'pvp', 'rating_system,lost_gold_death,spawn_full_mana,allowed_pvp', 3, 1, 20),
(4, 'Default - dm9 (TEST)', 'pvp/dm9-mmorpg.map', 'pvp', 'rating_system,lost_gold_death,spawn_full_mana,allowed_pvp', 4, 1, 20),
(5, 'Sea', 'worlds/sea.map', 'default', 'rating_system,crime_score,lost_gold_death,allowed_pvp', 1, 1, 5),
(6, 'Forest', 'worlds/Forest.map', 'default', 'rating_system,lost_gold_death,allowed_pvp', 1, 1, 15);

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
(1, 0, 3057, 1157, 1, 19699, 4309),
(2, 1, 10705, 3985, 6, 288, 6464),
(3, 6, 48, 6400, 1, 10673, 4017);

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
-- Indexes for table `enum_professions`
--
ALTER TABLE `enum_professions`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `ID` (`ID`);

--
-- Indexes for table `tw_accounts`
--
ALTER TABLE `tw_accounts`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `Password` (`Password`),
  ADD KEY `Username` (`Username`);

--
-- Indexes for table `tw_accounts_achievements`
--
ALTER TABLE `tw_accounts_achievements`
  ADD UNIQUE KEY `AccountAchievement` (`AccountID`,`AchievementType`,`Criteria`),
  ADD KEY `AccountID` (`AccountID`);

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
  ADD KEY `tw_accounts_data_ibfk_3` (`WorldID`),
  ADD KEY `ProfessionID` (`ProfessionID`);

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
  ADD KEY `OwnerID` (`UserID`);

--
-- Indexes for table `tw_accounts_professions`
--
ALTER TABLE `tw_accounts_professions`
  ADD PRIMARY KEY (`ID`);

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
-- Indexes for table `tw_accounts_skill_tree`
--
ALTER TABLE `tw_accounts_skill_tree`
  ADD PRIMARY KEY (`UserID`,`SkillID`,`LevelIndex`);

--
-- Indexes for table `tw_achievements`
--
ALTER TABLE `tw_achievements`
  ADD PRIMARY KEY (`ID`);

--
-- Indexes for table `tw_aethers`
--
ALTER TABLE `tw_aethers`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Indexes for table `tw_attributes`
--
ALTER TABLE `tw_attributes`
  ADD PRIMARY KEY (`ID`);

--
-- Indexes for table `tw_auction_slots`
--
ALTER TABLE `tw_auction_slots`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `OwnerID` (`OwnerID`),
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
  ADD KEY `Effect` (`Debuffs`),
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
  ADD KEY `QuestID` (`QuestID`),
  ADD KEY `WorldID` (`WorldID`);

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
  ADD KEY `Seconds` (`Time`);

--
-- Indexes for table `tw_groups`
--
ALTER TABLE `tw_groups`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_groups_ibfk_1` (`OwnerUID`);

--
-- Indexes for table `tw_guilds`
--
ALTER TABLE `tw_guilds`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`LeaderUID`),
  ADD KEY `Bank` (`Bank`(768)),
  ADD KEY `Level` (`Level`),
  ADD KEY `Experience` (`Exp`);

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
  ADD KEY `WorldID` (`WorldID`);

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
  ADD KEY `ItemBonus` (`AT1`),
  ADD KEY `ItemID_2` (`ID`),
  ADD KEY `tw_items_list_ibfk_5` (`AT2`);

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
-- Indexes for table `tw_quests_board_list`
--
ALTER TABLE `tw_quests_board_list`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_quests_board_list_ibfk_1` (`DailyBoardID`),
  ADD KEY `tw_quests_board_list_ibfk_2` (`QuestID`);

--
-- Indexes for table `tw_quests_list`
--
ALTER TABLE `tw_quests_list`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `NextQuestID` (`NextQuestID`);

--
-- Indexes for table `tw_quest_boards`
--
ALTER TABLE `tw_quest_boards`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Indexes for table `tw_skills_list`
--
ALTER TABLE `tw_skills_list`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ProfessionID` (`ProfessionID`);

--
-- Indexes for table `tw_skills_tree`
--
ALTER TABLE `tw_skills_tree`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `SkillID` (`SkillID`);

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
  ADD KEY `UserID` (`UserID`);

--
-- Indexes for table `tw_warehouses`
--
ALTER TABLE `tw_warehouses`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `Currency` (`Currency`);

--
-- Indexes for table `tw_worlds`
--
ALTER TABLE `tw_worlds`
  ADD PRIMARY KEY (`ID`);

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
-- AUTO_INCREMENT for table `tw_accounts`
--
ALTER TABLE `tw_accounts`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_accounts_aethers`
--
ALTER TABLE `tw_accounts_aethers`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_accounts_bans`
--
ALTER TABLE `tw_accounts_bans`
  MODIFY `Id` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_accounts_data`
--
ALTER TABLE `tw_accounts_data`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_accounts_items`
--
ALTER TABLE `tw_accounts_items`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_accounts_mailbox`
--
ALTER TABLE `tw_accounts_mailbox`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_accounts_professions`
--
ALTER TABLE `tw_accounts_professions`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_accounts_quests`
--
ALTER TABLE `tw_accounts_quests`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_accounts_skills`
--
ALTER TABLE `tw_accounts_skills`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_achievements`
--
ALTER TABLE `tw_achievements`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=156;

--
-- AUTO_INCREMENT for table `tw_aethers`
--
ALTER TABLE `tw_aethers`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=7;

--
-- AUTO_INCREMENT for table `tw_auction_slots`
--
ALTER TABLE `tw_auction_slots`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_bots_info`
--
ALTER TABLE `tw_bots_info`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1054;

--
-- AUTO_INCREMENT for table `tw_bots_mobs`
--
ALTER TABLE `tw_bots_mobs`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=121;

--
-- AUTO_INCREMENT for table `tw_bots_npc`
--
ALTER TABLE `tw_bots_npc`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=26;

--
-- AUTO_INCREMENT for table `tw_bots_quest`
--
ALTER TABLE `tw_bots_quest`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=129;

--
-- AUTO_INCREMENT for table `tw_crafts_list`
--
ALTER TABLE `tw_crafts_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=198;

--
-- AUTO_INCREMENT for table `tw_dungeons`
--
ALTER TABLE `tw_dungeons`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT for table `tw_dungeons_door`
--
ALTER TABLE `tw_dungeons_door`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=9;

--
-- AUTO_INCREMENT for table `tw_dungeons_records`
--
ALTER TABLE `tw_dungeons_records`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_groups`
--
ALTER TABLE `tw_groups`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_guilds`
--
ALTER TABLE `tw_guilds`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_guilds_history`
--
ALTER TABLE `tw_guilds_history`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_guilds_houses`
--
ALTER TABLE `tw_guilds_houses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=7;

--
-- AUTO_INCREMENT for table `tw_guilds_invites`
--
ALTER TABLE `tw_guilds_invites`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_guilds_ranks`
--
ALTER TABLE `tw_guilds_ranks`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_houses`
--
ALTER TABLE `tw_houses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=27;

--
-- AUTO_INCREMENT for table `tw_houses_decorations`
--
ALTER TABLE `tw_houses_decorations`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_items_list`
--
ALTER TABLE `tw_items_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=408;

--
-- AUTO_INCREMENT for table `tw_logics_worlds`
--
ALTER TABLE `tw_logics_worlds`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=27;

--
-- AUTO_INCREMENT for table `tw_quests_board_list`
--
ALTER TABLE `tw_quests_board_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=48;

--
-- AUTO_INCREMENT for table `tw_quests_list`
--
ALTER TABLE `tw_quests_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=73;

--
-- AUTO_INCREMENT for table `tw_quest_boards`
--
ALTER TABLE `tw_quest_boards`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- AUTO_INCREMENT for table `tw_skills_list`
--
ALTER TABLE `tw_skills_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=14;

--
-- AUTO_INCREMENT for table `tw_skills_tree`
--
ALTER TABLE `tw_skills_tree`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=63;

--
-- AUTO_INCREMENT for table `tw_voucher`
--
ALTER TABLE `tw_voucher`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=7;

--
-- AUTO_INCREMENT for table `tw_voucher_redeemed`
--
ALTER TABLE `tw_voucher_redeemed`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=1;

--
-- AUTO_INCREMENT for table `tw_warehouses`
--
ALTER TABLE `tw_warehouses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=14;

--
-- AUTO_INCREMENT for table `tw_worlds`
--
ALTER TABLE `tw_worlds`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=7;

--
-- AUTO_INCREMENT for table `tw_world_swap`
--
ALTER TABLE `tw_world_swap`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- Constraints for dumped tables
--

--
-- Constraints for table `tw_accounts_achievements`
--
ALTER TABLE `tw_accounts_achievements`
  ADD CONSTRAINT `tw_accounts_achievements_ibfk_1` FOREIGN KEY (`AccountID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

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
  ADD CONSTRAINT `tw_accounts_data_ibfk_3` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_data_ibfk_5` FOREIGN KEY (`ID`) REFERENCES `tw_accounts` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_data_ibfk_6` FOREIGN KEY (`ProfessionID`) REFERENCES `enum_professions` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_accounts_items`
--
ALTER TABLE `tw_accounts_items`
  ADD CONSTRAINT `tw_accounts_items_ibfk_2` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_items_ibfk_3` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_accounts_mailbox`
--
ALTER TABLE `tw_accounts_mailbox`
  ADD CONSTRAINT `tw_accounts_mailbox_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

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
  ADD CONSTRAINT `tw_aethers_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_auction_slots`
--
ALTER TABLE `tw_auction_slots`
  ADD CONSTRAINT `tw_auction_slots_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_auction_slots_ibfk_2` FOREIGN KEY (`OwnerID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_bots_info`
--
ALTER TABLE `tw_bots_info`
  ADD CONSTRAINT `tw_bots_info_ibfk_1` FOREIGN KEY (`SlotArmor`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_2` FOREIGN KEY (`SlotGrenade`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_3` FOREIGN KEY (`SlotGun`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_4` FOREIGN KEY (`SlotRifle`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_5` FOREIGN KEY (`SlotShotgun`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_6` FOREIGN KEY (`SlotHammer`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_bots_mobs`
--
ALTER TABLE `tw_bots_mobs`
  ADD CONSTRAINT `tw_bots_mobs_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_14` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_15` FOREIGN KEY (`it_drop_0`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_16` FOREIGN KEY (`it_drop_1`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_17` FOREIGN KEY (`it_drop_2`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_18` FOREIGN KEY (`it_drop_3`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_19` FOREIGN KEY (`it_drop_4`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Constraints for table `tw_bots_npc`
--
ALTER TABLE `tw_bots_npc`
  ADD CONSTRAINT `tw_bots_npc_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_npc_ibfk_4` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_npc_ibfk_5` FOREIGN KEY (`GiveQuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_bots_quest`
--
ALTER TABLE `tw_bots_quest`
  ADD CONSTRAINT `tw_bots_quest_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_8` FOREIGN KEY (`QuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_9` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Constraints for table `tw_crafts_list`
--
ALTER TABLE `tw_crafts_list`
  ADD CONSTRAINT `tw_crafts_list_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_crafts_list_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

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
-- Constraints for table `tw_groups`
--
ALTER TABLE `tw_groups`
  ADD CONSTRAINT `tw_groups_ibfk_1` FOREIGN KEY (`OwnerUID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE NO ACTION ON UPDATE NO ACTION;

--
-- Constraints for table `tw_guilds`
--
ALTER TABLE `tw_guilds`
  ADD CONSTRAINT `tw_guilds_ibfk_1` FOREIGN KEY (`LeaderUID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Constraints for table `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_2` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_4` FOREIGN KEY (`HouseID`) REFERENCES `tw_guilds_houses` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_5` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_guilds_houses`
--
ALTER TABLE `tw_guilds_houses`
  ADD CONSTRAINT `tw_guilds_houses_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_guilds_invites`
--
ALTER TABLE `tw_guilds_invites`
  ADD CONSTRAINT `tw_guilds_invites_ibfk_1` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_invites_ibfk_2` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_houses`
--
ALTER TABLE `tw_houses`
  ADD CONSTRAINT `tw_houses_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_items_list`
--
ALTER TABLE `tw_items_list`
  ADD CONSTRAINT `tw_items_list_ibfk_1` FOREIGN KEY (`AT1`) REFERENCES `tw_attributes` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_items_list_ibfk_2` FOREIGN KEY (`AT2`) REFERENCES `tw_attributes` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_skills_tree`
--
ALTER TABLE `tw_skills_tree`
  ADD CONSTRAINT `tw_skills_tree_ibfk_1` FOREIGN KEY (`SkillID`) REFERENCES `tw_skills_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_voucher_redeemed`
--
ALTER TABLE `tw_voucher_redeemed`
  ADD CONSTRAINT `tw_voucher_redeemed_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_warehouses`
--
ALTER TABLE `tw_warehouses`
  ADD CONSTRAINT `tw_warehouses_ibfk_1` FOREIGN KEY (`Currency`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
