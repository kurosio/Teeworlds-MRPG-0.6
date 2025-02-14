/*M!999999\- enable the sandbox mode */ 
-- MariaDB dump 10.19-11.4.4-MariaDB, for debian-linux-gnu (x86_64)
--
-- Host: localhost    Database: mmorpg
-- ------------------------------------------------------
-- Server version	11.4.4-MariaDB-3

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*M!100616 SET @OLD_NOTE_VERBOSITY=@@NOTE_VERBOSITY, NOTE_VERBOSITY=0 */;

--
-- Table structure for table `enum_behavior_mobs`
--

DROP TABLE IF EXISTS `enum_behavior_mobs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `enum_behavior_mobs` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Behavior` varchar(32) NOT NULL DEFAULT 'Standard',
  PRIMARY KEY (`ID`),
  KEY `Behavior` (`Behavior`)
) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `enum_behavior_mobs`
--

LOCK TABLES `enum_behavior_mobs` WRITE;
/*!40000 ALTER TABLE `enum_behavior_mobs` DISABLE KEYS */;
INSERT INTO `enum_behavior_mobs` VALUES
(3,'Sleepy'),
(2,'Slower'),
(1,'Standard');
/*!40000 ALTER TABLE `enum_behavior_mobs` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `enum_effects_list`
--

DROP TABLE IF EXISTS `enum_effects_list`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `enum_effects_list` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(16) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL,
  PRIMARY KEY (`ID`),
  KEY `Name` (`Name`)
) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `enum_effects_list`
--

LOCK TABLES `enum_effects_list` WRITE;
/*!40000 ALTER TABLE `enum_effects_list` DISABLE KEYS */;
INSERT INTO `enum_effects_list` VALUES
(3,'Fire'),
(2,'Poison'),
(1,'Slowdown');
/*!40000 ALTER TABLE `enum_effects_list` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `enum_items_functional`
--

DROP TABLE IF EXISTS `enum_items_functional`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `enum_items_functional` (
  `FunctionID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(64) NOT NULL,
  PRIMARY KEY (`FunctionID`)
) ENGINE=InnoDB AUTO_INCREMENT=18 DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `enum_items_functional`
--

LOCK TABLES `enum_items_functional` WRITE;
/*!40000 ALTER TABLE `enum_items_functional` DISABLE KEYS */;
INSERT INTO `enum_items_functional` VALUES
(-1,'Not have function'),
(0,'Equip hammer (Only equip type)'),
(1,'Equip gun (Only equip type)'),
(2,'Equip shotgun (Only equip type)'),
(3,'Equip grenade (Only equip type)'),
(4,'Equip rifle (Only equip type)'),
(5,'Equip pickaxe (Only equip type)'),
(6,'Equip rake (Only equip type)'),
(7,'Equip armor (Only equip type)'),
(8,'Equip eidolon (Only equip type)'),
(9,'Equip potion HP (Only equip type)'),
(10,'Equip potion MP (Only equip type)'),
(11,'Equip Tittle (Only equip type)'),
(12,'Single use x1'),
(13,'Multiple use x99'),
(14,'Setting (Only settings or modules type)'),
(15,'Resource harvestable'),
(16,'Resource mineable');
/*!40000 ALTER TABLE `enum_items_functional` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `enum_items_types`
--

DROP TABLE IF EXISTS `enum_items_types`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `enum_items_types` (
  `TypeID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(32) NOT NULL,
  PRIMARY KEY (`TypeID`)
) ENGINE=InnoDB AUTO_INCREMENT=9 DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `enum_items_types`
--

LOCK TABLES `enum_items_types` WRITE;
/*!40000 ALTER TABLE `enum_items_types` DISABLE KEYS */;
INSERT INTO `enum_items_types` VALUES
(0,'Invisible'),
(1,'Usable'),
(2,'Crafting material'),
(3,'Module'),
(4,'Other'),
(5,'Setting'),
(6,'Equipment'),
(7,'Decoration'),
(8,'Potion');
/*!40000 ALTER TABLE `enum_items_types` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `enum_professions`
--

DROP TABLE IF EXISTS `enum_professions`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `enum_professions` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL,
  PRIMARY KEY (`ID`),
  KEY `ID` (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `enum_professions`
--

LOCK TABLES `enum_professions` WRITE;
/*!40000 ALTER TABLE `enum_professions` DISABLE KEYS */;
INSERT INTO `enum_professions` VALUES
(-1,'Unclassed'),
(0,'Tank'),
(1,'DPS'),
(2,'Healer'),
(3,'Miner'),
(4,'Farmer');
/*!40000 ALTER TABLE `enum_professions` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_accounts`
--

DROP TABLE IF EXISTS `tw_accounts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_accounts` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Username` varchar(64) NOT NULL,
  `Password` varchar(64) NOT NULL,
  `PasswordSalt` varchar(64) DEFAULT NULL,
  `RegisterDate` varchar(64) NOT NULL,
  `LoginDate` varchar(64) NOT NULL DEFAULT 'First log in',
  `RegisteredIP` varchar(64) NOT NULL DEFAULT '0.0.0.0',
  `LoginIP` varchar(64) NOT NULL DEFAULT '0.0.0.0',
  `Language` varchar(8) NOT NULL DEFAULT 'en',
  `CountryISO` varchar(32) NOT NULL DEFAULT 'UN',
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `Password` (`Password`),
  KEY `Username` (`Username`)
) ENGINE=InnoDB AUTO_INCREMENT=15 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_accounts`
--

LOCK TABLES `tw_accounts` WRITE;
/*!40000 ALTER TABLE `tw_accounts` DISABLE KEYS */;
INSERT INTO `tw_accounts` VALUES
(1,'kuro','837a184441a8855cc657cfc742dec26a072ea18876f91474b33e28759db35e0e','LbcD4cDE37kYh86Xhk32op3g','2025-01-03 18:01:30','2025-01-31 15:36:14','[::1]','91.222.217.50','en','RU'),
(2,'halloween','07b355a27b30f051e12294cb517937790f6ad25302ef074aac2274109bc4c9cb','HKNRGokBkmSYWeECMULEbkFX','2025-01-07 14:42:29','2025-02-02 18:19:11','185.77.207.82','185.77.207.82','ru','RU'),
(3,'kuro','d9629db8e23863fd6f52cc6f971c0475ff4891025a14d7e17e714556f1c367b6','YK5VoVDZm5EpLm34UggS6at5','2025-01-08 08:47:09','2025-02-01 19:20:20','91.222.217.50','91.222.217.50','ru','RU'),
(4,'kuro','dc9680df3c476248808d92a57a7c965398a5b8f2501069bdb8a0ae4438d1932d','PBnF4ndpa5547AmajahNkmUX','2025-01-11 11:41:29','2025-01-18 11:32:18','91.222.217.50','91.222.217.50','en','RU'),
(5,'Rune','3f535f13318679a77e5f575a54eb5728462c2d19296233c490176222bb3c0128','c8eSfe73CDdaKX6E7mm3NMjP','2025-01-16 10:50:45','2025-01-16 14:05:40','109.234.29.10','109.234.29.10','en','RU'),
(6,'xpmob1us','97df4bea1bfca53ab78077cd192b70d1fbb1d16639f1a6ee0c94873b175de7c8','YRT5nR6jmUjXMGM4bBtXPoLP','2025-01-18 19:04:13','2025-01-29 23:53:28','176.109.186.193','176.109.186.52','ru','UA'),
(7,'2000','f84801c45f660fe454f5ccb325d3810bf19c040782544b8b1c7bcbd6d0738ba5','YTT3EXB4bN2dXc2tMKdEXnVn','2025-01-22 12:42:58','2025-02-02 19:08:50','185.77.207.82','185.77.207.82','ru','RU'),
(8,'kuro','bb34baebee4b3f09ef0cbf2574d10694df921f22ba64837c63f0cf9e115b21b4','Tnb3F3N6fqMfk935CPnVda3e','2025-01-29 17:09:35','2025-01-29 20:26:34','91.222.217.50','91.222.217.50','en','RU'),
(9,'zombie','b4c88ffce54263eb2122f6df54cf7d3a7db8f57b6c0dcb561979d9d974026f7b','6S5G7hjUNtRTk2K5EXVFHGDW','2025-01-29 20:54:54','2025-01-29 23:55:00','89.249.238.179','89.249.238.179','ru','RU'),
(10,'Hypocrite','f06fb8455609d0379c7f53cd0d4659486932bd4b17cd2b92abda5695f258b595','f6GDHSkYTRGLgBVSEqD7V9MN','2025-01-31 16:53:19','2025-01-31 19:53:25','5.140.82.36','5.140.82.36','en','RU'),
(11,'kuro','45d3ffd6252aa11c86097e38b92b6ddf6f24fbe9b83ce3aeab5eca4e6276162f','qhfLT46oj5akaEaVoS4mSLfN','2025-02-01 10:44:54','2025-02-01 19:09:20','91.222.217.50','91.222.217.50','ru','RU'),
(12,'erze','4dc822faa6bd50ddc9bd6925989aac62ae253067771e0e49f5f463d9f95d1f4a','TpgNFmhNc6SL6AfZd7TYa3n2','2025-02-01 16:21:28','2025-02-01 19:21:35','89.249.238.179','89.249.238.179','ru','RU'),
(13,'Trisha','7216365f496290f8d425dfa57a5a8d88fa34d00cee91d67edfde7a3009f07d4d','G6poZRESkhdj2Cd7c6oHMZF6','2025-02-01 16:41:04','2025-02-02 20:28:33','5.228.82.149','5.228.82.149','ru','RU'),
(14,'xpmob1us','597306e6e57a48196c4af8f350efff72908139aaac7e6796c7a9f53764dd89a7','B6hA78YjenF659HUGDFAj8Gk','2025-02-02 17:29:23','2025-02-02 20:29:25','94.158.35.93','94.158.35.93','en','UA');
/*!40000 ALTER TABLE `tw_accounts` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_accounts_aethers`
--

DROP TABLE IF EXISTS `tw_accounts_aethers`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_accounts_aethers` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `UserID` int(11) NOT NULL,
  `AetherID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `OwnerID` (`UserID`),
  KEY `TeleportID` (`AetherID`),
  CONSTRAINT `tw_accounts_aethers_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_accounts_aethers_ibfk_2` FOREIGN KEY (`AetherID`) REFERENCES `tw_aethers` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=22 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_accounts_aethers`
--

LOCK TABLES `tw_accounts_aethers` WRITE;
/*!40000 ALTER TABLE `tw_accounts_aethers` DISABLE KEYS */;
INSERT INTO `tw_accounts_aethers` VALUES
(1,2,2),
(2,2,1),
(3,2,3),
(4,2,4),
(5,5,1),
(6,4,3),
(7,4,2),
(8,3,1),
(9,3,2),
(10,7,2),
(11,7,1),
(12,6,1),
(13,6,3),
(14,7,3),
(15,8,1),
(16,9,1),
(17,3,4),
(18,10,1),
(19,11,1),
(20,12,1),
(21,13,2);
/*!40000 ALTER TABLE `tw_accounts_aethers` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_accounts_bans`
--

DROP TABLE IF EXISTS `tw_accounts_bans`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_accounts_bans` (
  `Id` int(11) NOT NULL AUTO_INCREMENT,
  `AccountId` int(11) NOT NULL,
  `BannedSince` timestamp NULL DEFAULT current_timestamp(),
  `BannedUntil` timestamp NOT NULL DEFAULT current_timestamp(),
  `Reason` varchar(512) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NOT NULL DEFAULT 'No Reason Given',
  PRIMARY KEY (`Id`),
  KEY `tw_accounts_bans_tw_accounts_ID_fk` (`AccountId`),
  CONSTRAINT `tw_accounts_bans_tw_accounts_ID_fk` FOREIGN KEY (`AccountId`) REFERENCES `tw_accounts` (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_accounts_bans`
--

LOCK TABLES `tw_accounts_bans` WRITE;
/*!40000 ALTER TABLE `tw_accounts_bans` DISABLE KEYS */;
/*!40000 ALTER TABLE `tw_accounts_bans` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_accounts_data`
--

DROP TABLE IF EXISTS `tw_accounts_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_accounts_data` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Nick` varchar(32) NOT NULL,
  `WorldID` int(11) DEFAULT NULL,
  `Rating` int(11) NOT NULL DEFAULT 0,
  `ProfessionID` int(11) NOT NULL DEFAULT -1,
  `Bank` varchar(512) DEFAULT '0',
  `CrimeScore` int(11) NOT NULL DEFAULT 0,
  `DailyStamp` bigint(20) NOT NULL DEFAULT 0,
  `WeekStamp` bigint(20) NOT NULL DEFAULT 0,
  `MonthStamp` bigint(20) NOT NULL DEFAULT 0,
  `Upgrade` int(11) NOT NULL DEFAULT 0,
  `Achievements` longtext NOT NULL DEFAULT '',
  PRIMARY KEY (`ID`),
  UNIQUE KEY `Nick` (`Nick`),
  KEY `tw_accounts_data_ibfk_3` (`WorldID`),
  KEY `ProfessionID` (`ProfessionID`),
  CONSTRAINT `tw_accounts_data_ibfk_3` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `tw_accounts_data_ibfk_5` FOREIGN KEY (`ID`) REFERENCES `tw_accounts` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_accounts_data_ibfk_6` FOREIGN KEY (`ProfessionID`) REFERENCES `enum_professions` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=15 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_accounts_data`
--

LOCK TABLES `tw_accounts_data` WRITE;
/*!40000 ALTER TABLE `tw_accounts_data` DISABLE KEYS */;
INSERT INTO `tw_accounts_data` VALUES
(1,'Kurosio',1,1000,-1,'700',0,1738326913,1736356677,1735927291,0,'[{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":false,\"progress\":2},{\"aid\":32,\"completed\":false,\"progress\":2},{\"aid\":33,\"completed\":false,\"progress\":2},{\"aid\":34,\"completed\":false,\"progress\":2},{\"aid\":35,\"completed\":false,\"progress\":2},{\"aid\":36,\"completed\":false,\"progress\":2},{\"aid\":37,\"completed\":false,\"progress\":2},{\"aid\":38,\"completed\":false,\"progress\":2},{\"aid\":39,\"completed\":false,\"progress\":2}]'),
(2,'HaLLoWeeN`iPro',1,1133,2,'45850397616',0,1738500807,1738500807,1738406595,0,'[{\"aid\":16,\"completed\":true,\"progress\":1},{\"aid\":17,\"completed\":true,\"progress\":10},{\"aid\":18,\"completed\":true,\"progress\":50},{\"aid\":19,\"completed\":true,\"progress\":100},{\"aid\":20,\"completed\":true,\"progress\":200},{\"aid\":21,\"completed\":false,\"progress\":1125},{\"aid\":22,\"completed\":false,\"progress\":1125},{\"aid\":23,\"completed\":false,\"progress\":1125},{\"aid\":24,\"completed\":false,\"progress\":1125},{\"aid\":25,\"completed\":false,\"progress\":1125},{\"aid\":26,\"completed\":false,\"progress\":1125},{\"aid\":27,\"completed\":false,\"progress\":1125},{\"aid\":28,\"completed\":false,\"progress\":1125},{\"aid\":29,\"completed\":false,\"progress\":1125},{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":75,\"completed\":true,\"progress\":100},{\"aid\":76,\"completed\":true,\"progress\":1000},{\"aid\":77,\"completed\":true,\"progress\":10000},{\"aid\":78,\"completed\":false,\"progress\":24024},{\"aid\":79,\"completed\":false,\"progress\":24024},{\"aid\":80,\"completed\":true,\"progress\":500},{\"aid\":89,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":true,\"progress\":50},{\"aid\":32,\"completed\":false,\"progress\":149},{\"aid\":33,\"completed\":false,\"progress\":149},{\"aid\":34,\"completed\":false,\"progress\":149},{\"aid\":35,\"completed\":false,\"progress\":149},{\"aid\":36,\"completed\":false,\"progress\":149},{\"aid\":37,\"completed\":false,\"progress\":149},{\"aid\":38,\"completed\":false,\"progress\":149},{\"aid\":39,\"completed\":false,\"progress\":149},{\"aid\":81,\"completed\":true,\"progress\":5000},{\"aid\":82,\"completed\":false,\"progress\":3586},{\"aid\":83,\"completed\":false,\"progress\":3586},{\"aid\":84,\"completed\":false,\"progress\":3586},{\"aid\":95,\"completed\":false,\"progress\":279},{\"aid\":96,\"completed\":false,\"progress\":257},{\"aid\":106,\"completed\":false,\"progress\":51},{\"aid\":107,\"completed\":false,\"progress\":13},{\"aid\":97,\"completed\":false,\"progress\":135},{\"aid\":99,\"completed\":false,\"progress\":18},{\"aid\":102,\"completed\":false,\"progress\":43},{\"aid\":103,\"completed\":false,\"progress\":31},{\"aid\":101,\"completed\":false,\"progress\":46},{\"aid\":98,\"completed\":false,\"progress\":40},{\"aid\":40,\"completed\":true,\"progress\":5},{\"aid\":41,\"completed\":true,\"progress\":15},{\"aid\":42,\"completed\":false,\"progress\":10},{\"aid\":43,\"completed\":false,\"progress\":10},{\"aid\":44,\"completed\":false,\"progress\":10},{\"aid\":45,\"completed\":false,\"progress\":10},{\"aid\":46,\"completed\":false,\"progress\":10},{\"aid\":105,\"completed\":false,\"progress\":12},{\"aid\":100,\"completed\":false,\"progress\":14},{\"aid\":104,\"completed\":false,\"progress\":4},{\"aid\":47,\"completed\":true,\"progress\":5},{\"aid\":48,\"completed\":false,\"progress\":7},{\"aid\":49,\"completed\":false,\"progress\":7},{\"aid\":50,\"completed\":false,\"progress\":7},{\"aid\":51,\"completed\":false,\"progress\":7},{\"aid\":52,\"completed\":false,\"progress\":7},{\"aid\":53,\"completed\":false,\"progress\":7},{\"aid\":54,\"completed\":true,\"progress\":5},{\"aid\":55,\"completed\":false,\"progress\":10},{\"aid\":56,\"completed\":false,\"progress\":10},{\"aid\":57,\"completed\":false,\"progress\":10},{\"aid\":58,\"completed\":false,\"progress\":10},{\"aid\":59,\"completed\":false,\"progress\":10},{\"aid\":60,\"completed\":false,\"progress\":10},{\"aid\":1,\"completed\":true,\"progress\":1},{\"aid\":2,\"completed\":true,\"progress\":10},{\"aid\":3,\"completed\":true,\"progress\":25},{\"aid\":4,\"completed\":false,\"progress\":27},{\"aid\":5,\"completed\":false,\"progress\":27},{\"aid\":6,\"completed\":false,\"progress\":27},{\"aid\":7,\"completed\":false,\"progress\":27},{\"aid\":8,\"completed\":false,\"progress\":27},{\"aid\":9,\"completed\":false,\"progress\":27},{\"aid\":10,\"completed\":false,\"progress\":27},{\"aid\":11,\"completed\":false,\"progress\":27},{\"aid\":12,\"completed\":false,\"progress\":27},{\"aid\":13,\"completed\":false,\"progress\":27},{\"aid\":14,\"completed\":false,\"progress\":27},{\"aid\":86,\"completed\":true,\"progress\":1},{\"aid\":125,\"completed\":true,\"progress\":50},{\"aid\":126,\"completed\":true,\"progress\":300},{\"aid\":127,\"completed\":true,\"progress\":1000},{\"aid\":128,\"completed\":true,\"progress\":10000},{\"aid\":129,\"completed\":false,\"progress\":98750},{\"aid\":130,\"completed\":true,\"progress\":50},{\"aid\":131,\"completed\":true,\"progress\":5000},{\"aid\":132,\"completed\":false,\"progress\":10057},{\"aid\":133,\"completed\":false,\"progress\":10057},{\"aid\":134,\"completed\":false,\"progress\":10057},{\"aid\":119,\"completed\":false,\"progress\":4},{\"aid\":124,\"completed\":true,\"progress\":1},{\"aid\":120,\"completed\":false,\"progress\":3},{\"aid\":121,\"completed\":false,\"progress\":7}]'),
(3,'Kurosio2',1,1392,1,'640735572',100,1738426820,1738073716,1738426820,0,'[{\"aid\":2,\"completed\":true,\"progress\":10},{\"aid\":4,\"completed\":true,\"progress\":50},{\"aid\":11,\"completed\":false,\"progress\":63},{\"aid\":7,\"completed\":false,\"progress\":63},{\"aid\":10,\"completed\":false,\"progress\":63},{\"aid\":8,\"completed\":false,\"progress\":63},{\"aid\":13,\"completed\":false,\"progress\":63},{\"aid\":6,\"completed\":false,\"progress\":63},{\"aid\":12,\"completed\":false,\"progress\":63},{\"aid\":1,\"completed\":true,\"progress\":1},{\"aid\":9,\"completed\":false,\"progress\":63},{\"aid\":3,\"completed\":true,\"progress\":25},{\"aid\":75,\"completed\":true,\"progress\":100},{\"aid\":76,\"completed\":true,\"progress\":1000},{\"aid\":77,\"completed\":false,\"progress\":4815},{\"aid\":78,\"completed\":false,\"progress\":4815},{\"aid\":79,\"completed\":false,\"progress\":4815},{\"aid\":100,\"completed\":false,\"progress\":12},{\"aid\":16,\"completed\":true,\"progress\":1},{\"aid\":17,\"completed\":true,\"progress\":10},{\"aid\":18,\"completed\":true,\"progress\":50},{\"aid\":19,\"completed\":true,\"progress\":100},{\"aid\":20,\"completed\":false,\"progress\":388},{\"aid\":21,\"completed\":false,\"progress\":388},{\"aid\":22,\"completed\":false,\"progress\":388},{\"aid\":23,\"completed\":false,\"progress\":388},{\"aid\":24,\"completed\":false,\"progress\":388},{\"aid\":25,\"completed\":false,\"progress\":388},{\"aid\":26,\"completed\":false,\"progress\":388},{\"aid\":27,\"completed\":false,\"progress\":388},{\"aid\":28,\"completed\":false,\"progress\":388},{\"aid\":29,\"completed\":false,\"progress\":388},{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":false,\"progress\":48},{\"aid\":32,\"completed\":false,\"progress\":48},{\"aid\":33,\"completed\":false,\"progress\":48},{\"aid\":34,\"completed\":false,\"progress\":48},{\"aid\":35,\"completed\":false,\"progress\":48},{\"aid\":36,\"completed\":false,\"progress\":48},{\"aid\":37,\"completed\":false,\"progress\":48},{\"aid\":38,\"completed\":false,\"progress\":48},{\"aid\":39,\"completed\":false,\"progress\":48},{\"aid\":80,\"completed\":true,\"progress\":500},{\"aid\":81,\"completed\":true,\"progress\":5000},{\"aid\":82,\"completed\":true,\"progress\":100000},{\"aid\":83,\"completed\":true,\"progress\":1000000},{\"aid\":84,\"completed\":true,\"progress\":10000000},{\"aid\":5,\"completed\":false,\"progress\":57},{\"aid\":14,\"completed\":false,\"progress\":57},{\"aid\":61,\"completed\":false,\"progress\":3},{\"aid\":62,\"completed\":false,\"progress\":3},{\"aid\":63,\"completed\":false,\"progress\":3},{\"aid\":64,\"completed\":false,\"progress\":3},{\"aid\":65,\"completed\":false,\"progress\":3},{\"aid\":66,\"completed\":false,\"progress\":3},{\"aid\":67,\"completed\":false,\"progress\":3},{\"aid\":40,\"completed\":true,\"progress\":5},{\"aid\":41,\"completed\":true,\"progress\":15},{\"aid\":42,\"completed\":false,\"progress\":3},{\"aid\":43,\"completed\":false,\"progress\":3},{\"aid\":44,\"completed\":false,\"progress\":3},{\"aid\":45,\"completed\":false,\"progress\":3},{\"aid\":46,\"completed\":false,\"progress\":3},{\"aid\":95,\"completed\":false,\"progress\":109},{\"aid\":96,\"completed\":false,\"progress\":101},{\"aid\":97,\"completed\":false,\"progress\":48},{\"aid\":99,\"completed\":false,\"progress\":5},{\"aid\":98,\"completed\":false,\"progress\":41},{\"aid\":101,\"completed\":false,\"progress\":12},{\"aid\":102,\"completed\":false,\"progress\":7},{\"aid\":54,\"completed\":true,\"progress\":5},{\"aid\":55,\"completed\":true,\"progress\":15},{\"aid\":56,\"completed\":false,\"progress\":15},{\"aid\":57,\"completed\":false,\"progress\":15},{\"aid\":58,\"completed\":false,\"progress\":15},{\"aid\":59,\"completed\":false,\"progress\":15},{\"aid\":60,\"completed\":false,\"progress\":15},{\"aid\":86,\"completed\":true,\"progress\":1},{\"aid\":125,\"completed\":true,\"progress\":50},{\"aid\":126,\"completed\":true,\"progress\":300},{\"aid\":127,\"completed\":false,\"progress\":374},{\"aid\":128,\"completed\":false,\"progress\":374},{\"aid\":129,\"completed\":false,\"progress\":374},{\"aid\":103,\"completed\":false,\"progress\":2},{\"aid\":105,\"completed\":false,\"progress\":1},{\"aid\":107,\"completed\":false,\"progress\":3},{\"aid\":106,\"completed\":false,\"progress\":1},{\"aid\":68,\"completed\":false,\"progress\":3},{\"aid\":69,\"completed\":false,\"progress\":3},{\"aid\":70,\"completed\":false,\"progress\":3},{\"aid\":71,\"completed\":false,\"progress\":3},{\"aid\":72,\"completed\":false,\"progress\":3},{\"aid\":73,\"completed\":false,\"progress\":3},{\"aid\":74,\"completed\":false,\"progress\":3},{\"aid\":47,\"completed\":false,\"progress\":3},{\"aid\":48,\"completed\":false,\"progress\":3},{\"aid\":49,\"completed\":false,\"progress\":3},{\"aid\":50,\"completed\":false,\"progress\":3},{\"aid\":51,\"completed\":false,\"progress\":3},{\"aid\":52,\"completed\":false,\"progress\":3},{\"aid\":53,\"completed\":false,\"progress\":3},{\"aid\":89,\"completed\":true,\"progress\":1},{\"aid\":120,\"completed\":false,\"progress\":3},{\"aid\":121,\"completed\":false,\"progress\":10}]'),
(4,'Kurosio23',1,0,0,'1193',100,1737189138,0,1736595690,0,''),
(5,'Rune',0,0,-1,'40',20,1737024678,0,1737024678,0,''),
(6,'xpmob1us',1,1000,1,'39',0,1738184008,1738184008,1737227058,0,'[{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":false,\"progress\":6},{\"aid\":32,\"completed\":false,\"progress\":6},{\"aid\":33,\"completed\":false,\"progress\":6},{\"aid\":34,\"completed\":false,\"progress\":6},{\"aid\":35,\"completed\":false,\"progress\":6},{\"aid\":36,\"completed\":false,\"progress\":6},{\"aid\":37,\"completed\":false,\"progress\":6},{\"aid\":38,\"completed\":false,\"progress\":6},{\"aid\":39,\"completed\":false,\"progress\":6},{\"aid\":75,\"completed\":false,\"progress\":44},{\"aid\":76,\"completed\":false,\"progress\":44},{\"aid\":77,\"completed\":false,\"progress\":44},{\"aid\":78,\"completed\":false,\"progress\":44},{\"aid\":79,\"completed\":false,\"progress\":44},{\"aid\":47,\"completed\":false,\"progress\":4},{\"aid\":48,\"completed\":false,\"progress\":4},{\"aid\":49,\"completed\":false,\"progress\":4},{\"aid\":50,\"completed\":false,\"progress\":4},{\"aid\":51,\"completed\":false,\"progress\":4},{\"aid\":52,\"completed\":false,\"progress\":4},{\"aid\":53,\"completed\":false,\"progress\":4},{\"aid\":40,\"completed\":false,\"progress\":4},{\"aid\":41,\"completed\":false,\"progress\":4},{\"aid\":42,\"completed\":false,\"progress\":4},{\"aid\":43,\"completed\":false,\"progress\":4},{\"aid\":44,\"completed\":false,\"progress\":4},{\"aid\":45,\"completed\":false,\"progress\":4},{\"aid\":46,\"completed\":false,\"progress\":4},{\"aid\":80,\"completed\":false,\"progress\":277},{\"aid\":81,\"completed\":false,\"progress\":277},{\"aid\":82,\"completed\":false,\"progress\":277},{\"aid\":83,\"completed\":false,\"progress\":277},{\"aid\":84,\"completed\":false,\"progress\":277},{\"aid\":68,\"completed\":false,\"progress\":3},{\"aid\":69,\"completed\":false,\"progress\":3},{\"aid\":70,\"completed\":false,\"progress\":3},{\"aid\":71,\"completed\":false,\"progress\":3},{\"aid\":72,\"completed\":false,\"progress\":3},{\"aid\":73,\"completed\":false,\"progress\":3},{\"aid\":74,\"completed\":false,\"progress\":3},{\"aid\":95,\"completed\":false,\"progress\":6},{\"aid\":16,\"completed\":true,\"progress\":1},{\"aid\":17,\"completed\":false,\"progress\":9},{\"aid\":18,\"completed\":false,\"progress\":9},{\"aid\":19,\"completed\":false,\"progress\":9},{\"aid\":20,\"completed\":false,\"progress\":9},{\"aid\":21,\"completed\":false,\"progress\":9},{\"aid\":22,\"completed\":false,\"progress\":9},{\"aid\":23,\"completed\":false,\"progress\":9},{\"aid\":24,\"completed\":false,\"progress\":9},{\"aid\":25,\"completed\":false,\"progress\":9},{\"aid\":26,\"completed\":false,\"progress\":9},{\"aid\":27,\"completed\":false,\"progress\":9},{\"aid\":28,\"completed\":false,\"progress\":9},{\"aid\":29,\"completed\":false,\"progress\":9},{\"aid\":96,\"completed\":false,\"progress\":2},{\"aid\":97,\"completed\":false,\"progress\":1}]'),
(7,'Кобальт',1,1000,1,'999944650',0,1738446007,1738446007,1738406772,0,'[{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":true,\"progress\":50},{\"aid\":32,\"completed\":false,\"progress\":151},{\"aid\":33,\"completed\":false,\"progress\":151},{\"aid\":34,\"completed\":false,\"progress\":151},{\"aid\":35,\"completed\":false,\"progress\":151},{\"aid\":36,\"completed\":false,\"progress\":151},{\"aid\":37,\"completed\":false,\"progress\":151},{\"aid\":38,\"completed\":false,\"progress\":151},{\"aid\":39,\"completed\":false,\"progress\":151},{\"aid\":75,\"completed\":true,\"progress\":100},{\"aid\":76,\"completed\":true,\"progress\":1000},{\"aid\":77,\"completed\":false,\"progress\":4267},{\"aid\":78,\"completed\":false,\"progress\":4267},{\"aid\":79,\"completed\":false,\"progress\":4267},{\"aid\":80,\"completed\":true,\"progress\":500},{\"aid\":81,\"completed\":true,\"progress\":5000},{\"aid\":82,\"completed\":false,\"progress\":99},{\"aid\":83,\"completed\":false,\"progress\":99},{\"aid\":84,\"completed\":false,\"progress\":99},{\"aid\":95,\"completed\":false,\"progress\":722},{\"aid\":16,\"completed\":true,\"progress\":1},{\"aid\":17,\"completed\":true,\"progress\":10},{\"aid\":18,\"completed\":true,\"progress\":50},{\"aid\":19,\"completed\":true,\"progress\":100},{\"aid\":20,\"completed\":true,\"progress\":200},{\"aid\":21,\"completed\":false,\"progress\":1395},{\"aid\":22,\"completed\":false,\"progress\":1395},{\"aid\":23,\"completed\":false,\"progress\":1395},{\"aid\":24,\"completed\":false,\"progress\":1395},{\"aid\":25,\"completed\":false,\"progress\":1395},{\"aid\":26,\"completed\":false,\"progress\":1395},{\"aid\":27,\"completed\":false,\"progress\":1395},{\"aid\":28,\"completed\":false,\"progress\":1395},{\"aid\":29,\"completed\":false,\"progress\":1395},{\"aid\":40,\"completed\":true,\"progress\":5},{\"aid\":41,\"completed\":false,\"progress\":5},{\"aid\":42,\"completed\":false,\"progress\":5},{\"aid\":43,\"completed\":false,\"progress\":5},{\"aid\":44,\"completed\":false,\"progress\":5},{\"aid\":45,\"completed\":false,\"progress\":5},{\"aid\":46,\"completed\":false,\"progress\":5},{\"aid\":68,\"completed\":true,\"progress\":5},{\"aid\":69,\"completed\":false,\"progress\":3},{\"aid\":70,\"completed\":false,\"progress\":3},{\"aid\":71,\"completed\":false,\"progress\":3},{\"aid\":72,\"completed\":false,\"progress\":3},{\"aid\":73,\"completed\":false,\"progress\":3},{\"aid\":74,\"completed\":false,\"progress\":3},{\"aid\":96,\"completed\":false,\"progress\":444},{\"aid\":97,\"completed\":false,\"progress\":102},{\"aid\":61,\"completed\":true,\"progress\":5},{\"aid\":62,\"completed\":false,\"progress\":6},{\"aid\":63,\"completed\":false,\"progress\":6},{\"aid\":64,\"completed\":false,\"progress\":6},{\"aid\":65,\"completed\":false,\"progress\":6},{\"aid\":66,\"completed\":false,\"progress\":6},{\"aid\":67,\"completed\":false,\"progress\":6},{\"aid\":47,\"completed\":true,\"progress\":5},{\"aid\":48,\"completed\":false,\"progress\":5},{\"aid\":49,\"completed\":false,\"progress\":5},{\"aid\":50,\"completed\":false,\"progress\":5},{\"aid\":51,\"completed\":false,\"progress\":5},{\"aid\":52,\"completed\":false,\"progress\":5},{\"aid\":53,\"completed\":false,\"progress\":5},{\"aid\":109,\"completed\":true,\"progress\":1},{\"aid\":1,\"completed\":true,\"progress\":1},{\"aid\":2,\"completed\":false,\"progress\":4},{\"aid\":3,\"completed\":false,\"progress\":4},{\"aid\":4,\"completed\":false,\"progress\":4},{\"aid\":5,\"completed\":false,\"progress\":4},{\"aid\":6,\"completed\":false,\"progress\":4},{\"aid\":7,\"completed\":false,\"progress\":4},{\"aid\":8,\"completed\":false,\"progress\":4},{\"aid\":9,\"completed\":false,\"progress\":4},{\"aid\":10,\"completed\":false,\"progress\":4},{\"aid\":11,\"completed\":false,\"progress\":4},{\"aid\":12,\"completed\":false,\"progress\":4},{\"aid\":13,\"completed\":false,\"progress\":4},{\"aid\":14,\"completed\":false,\"progress\":4},{\"aid\":106,\"completed\":false,\"progress\":3},{\"aid\":99,\"completed\":false,\"progress\":4},{\"aid\":130,\"completed\":true,\"progress\":50},{\"aid\":131,\"completed\":false,\"progress\":50},{\"aid\":132,\"completed\":false,\"progress\":50},{\"aid\":133,\"completed\":false,\"progress\":50},{\"aid\":134,\"completed\":false,\"progress\":50},{\"aid\":125,\"completed\":true,\"progress\":50},{\"aid\":126,\"completed\":false,\"progress\":66},{\"aid\":127,\"completed\":false,\"progress\":66},{\"aid\":128,\"completed\":false,\"progress\":66},{\"aid\":129,\"completed\":false,\"progress\":66},{\"aid\":118,\"completed\":true,\"progress\":1},{\"aid\":98,\"completed\":false,\"progress\":77},{\"aid\":100,\"completed\":false,\"progress\":1},{\"aid\":119,\"completed\":false,\"progress\":1},{\"aid\":54,\"completed\":true,\"progress\":5},{\"aid\":55,\"completed\":false,\"progress\":7},{\"aid\":56,\"completed\":false,\"progress\":7},{\"aid\":57,\"completed\":false,\"progress\":7},{\"aid\":58,\"completed\":false,\"progress\":7},{\"aid\":59,\"completed\":false,\"progress\":7},{\"aid\":60,\"completed\":false,\"progress\":7},{\"aid\":101,\"completed\":false,\"progress\":5},{\"aid\":120,\"completed\":false,\"progress\":2},{\"aid\":121,\"completed\":false,\"progress\":7},{\"aid\":102,\"completed\":false,\"progress\":1},{\"aid\":123,\"completed\":true,\"progress\":1},{\"aid\":124,\"completed\":true,\"progress\":1},{\"aid\":114,\"completed\":false,\"progress\":1}]'),
(8,'kupka',1,1000,-1,'0',0,1738170576,1738170576,1738170576,0,'[{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":false,\"progress\":8},{\"aid\":32,\"completed\":false,\"progress\":8},{\"aid\":33,\"completed\":false,\"progress\":8},{\"aid\":34,\"completed\":false,\"progress\":8},{\"aid\":35,\"completed\":false,\"progress\":8},{\"aid\":36,\"completed\":false,\"progress\":8},{\"aid\":37,\"completed\":false,\"progress\":8},{\"aid\":38,\"completed\":false,\"progress\":8},{\"aid\":39,\"completed\":false,\"progress\":8},{\"aid\":75,\"completed\":false,\"progress\":47},{\"aid\":76,\"completed\":false,\"progress\":47},{\"aid\":77,\"completed\":false,\"progress\":47},{\"aid\":78,\"completed\":false,\"progress\":47},{\"aid\":79,\"completed\":false,\"progress\":47}]'),
(9,'Zombie\'NewYeera',1,1027,-1,'0',20,1738185434,1738185434,1738184100,0,'[{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":false,\"progress\":8},{\"aid\":32,\"completed\":false,\"progress\":8},{\"aid\":33,\"completed\":false,\"progress\":8},{\"aid\":34,\"completed\":false,\"progress\":8},{\"aid\":35,\"completed\":false,\"progress\":8},{\"aid\":36,\"completed\":false,\"progress\":8},{\"aid\":37,\"completed\":false,\"progress\":8},{\"aid\":38,\"completed\":false,\"progress\":8},{\"aid\":39,\"completed\":false,\"progress\":8},{\"aid\":75,\"completed\":true,\"progress\":100},{\"aid\":76,\"completed\":false,\"progress\":105},{\"aid\":77,\"completed\":false,\"progress\":105},{\"aid\":78,\"completed\":false,\"progress\":105},{\"aid\":79,\"completed\":false,\"progress\":105},{\"aid\":80,\"completed\":false,\"progress\":488},{\"aid\":81,\"completed\":false,\"progress\":488},{\"aid\":82,\"completed\":false,\"progress\":488},{\"aid\":83,\"completed\":false,\"progress\":488},{\"aid\":84,\"completed\":false,\"progress\":488},{\"aid\":95,\"completed\":false,\"progress\":2},{\"aid\":16,\"completed\":true,\"progress\":1},{\"aid\":17,\"completed\":false,\"progress\":2},{\"aid\":18,\"completed\":false,\"progress\":2},{\"aid\":19,\"completed\":false,\"progress\":2},{\"aid\":20,\"completed\":false,\"progress\":2},{\"aid\":21,\"completed\":false,\"progress\":2},{\"aid\":22,\"completed\":false,\"progress\":2},{\"aid\":23,\"completed\":false,\"progress\":2},{\"aid\":24,\"completed\":false,\"progress\":2},{\"aid\":25,\"completed\":false,\"progress\":2},{\"aid\":26,\"completed\":false,\"progress\":2},{\"aid\":27,\"completed\":false,\"progress\":2},{\"aid\":28,\"completed\":false,\"progress\":2},{\"aid\":29,\"completed\":false,\"progress\":2},{\"aid\":1,\"completed\":true,\"progress\":1},{\"aid\":2,\"completed\":false,\"progress\":2},{\"aid\":3,\"completed\":false,\"progress\":2},{\"aid\":4,\"completed\":false,\"progress\":2},{\"aid\":5,\"completed\":false,\"progress\":2},{\"aid\":6,\"completed\":false,\"progress\":2},{\"aid\":7,\"completed\":false,\"progress\":2},{\"aid\":8,\"completed\":false,\"progress\":2},{\"aid\":9,\"completed\":false,\"progress\":2},{\"aid\":10,\"completed\":false,\"progress\":2},{\"aid\":11,\"completed\":false,\"progress\":2},{\"aid\":12,\"completed\":false,\"progress\":2},{\"aid\":13,\"completed\":false,\"progress\":2},{\"aid\":14,\"completed\":false,\"progress\":2}]'),
(10,'Hypocrite',1,1000,-1,'0',0,1738342405,1738342405,1738342405,0,'[{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":false,\"progress\":4},{\"aid\":32,\"completed\":false,\"progress\":4},{\"aid\":33,\"completed\":false,\"progress\":4},{\"aid\":34,\"completed\":false,\"progress\":4},{\"aid\":35,\"completed\":false,\"progress\":4},{\"aid\":36,\"completed\":false,\"progress\":4},{\"aid\":37,\"completed\":false,\"progress\":4},{\"aid\":38,\"completed\":false,\"progress\":4},{\"aid\":39,\"completed\":false,\"progress\":4},{\"aid\":75,\"completed\":false,\"progress\":19},{\"aid\":76,\"completed\":false,\"progress\":19},{\"aid\":77,\"completed\":false,\"progress\":19},{\"aid\":78,\"completed\":false,\"progress\":19},{\"aid\":79,\"completed\":false,\"progress\":19},{\"aid\":80,\"completed\":false,\"progress\":36},{\"aid\":81,\"completed\":false,\"progress\":36},{\"aid\":82,\"completed\":false,\"progress\":36},{\"aid\":83,\"completed\":false,\"progress\":36},{\"aid\":84,\"completed\":false,\"progress\":36},{\"aid\":95,\"completed\":false,\"progress\":4},{\"aid\":16,\"completed\":true,\"progress\":1},{\"aid\":17,\"completed\":false,\"progress\":5},{\"aid\":18,\"completed\":false,\"progress\":5},{\"aid\":19,\"completed\":false,\"progress\":5},{\"aid\":20,\"completed\":false,\"progress\":5},{\"aid\":21,\"completed\":false,\"progress\":5},{\"aid\":22,\"completed\":false,\"progress\":5},{\"aid\":23,\"completed\":false,\"progress\":5},{\"aid\":24,\"completed\":false,\"progress\":5},{\"aid\":25,\"completed\":false,\"progress\":5},{\"aid\":26,\"completed\":false,\"progress\":5},{\"aid\":27,\"completed\":false,\"progress\":5},{\"aid\":28,\"completed\":false,\"progress\":5},{\"aid\":29,\"completed\":false,\"progress\":5},{\"aid\":96,\"completed\":false,\"progress\":1}]'),
(11,'Kurosiow',1,1191,0,'616',0,1738406696,0,1738406696,0,'[{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":false,\"progress\":43},{\"aid\":32,\"completed\":false,\"progress\":43},{\"aid\":33,\"completed\":false,\"progress\":43},{\"aid\":34,\"completed\":false,\"progress\":43},{\"aid\":35,\"completed\":false,\"progress\":43},{\"aid\":36,\"completed\":false,\"progress\":43},{\"aid\":37,\"completed\":false,\"progress\":43},{\"aid\":38,\"completed\":false,\"progress\":43},{\"aid\":39,\"completed\":false,\"progress\":43},{\"aid\":80,\"completed\":true,\"progress\":500},{\"aid\":81,\"completed\":false,\"progress\":2200},{\"aid\":82,\"completed\":false,\"progress\":2200},{\"aid\":83,\"completed\":false,\"progress\":2200},{\"aid\":84,\"completed\":false,\"progress\":2200},{\"aid\":75,\"completed\":true,\"progress\":100},{\"aid\":76,\"completed\":true,\"progress\":1000},{\"aid\":77,\"completed\":false,\"progress\":1878},{\"aid\":78,\"completed\":false,\"progress\":1878},{\"aid\":79,\"completed\":false,\"progress\":1878},{\"aid\":1,\"completed\":true,\"progress\":1},{\"aid\":2,\"completed\":true,\"progress\":10},{\"aid\":3,\"completed\":false,\"progress\":13},{\"aid\":4,\"completed\":false,\"progress\":13},{\"aid\":5,\"completed\":false,\"progress\":13},{\"aid\":6,\"completed\":false,\"progress\":13},{\"aid\":7,\"completed\":false,\"progress\":13},{\"aid\":8,\"completed\":false,\"progress\":13},{\"aid\":9,\"completed\":false,\"progress\":13},{\"aid\":10,\"completed\":false,\"progress\":13},{\"aid\":11,\"completed\":false,\"progress\":13},{\"aid\":12,\"completed\":false,\"progress\":13},{\"aid\":13,\"completed\":false,\"progress\":13},{\"aid\":14,\"completed\":false,\"progress\":13},{\"aid\":95,\"completed\":false,\"progress\":119},{\"aid\":16,\"completed\":true,\"progress\":1},{\"aid\":17,\"completed\":true,\"progress\":50},{\"aid\":18,\"completed\":true,\"progress\":200},{\"aid\":19,\"completed\":false,\"progress\":246},{\"aid\":20,\"completed\":false,\"progress\":246},{\"aid\":21,\"completed\":false,\"progress\":246},{\"aid\":22,\"completed\":false,\"progress\":246},{\"aid\":23,\"completed\":false,\"progress\":246},{\"aid\":24,\"completed\":false,\"progress\":246},{\"aid\":25,\"completed\":false,\"progress\":246},{\"aid\":26,\"completed\":false,\"progress\":246},{\"aid\":27,\"completed\":false,\"progress\":246},{\"aid\":28,\"completed\":false,\"progress\":246},{\"aid\":29,\"completed\":false,\"progress\":246},{\"aid\":96,\"completed\":false,\"progress\":94},{\"aid\":68,\"completed\":true,\"progress\":5},{\"aid\":69,\"completed\":false,\"progress\":5},{\"aid\":70,\"completed\":false,\"progress\":5},{\"aid\":71,\"completed\":false,\"progress\":5},{\"aid\":72,\"completed\":false,\"progress\":5},{\"aid\":73,\"completed\":false,\"progress\":5},{\"aid\":74,\"completed\":false,\"progress\":5},{\"aid\":40,\"completed\":true,\"progress\":5},{\"aid\":41,\"completed\":false,\"progress\":9},{\"aid\":42,\"completed\":false,\"progress\":9},{\"aid\":43,\"completed\":false,\"progress\":9},{\"aid\":44,\"completed\":false,\"progress\":9},{\"aid\":45,\"completed\":false,\"progress\":9},{\"aid\":46,\"completed\":false,\"progress\":9},{\"aid\":97,\"completed\":false,\"progress\":4},{\"aid\":101,\"completed\":false,\"progress\":4},{\"aid\":98,\"completed\":false,\"progress\":14},{\"aid\":122,\"completed\":true,\"progress\":1},{\"aid\":120,\"completed\":false,\"progress\":3},{\"aid\":121,\"completed\":false,\"progress\":3},{\"aid\":106,\"completed\":false,\"progress\":1}]'),
(12,'(1) Zombie\'NewY',1,1045,-1,'0',90,1738426895,0,1738426895,0,'[{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":false,\"progress\":21},{\"aid\":32,\"completed\":false,\"progress\":21},{\"aid\":33,\"completed\":false,\"progress\":21},{\"aid\":34,\"completed\":false,\"progress\":21},{\"aid\":35,\"completed\":false,\"progress\":21},{\"aid\":36,\"completed\":false,\"progress\":21},{\"aid\":37,\"completed\":false,\"progress\":21},{\"aid\":38,\"completed\":false,\"progress\":21},{\"aid\":39,\"completed\":false,\"progress\":21},{\"aid\":75,\"completed\":true,\"progress\":100},{\"aid\":76,\"completed\":false,\"progress\":517},{\"aid\":77,\"completed\":false,\"progress\":517},{\"aid\":78,\"completed\":false,\"progress\":517},{\"aid\":79,\"completed\":false,\"progress\":517},{\"aid\":80,\"completed\":false,\"progress\":185},{\"aid\":81,\"completed\":false,\"progress\":185},{\"aid\":82,\"completed\":false,\"progress\":185},{\"aid\":83,\"completed\":false,\"progress\":185},{\"aid\":84,\"completed\":false,\"progress\":185},{\"aid\":95,\"completed\":false,\"progress\":10},{\"aid\":16,\"completed\":true,\"progress\":1},{\"aid\":17,\"completed\":false,\"progress\":10},{\"aid\":18,\"completed\":false,\"progress\":10},{\"aid\":19,\"completed\":false,\"progress\":10},{\"aid\":20,\"completed\":false,\"progress\":10},{\"aid\":21,\"completed\":false,\"progress\":10},{\"aid\":22,\"completed\":false,\"progress\":10},{\"aid\":23,\"completed\":false,\"progress\":10},{\"aid\":24,\"completed\":false,\"progress\":10},{\"aid\":25,\"completed\":false,\"progress\":10},{\"aid\":26,\"completed\":false,\"progress\":10},{\"aid\":27,\"completed\":false,\"progress\":10},{\"aid\":28,\"completed\":false,\"progress\":10},{\"aid\":29,\"completed\":false,\"progress\":10},{\"aid\":122,\"completed\":true,\"progress\":1},{\"aid\":123,\"completed\":true,\"progress\":1},{\"aid\":124,\"completed\":true,\"progress\":1},{\"aid\":1,\"completed\":true,\"progress\":1},{\"aid\":2,\"completed\":false,\"progress\":5},{\"aid\":3,\"completed\":false,\"progress\":5},{\"aid\":4,\"completed\":false,\"progress\":5},{\"aid\":5,\"completed\":false,\"progress\":5},{\"aid\":6,\"completed\":false,\"progress\":5},{\"aid\":7,\"completed\":false,\"progress\":5},{\"aid\":8,\"completed\":false,\"progress\":5},{\"aid\":9,\"completed\":false,\"progress\":5},{\"aid\":10,\"completed\":false,\"progress\":5},{\"aid\":11,\"completed\":false,\"progress\":5},{\"aid\":12,\"completed\":false,\"progress\":5},{\"aid\":13,\"completed\":false,\"progress\":5},{\"aid\":14,\"completed\":false,\"progress\":5}]'),
(13,'Trisha',1,1042,1,'80',0,1738507538,1738507538,1738428069,0,'[{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":false,\"progress\":44},{\"aid\":32,\"completed\":false,\"progress\":44},{\"aid\":33,\"completed\":false,\"progress\":44},{\"aid\":34,\"completed\":false,\"progress\":44},{\"aid\":35,\"completed\":false,\"progress\":44},{\"aid\":36,\"completed\":false,\"progress\":44},{\"aid\":37,\"completed\":false,\"progress\":44},{\"aid\":38,\"completed\":false,\"progress\":44},{\"aid\":39,\"completed\":false,\"progress\":44},{\"aid\":75,\"completed\":true,\"progress\":100},{\"aid\":76,\"completed\":true,\"progress\":1000},{\"aid\":77,\"completed\":false,\"progress\":2527},{\"aid\":78,\"completed\":false,\"progress\":2527},{\"aid\":79,\"completed\":false,\"progress\":2527},{\"aid\":80,\"completed\":true,\"progress\":500},{\"aid\":81,\"completed\":true,\"progress\":5000},{\"aid\":82,\"completed\":false,\"progress\":610},{\"aid\":83,\"completed\":false,\"progress\":610},{\"aid\":84,\"completed\":false,\"progress\":610},{\"aid\":95,\"completed\":false,\"progress\":122},{\"aid\":16,\"completed\":true,\"progress\":1},{\"aid\":17,\"completed\":true,\"progress\":50},{\"aid\":18,\"completed\":false,\"progress\":190},{\"aid\":19,\"completed\":false,\"progress\":190},{\"aid\":20,\"completed\":false,\"progress\":190},{\"aid\":21,\"completed\":false,\"progress\":190},{\"aid\":22,\"completed\":false,\"progress\":190},{\"aid\":23,\"completed\":false,\"progress\":190},{\"aid\":24,\"completed\":false,\"progress\":190},{\"aid\":25,\"completed\":false,\"progress\":190},{\"aid\":26,\"completed\":false,\"progress\":190},{\"aid\":27,\"completed\":false,\"progress\":190},{\"aid\":28,\"completed\":false,\"progress\":190},{\"aid\":29,\"completed\":false,\"progress\":190},{\"aid\":96,\"completed\":false,\"progress\":64},{\"aid\":1,\"completed\":true,\"progress\":1},{\"aid\":2,\"completed\":false,\"progress\":4},{\"aid\":3,\"completed\":false,\"progress\":4},{\"aid\":4,\"completed\":false,\"progress\":4},{\"aid\":5,\"completed\":false,\"progress\":4},{\"aid\":6,\"completed\":false,\"progress\":4},{\"aid\":7,\"completed\":false,\"progress\":4},{\"aid\":8,\"completed\":false,\"progress\":4},{\"aid\":9,\"completed\":false,\"progress\":4},{\"aid\":10,\"completed\":false,\"progress\":4},{\"aid\":11,\"completed\":false,\"progress\":4},{\"aid\":12,\"completed\":false,\"progress\":4},{\"aid\":13,\"completed\":false,\"progress\":4},{\"aid\":14,\"completed\":false,\"progress\":4},{\"aid\":61,\"completed\":false,\"progress\":3},{\"aid\":62,\"completed\":false,\"progress\":3},{\"aid\":63,\"completed\":false,\"progress\":3},{\"aid\":64,\"completed\":false,\"progress\":3},{\"aid\":65,\"completed\":false,\"progress\":3},{\"aid\":66,\"completed\":false,\"progress\":3},{\"aid\":67,\"completed\":false,\"progress\":3},{\"aid\":40,\"completed\":true,\"progress\":5},{\"aid\":41,\"completed\":false,\"progress\":5},{\"aid\":42,\"completed\":false,\"progress\":5},{\"aid\":43,\"completed\":false,\"progress\":5},{\"aid\":44,\"completed\":false,\"progress\":5},{\"aid\":45,\"completed\":false,\"progress\":5},{\"aid\":46,\"completed\":false,\"progress\":5},{\"aid\":68,\"completed\":true,\"progress\":5},{\"aid\":69,\"completed\":false,\"progress\":5},{\"aid\":70,\"completed\":false,\"progress\":5},{\"aid\":71,\"completed\":false,\"progress\":5},{\"aid\":72,\"completed\":false,\"progress\":5},{\"aid\":73,\"completed\":false,\"progress\":5},{\"aid\":74,\"completed\":false,\"progress\":5},{\"aid\":89,\"completed\":true,\"progress\":1},{\"aid\":122,\"completed\":true,\"progress\":1},{\"aid\":47,\"completed\":true,\"progress\":5},{\"aid\":48,\"completed\":false,\"progress\":6},{\"aid\":49,\"completed\":false,\"progress\":6},{\"aid\":50,\"completed\":false,\"progress\":6},{\"aid\":51,\"completed\":false,\"progress\":6},{\"aid\":52,\"completed\":false,\"progress\":6},{\"aid\":53,\"completed\":false,\"progress\":6},{\"aid\":98,\"completed\":false,\"progress\":1},{\"aid\":97,\"completed\":false,\"progress\":3},{\"aid\":123,\"completed\":true,\"progress\":1},{\"aid\":130,\"completed\":true,\"progress\":50},{\"aid\":131,\"completed\":false,\"progress\":50},{\"aid\":132,\"completed\":false,\"progress\":50},{\"aid\":133,\"completed\":false,\"progress\":50},{\"aid\":134,\"completed\":false,\"progress\":50},{\"aid\":120,\"completed\":false,\"progress\":2},{\"aid\":121,\"completed\":false,\"progress\":2}]'),
(14,'jamess',1,1000,-1,'0',0,1738517365,1738517365,1738517365,0,'[{\"aid\":30,\"completed\":true,\"progress\":1},{\"aid\":31,\"completed\":false,\"progress\":5},{\"aid\":32,\"completed\":false,\"progress\":5},{\"aid\":33,\"completed\":false,\"progress\":5},{\"aid\":34,\"completed\":false,\"progress\":5},{\"aid\":35,\"completed\":false,\"progress\":5},{\"aid\":36,\"completed\":false,\"progress\":5},{\"aid\":37,\"completed\":false,\"progress\":5},{\"aid\":38,\"completed\":false,\"progress\":5},{\"aid\":39,\"completed\":false,\"progress\":5},{\"aid\":120,\"completed\":false,\"progress\":3},{\"aid\":121,\"completed\":false,\"progress\":3}]');
/*!40000 ALTER TABLE `tw_accounts_data` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_accounts_items`
--

DROP TABLE IF EXISTS `tw_accounts_items`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_accounts_items` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `ItemID` int(11) NOT NULL,
  `Value` int(11) NOT NULL,
  `Settings` int(11) NOT NULL,
  `Enchant` int(11) NOT NULL,
  `Durability` int(11) NOT NULL DEFAULT 100,
  `UserID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  KEY `OwnerID` (`UserID`),
  KEY `ItemID` (`ItemID`),
  CONSTRAINT `tw_accounts_items_ibfk_2` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_accounts_items_ibfk_3` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=662 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_accounts_items`
--

LOCK TABLES `tw_accounts_items` WRITE;
/*!40000 ALTER TABLE `tw_accounts_items` DISABLE KEYS */;
INSERT INTO `tw_accounts_items` VALUES
(1,98,1,1,0,100,1),
(2,34,1,1,0,100,1),
(3,2,1,1,0,100,1),
(4,25,1,0,0,100,1),
(5,93,1,1,0,100,1),
(6,29,320,0,0,100,1),
(7,31,18,0,0,100,1),
(8,35,1,1,0,100,1),
(9,36,1,1,0,100,1),
(10,37,1,1,0,100,1),
(11,38,1,1,0,100,1),
(24,30,1,1,0,100,1),
(26,41,3,0,0,100,1),
(29,40,5,0,0,100,1),
(32,17,2,0,0,100,1),
(35,93,1,1,0,100,2),
(36,98,1,1,0,100,2),
(37,25,1,0,0,100,2),
(38,34,1,1,0,100,2),
(39,2,1,0,0,100,2),
(43,29,2645,0,0,100,2),
(46,30,1,1,0,100,2),
(47,98,1,1,0,100,3),
(48,25,1,1,0,100,3),
(49,93,1,1,0,100,3),
(50,2,1,1,0,100,3),
(51,34,1,1,0,100,3),
(52,3,1,0,0,100,3),
(54,29,520,0,0,100,3),
(56,40,1,0,0,100,2),
(60,30,1,0,0,100,3),
(61,17,10,0,0,100,3),
(62,102,1,0,0,100,3),
(64,9,109909,0,0,100,3),
(66,32,480,0,0,100,3),
(67,9,271,0,0,100,2),
(68,32,252,0,0,100,2),
(91,102,1,1,0,100,2),
(92,103,1,0,0,100,3),
(93,101,1,1,0,100,3),
(94,100,1,1,1,100,3),
(97,96,1,1,0,100,3),
(99,97,1,1,0,100,3),
(100,97,1,1,0,100,2),
(101,64,1,1,0,100,3),
(102,65,1,1,0,100,3),
(103,66,1,1,0,100,3),
(104,35,1,1,0,100,2),
(105,36,1,1,0,100,2),
(106,37,1,0,0,100,2),
(107,38,1,0,0,100,2),
(108,39,1,1,0,100,2),
(110,38,1,1,0,100,3),
(111,37,1,1,0,100,3),
(112,36,1,1,0,100,3),
(113,35,1,1,6,100,3),
(116,100,1,1,0,100,2),
(117,99,1,0,0,100,2),
(118,99,1,0,2,100,3),
(121,57,1,1,6,0,3),
(122,7,94350,0,0,100,2),
(123,7,374,0,0,100,3),
(124,67,1,0,0,100,3),
(125,68,1,0,0,100,3),
(126,69,1,1,6,100,3),
(128,103,1,0,0,100,2),
(129,11,205,0,0,100,3),
(130,11,95,0,0,100,2),
(131,12,97,0,0,100,2),
(132,13,100,0,0,100,2),
(133,94,101,0,0,100,2),
(135,12,103,0,0,100,3),
(136,8,100,0,0,100,3),
(137,13,80,0,0,100,3),
(138,14,95,1,0,100,3),
(142,24,3,0,0,100,2),
(147,23,1,0,0,100,2),
(148,22,41,0,0,100,2),
(150,44,1,0,0,100,2),
(153,46,1,0,0,100,2),
(154,33,7,0,0,100,3),
(155,22,9,0,0,100,3),
(157,47,1,0,0,100,2),
(159,34,1,1,0,100,4),
(160,93,1,1,0,100,4),
(161,98,1,1,0,100,4),
(162,25,1,0,0,100,4),
(163,2,1,0,0,100,4),
(164,3,1,1,0,100,4),
(166,29,480,0,0,100,4),
(169,30,1,1,0,100,4),
(170,1,4301,0,0,100,4),
(171,9,1520,0,0,100,4),
(173,33,11,0,0,100,4),
(174,22,12,0,0,100,4),
(175,102,1,1,0,100,4),
(176,77,1,0,0,100,4),
(177,76,1,0,0,100,4),
(178,60,1,1,0,100,4),
(179,97,1,1,0,100,4),
(189,49,1,0,0,100,4),
(190,15,93,0,0,100,2),
(191,15,99,0,0,100,4),
(192,17,6,0,0,100,4),
(193,51,1,1,1,100,2),
(194,33,20,0,0,100,2),
(195,52,1,0,0,100,2),
(197,54,5,0,0,100,2),
(198,72,1,0,0,100,2),
(199,90,1,1,0,100,2),
(200,91,1,1,0,100,2),
(201,92,12,0,0,100,2),
(202,17,7,0,0,100,2),
(203,109,2,0,0,100,2),
(204,50,11,0,0,100,2),
(205,112,5,0,0,100,2),
(206,107,6,0,0,100,2),
(207,93,1,1,0,100,5),
(208,34,1,1,0,100,5),
(209,25,1,0,0,100,5),
(210,2,1,1,0,100,5),
(211,98,1,1,0,100,5),
(212,3,1,1,0,100,5),
(214,29,40,0,0,100,5),
(215,40,6,0,0,100,5),
(216,30,1,1,0,100,5),
(217,1,126,0,0,100,5),
(218,92,2,0,0,100,4),
(219,7,1000,0,0,100,4),
(220,112,3,0,0,100,4),
(221,2,1,1,0,100,6),
(222,25,1,0,0,100,6),
(223,93,1,1,0,100,6),
(224,34,1,1,0,100,6),
(225,98,1,1,0,100,6),
(226,3,1,1,0,100,6),
(229,29,100,0,0,100,6),
(230,40,1,0,0,100,6),
(231,107,2,0,0,100,3),
(232,110,5,0,0,100,2),
(233,137,2,0,0,100,3),
(234,137,17,0,0,100,2),
(236,111,5,0,0,100,2),
(237,10,33,0,0,100,2),
(238,70,5,0,0,100,2),
(243,10,56,0,0,100,3),
(245,61,47,0,0,100,2),
(246,71,43,0,0,100,2),
(247,4,1,1,0,100,2),
(251,34,1,1,0,100,7),
(252,93,1,1,0,100,7),
(253,98,1,1,0,100,7),
(254,25,1,0,0,100,7),
(255,2,1,1,0,100,7),
(256,3,1,1,0,100,7),
(258,29,225,0,0,100,7),
(261,30,1,1,0,100,7),
(262,10,27,0,0,100,7),
(265,9,191,0,0,100,7),
(266,107,20,0,0,100,7),
(274,4,1,1,0,100,7),
(278,70,75,0,0,100,3),
(287,61,7,0,0,100,3),
(288,58,14,0,0,100,3),
(289,54,44,0,0,100,3),
(291,42,1,1,3,71,3),
(292,43,1,1,3,81,3),
(293,17,7,0,0,100,7),
(294,59,1,0,0,0,3),
(295,59,1,0,0,100,7),
(298,50,96,0,0,100,7),
(299,7,66,0,0,100,7),
(316,27,1,0,0,100,7),
(317,54,76,0,0,100,7),
(323,33,35,0,0,100,7),
(324,22,58,0,0,100,7),
(326,72,3,0,0,100,7),
(337,30,1,1,0,100,6),
(338,10,2,0,0,100,6),
(339,9,11,0,0,100,6),
(342,1,264,0,0,100,6),
(347,70,44,0,0,100,6),
(348,58,18,0,0,100,6),
(349,61,5,0,0,100,6),
(350,22,1,0,0,100,6),
(351,151,1,0,0,100,2),
(352,150,1,1,0,100,2),
(353,137,1,0,0,100,7),
(356,39,1,1,0,100,7),
(364,112,3,0,0,100,7),
(368,59,1,0,0,100,2),
(369,151,1,1,0,100,3),
(370,3,1,1,0,100,2),
(371,105,4,0,0,100,2),
(372,149,1,1,0,100,3),
(373,98,1,1,0,100,8),
(374,34,1,1,0,100,8),
(375,25,1,0,0,100,8),
(376,93,1,1,0,100,8),
(377,2,1,1,0,100,8),
(378,3,1,1,0,100,8),
(380,29,15,0,0,100,8),
(381,10,1,0,0,100,8),
(383,41,2,0,0,100,2),
(384,51,1,1,0,100,8),
(385,52,1,1,0,100,8),
(386,53,1,0,0,100,8),
(387,54,1,0,0,100,8),
(388,40,2,0,0,100,8),
(391,30,1,1,0,100,8),
(393,19,1,1,0,100,8),
(394,20,1,1,0,100,2),
(396,16,3,0,0,100,2),
(397,34,1,1,0,100,9),
(398,2,1,1,0,100,9),
(399,25,1,0,0,100,9),
(400,98,1,1,0,100,9),
(401,93,1,1,0,100,9),
(402,3,1,1,0,100,9),
(404,29,15,0,0,100,9),
(406,30,1,1,0,100,9),
(407,10,4,0,0,100,9),
(408,1,464,0,0,100,9),
(409,1,464,0,0,100,9),
(410,173,33,0,0,100,7),
(411,167,5,0,0,100,7),
(412,16,1,0,0,100,7),
(419,101,1,0,0,100,2),
(420,6,1,0,1,100,2),
(421,14,1,0,0,100,2),
(422,173,2,0,0,100,2),
(423,112,1,0,0,100,3),
(424,105,2,0,0,100,3),
(425,110,5,0,0,100,3),
(426,111,5,0,0,100,3),
(429,5,1,0,0,100,2),
(430,10,1,0,0,100,1),
(431,173,48,0,0,100,3),
(432,168,2,0,0,100,3),
(433,169,1,0,0,100,3),
(434,58,1,0,0,100,2),
(435,1,3586,0,0,100,2),
(436,98,1,1,0,100,10),
(437,34,1,1,0,100,10),
(438,25,1,0,0,100,10),
(439,93,1,1,0,100,10),
(440,2,1,1,0,100,10),
(441,3,1,1,0,100,10),
(443,29,5,0,0,100,10),
(446,30,1,1,0,100,10),
(447,10,2,0,0,100,10),
(448,1,35,0,0,100,10),
(449,70,3,0,0,100,10),
(450,58,1,0,0,100,10),
(451,98,1,1,0,100,11),
(452,34,1,1,0,100,11),
(453,25,1,0,0,100,11),
(454,93,1,1,0,100,11),
(455,2,1,1,0,100,11),
(456,3,1,1,0,100,11),
(458,29,35,0,0,100,11),
(459,40,1,0,0,100,11),
(461,30,1,1,0,100,11),
(462,10,16,0,0,100,11),
(464,22,6,0,0,100,11),
(474,70,14,0,0,100,7),
(475,61,1,0,0,100,7),
(476,58,3,0,0,100,7),
(482,105,1,0,0,100,11),
(483,9,13,0,0,100,11),
(484,33,8,0,0,100,11),
(485,92,4,0,0,100,7),
(486,128,1,0,0,100,7),
(487,39,1,1,0,100,11),
(488,4,1,1,0,100,11),
(489,1,2090,0,0,100,11),
(490,1,2090,0,0,100,11),
(491,17,3,0,0,100,11),
(492,70,2,0,0,100,11),
(493,111,5,0,0,100,11),
(494,173,2,0,0,100,11),
(495,54,2,0,0,100,11),
(496,5,1,1,0,100,7),
(497,6,1,1,0,100,7),
(499,126,1,0,0,100,2),
(500,126,1,0,0,100,3),
(501,98,1,1,0,100,12),
(502,2,1,1,0,100,12),
(503,25,1,0,0,100,12),
(504,34,1,1,0,100,12),
(505,93,1,1,0,100,12),
(506,150,1,0,0,100,3),
(507,3,1,1,0,100,12),
(509,29,5,0,0,100,12),
(510,40,11,0,0,100,12),
(511,40,27,0,0,100,3),
(516,30,1,1,0,100,12),
(517,10,7,0,0,100,12),
(518,1,176,0,0,100,12),
(519,9,1,0,0,100,12),
(520,39,1,1,0,100,12),
(521,22,1,0,0,100,12),
(522,4,1,1,0,100,12),
(523,5,1,1,0,100,12),
(524,6,1,1,0,100,12),
(525,98,1,1,0,100,13),
(526,25,1,0,0,100,13),
(527,34,1,1,0,100,13),
(528,93,1,1,0,100,13),
(529,2,1,1,0,100,13),
(530,3,1,1,0,100,13),
(532,29,110,0,0,100,13),
(533,40,2,0,0,100,13),
(535,30,1,1,0,100,13),
(536,10,20,0,0,100,13),
(539,39,1,0,0,100,3),
(540,39,1,1,0,100,13),
(544,171,1,0,0,100,3),
(549,167,4,0,0,100,13),
(551,168,8,0,0,100,13),
(556,167,1,0,0,100,3),
(560,172,2,0,0,100,13),
(561,170,2,0,0,100,13),
(562,50,4,0,0,100,3),
(563,47,1,0,0,100,3),
(567,169,1,0,0,100,13),
(571,1,1227,0,0,100,3),
(579,95,3,0,0,100,13),
(580,95,2,0,0,100,2),
(581,95,1,0,0,100,3),
(605,4,1,1,0,100,13),
(607,9,23,0,0,100,13),
(611,107,7,0,0,100,13),
(613,22,16,0,0,100,13),
(629,5,1,1,0,100,13),
(630,1,580,0,0,100,13),
(636,126,1,0,0,100,7),
(637,1,95,0,0,100,7),
(644,70,20,0,0,100,13),
(645,61,4,0,0,100,13),
(646,58,4,0,0,100,13),
(647,125,1,0,0,100,2),
(648,25,1,0,0,100,14),
(649,98,1,1,0,100,14),
(650,34,1,1,0,100,14),
(651,93,1,1,0,100,14),
(652,2,1,1,0,100,14),
(653,3,1,1,0,100,14),
(655,29,5,0,0,100,14),
(656,40,1,0,0,100,14),
(658,30,1,1,0,100,14),
(659,10,1,0,0,100,14),
(660,17,3,0,0,100,14),
(661,17,2,0,0,100,13);
/*!40000 ALTER TABLE `tw_accounts_items` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_accounts_mailbox`
--

DROP TABLE IF EXISTS `tw_accounts_mailbox`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_accounts_mailbox` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `UserID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL,
  `Sender` varchar(32) NOT NULL DEFAULT '''Game''',
  `Description` varchar(64) NOT NULL,
  `AttachedItems` longtext DEFAULT NULL,
  `Readed` tinyint(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `OwnerID` (`UserID`),
  CONSTRAINT `tw_accounts_mailbox_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=27 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_accounts_mailbox`
--

LOCK TABLES `tw_accounts_mailbox` WRITE;
/*!40000 ALTER TABLE `tw_accounts_mailbox` DISABLE KEYS */;
INSERT INTO `tw_accounts_mailbox` VALUES
(2,2,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":1,\"id\":3,\"value\":1}]}',0),
(3,2,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":1,\"id\":3,\"value\":1}]}',0),
(5,2,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":1,\"id\":3,\"value\":1}]}',0),
(6,3,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":93,\"value\":1}]}',0),
(7,2,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":34,\"value\":1}]}',0),
(8,3,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":34,\"value\":1}]}',0),
(9,3,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":102,\"value\":1}]}',0),
(10,2,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":102,\"value\":1}]}',1),
(11,2,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":102,\"value\":1}]}',0),
(12,2,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":98,\"value\":1}]}',0),
(13,3,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":98,\"value\":1}]}',0),
(14,3,'The sender heavens.','Console','Sent from console\n','{\"items\":[{\"durability\":0,\"enchant\":0,\"id\":57,\"value\":1}]}',0),
(15,3,'The sender heavens.','Console','Sent from console\n','{\"items\":[{\"durability\":0,\"enchant\":0,\"id\":7,\"value\":100000}]}',0),
(16,3,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":66,\"value\":1}]}',0),
(17,3,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":65,\"value\":1}]}',1),
(19,3,'Guild house is sold.','System','We returned some gold from your guild.\n','{\"items\":[{\"durability\":0,\"enchant\":0,\"id\":1,\"value\":0}]}',0),
(24,2,'Guild house is sold.','System','We returned some gold from your guild.\n','{\"items\":[{\"durability\":0,\"enchant\":0,\"id\":1,\"value\":9720}]}',0),
(25,2,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":150,\"value\":1}]}',0),
(26,2,'No place for item.','System','You already have this item.\nWe can\'t put it in inventory\n','{\"items\":[{\"durability\":100,\"enchant\":0,\"id\":39,\"value\":1}]}',0);
/*!40000 ALTER TABLE `tw_accounts_mailbox` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_accounts_professions`
--

DROP TABLE IF EXISTS `tw_accounts_professions`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_accounts_professions` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `ProfessionID` int(11) NOT NULL,
  `Data` longtext NOT NULL,
  `UserID` int(11) NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB AUTO_INCREMENT=91 DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_accounts_professions`
--

LOCK TABLES `tw_accounts_professions` WRITE;
/*!40000 ALTER TABLE `tw_accounts_professions` DISABLE KEYS */;
INSERT INTO `tw_accounts_professions` VALUES
(46,0,'{\"attributes\":{\"5\":0,\"6\":0},\"exp\":0,\"level\":1,\"up\":0}',1),
(47,2,'{\"attributes\":{\"7\":0,\"8\":0},\"exp\":0,\"level\":1,\"up\":0}',1),
(48,4,'{\"attributes\":{\"12\":1},\"exp\":0,\"level\":1,\"up\":0}',1),
(49,3,'{\"attributes\":{\"11\":1},\"exp\":0,\"level\":1,\"up\":0}',1),
(50,1,'{\"attributes\":{\"2\":0,\"4\":0},\"exp\":0,\"level\":1,\"up\":0}',1),
(51,0,'{\"attributes\":{\"5\":0,\"6\":0},\"exp\":88,\"level\":12,\"up\":11}',2),
(52,1,'{\"attributes\":{\"2\":0,\"4\":0},\"exp\":166,\"level\":7,\"up\":6}',2),
(53,2,'{\"attributes\":{\"7\":0,\"8\":0},\"exp\":614,\"level\":10,\"up\":9}',2),
(54,4,'{\"attributes\":{\"12\":1},\"exp\":2,\"level\":1,\"up\":0}',2),
(55,3,'{\"attributes\":{\"11\":1},\"exp\":0,\"level\":1,\"up\":0}',2),
(56,0,'{\"attributes\":{\"5\":0,\"6\":0},\"exp\":283,\"level\":4,\"up\":3}',3),
(57,1,'{\"attributes\":{\"2\":0,\"4\":0},\"exp\":18,\"level\":3,\"up\":2}',3),
(58,2,'{\"attributes\":{\"7\":0,\"8\":0},\"exp\":0,\"level\":1,\"up\":0}',3),
(59,4,'{\"attributes\":{\"12\":1},\"exp\":2,\"level\":3,\"up\":2}',3),
(60,3,'{\"attributes\":{\"11\":1},\"exp\":5,\"level\":3,\"up\":2}',3),
(61,0,'{\"attributes\":{\"5\":0,\"6\":0},\"exp\":0,\"level\":1,\"up\":0}',10),
(62,1,'{\"attributes\":{\"2\":0,\"4\":0},\"exp\":0,\"level\":1,\"up\":0}',10),
(63,2,'{\"attributes\":{\"7\":0,\"8\":0},\"exp\":0,\"level\":1,\"up\":0}',10),
(64,4,'{\"attributes\":{\"12\":1},\"exp\":4,\"level\":1,\"up\":0}',10),
(65,3,'{\"attributes\":{\"11\":1},\"exp\":0,\"level\":1,\"up\":0}',10),
(66,0,'{\"attributes\":{\"5\":0,\"6\":0},\"exp\":350,\"level\":9,\"up\":8}',11),
(67,2,'{\"attributes\":{\"7\":0,\"8\":0},\"exp\":0,\"level\":1,\"up\":0}',11),
(68,4,'{\"attributes\":{\"12\":4},\"exp\":67,\"level\":5,\"up\":1}',11),
(69,3,'{\"attributes\":{\"11\":1},\"exp\":1,\"level\":1,\"up\":0}',11),
(70,1,'{\"attributes\":{\"2\":0,\"4\":0},\"exp\":0,\"level\":1,\"up\":0}',11),
(71,0,'{\"attributes\":{\"5\":0,\"6\":0},\"exp\":0,\"level\":1,\"up\":0}',7),
(72,1,'{\"attributes\":{\"2\":0,\"4\":0},\"exp\":336,\"level\":5,\"up\":4}',7),
(73,2,'{\"attributes\":{\"7\":0,\"8\":0},\"exp\":495,\"level\":7,\"up\":6}',7),
(74,4,'{\"attributes\":{\"12\":3},\"exp\":36,\"level\":3,\"up\":0}',7),
(75,3,'{\"attributes\":{\"11\":1},\"exp\":1,\"level\":1,\"up\":0}',7),
(76,0,'{\"attributes\":{\"5\":0,\"6\":0},\"exp\":0,\"level\":1,\"up\":0}',12),
(77,1,'{\"attributes\":{\"2\":0,\"4\":0},\"exp\":0,\"level\":1,\"up\":0}',12),
(78,2,'{\"attributes\":{\"7\":0,\"8\":0},\"exp\":0,\"level\":1,\"up\":0}',12),
(79,4,'{\"attributes\":{\"12\":1},\"exp\":6,\"level\":1,\"up\":0}',12),
(80,3,'{\"attributes\":{\"11\":1},\"exp\":0,\"level\":1,\"up\":0}',12),
(81,0,'{\"attributes\":{\"5\":0,\"6\":0},\"exp\":55,\"level\":4,\"up\":3}',13),
(82,1,'{\"attributes\":{\"2\":5,\"4\":0},\"exp\":670,\"level\":6,\"up\":0}',13),
(83,2,'{\"attributes\":{\"7\":0,\"8\":0},\"exp\":0,\"level\":1,\"up\":0}',13),
(84,4,'{\"attributes\":{\"12\":5},\"exp\":285,\"level\":5,\"up\":0}',13),
(85,3,'{\"attributes\":{\"11\":2},\"exp\":123,\"level\":3,\"up\":1}',13),
(86,0,'{\"attributes\":{\"5\":0,\"6\":0},\"exp\":0,\"level\":1,\"up\":0}',14),
(87,1,'{\"attributes\":{\"2\":0,\"4\":0},\"exp\":0,\"level\":1,\"up\":0}',14),
(88,2,'{\"attributes\":{\"7\":0,\"8\":0},\"exp\":0,\"level\":1,\"up\":0}',14),
(89,4,'{\"attributes\":{\"12\":1},\"exp\":0,\"level\":1,\"up\":0}',14),
(90,3,'{\"attributes\":{\"11\":1},\"exp\":0,\"level\":1,\"up\":0}',14);
/*!40000 ALTER TABLE `tw_accounts_professions` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_accounts_quests`
--

DROP TABLE IF EXISTS `tw_accounts_quests`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_accounts_quests` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `QuestID` int(11) DEFAULT NULL,
  `UserID` int(11) NOT NULL,
  `Type` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  UNIQUE KEY `UK_tw_accounts_quests` (`QuestID`,`UserID`),
  KEY `OwnerID` (`UserID`),
  KEY `tw_accounts_quests_ibfk_4` (`QuestID`),
  CONSTRAINT `tw_accounts_quests_ibfk_3` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_accounts_quests_ibfk_4` FOREIGN KEY (`QuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=337 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_accounts_quests`
--

LOCK TABLES `tw_accounts_quests` WRITE;
/*!40000 ALTER TABLE `tw_accounts_quests` DISABLE KEYS */;
INSERT INTO `tw_accounts_quests` VALUES
(95,2,3,2),
(97,3,3,2),
(98,4,3,2),
(105,5,3,2),
(108,6,3,2),
(117,2,2,2),
(132,7,3,2),
(133,8,3,2),
(134,9,3,2),
(135,10,3,2),
(136,11,3,2),
(137,12,3,2),
(138,13,3,2),
(139,14,3,2),
(151,15,3,2),
(153,16,3,2),
(154,17,3,2),
(157,18,3,2),
(158,19,3,2),
(159,20,3,2),
(194,3,2,2),
(195,4,2,2),
(196,5,2,2),
(204,6,2,2),
(206,7,2,2),
(208,9,2,2),
(209,10,2,2),
(210,11,2,2),
(215,8,2,2),
(218,12,2,2),
(219,13,2,2),
(220,14,2,2),
(221,15,2,2),
(222,16,2,2),
(223,17,2,2),
(224,18,2,2),
(227,19,2,2),
(228,20,2,2),
(233,21,2,2),
(234,21,3,1),
(235,22,2,1),
(236,1,4,2),
(237,2,4,2),
(238,3,4,2),
(239,4,4,2),
(240,5,4,2),
(241,6,4,2),
(242,7,4,2),
(243,8,4,2),
(244,9,4,2),
(245,10,4,2),
(246,11,4,2),
(247,12,4,2),
(248,13,4,2),
(249,14,4,2),
(250,15,4,2),
(251,16,4,2),
(252,17,4,2),
(253,18,4,2),
(254,19,4,2),
(255,20,4,2),
(256,21,4,2),
(257,22,4,2),
(258,23,4,2),
(259,24,4,2),
(260,1,5,2),
(261,2,5,2),
(262,3,5,1),
(274,1,7,2),
(275,2,7,2),
(276,3,7,2),
(277,4,7,2),
(278,5,7,2),
(280,36,3,1),
(281,1,6,2),
(282,2,6,2),
(283,3,6,2),
(284,4,6,2),
(285,5,6,1),
(286,6,7,2),
(287,7,7,2),
(288,8,7,2),
(289,9,7,2),
(290,10,7,2),
(291,11,7,2),
(294,12,7,2),
(295,13,7,1),
(298,1,8,2),
(299,1,9,2),
(300,1,2,1),
(301,2,9,2),
(302,3,9,1),
(308,28,3,2),
(309,27,3,1),
(310,1,10,2),
(311,1,11,2),
(312,2,11,2),
(313,3,11,2),
(314,4,11,2),
(315,5,11,1),
(316,1,12,2),
(317,1,13,2),
(318,2,13,2),
(319,3,13,2),
(320,4,13,2),
(321,5,13,2),
(322,6,13,2),
(323,7,13,2),
(324,8,13,2),
(325,9,13,2),
(326,10,13,2),
(327,11,13,2),
(328,12,13,1),
(329,41,7,2),
(331,50,2,2),
(333,51,2,2),
(334,41,13,2),
(335,1,14,2),
(336,41,14,1);
/*!40000 ALTER TABLE `tw_accounts_quests` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_accounts_skills`
--

DROP TABLE IF EXISTS `tw_accounts_skills`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_accounts_skills` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `SkillID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `Level` int(11) NOT NULL,
  `UsedByEmoticon` int(11) DEFAULT -1,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `SkillID` (`SkillID`),
  KEY `OwnerID` (`UserID`),
  CONSTRAINT `tw_accounts_skills_ibfk_1` FOREIGN KEY (`SkillID`) REFERENCES `tw_skills_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_accounts_skills_ibfk_2` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=30 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_accounts_skills`
--

LOCK TABLES `tw_accounts_skills` WRITE;
/*!40000 ALTER TABLE `tw_accounts_skills` DISABLE KEYS */;
INSERT INTO `tw_accounts_skills` VALUES
(1,7,2,1,-1),
(2,1,2,2,-1),
(3,6,4,4,-1),
(4,1,4,3,-1),
(5,7,4,1,-1),
(6,4,4,1,-1),
(7,9,4,1,-1),
(8,8,2,1,-1),
(9,6,2,1,-1),
(10,3,7,2,-1),
(11,7,7,1,-1),
(12,7,6,2,-1),
(13,2,2,1,-1),
(14,9,2,2,-1),
(15,12,2,1,-1),
(16,5,2,1,-1),
(17,10,2,1,-1),
(18,11,2,1,-1),
(19,3,2,1,-1),
(20,4,7,1,-1),
(21,10,3,5,-1),
(22,11,3,1,-1),
(23,2,3,1,2),
(24,7,11,4,-1),
(25,6,3,1,0),
(26,12,3,1,1),
(27,5,3,1,2),
(28,4,3,1,-1),
(29,5,13,1,-1);
/*!40000 ALTER TABLE `tw_accounts_skills` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_achievements`
--

DROP TABLE IF EXISTS `tw_achievements`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_achievements` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Type` int(11) NOT NULL,
  `Criteria` int(11) NOT NULL,
  `Required` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL,
  `Reward` varchar(256) DEFAULT NULL,
  `AchievementPoint` int(11) NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB AUTO_INCREMENT=135 DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_achievements`
--

LOCK TABLES `tw_achievements` WRITE;
/*!40000 ALTER TABLE `tw_achievements` DISABLE KEYS */;
INSERT INTO `tw_achievements` VALUES
(1,1,0,1,'PVP 1.1 | The first one?','{\r\n    \"exp\": 10\r\n}',1),
(2,1,0,10,'PVP 1.2 | A rookie hit man','{\r\n    \"exp\": 50,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 150\r\n        }\r\n    ]\r\n}',1),
(3,1,0,25,'PVP 1.3 | The silent killer?','{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 250\r\n        }\r\n    ]\r\n}',1),
(4,1,0,50,'PVP 1.4 | Bounty hunter','{\r\n    \"exp\": 200,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}',1),
(5,1,0,100,'PVP 2.1 | The Butcher','{\r\n    \"exp\": 400,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 450\r\n        }\r\n    ]\r\n}',4),
(6,1,0,200,'PVP 2.2 | King of the battle','{\r\n    \"exp\": 800,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1000\r\n        }\r\n    ]\r\n}',4),
(7,1,0,500,'PVP 2.3 | Merciless tee','{\r\n    \"exp\": 1300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2000\r\n        }\r\n    ]\r\n}',5),
(8,1,0,750,'PVP 2.4 | The unstoppable','{\r\n    \"exp\": 1800,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 3000\r\n        }\r\n    ]\r\n}',6),
(9,1,0,1000,'PVP 2.5 | Lord of War','{\r\n    \"exp\": 2000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 139,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',10),
(10,1,0,5000,'PVP 3.1 | God of destruction','{\r\n    \"exp\": 2500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}',30),
(11,1,0,10000,'PVP 3.2 | Predator and prey','{\r\n    \"exp\": 5000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 10000\r\n        }\r\n    ]\r\n}',40),
(12,1,0,15000,'PVP 3.3 | Scarred face','{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 20000\r\n        }\r\n    ]\r\n}',55),
(13,1,0,20000,'PVP 3.4 | Dominator','{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 30000\r\n        }\r\n    ]\r\n}',60),
(14,1,0,50000,'PVP 3.5 | Legend of the DM','{\r\n    \"exp\": 30000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 140,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',99),
(16,2,0,1,'PVE 1.1 | Rookie in the MMORPG','{\r\n    \"exp\": 10,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5\r\n        }\r\n    ]\r\n}',1),
(17,2,0,50,'PVE 1.2 | First booty?','{\r\n    \"exp\": 20,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 50\r\n        }\r\n    ]\r\n}',1),
(18,2,0,200,'PVE 1.3 | Hunter\'s apprentice','{\r\n    \"exp\": 60,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}',1),
(19,2,0,800,'PVE 1.4 | Defender of Gridania','{\r\n    \"exp\": 400,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1000\r\n        }\r\n    ]\r\n}',1),
(20,2,0,3200,'PVE 1.5 | Who\'s in charge here?','{\r\n    \"exp\": 800,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1500\r\n        }\r\n    ]\r\n}',1),
(21,2,0,9600,'PVE 2.1 | Who\'s next?','{\r\n    \"exp\": 1500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 4000\r\n        }\r\n    ]\r\n}',5),
(22,2,0,28800,'PVE 2.3 | A slayer of evil','{\r\n    \"exp\": 2500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 7000\r\n        }\r\n    ]\r\n}',10),
(23,2,0,86400,'PVE 2.4 | Meat grinder','{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 18000\r\n        }\r\n    ]\r\n}',15),
(24,2,0,259200,'PVE 2.5 | The legendary traveler','{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 24000\r\n        }\r\n    ]\r\n}',30),
(25,2,0,518400,'PVE 3.1 | The eternal hunter','{\r\n    \"exp\": 25000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 50000\r\n        }\r\n    ]\r\n}',40),
(26,2,0,1036800,'PVE 3.2 | Who will survive?','{\r\n    \"exp\": 50000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 75000\r\n        }\r\n    ]\r\n}',55),
(27,2,0,1555200,'PVE 3.3 | A trail of blood','{\r\n    \"exp\": 70000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100000\r\n        }\r\n    ]\r\n}',70),
(28,2,0,2177280,'PVE 3.4 | Reaper of Souls','{\r\n    \"exp\": 80000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100000\r\n        }\r\n    ]\r\n}',80),
(29,2,0,3265920,'PVE 3.5 | Dungeon King','{\r\n    \"exp\": 100000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 142,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',99),
(30,4,0,1,'Died 1.1 | First blood','{\r\n    \"exp\": 10\r\n}',1),
(31,4,0,50,'Died 1.2 | You\'re so stubborn','{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}',1),
(32,4,0,200,'Died 1.3 | Master of Respawn','{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 150\r\n        }\r\n    ]\r\n}',1),
(33,4,0,400,'Died 1.4 | Immortal... almost','{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 200\r\n        }\r\n    ]\r\n}',1),
(34,4,0,1000,'Died 1.5 | Unbreakable? joke','{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 500\r\n        }\r\n    ]\r\n}',1),
(35,4,0,5000,'Died 2.1 | Infinite loop','{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1000\r\n        }\r\n    ]\r\n}',5),
(36,4,0,10000,'Died 2.2 | Death is your ally','{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}',5),
(37,4,0,50000,'Died 2.3 | Death by boredom','{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 10000\r\n        }\r\n    ]\r\n}',25),
(38,4,0,100000,'Died 2.4 | Self-enemy','{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 20000\r\n        }\r\n    ]\r\n}',50),
(39,4,0,200000,'Died 2.5 | Beyond...','{\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 50000\r\n        }\r\n    ]\r\n}',99),
(40,11,0,5,'Tank 1. | Rookie in armor','{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}',1),
(41,11,0,15,'Tank 2. | Iron Guardian','{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}',3),
(42,11,0,25,'Tank 3. | Lockout Wizard','{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 600\r\n        }\r\n    ]\r\n}',6),
(43,11,0,50,'Tank 4. | A fortress in armor','{\r\n    \"exp\": 500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}',12),
(44,11,0,100,'Tank 5. | Iron Garrison','{\r\n    \"exp\": 1000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}',24),
(45,11,0,150,'Tank 6. | An unconquered warrior','{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}',36),
(46,11,0,200,'Tank 7. | Lord of Armor','{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 143,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',48),
(47,11,1,5,'Dmg 1. | A shadow in the night','{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}',1),
(48,11,1,15,'Dmg 2. | A budding assassin','{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}',3),
(49,11,1,25,'Dmg 3. | A ghost in the shadows','{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 600\r\n        }\r\n    ]\r\n}',6),
(50,11,1,50,'Dmg 4. | Blade of Fate','{\r\n    \"exp\": 500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}',12),
(51,11,1,100,'Dmg 5. | The Dark Avenger','{\r\n    \"exp\": 1000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}',24),
(52,11,1,150,'Dmg 6. | The death stare','{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}',36),
(53,11,1,200,'Dmg 7. | Lord of DMG','{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 144,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',48),
(54,11,2,5,'Healer 1. | Light of Hope','{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}',1),
(55,11,2,15,'Healer 2. | Beginning healer','{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}',3),
(56,11,2,25,'Healer 3. | Guardian of Life','{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 600\r\n        }\r\n    ]\r\n}',6),
(57,11,2,50,'Healer 4. | Recovery Wizard','{\r\n    \"exp\": 500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}',12),
(58,11,2,100,'Healer 5. | Legend of Healing','{\r\n    \"exp\": 1000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}',24),
(59,11,2,150,'Healer 6. | Bright angel','{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}',36),
(60,11,2,200,'Healer 7. | Lord of Life','{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 145,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',48),
(61,11,3,5,'Miner 1. | Rookie and pickaxe','{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}',1),
(62,11,3,15,'Miner 2. | Ore Seeker','{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}',2),
(63,11,3,25,'Miner 3. | Ore-savvy','{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 600\r\n        }\r\n    ]\r\n}',4),
(64,11,3,50,'Miner 4. | Master of Mining','{\r\n    \"exp\": 500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}',8),
(65,11,3,100,'Miner 5. | The Digger of Depths','{\r\n    \"exp\": 1000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}',16),
(66,11,3,150,'Miner 6. | King of the mines','{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}',32),
(67,11,3,200,'Miner 7. | Master of the deep','{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 146,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',48),
(68,11,4,5,'Farmer 1. | A seed of hope','{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}',1),
(69,11,4,15,'Farmer 2. | Rookie farmer','{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 300\r\n        }\r\n    ]\r\n}',2),
(70,11,4,25,'Farmer 3. | Green Thumb','{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 600\r\n        }\r\n    ]\r\n}',4),
(71,11,4,50,'Farmer 4. | Harvest Master','{\r\n    \"exp\": 500,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}',8),
(72,11,4,100,'Farmer 5. | Lord of the beds','{\r\n    \"exp\": 1000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 5000\r\n        }\r\n    ]\r\n}',16),
(73,11,4,150,'Farmer 6. | Gatherer of Plenty','{\r\n    \"exp\": 10000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 15000\r\n        }\r\n    ]\r\n}',32),
(74,11,4,200,'Farmer 7. | Harvest King','{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 147,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',48),
(75,5,0,100,'DAMAGE 1. | A light punch','{\r\n    \"exp\": 10\r\n}',1),
(76,5,0,1000,'DAMAGE 2. | Attempt to do damage','{\r\n    \"exp\": 100\r\n}',5),
(77,5,0,10000,'DAMAGE 3. | A serious threat?','{\r\n    \"exp\": 500\r\n}',10),
(78,5,0,100000,'DAMAGE 4. | Armageddon','{\r\n    \"exp\": 1000\r\n}',20),
(79,5,0,1000000,'DAMAGE 5. | Lord of Damage','{\r\n    \"exp\": 5000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 148,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',40),
(80,8,1,500,'Gold 1. | The first coin','{\r\n    \"exp\": 100\r\n}',1),
(81,8,1,5000,'Gold 2. | An aspiring rich man','{\r\n    \"exp\": 500\r\n}',2),
(82,8,1,100000,'Gold 3. | A lover of glitter','{\r\n    \"exp\": 1000\r\n}',4),
(83,8,1,1000000,'Gold 4. | MMORPG Oligarch','{\r\n    \"exp\": 5000\r\n}',8),
(84,8,1,10000000,'Gold 5. | King of Gold','{\r\n    \"exp\": 15000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 149,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',16),
(85,6,100,1,'Weapons | Pizdamet LOL?','{\r\n    \"exp\": 200\r\n}',1),
(86,6,103,1,'Weapons | A wall of laser. What?','{\r\n    \"exp\": 200\r\n}',1),
(87,6,102,1,'Weapons | Don\'t blow yourself up','{\r\n    \"exp\": 200\r\n}',1),
(88,6,99,1,'Weapons | A hammer on a leash','{\r\n    \"exp\": 200\r\n}',1),
(89,7,95,1,'Items | It\'s guild time','{\r\n    \"exp\": 50\r\n}',1),
(90,6,96,1,'Module | I wish I had a new skin','{\r\n    \"exp\": 50\r\n}',1),
(91,6,97,1,'Module | I\'m not in pain','{\r\n    \"exp\": 100\r\n}',1),
(92,6,64,1,'Module | What a poisonous tail','{\r\n    \"exp\": 100\r\n}',1),
(93,6,65,1,'Module | Jump and explode','{\r\n    \"exp\": 100\r\n}',1),
(94,6,66,1,'Module | I\'m a spider','{\r\n    \"exp\": 100\r\n}',1),
(95,3,31,5000,'Defeat | Chigoe Killer','{\r\n    \"exp\": 200\r\n}',1),
(96,3,32,5000,'Defeat | Anole Killer','{\r\n    \"exp\": 100,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1500\r\n        }\r\n    ]\r\n}',5),
(97,3,33,5000,'Defeat | Scrambler Killer','{\r\n    \"exp\": 120,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1700\r\n        }\r\n    ]\r\n}',5),
(98,3,34,5000,'Defeat | Mushroom Killer','{\r\n    \"exp\": 110,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 1600\r\n        }\r\n    ]\r\n}',5),
(99,3,35,5000,'Defeat | Shadow shape Killer','{\r\n    \"exp\": 150,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2000\r\n        }\r\n    ]\r\n}',5),
(100,3,36,5000,'Defeat | Winged beast Killer','{\r\n    \"exp\": 170,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2300\r\n        }\r\n    ]\r\n}',5),
(101,3,38,5000,'Defeat | Spider Killer','{\r\n    \"exp\": 180,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2400\r\n        }\r\n    ]\r\n}',5),
(102,3,39,5000,'Defeat | Bear Killer','{\r\n    \"exp\": 190,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2400\r\n        }\r\n    ]\r\n}',5),
(103,3,40,5000,'Defeat | Orc Tee Killer','{\r\n    \"exp\": 200,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2500\r\n        }\r\n    ]\r\n}',5),
(104,3,41,5000,'Defeat | Dynamt ogre Killer','{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 3000\r\n        }\r\n    ]\r\n}',5),
(105,3,42,5000,'Defeat | Mad Wolf Killer','{\r\n    \"exp\": 200,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 2500\r\n        }\r\n    ]\r\n}',5),
(106,3,43,5000,'Defeat | Doll Tee Killer','{\r\n    \"exp\": 300,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 4000\r\n        }\r\n    ]\r\n}',5),
(107,3,44,5000,'Defeat | Zombie Killer','{\r\n    \"exp\": 5000,\r\n    \"items\": [\r\n        {\r\n            \"id\": 1,\r\n            \"value\": 6000\r\n        }\r\n    ]\r\n}',5),
(108,10,1,1,'Worlds | An aspiring wanderer','{\r\n    \"exp\": 10\r\n}',1),
(109,9,2,1,'Craft | Hard-earned iron','{\r\n    \"exp\": 100\r\n}',1),
(110,9,9,1,'Craft | I\'m stronger now','{\r\n    \"exp\": 80\r\n}',1),
(111,9,12,1,'Craft | The iron guy','{\r\n    \"exp\": 200\r\n}',1),
(112,9,15,1,'Craft | Hands-free work','{\r\n    \"exp\": 80\r\n}',1),
(113,9,18,1,'Craft | First pickaxe for ore','{\r\n    \"exp\": 50\r\n}',1),
(114,9,2,500,'Craft | Blacksmith','{\r\n    \"exp\": 500\r\n}',5),
(115,9,23,500,'Craft | Leatherworker','{\r\n    \"exp\": 500\r\n}',5),
(116,9,22,500,'Craft | Poison lover','{\r\n    \"exp\": 500\r\n}',5),
(117,9,21,500,'Craft | Thread weaver','{\r\n    \"exp\": 500\r\n}',5),
(118,9,72,1,'Craft | King of the junkyard','{\r\n    \"exp\": 50\r\n}',1),
(119,7,16,50,'Items | A capsule experience?','{\r\n    \"exp\": 50\r\n}',1),
(120,7,17,50,'Items | Golden find','{\r\n    \"exp\": 50\r\n}',1),
(121,8,17,15,'Items | I could use a drink','{\r\n    \"exp\": 50\r\n}',1),
(122,6,4,1,'Weapons | Whoo-hoo. Come here','{\r\n    \"exp\": 30\r\n}',1),
(123,6,5,1,'Weapons | It\'s gonna boom','{\r\n    \"exp\": 40\r\n}',1),
(124,6,6,1,'Weapons | Pew pew. I\'m a sniper','{\r\n    \"exp\": 50\r\n}',1),
(125,8,7,50,'Synth 1. | How do you get back','{\r\n    \"exp\": 10\r\n}',1),
(126,8,7,300,'Synth 2. | Disassembled. And?','{\r\n    \"exp\": 60\r\n}',2),
(127,8,7,1000,'Synth 3. | Disassembler','{\r\n    \"exp\": 150\r\n}',3),
(128,8,7,10000,'Synth 4. | Material Generator','{\r\n    \"exp\": 500\r\n}',4),
(129,8,7,100000,'Synth 5. |  Lord Desynthesizer','{\r\n    \"exp\": 1500\r\n}',5),
(130,7,8,50,'Deliverer 1. | I\'m a courier?','{\r\n    \"exp\": 10\r\n}',1),
(131,7,8,5000,'Deliverer 2. | New job','{\r\n    \"exp\": 100\r\n}',2),
(132,7,8,50000,'Deliverer 3. | Where\'s the tip?','{\r\n    \"exp\": 500\r\n}',3),
(133,7,8,500000,'Deliverer 4. | Who else but me?','{\r\n    \"exp\": 500\r\n}',4),
(134,7,8,5000000,'Deliverer 5. | Grocery delivery','{\r\n    \"exp\": 2500\r\n}',5);
/*!40000 ALTER TABLE `tw_achievements` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_aethers`
--

DROP TABLE IF EXISTS `tw_aethers`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_aethers` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(64) NOT NULL DEFAULT 'Teleport name',
  `WorldID` int(11) NOT NULL,
  `TeleX` int(11) NOT NULL,
  `TeleY` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `WorldID` (`WorldID`),
  CONSTRAINT `tw_aethers_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_aethers`
--

LOCK TABLES `tw_aethers` WRITE;
/*!40000 ALTER TABLE `tw_aethers` DISABLE KEYS */;
INSERT INTO `tw_aethers` VALUES
(1,'Gridania (Center)',1,19552,4718),
(2,'Apartaments 1 (Left)',1,13438,1747),
(3,'Apartaments 2  (Right)',1,25600,2661),
(4,'Swamp (Left)',1,8063,3781);
/*!40000 ALTER TABLE `tw_aethers` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_attributes`
--

DROP TABLE IF EXISTS `tw_attributes`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_attributes` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL,
  `Price` int(11) NOT NULL,
  `Group` int(11) NOT NULL COMMENT '	0-tank. 1-healer. 2-dps. 3-weapon. 4-damage. 5-jobs. 6-others.',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_attributes`
--

LOCK TABLES `tw_attributes` WRITE;
/*!40000 ALTER TABLE `tw_attributes` DISABLE KEYS */;
INSERT INTO `tw_attributes` VALUES
(1,'DMG',5,4),
(2,'Attack SPD',1,2),
(3,'Crit DMG',1,4),
(4,'Crit',1,2),
(5,'HP',1,0),
(6,'Lucky',1,0),
(7,'MP',1,1),
(8,'Vampirism',1,1),
(9,'Ammo Regen',1,3),
(10,'Ammo',5,3),
(11,'Efficiency',1,5),
(12,'Extraction',1,5),
(13,'Hammer DMG',5,4),
(14,'Gun DMG',5,4),
(15,'Shotgun DMG',5,4),
(16,'Grenade DMG',5,4),
(17,'Rifle DMG',5,4),
(18,'Lucky Drop',1,6),
(19,'Eidolon PWR',1,6),
(20,'Gold Capacity',1,6);
/*!40000 ALTER TABLE `tw_attributes` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_auction_slots`
--

DROP TABLE IF EXISTS `tw_auction_slots`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_auction_slots` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `ItemID` int(11) NOT NULL,
  `Value` int(11) NOT NULL,
  `Enchant` int(11) NOT NULL DEFAULT 0,
  `Price` int(11) NOT NULL,
  `OwnerID` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `ItemID` (`ItemID`),
  KEY `OwnerID` (`OwnerID`),
  KEY `Price` (`Price`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_auction_slots`
--

LOCK TABLES `tw_auction_slots` WRITE;
/*!40000 ALTER TABLE `tw_auction_slots` DISABLE KEYS */;
/*!40000 ALTER TABLE `tw_auction_slots` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_bots_info`
--

DROP TABLE IF EXISTS `tw_bots_info`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_bots_info` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(32) NOT NULL DEFAULT 'Bot name',
  `JsonTeeInfo` varchar(128) NOT NULL DEFAULT '{ "skin": "default", 	"custom_color": 0, 	"color_body": 0, 	"color_feer": 0}',
  `EquippedModules` varchar(64) NOT NULL DEFAULT '0',
  `SlotHammer` int(11) DEFAULT 2,
  `SlotGun` int(11) DEFAULT 3,
  `SlotShotgun` int(11) DEFAULT 4,
  `SlotGrenade` int(11) DEFAULT 5,
  `SlotRifle` int(11) DEFAULT 6,
  `SlotArmor` int(11) DEFAULT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `SlotWings` (`SlotArmor`),
  KEY `SlotHammer` (`SlotHammer`),
  KEY `SlotGun` (`SlotGun`),
  KEY `tw_bots_world_ibfk_4` (`SlotShotgun`),
  KEY `SlotGrenade` (`SlotGrenade`),
  KEY `SlotRifle` (`SlotRifle`),
  CONSTRAINT `tw_bots_info_ibfk_1` FOREIGN KEY (`SlotArmor`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_info_ibfk_2` FOREIGN KEY (`SlotGrenade`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_info_ibfk_3` FOREIGN KEY (`SlotGun`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_info_ibfk_4` FOREIGN KEY (`SlotRifle`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_info_ibfk_5` FOREIGN KEY (`SlotShotgun`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=1019 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_bots_info`
--

LOCK TABLES `tw_bots_info` WRITE;
/*!40000 ALTER TABLE `tw_bots_info` DISABLE KEYS */;
INSERT INTO `tw_bots_info` VALUES
(1,'Batya','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"chinese_by_whis\"}','0',2,3,4,5,6,NULL),
(2,'Mamya','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"giraffe\"}','0',2,3,4,5,6,NULL),
(3,'Bertennant','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"wartee\"}','0',2,3,4,5,6,NULL),
(4,'Protector','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Soldier\"}','90,91,35,36,37',102,3,4,100,101,84),
(5,'Lead','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"TFT_Ninja\"}','0',2,3,4,5,6,NULL),
(6,'Mother Miounne','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Hellokittygirl\"}','0',2,3,4,5,6,NULL),
(7,'Madelle','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"cutee_glow\"}','0',2,3,4,5,6,NULL),
(8,'Athelyna','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"robin_hood\"}','0',2,3,4,5,6,NULL),
(9,'Jillian','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"swordl\"}','0',NULL,NULL,NULL,NULL,NULL,NULL),
(10,'Nicia','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Chinswords\"}','0',2,3,4,5,6,NULL),
(11,'Galfrid','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"conquistador\"}','0',2,3,4,5,6,NULL),
(12,'Monranguin','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"eliteknight\"}','0',2,3,4,5,6,NULL),
(13,'Pauline','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"fleur\"}','0',2,3,4,5,6,NULL),
(14,'Tsubh Khamazom','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"fleurdelacour\"}','0',2,3,4,5,6,NULL),
(15,'Alestan','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"link\"}','0',2,3,4,5,6,NULL),
(16,'Keitha','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"lowpolybop\"}','0',2,3,4,5,6,NULL),
(17,'Roseline','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"deadpool\"}','0',2,3,4,5,6,NULL),
(18,'Osha Jaab','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"aragorn\"}','0',2,3,4,5,6,NULL),
(19,'Theodore','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Evil Puffi\"}','0',2,3,4,5,6,NULL),
(20,'Elmar','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"ronny\"}','0',2,3,4,5,6,NULL),
(21,'Bernard','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"scary man\"}','0',2,3,4,5,6,NULL),
(22,'Eylgar','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"TFT_Ninja\"}','0',2,3,4,5,6,NULL),
(23,'Lothaire','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"XiaoHan\"}','0',2,3,4,5,6,NULL),
(24,'Leonnie','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"xxmimi\"}','0',2,3,4,5,6,NULL),
(25,'Armelle','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"zoro\"}','0',2,3,4,5,6,NULL),
(26,'Jackson','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"cammo\"}','0',2,3,4,5,6,NULL),
(27,'Luquelot','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Bouldy\"}','0',2,3,4,5,6,NULL),
(28,'Lewin','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"P-Amazing\"}','0',2,3,4,5,6,NULL),
(29,'Beatin','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"Acid\"}','0',2,3,4,5,6,NULL),
(30,'Kan-E-Senna','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"mofa_tee\"}','0',2,3,4,5,6,NULL),
(31,'Chigoe','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"bao_fu for 26\"}','0',2,NULL,NULL,NULL,NULL,5),
(32,'Anole','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"Bob\"}','0',2,NULL,NULL,NULL,NULL,5),
(33,'Scrambler','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"bugofdoom\"}','0',2,3,NULL,NULL,NULL,NULL),
(34,'Mushroom','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"Goomba Mario\"}','0',2,NULL,NULL,NULL,NULL,NULL),
(35,'Shadow shape','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"blacktee\"}','66',2,NULL,NULL,NULL,6,NULL),
(36,'Winged beast','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"demonlimekitty\"}','66',2,NULL,NULL,NULL,NULL,NULL),
(37,'Janremi Black','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"ahl_War\"}','0',2,3,4,5,6,NULL),
(38,'Spider','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"Spider_KZ\"}','64,66',2,NULL,NULL,NULL,NULL,NULL),
(39,'Bear','{ \"skin\": \"beardog\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}','0',2,NULL,NULL,NULL,NULL,NULL),
(40,'Orc Tee','{ \"skin\": \"orc_tee\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}','0',2,NULL,4,NULL,NULL,NULL),
(41,'Dynamt ogre','{ \"skin\": \"dynamite\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}','65',102,NULL,NULL,5,NULL,NULL),
(42,'Mad Wolf','{ \"skin\": \"WolfTee\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}','0',2,NULL,NULL,NULL,NULL,NULL),
(43,'Doll Tee','{ \"skin\": \"voodoo_tee\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}','0',2,NULL,NULL,NULL,NULL,NULL),
(44,'Zombie','{ \"skin\": \"zombie\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}','0',2,NULL,NULL,NULL,NULL,NULL),
(45,'Spawn','{ \"skin\": \"cloud_ball\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}','66',2,NULL,NULL,NULL,NULL,NULL),
(46,'Slimtee','{ \"skin\": \"A Slime Blob\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}','66',2,NULL,NULL,NULL,NULL,NULL),
(47,'Small Spider ','{ \"skin\": \"Spider Small_KZ\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}','0',2,NULL,NULL,NULL,NULL,NULL),
(48,'Big Spider ','{ \"skin\": \"spider_god\", 	\"custom_color\": 0, 	\"color_body\": 0, 	\"color_feet\": 0}','66,64',2,NULL,NULL,NULL,NULL,NULL),
(49,'Smart mushroom','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"mushroom\"}','0',2,3,NULL,NULL,NULL,NULL),
(50,'Horned beast','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"TeeDevil\"}','66',2,NULL,NULL,NULL,NULL,5),
(51,'Chameleon','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"Exotix\"}','0',2,NULL,NULL,4,NULL,NULL),
(52,'Swamp Ghost ','{\"color_body\":1507072,\"color_feet\":2359295,\"custom_color\":0,\"skin\":\"ghost_greensward\"}','66,19',2,3,NULL,NULL,NULL,NULL),
(53,'Ghost','{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"defatulYoda\"}','66',2,NULL,NULL,NULL,NULL,NULL);
/*!40000 ALTER TABLE `tw_bots_info` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_bots_mobs`
--

DROP TABLE IF EXISTS `tw_bots_mobs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_bots_mobs` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `BotID` int(11) NOT NULL DEFAULT -1,
  `WorldID` int(11) DEFAULT NULL,
  `PositionX` int(11) NOT NULL,
  `PositionY` int(11) NOT NULL,
  `Debuffs` set('Slowdown','Poison','Fire') DEFAULT NULL,
  `Behavior` set('','Sleepy','Slower') DEFAULT '',
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
  `it_drop_chance` varchar(64) NOT NULL DEFAULT '|0|0|0|0|0|',
  PRIMARY KEY (`ID`),
  KEY `MobID` (`BotID`),
  KEY `it_drop_0` (`it_drop_0`),
  KEY `it_drop_1` (`it_drop_1`),
  KEY `it_drop_2` (`it_drop_2`),
  KEY `it_drop_3` (`it_drop_3`),
  KEY `it_drop_4` (`it_drop_4`),
  KEY `WorldID` (`WorldID`),
  KEY `Effect` (`Debuffs`),
  KEY `Behavior` (`Behavior`),
  CONSTRAINT `tw_bots_mobs_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_mobs_ibfk_14` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_mobs_ibfk_15` FOREIGN KEY (`it_drop_0`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_mobs_ibfk_16` FOREIGN KEY (`it_drop_1`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_mobs_ibfk_17` FOREIGN KEY (`it_drop_2`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_mobs_ibfk_18` FOREIGN KEY (`it_drop_3`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_mobs_ibfk_19` FOREIGN KEY (`it_drop_4`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=28 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_bots_mobs`
--

LOCK TABLES `tw_bots_mobs` WRITE;
/*!40000 ALTER TABLE `tw_bots_mobs` DISABLE KEYS */;
INSERT INTO `tw_bots_mobs` VALUES
(1,31,1,11296,4384,'Poison','Sleepy',3,8,3,5,800,0,22,107,NULL,NULL,NULL,'|1|1|0|0|0|','|30|10|0|0|0|'),
(2,32,1,10048,4384,NULL,'Sleepy',4,16,3,5,800,0,33,107,NULL,NULL,NULL,'|1|1|0|0|0|','|30|10|0|0|0|'),
(3,33,1,8704,4384,NULL,'Slower',9,60,4,6,800,0,105,113,NULL,NULL,NULL,'|1|1|0|0|0|','|10|10|0|0|0|'),
(4,34,1,9888,5728,NULL,'Sleepy',8,50,7,10,1152,0,112,NULL,NULL,NULL,NULL,'|1|0|0|0|0|','|30|0|0|0|0|'),
(5,35,1,8512,4416,'Fire',NULL,13,375,1,1100,448,1,111,NULL,NULL,NULL,NULL,'|10|0|0|0|0|','|100|0|0|0|0|'),
(6,38,1,7456,6240,'Poison',NULL,18,135,3,15,1184,0,92,105,NULL,NULL,NULL,'|5|3|0|0|0|','|50|15|0|0|0|'),
(7,39,1,6400,4352,NULL,'Slower',28,210,4,8,1120,0,107,110,NULL,NULL,NULL,'|5|4|0|0|0|','|30|30|0|0|0|'),
(8,40,1,3424,3616,'Slowdown',NULL,30,225,7,12,1120,0,109,50,NULL,NULL,NULL,'|2|1|0|0|0|','|40|100|0|0|0|'),
(9,41,1,3424,3936,'Slowdown,Fire','Slower',33,825,1,3500,320,0,127,109,NULL,NULL,NULL,'|10|2|0|0|0|','|30|100|0|0|0|'),
(10,36,1,8320,2432,NULL,NULL,23,172,4,4,1184,0,110,107,116,NULL,NULL,'|1|1|1|0|0|','|30|10|5|0|0|'),
(11,42,1,5792,2464,NULL,NULL,26,195,5,12,1120,0,110,107,NULL,NULL,NULL,'|2|1|0|0|0|','|30|25|0|0|0|'),
(12,43,1,28672,3424,NULL,'Slower',50,375,5,15,1120,0,111,NULL,NULL,NULL,NULL,'|5|0|0|0|0|','|3|0|0|0|0|'),
(13,44,1,26528,4576,'Poison','Slower',55,412,5,15,640,0,114,NULL,NULL,NULL,NULL,'|5|0|0|0|0|','|3|0|0|0|0|'),
(14,45,1,2624,4960,NULL,'',36,270,5,50,640,0,115,NULL,NULL,NULL,NULL,'|5|0|0|0|0|','|3|0|0|0|0|'),
(15,46,1,9728,4416,'Slowdown','Slower',10,250,1,900,640,1,137,125,NULL,NULL,NULL,'|15|1|0|0|0|','|50|15|0|0|0|'),
(16,47,1,7456,6240,'Poison','Slower',15,75,6,8,1184,0,92,NULL,NULL,NULL,NULL,'|3|0|0|0|0|','|30|0|0|0|0|'),
(17,48,1,5632,6048,'Slowdown,Poison','Slower',20,500,1,40,320,1,92,105,128,NULL,NULL,'|4|2|1|0|0|','|70|50|100|0|0|'),
(18,49,1,9888,5728,'Poison',NULL,12,300,1,1000,1152,1,105,134,NULL,NULL,NULL,'|15|1|0|0|0|','|50|5|0|0|0|'),
(19,50,1,8320,2432,NULL,NULL,25,625,1,1800,1184,0,110,116,107,NULL,NULL,'|10|5|1|0|0|','|50|40|30|0|0|'),
(20,51,1,5024,2592,'Slowdown,Poison,Fire','Slower',27,675,1,2000,320,1,110,115,111,NULL,NULL,'|10|20|15|0|0|','|30|50|30|0|0|'),
(21,52,1,5536,4160,'Slowdown','Slower',29,725,1,2300,320,1,137,115,114,113,111,'|15|10|5|4|10|','|100|30|20|70|40|'),
(22,53,1,4032,5824,NULL,'',39,292,5,50,768,0,115,113,NULL,NULL,NULL,'|5|1|0|0|0|','|10|1|0|0|0|'),
(23,1,1,2624,6208,NULL,'',41,307,5,50,512,0,NULL,NULL,NULL,NULL,NULL,'|5|0|0|0|0|','|3|0|0|0|0|'),
(25,1,1,6176,7360,NULL,'',43,322,5,50,640,0,NULL,NULL,NULL,NULL,NULL,'|5|0|0|0|0|','|3|0|0|0|0|');
/*!40000 ALTER TABLE `tw_bots_mobs` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_bots_npc`
--

DROP TABLE IF EXISTS `tw_bots_npc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_bots_npc` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `BotID` int(11) NOT NULL DEFAULT -1,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `GiveQuestID` int(11) DEFAULT NULL,
  `DialogData` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL CHECK (json_valid(`DialogData`)),
  `Function` int(11) NOT NULL DEFAULT -1,
  `Static` int(11) NOT NULL,
  `Emote` enum('Pain','Happy','Surprise','Angry','Blink') DEFAULT NULL,
  `WorldID` int(11) DEFAULT NULL,
  PRIMARY KEY (`ID`),
  KEY `MobID` (`BotID`),
  KEY `WorldID` (`WorldID`),
  KEY `tw_bots_npc_ibfk_3` (`Emote`),
  KEY `tw_bots_npc_ibfk_5` (`GiveQuestID`),
  CONSTRAINT `tw_bots_npc_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_npc_ibfk_4` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_npc_ibfk_5` FOREIGN KEY (`GiveQuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=34 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_bots_npc`
--

LOCK TABLES `tw_bots_npc` WRITE;
/*!40000 ALTER TABLE `tw_bots_npc` DISABLE KEYS */;
INSERT INTO `tw_bots_npc` VALUES
(1,1,19278,3953,NULL,'[{\"text\":\"If you found me, you must have forgotten something. Okay, I\'ll say it again.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The voting menu has all the important settings for you. There you can change the class. Pump up your skills that you have. Manage your inventory, such as throwing away items and disassemble them into materials and also enchant. Manage guilds, create groups to play together, In the settings you can disable settings and change the language or anything else.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In the crafting area:\\nRequired items quantity\\n\\nCraft button.\\n\\nImmediately I tell you that crafting can be paid. In the window where the reason you can select the number of items to craft instead of clicking constantly\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In farming and mining zones. you gather resources. Which you can also use in crafting. These resources provide a stable economy. You mine, you can buy things. You can buy things with products. You can also carry products and get gold for it.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"There are homes you can buy them from. All functions in the voting menu. These are your houses and you can use them as you like. Bring your friends or be alone there, for example.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If you forgot what how to use skils or where to buy. Buy them here next to need skills to raise or pump them. The usual command to use skills /useskill number\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Pump yourself up first, then go into battle!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',-1,1,'Blink',1),
(2,3,19550,4305,2,'[{\"action\":true,\"text\":\"Who are you?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',-1,1,'Surprise',1),
(3,16,15153,1393,NULL,'[{\"action\":true,\"text\":\"Hey, stranger. Have you come to sell your harvest?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',-1,1,NULL,1),
(4,26,21678,6033,NULL,'[{\"action\":true,\"text\":\"Hi. Got the rock again, huh? All right, spill it while I\'m being nice.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',-1,1,'Surprise',1),
(5,6,22609,2961,NULL,'[{\"action\":true,\"text\":\"Hey, stranger. Help yourself to any beverage you want. Even the illegal stuff. Heehee.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',-1,1,'Happy',1),
(28,4,12510,4273,NULL,NULL,2,0,'Angry',1),
(29,4,27183,3121,NULL,NULL,2,0,'Angry',1),
(30,4,27548,3121,NULL,NULL,2,0,'Angry',1),
(31,4,27418,3121,NULL,NULL,2,0,'Angry',1);
/*!40000 ALTER TABLE `tw_bots_npc` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_bots_quest`
--

DROP TABLE IF EXISTS `tw_bots_quest`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_bots_quest` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `BotID` int(11) NOT NULL DEFAULT -1,
  `QuestID` int(11) NOT NULL DEFAULT -1,
  `Step` int(11) NOT NULL DEFAULT 1,
  `WorldID` int(11) DEFAULT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `DialogData` longtext DEFAULT NULL,
  `ScenarioData` longtext DEFAULT NULL,
  `TasksData` longtext DEFAULT NULL,
  PRIMARY KEY (`ID`),
  KEY `MobID` (`BotID`),
  KEY `QuestID` (`QuestID`),
  KEY `WorldID` (`WorldID`),
  CONSTRAINT `tw_bots_quest_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_quest_ibfk_8` FOREIGN KEY (`QuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_bots_quest_ibfk_9` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=105 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_bots_quest`
--

LOCK TABLES `tw_bots_quest` WRITE;
/*!40000 ALTER TABLE `tw_bots_quest` DISABLE KEYS */;
INSERT INTO `tw_bots_quest` VALUES
(1,1,1,1,0,817,2193,'[{\"text\":\"Oh, it\'s good to see you <player>!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I have a little favor to ask of you. Do you see this <item_31>?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It contains important supplies for <bot_2> that he needs urgently.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Can I ask you to deliver it?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Take the crate\",\n            \"completion_text\": \"You picked up the box\",\n            \"x\": 2006,\n            \"y\": 2065,\n            \"mode\": \"move_press\",\n            \"cooldown\": 3,\n            \"pick_up_item\": {\n                \"id\": 31,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(2,2,1,2,0,2949,2225,'[{\"text\":\"Oh, hey! You look like you brought something.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Is it from <bot_1>\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Thank you so much! It\'s exactly what I\'ve been waiting for. You\'ve helped me more than you can imagine.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"required_items\": [\n        {\n            \"id\": 31,\n            \"value\": 1\n        }\n    ]\n}'),
(3,3,2,1,1,19550,4305,'[{\"text\":\"Another green adventurer, I presume?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I thought as much. We cannot allow strangers to wander Gridania unchecked and untested.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Before you rush off and begin pestering every second citizen for work, I suggest you make yourself known at the Carline Canopy. That\'s the headquarters of the local Adventurers\' Guild, in case you were wondering.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"The Carline Canopy is the building you see behind me. Speak to Mother Miounne within, and she will take you in hand.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]\r\n','','{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Find Miounne\",\n            \"x\": 22192,\n            \"y\": 3313,\n            \"mode\": \"move\",\n            \"cooldown\": 3\n        }\n    ]\n}'),
(4,6,2,2,1,22989,3281,'[{\"text\":\"Well, well, what have we here?\",\"side\":\"default\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"text\":\"A wide-eyed and wondering young adventurer, come to put your name down at the guild, I assume?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Actually...haven\'t you registered with us already? There\'s something strikingly familiar about you, but I can\'t for the life of me remember when or where we might have met.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Oh, I probably just have you confused with another adventurer─dozens of you come through here every day, after all. Now, where was I? Ah, yes.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Welcome. Miounne\'s my name, or Mother Miounne as most call me, and the Carline Canopy is my place.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As the head of the Adventurers\' Guild in Gridania, I have the honor of providing guidance to the fledgling heroes who pass through our gates.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"No matter your ambitions, the guild is here to help you attain them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In return, we expect you to fulfill your duties as an adventurer by assisting the people of Gridania. A fine deal, wouldn\'t you agree?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"To an outsider\'s eyes, all may seem well with our nation, but naught could be further from the truth. The people live in a state of constant apprehension.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Ixal and various gangs of common bandits provide an unending supply of trouble─trouble compounded by the ever-present threat of the Garlean Empire to the north. And that is to say nothing of the Calamity...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Even now, the wounds have barely begun to heal. Ah, but I speak of it as if you were there. Forgive me. Five years past, Eorzea was well-nigh laid to waste when a dread wyrm emerged from within the lesser moon, Dalamud, and rained fire upon the realm. It is this which people call “the Calamity.”\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Scarcely a square malm of the Twelveswood was spared the devastation. Yet despite the forest\'s extensive wounds, not a soul among us can recall precisely how it all happened.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I am well aware of how improbable that must sound to an outsider... It is improbable. But it\'s also true. For reasons we can ill explain, the facts surrounding the Calamity are shrouded in mystery. There are as many versions of events as there are people willing to recount them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Yet amidst the hazy recollections and conflicting accounts, all agree on one thing: that Eorzea was saved from certain doom by a band of valiant adventurers.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Whatever else we\'ve misremembered, none of us have forgotten the heroes who risked life and limb for the sake of the realm. And yet...whenever we try to say their names, the words die upon our lips.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And whenever we try to call their faces to mind, we see naught but silhouettes amidst a blinding glare.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Thus have these adventurers come to be known as “the Warriors of Light.”\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"...Ahem. Pray do not feel daunted by the deeds of legends. We do not ask that you become another Warrior of Light, only that you do what you can to assist the people of Gridania.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Great or small, every contribution counts. I trust you will play your part.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You have my gratitude!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"All that\'s left, then, is to conclude the business of registration. Here\'s a quill. Scrawl your name right there.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Oh, and I would appreciate it if you used your real name─there is a special place in the seventh hell for those who use “amusing” aliases.\",\"side\":\"default\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"text\":\"...Forename Surname, hm? And you\'re quite sure that isn\'t an amusing alias?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Very well. From this moment forward, you are a registered adventurer of Gridania, nation blessed of the elementals and the bounty of the Twelveswood. The guild expects great things from you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]\r\n','',''),
(5,5,2,3,1,22865,3249,'[{\"action\":true,\"text\":\"Look at what just arrived─another godsdamned adventurer...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]\n','',''),
(6,6,2,3,1,22989,3281,'[{\"text\":\"Don\'t you start with that. Adventurers are the very salve that Gridania needs.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"The Elder Seedseer herself bade us welcome them with open arms. Do you mean to disregard her will?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]\n','',''),
(7,5,2,4,1,22865,3249,'[{\"text\":\"Ordinarily, the forest funguars that inhabit the Central Shroud are naught more than a nuisance.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"However, we have received reports that vast clouds of the creatures\' spores have rendered parts of the Twelveswood impassable, and ruined crops besides.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"We periodically cull the funguar population in order to prevent such occurrences, but the creatures have taken to spawning out of season, making it ever more difficult to keep their numbers in check.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Though this is indeed a troubling development, the Gods\' Quiver has more pressing concerns and can ill afford to waste time fighting funguars.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If you wish to prove yourself, go to the Central Shroud and exterminate six of the pests. Use caution and approach them one at a time, lest your adventuring career be cut disappointingly short.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Don\'t you start with that. Adventurers are the very salve that Gridania needs.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Elder Seedseer herself bade us welcome them with open arms. Do you mean to disregard her will?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Don\'t you start with that. Adventurers are the very salve that Gridania needs.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Elder Seedseer herself bade us welcome them with open arms. Do you mean to disregard her will?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Of course not! Lest you forget, it is my sworn duty to uphold the peace! Am I to blame if outsiders bring mistrust upon themselves?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You─adventurer! Mind that you do not cause any trouble here, or I shall personally cast you out of this realm and into the seventh hell.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Of course not! Lest you forget, it is my sworn duty to uphold the peace! Am I to blame if outsiders bring mistrust upon themselves?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"You─adventurer! Mind that you do not cause any trouble here, or I shall personally cast you out of this realm and into the seventh hell.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(8,6,2,5,1,22989,3281,'[{\"text\":\"Ahem. Pay that outburst no mind. He meant only to...counsel you. Suspicious characters have been prowling the Twelveswood of late, you see, and the Wood Wailers feel they cannot afford to take any chances.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As is often the way with folk who live in isolation, Gridanians are wont to mistrust things they do not well know, your good self included. Fear not, however─given a catalog of exemplary deeds, and no more than a handful of years, the locals will surely warm to you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"On behalf of my fellow citizens, I welcome you to Gridania. May you come to consider our nation as your own in time.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Now then, you may depend on old Mother Miounne to teach you a few things that every adventurer should know.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]\n','',''),
(9,6,3,1,1,22989,3281,'[{\"text\":\"Let us begin at the beginning, shall we? Now that you are a formal member of the Adventurers\' Guild, we must be sure you have a firm grasp of the fundamentals of adventuring. To that end, I have three tasks I wish you to perform.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Your first task is to visit the aetheryte. This massive crystal stands in the middle of the aetheryte plaza, not far from the Carline Canopy.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As a device that enables instantaneous transportation, the aetheryte plays a key role in the life of the ever-wandering adventurer.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Once you have located the crystal, all you need do is touch its surface. A member of the Wood Wailers will be present to offer further instruction.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"false\",\n            \"name\": \"Visit the aetheryte\",\n            \"completion_text\": \"You have visited aetheryte\",\n            \"x\": 19551,\n            \"y\": 4694,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        }\n    ]\n}'),
(10,6,3,2,1,22989,3281,'[{\"text\":\"For your second task, you are to visit the Conjurers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"There is no better place to learn the arts of conjury. Speak with Madelle, and she will explain the benefits of joining the guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(14,7,3,3,1,19475,3217,'[{\"text\":\"You seek the secrets of conjury, adventurer? Then search no longer, for you have found your way to the Conjurers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It is at Miounne\'s request that you have come? Then allow me to provide you with an overview of what it is to be a conjurer.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Conjury is the art of healing and purification. Its practitioners harness the power of nature, that they might bring about change in the form of spells.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Primitive magic such as that once wrought by individuals known as mages─meaning those with the ability to manipulate aether─has existed since the dawn of time.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It was not until some five centuries ago that conjury emerged from this shapeless agglomeration of spells and charms─an event which led to the founding of Gridania.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In those dark days, the elementals would not suffer man\'s presence in the Twelveswood, forcing our forebears to make their homes beneath the earth, in the great subterranean city of Gelmorra.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But their desire to settle in the Twelveswood continued to burn fiercely; time and again they sought to curry the elementals\' favor.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Unlike men and other creatures bound in temples of flesh, the elementals are beings of pure aether. Recognizing this, the mages of eld reasoned that their talent for aetheric manipulation might allow them to commune with these theretofore enigmatic entities.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It took five long decades, but our forebears finally succeeded. Their reward: the elementals\' permission to dwell in the Twelveswood. So it was that the nation of Gridania was born.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Since that time, the elementals have taught us to live as one with nature, speaking to all Gridanians through the Hearers─those mages who are able to commune with them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And for their intimacy with the elementals, the Hearers would go on to attain greater mastery over the forces of nature. Thus did they conceive the art of conjury.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I hope this has helped you gain a greater understanding of the Conjurers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Should you wish to delve further into the mysteries of conjury, then I urge you to consider joining our ranks.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I can begin your initiation whenever you desire. Call upon me when you are ready to take the first step.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(15,6,3,4,1,22989,3281,'[{\"text\":\"For your second task, you are to visit the Archers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"There is no better place to learn the arts of the bow. Speak with Athelyna, and she will explain the benefits of joining the guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(16,8,3,5,1,19396,3409,'[{\"text\":\"Greetings, friend. You have found your way to the Archers\' Guild. Do you seek to uncover the secrets of our art?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Ah, now that I think on it, you have the look of one who has received Mother Miounne\'s “gentle” instruction. Very well, I shall give you a brief introduction to archery and the Archers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The skills practiced by our archers allow them to gauge an enemy\'s weaknesses from afar, and turn the tide of a battle with a single, well-placed arrow. Should you join us, you will be taught to do the same.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Archery as practiced in Gridania was born of two distinct styles of bowmanship.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The first was devised by the longbow sentries of the Elezen who once ruled the lowlands, while the second belonged to the shortbow hunters of the formerly nomadic Miqo\'te. As you will doubtless be aware, both races ultimately came to call the Twelveswood home.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Though the two peoples began as rivals, they gradually learned to live together in harmony. During this time, they learned from one another, their two schools of archery intermingling to give birth to the art as it is known today.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"For a time, the bow was used primarily for hunting. But as the hunters vied with one another to prove who was the better shot, there emerged a group of archers whose ultimate goal lay not in the practical pursuit of prey, but in perfection.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Thus was the Archers\' Guild born from the ranks of the Trappers\' League.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It is the way of the guild to promote greatness in archery through friendly competition. And the results of our methods can be seen in the vaunted archers of the Gods\' Quiver, many of whom spent their formative years loosing arrows at the guild\'s practice butts.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I hope this gives you a better idea of who we are, and what we do here. Oho, did I see the spark of ambition flare within your eyes?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If you wish to draw a string with the finest archers in Eorzea, look no further than the Archers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Before you can enlist, however, you must gain the approval of the guildmaster. Once you are ready to proceed, speak with me again and we can begin seeing about your enrollment.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(17,6,3,6,1,22989,3281,'[{\"text\":\"For your second task, you are to visit the Lancers\' Guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"There is no better place to learn the arts of the polearm. Speak with Jillian, and she will explain the benefits of joining the guild.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(18,9,3,7,1,19862,3473,'[{\"text\":\"Welcome to the Lancers\' Guild. I see you brought your own spear.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If you seek to refine your skills with the polearm, then you have come to the right place.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Here at the Lancers\' Guild, spear wielders gather to train with one another, and further hone their abilities under the tutelage of our fine instructors.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"More than just an instrument of war, the spear is also a tool for hunting, and with game ever plentiful in the Twelveswood, the weapon has been the mainstay of the locals here since before the founding of Gridania.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"With the passing of time, our nation became a gathering place for spearmasters from across the realm─many eager to test their mettle against the famed might of our Wood Wailers.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And it was here in Gridania that their myriad fighting styles came into contact, eventually giving rise to the art taught here today.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"That spear technique could be formalized at all owed much to the founding of the Lancers\' Guild by Wood Wailer captain Mistalle nigh on a century past.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The tradition of accepting students from without as well as within Gridania\'s borders persists to this day, ensuring that the art of the polearm may not only survive, but also continue to evolve.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Well, there you have it. I hope this brief history of the guild has helped settle any doubts in your mind.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Lancers\' Guild is always eager to welcome new initiates.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"If you\'ve a will to enlist, speak with me again and we can begin the enrollment procedures.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(19,6,3,8,1,22989,3281,'[{\"text\":\"For your third and final task, I would have you visit the markets at the heart of Old Gridania\'s commercial district. There you shall find weapons and armor, and all the various items that an adventurer might need on his/her travels.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"There is, however, more to the markets than buying and selling goods. Speak with Parsemontret, and listen well to his counsel.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The master merchant can be...uncooperative at times, so be sure to offer him one of my famous eel pies. Like so many men, he is much more charitable when his stomach is full. Here, I made a batch not long ago.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You have your tasks. May Nophica guide your path.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Ah, and one more thing: should you happen to come across any citizens in need, don\'t be afraid to proffer a helping hand. I am certain they will be pleased to meet an adventurer in whom they can confide their woes.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Granted, the work they offer is unlikely to be of realm-shattering importance─but prove your worth and build a reputation, and in time folk will be more inclined to entrust you with matters of moment.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I also suggest that you lend an ear to the Smith here in the Canopy. The Smiths are trusted representatives of the Adventurers\' Guild, and are an invaluable source of advice for neophyte heroes seeking to attain greatness.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(20,10,3,9,1,23127,3281,'[{\"text\":\"Greetings, adventurer. I see you are faithfully following Mother Miounne\'s instructions.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Allow me to offer you a bit of instruction myself. I am Nicia of the Wood Wailers, and I know a thing or two about the aetheryte─yes, that big crystal right there.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Aetherytes are devices that tap into aetherial energies, and are primarily used as a means to travel swiftly from one place to another.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Return and Teleport─the most common transportation spells─make direct use of the aetherytes and their connection to the flow of aether.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And as these devices are found in almost every corner of Eorzea, any adventurer worthy of the name will wish to seek out and attune himself/herself to each one.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Truly, few things in this world are so useful to an intrepid explorer.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But you need not locate them all at once. Before rushing out into the wilds, I suggest you start with the aetherytes found here in Gridania.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Should you wish to learn more about the aetheryte or transportation magic, I am here to answer your questions.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(21,6,3,10,1,22989,3281,'[{\"text\":\"The conquering hero returns. You have completed my little tasks, I trust?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The three locations you visited will feature prominently in your life as an adventurer─it is best you grow familiar with them as soon as possible.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And you took the time to listen to the woes of the citizenry? I cannot emphasize enough how important it is to lend your talents to one and all, no matter how trivial the matter may seem.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I am thankful that you are an obliging sort,. It is adventurers like you who will win the hearts of the locals and pave the way for those who follow. I pray Gridania can rely on your aid in its struggles to come.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(22,6,4,1,1,22989,3281,'[{\"text\":\"<player>, have you visited the Bannock on your wanderings?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It is a training ground found just outside the city where the soldiers of the Order of the Twin Adder are drilled in swordplay and other martial matters.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I mention this because an acquaintance of mine -- a gentleman by the name of Galfrid -- is an instructor there, and I think you may be of use to him. Go and introduce yourself, and find out if there is anything you can do to help.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Mind you do not stray far from the path -- the Twelveswood is no place for merry strolls through the underbrush.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"The instructor at the Bannock is in need of a help\",\n            \"completion_text\": \"Talk to the instructor.\",\n            \"x\": 13660,\n            \"y\": 4369,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 3\n        }\n    ]\n}'),
(23,11,4,2,1,13542,4369,'[{\"text\":\"Greetings, <player>. Miounne sent word to expect you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"My name is Galfrid, and I am responsible for training our Twin Adder recruits.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I thank you for volunteering your assistance. The Twelveswood is much changed since the calamitous arrival of the Seventh Umbral Era five years ago.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The power of the elementals wanes, and the harmony of the forest gives way to chaos. A great abundance of life has been lost as the strong run rampant, stifling the weak and new-sprung.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Though it may not appear so to the eyes of an outsider, the Twelveswood is ailing -- its once rich variety a fading memory.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"For the citizens of Gridania, the restoration of the forest is a sacred duty. And it is my hope that adventurers such as you will offer to aid them in their struggle.\",\"side\":\"default\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"action\":true,\"text\":\"Listen to their requests, and do all that you can. May the elementals bless your endeavors, <player>.\",\"side\":\"default\",\"left_speaker_id\":-1,\"right_speaker_id\":0}]','',''),
(24,11,5,1,1,13542,4369,'[{\"text\":\"I see you are eager to lend a hand, <player>. That is well. But I cannot in good conscience send you into the forest until I have established that your equipment is equal to the task.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It bears repeating that, in the five years since the dawn of the Seventh Umbral Era, many of the Twelveswood\'s creatures have transformed into vicious, bloodthirsty monsters. Venturing into the forest without the proper gear is tantamount to suicide.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I suggest you take some time to evaluate your equipment. Once you deem your armor to be of sufficient quality, present yourself to me for inspection.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"System: Equip your head, body, hands, legs, and feet with gear of item level 5 or above before returning to speak with Galfrid.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"required_items\": [\n        {\n            \"id\": 3,\n            \"value\": 1,\n            \"type\": \"show\"\n        },\n        {\n            \"id\": 4,\n            \"value\": 1,\n            \"type\": \"show\"\n        },\n        {\n            \"id\": 39,\n            \"value\": 1,\n            \"type\": \"show\"\n        }\n    ]\n}'),
(25,11,5,2,1,13542,4369,'[{\"text\":\"Ready for inspection are we? Right, then! Eyes forward! Back straight!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Hmmm... Yes, I think you pass muster.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You would be surprised at how many young, promising soldiers get themselves killed by rushing off into the woods without first donning a decent set of armor.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Your equipment, however, should provide the required degree of protection. Consider yourself ready for duty, <player>.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(26,11,6,1,1,13542,4369,'[{\"text\":\"Ah, <player>. By your tireless efforts, you have proven yourself a friend to Gridania. I believe you can be trusted with sensitive intelligence.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I would assign you a mission of considerable import, yet the need for secrecy prevents me from disclosing its details until you have pledged your participation. I am authorized to tell you only that it concerns suspicious activity in the Twelveswood. Say that you will lend us your aid, and I shall proceed with the briefing.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Good. Time is of the essence, so listen well.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You will by now have heard that a suspicious individual has been seen prowling the Twelveswood.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And you may also be aware that Ixali activity has risen sharply in the region of late. What you may not know is that this increase coincided almost exactly with the first recorded sighting of the aforementioned individual.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Suspecting a connection, we tightened our surveillance in hopes of tracking down our unknown visitor. Alas, our quarry is proving to be exceedingly elusive -- almost as if he knows our movements ahead of time.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But where whole units have failed, a lone adventurer may yet succeed. Acting independently and covertly, you may be able to close in on our quarry unnoticed.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Fear and anxiety are beginning to take their toll upon the citizenry, <player>. For their sake, I ask that you aid us in this investigation.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You have my gratitude. With your help, I am hopeful we will shed light upon this mystery.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Begin your search at Lifemend Stump. It is there that the majority of the sightings took place.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Be forewarned: my people cannot offer you support, lest our quarry catch scent of our presence and evade us yet again. Proceed with caution.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"\\\"See what\'s dangerous in the forest\",\n            \"completion_text\": \"See what\'s dangerous in the forest\",\n            \"x\": 12278,\n            \"y\": 4273,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Galfrid asks you to investigate the Stump of Life.\",\n            \"completion_text\": \"You have removed the sword from Lifemend Stump. Take it back to the Bannock and show it to Galfrid.\",\n            \"x\": 9746,\n            \"y\": 4543,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"pick_up_item\": {\n                \"id\": 21,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(27,11,6,2,1,13542,4369,'[{\"text\":\"<player>! It is good to see you back!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"One of our patrols sent word that you had been spotted doing battle with enraged treants. I am relieved to find you none the worse for the experience.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But tell me, what were you able to discover at Lifemend Stump?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"A sword in the stump, and a dead Ixal? Hmmm...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I can say with absolute certainty that this blade is of Ixali origin. It is of a kind used exclusively in the beastmen\'s rituals.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Ixal rarely set foot in the Central Shroud, so tight is our guard over the area. What purpose could have driven them to take such a risk? I fear something is afoot...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"What\'s that? A dubious couple sporting peculiar spectacles? Hah hah hah!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"So you have finally been acquainted with Yda and Papalymo! Lay your suspicions to rest -- Gridania counts them among her staunchest allies. Both are scholars hailing from a distant land, and have been with us since before the Calamity.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Their garb may appear outlandish, and their exchanges baffling, but never once have they given us cause to doubt them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Indeed, they often assist the Gods\' Quiver and the Wood Wailers in their work -- much as I hope you will in the days to come, <player>.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Although our unknown visitor eludes us still, owing to your efforts, we have acquired important intelligence on the Ixali threat. You have my gratitude.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"We are fortunate indeed to have a capable adventurer such as you aiding us. I pray you will continue to serve the people of Gridania in whatever capacity you are able.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"required_items\": [\n        {\n            \"id\": 21,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(28,11,7,1,1,13542,4369,'[{\"text\":\"<player>, injuries to several of my men have left me shorthanded, and I require a capable sort to complete their unfinished duty.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The task is simple: put down as many of the local chigoe population as necessary to acquire eight of their egg sacs.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Having done so, you are to deliver them to Monranguin at Gilbert\'s Spire. He will answer any queries you might have. Now, I have other business to attend to.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"The Order of the Twin Adder makes no distinction between newcome adventurers and forestborn Gridanians. Your worth as a soldier is measured by your dedication to the cause.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"required_items\": [\n        {\n            \"id\": 22,\n            \"value\": 3,\n            \"type\": \"default\"\n        }\n    ],\n    \"defeat_bots\": [\n        {\n            \"id\": 31,\n            \"value\": 10\n        }\n    ]\n}'),
(29,12,7,2,1,11872,4017,'[{\"text\":\"Ah, you must be the adventurer standing in for our injured companions. Terribly unfortunate business, that.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"It seems, however, that you had little trouble gathering the egg sacs in their stead. Excellent work. I shall have them sent over to the Trappers\' League immediately.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Lest you wonder, these egg sacs are not destined for the dinner table! Members of the Twin Adder and the Wood Wailers are assisting the league by collecting the samples they need to check for signs of sickness.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The chigoe, you see, is one of the few creatures capable of transmitting the disease known as the Creeping Death. Until relatively recently, any Hyur who contracted this ghastly illness would almost invariably perish.\",\"side\":\"author\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"text\":\"Indeed, a single outbreak once claimed the lives of a third of the Hyuran population here in Gridania. That was a long time ago, of course. With the medicines available to us now, the Creeping Death is not the killer it once was.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Even so, it is best to halt any new outbreaks before they occur. Thus we gather chigoe eggs on a regular basis in order to assist the Trappers\' League with their ongoing research. Your timely assistance is most appreciated.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(30,12,8,1,1,11872,4017,'[{\"text\":\"Such an embarrassing turn of events... I sent a recruit from the Bannock on a surveying expedition only for the craven to turn tail and flee at the first sign of trouble.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"This is not how we treat requests from the conjurers! And as if such a poor showing weren\'t bad enough, the lily-livered half-wit left behind the surveying equipment provided by Hearer Pauline herself!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"While I attempt to instill some backbone into this so-called “soldier,” would you mind recovering the survey gear and returning it to Hearer Pauline at Gabineaux\'s Bower?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"According to my recruit\'s tale of woe, there should be a set of survey records, a surveyor\'s rope, and two boxes of surveyor\'s instruments strewn about the interior of a cave to the south of here. <sigh> It\'s a wonder the damn fool didn\'t lose his boots... Ahem. Matron watch over you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Get to the designated spot\",\n            \"completion_text\": \"Find the lost items\",\n            \"x\": 9852,\n            \"y\": 5297,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Recover the survey records\",\n            \"completion_text\": \"You have received the survey notes\",\n            \"x\": 9883,\n            \"y\": 5745,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 23,\n                \"value\": 1\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Recover the boxes of instruments\",\n            \"completion_text\": \"You have received recover the boxes of instruments\",\n            \"x\": 9006,\n            \"y\": 5745,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 24,\n                \"value\": 1\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Recover the boxes of instruments\",\n            \"completion_text\": \"You have received recover the boxes of instruments\",\n            \"x\": 10830,\n            \"y\": 5329,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 24,\n                \"value\": 1\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Recover the surveyor\'s rope\",\n            \"completion_text\": \"You have received recover the surveyor\'s rope\",\n            \"x\": 10129,\n            \"y\": 6033,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 32,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(31,13,8,2,1,14154,3153,'[{\"text\":\"Yes, may I assist you with some matter?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Oh? But this is the equipment I left with the soldiers of the Bannock...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Fled at the first sign of danger, you say? I see... Well, all is not lost: it appears the recruit managed to complete the surveying assignment. The records are actually quite detailed.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"With the changes wrought by the Calamity, I thought it wise to send the Order of the Twin Adder on a number of expeditions to map the region\'s topography. As fortune would have it, the officers saw these tasks as an excellent opportunity to train inexperienced soldiers.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"We can no longer rely on our past knowledge of the Twelveswood. If we are to survive these troubled times, we must reacquaint ourselves with our surroundings, that we may better discern the threats we face. Stay vigilant, adventurer\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"required_items\": [\n        {\n            \"id\": 23,\n            \"value\": 1,\n            \"type\": \"default\"\n        },\n        {\n            \"id\": 24,\n            \"value\": 2,\n            \"type\": \"default\"\n        },\n        {\n            \"id\": 32,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(32,13,9,1,1,14154,3153,'[{\"text\":\"I hesitate to make such a dangerous, request, but might you assist us in thinning the number of anoles on Naked Rock?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In our efforts to commune with the elementals, we conjurers often find ourselves in the area. Of late, however, our meditations have all too frequently been interrupted by unprovoked anole attacks. Truly, the beasts grow more aggressive by the day.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Their numbers have continued to grow since the advent of the Seventh Umbral Era, you see, forcing packs of the scalekin to come down from the mountains in search of food. If you could slay a handful of the beasts, that should lessen their need to hunt and also serve as a warning to the anoles to remain within their territory.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"But I am afraid you must do more than thin the existing population. If we are to truly break this spiraling growth, then we must also target their future offspring. Bring me one of their eggs, and you will have played your part in returning balance to this area of the forest.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"required_items\": [\n        {\n            \"id\": 33,\n            \"value\": 10,\n            \"type\": \"default\"\n        }\n    ],\n    \"defeat_bots\": [\n        {\n            \"id\": 32,\n            \"value\": 40\n        }\n    ]\n}'),
(33,13,9,2,1,14154,3153,'[{\"text\":\"Ah, you have returned. Now might my brothers and sisters continue their meditations undisturbed. You have my thanks.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As for the egg, may I ask you to deliver it to Tsubh Khamazom at the Bannock?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Anole eggs are both large and filled with nutrients - the perfect meal for a soldier. She will be more than a little pleased to see you, I should imagine.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(34,14,9,3,1,13754,4369,'[{\"text\":\"Who goes there! Oh, Adventurer, it\'s you. Hm? Another delivery?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I\'m not certain I should be the one to - By Nophica, that\'s an anole egg! The troops will be glad indeed to see one of these at table! And you say Hearer Pauline sent you on this errand?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I hear the anoles are more numerous than ever, yet you appear to have managed the task with your skin intact. Your skill and bravery continue to amaze me, Adventurer,\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(35,11,10,1,1,13542,4369,'[{\"text\":\"Adventurer! Thank the gods you\'ve come!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"We have a potential crisis on our hands, and I would appreciate your assistance. Will you hear me out?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Listen well, for we haven\'t much time.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"To the southeast of here lies a dungeon known as Spirithold. It was all but destroyed during the Calamity.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Desiring to offer the ruins back to the forest, a Hearer ventured inside to carry out the Rite of Returning.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Alas, it would seem something has gone awry. Word arrived just moments ago that the Hearer and his guards have been attacked by a towering shadow. Aye, you heard me true - a shadow.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"On any other day, I would dispatch my best Quivermen to provide support, but I sent them to repel an Ixali incursion in the West Shroud nary a bell ago.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The timing of these events cannot be mere coincidence. I fear the Ixal somehow caught wind of our plans, and are attempting to disrupt the rite in an effort to weaken the bond between man and elemental.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"They must not be allowed to succeed. And so I bid you go to Spirithold and do whatever is necessary to resolve the situation. Please, say you will help us.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I knew you would not let me down. You will have all the support I can muster.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Now, we are racing against time, so you had best make haste.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(36,15,10,2,1,12668,4305,'[{\"text\":\"Who goes there? An adventurer, is it?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Here at Instructor Galfrid\'s request, you say? Thank the Matron!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Doubtless you already know this, but a towering shadow manifested without warning and attacked the Hearer in the midst of the rite.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Most of his party has been successfully evacuated, but five remain unaccounted for.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Please find them, and see them out of harm\'s way.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Get to the battle site\",\n            \"x\": 12337,\n            \"y\": 4273,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing Hearer\",\n            \"completion_text\": \"You saved the Hearer.\",\n            \"x\": 11806,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 11388,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 10925,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 10592,\n            \"y\": 4530,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 10301,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 9822,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Rescue the missing conjurer\",\n            \"completion_text\": \"You saved the conjurer.\",\n            \"x\": 9537,\n            \"y\": 4433,\n            \"world_id\": 1,\n            \"mode\": \"move\",\n            \"cooldown\": 2\n        }\n    ]\n}'),
(37,15,10,3,1,12668,4305,'[{\"text\":\"All this happened inside Spirithold? Twelve preserve us...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Who was that masked mage, and by what dark ambition is he driven? SO many things shrouded in mystery...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Well, I shan\'t find any answers on my own. I must needs discuss this with Galfrid. The matter warrants a full investigation, if I am any judge, and that shall certainly be my recommendation.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Your courage has saved many lives this day, adventurer. For this you have my deepest gratitude.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Pray return to Gridania and seek out Miounne.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I understand she wishes to thank you for your efforts on our behalf.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(38,6,11,1,1,22989,3281,'[{\"text\":\"How is my favorite fresh-faced adventurer? Oh, do not scowl so—I speak out of habit. You\'ve come a long way since you first walked through my door, and I\'ll not deny it.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As a matter of fact, I think it\'s about time you made yourself useful at Bentbranch Meadows in the Central Shroud.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Unlike the recruits you see at the Bannock, the men and women out at Bentbranch are fully occupied with their own work. As such, I imagine there are more than a few who would welcome the assistance of a rapidly maturing adventurer like yourself.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Bentbranch is also home to a very usefully situated aetheryte. It is, in other words, the perfect place for you to begin the next stage of your journey as an adventurer.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And what better time than the present!? Leave the city via the Blue Badger Gate, and continue to the southwest until you come to a bridge.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Cross it, and when you spy an aetheryte in the distance, you may congratulate yourself on having successfully found Bentbranch Meadows.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If you so fancy, you may also make use of the chocobo porter service, doubtless the safest way to get to your destination. Chocobokeep Cingur should not hesitate to lend his birds to a capable adventurer like you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Oh, and be sure to introduce yourself to Keitha, the head chocobo wrangler, when you arrive.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(39,16,11,2,1,15153,1393,'[{\"text\":\"You must be the \'venturer Miounne sent word about. I\'m Keitha, head wrangler \'round these parts.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I\'ve heard many and more things about you—good things, lest you worry.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"When the Elder Seedseer bade us welcome \'venturers, \'tis fair to say we had our doubts—till hardworkin\' folks like yourself set about provin\' us wrong, that is. Consider me a convert!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Anyroad, you\'ve a mind to help out at the ranch, have you? Good. We could always do with a hand or two to keep the place runnin\' smoothly.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"As a matter of fact, I\'ve a task right here that wants doin\'.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Let me know when you\'re ready to get busy. Oh, and you come highly recommended, so don\'t go lettin\' no one down, eh?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(40,16,12,1,1,15153,1393,'[{\"text\":\"Seven hells! Some bastard Qiqirn has gone and broken one of me chocobo eggs!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The filthy little thief was busy lootin\' the barn when a guard startled it, promptin\' the damn thing to drop the egg it was clutchin\' and run. Some of the lads gave chase, bless \'em, but when three more of the vermin appeared, me lot had no choice but to turn back.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Somethin\' has to be done about those Qiqirn...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Would you mind headin\' over to the Matron\'s Lethe and havin\' a word with a soldier named Roseline for me? The ratmen nest in her neck of the woods, see... She\'ll know what to do.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(41,17,12,2,1,12180,3185,'[{\"text\":\"You\'re here on Keitha\'s behalf? Hm? I see. Broke one of her eggs, you say? And there were four of the creatures?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The Qiqirn are a nuisance at the best of times, but we must now add trespass and chocobo murder to their list of transgressions... They have forced our hand. Our retribution must needs be swift and decisive.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And who better than you to deliver it, adventurer? Find the lair to the west of here and make an example of exactly four Qiqirn scramblers. We can send no clearer message.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"defeat_bots\": [\n        {\n            \"id\": 33,\n            \"value\": 20\n        }\n    ],\n    \"move_to_completes_quest_step\": true\n}'),
(42,17,12,3,1,12180,3185,'[{\"text\":\"The deed is done? Good. A grim task, but a necessary one.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"You have shown them the folly of inciting the wrath of those who consort with adventurers. Perhaps now the Qiqirn will think twice before giving in to their larcenous proclivities.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(43,18,13,1,1,12692,3121,'[{\"text\":\"Ho there, adventurer. You seem light on your feet. Fancy a quick skip along the root of the heavenspillar here? I need someone to pick off a blue trumpet or two.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I know what you\'re thinking: it\'s naught more than a mushroom, so why the commotion? I\'ll tell you why. You allow that fungal menace to multiply, and within a moon they\'ll be covering the whole damn root and rotting the wood clear through.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Just watch your step while you\'re up there, though—the diremites on the ground won\'t waste any time adding insult to falling injuries.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Once you\'re done, head up to the top end of the root and report to Theodore. He\'ll be glad to hear someone\'s taken care of one of his more dreaded chores.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"defeat_bots\": [\n        {\n            \"id\": 34,\n            \"value\": 25\n        }\n    ],\n    \"move_to_completes_quest_step\": true\n}'),
(44,19,13,2,1,12449,3153,'[{\"text\":\"Oh, you\'ve cleared the root of blue trumpets? Wonderful! To be quite honest, I have this teeny-tiny problem with heights. <sigh> No, this is not my ideal posting, but we all do what we must.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Keeping the root passable is an important job, you see. It\'s one of the few ways folk can traverse the Central Shroud since the Calamity all but split the area in twain.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"It is not, perhaps, the easiest pathway to walk, but there are those who believe the will of the Matron Herself caused this tendril of a heavenspillar to remain thus suspended, that it might serve the forest\'s people. I\'m rather fond of the notion, myself.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(45,19,14,1,1,12449,3153,'[{\"text\":\"It is—regrettably—my duty to stand watch over the road from here to Bentbranch Meadows.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The ranch has become a veritable institution of Gridania, so any threats to its continued operation are taken quite seriously by the Wood Wailers.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Which reminds me—mayhap it was my imagination, but I believe I caught a glimpse of some shadowy fellow not too long ago. Would you mind passing word to Roseline down below? I would go myself but, well...it\'s hard enough marshaling the courage to walk the root for my shift...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(46,17,14,2,1,12180,3185,'[{\"text\":\"A shadowy fellow? Hmmm...now that you mention it, I may have seen something.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I dismissed it as a trick of the light before, but I thought I saw a shadow in the forest to the north. Still, I suspect it is nothing more than a Qiqirn thief on the run.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"If you must sate your curiosity, by all means investigate. Should you actually find something of note, I would like very much to see it.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"required_items\": [\n        {\n            \"id\": 44,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ],\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Search for signs of the shadowy figure\",\n            \"completion_text\": \"A leather bag fell out of the shadows, take it to Roseline.\",\n            \"x\": 10297,\n            \"y\": 4593,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 35,\n                \"attribute_power\": 100\n            },\n            \"pick_up_item\": {\n                \"id\": 44,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(47,17,14,3,1,12180,3185,'[{\"text\":\"Hmmm? Have you found something?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Remnants of a campfire and a forgotten bag...this could belong to any adventurer or traveler. And inside we have...a chocobo grooming brush and roseling oil?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But wait—why would a traveler make camp here, when it would be far safer to beg the hospitality of Bentbranch Meadows?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Bugger me, I think this belongs to that stranger said to be meddling with the chocobos! Thank you, adventurer. We have been lax in our duties, but no longer—I swear we will find this shadow.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(48,17,15,1,1,12180,3185,'[{\"text\":\"We cannot allow other sentries to dismiss similar sightings—they must know what we have learned. To that end, I\'ve prepared this letter containing everything we know about our mysterious stranger.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I need you to show it to my comrades throughout the Shroud. Once each sentry has committed the details to memory, have them write their name at the bottom for confirmation.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Start with Elmar at the Bannock, then find Bernard at the eastern gates of Bentbranch Meadows. They ought to relay the information to the others.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Finally, make sure Eylgar sees the letter. He stands watch within the stables, so if this stranger\'s aim is to harm the chocobos, Eylgar may have to personally put an end to it.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(49,20,15,2,1,13903,4337,'[{\"text\":\"You don\'t look like you\'re here for training...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"A shadowy figure... Understood. I\'ll pass word to the recruits as well as the sentries.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Let me just make my mark...there, that should do.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"If you follow the road south, you\'ll find Bernard by the bridge to Bentbranch. Godsspeed, adventurer.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(50,21,15,3,1,16370,1617,'[{\"text\":\"You have business with me, adventurer?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Roseline is right to be cautious. For this stranger to venture so close, yet to go to such great lengths to remain undetected, is highly suspicious. They clearly have designs on Bentbranch.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"We appreciate the help. I should write my name here, yes?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And...here, take it. You\'ll find Eylgar in the stables, past the aetheryte.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(51,22,15,4,1,16608,1073,'[{\"text\":\"Mind the birds, adventurer. They get nervous around strangers.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"What\'s this? A shadowy stranger near the Matron\'s Lethe... You\'ve already shown this to Bernard and Elmar, I see. Good, good.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Then all that\'s left is to inform the staff here. Not everyone here is a Wood Wailer, true, but even our stableboys wouldn\'t hesitate to take up arms to defend these chocobos.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(52,22,16,1,1,16629,1073,'[{\"text\":\"You\'re quite the compassionate adventurer, by the sound of it. Well far be it from me to look a gift chocobo in the mouth─I have need of a capable [GENDER]/woman like yourself.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"We received a peddler at the gates the other day─an excitable Lalafell that was sweating and swearing that he had been attacked by large winged beasts. Yet other than hornets, I know of no flying creatures in this region.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"If there\'s any truth to what he said, it might prove problematic for other travelers. Follow the road south and see if you can find any evidence to support his claim.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And, should you find something, kindly tell Lothaire to patrol his area sometime instead of just standing beneath the spire and staring at the godsdamned road. In those words.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Search for signs of the winged beasts 1/3\",\n            \"completion_text\": \"You searched the area and found feathers scattered on the ground.\",\n            \"x\": 16605,\n            \"y\": 1073,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 2\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Search for signs of the winged beasts 2/3\",\n            \"completion_text\": \"You noticed scratch marks on the trees. It appears the winged beasts have been here.\",\n            \"x\": 18361,\n            \"y\": 785,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 2\n        },\n        {\n            \"step\": 3,\n            \"navigator\": \"true\",\n            \"name\": \"Search for signs of the winged beasts 3/3\",\n            \"completion_text\": \"You found a nest.\",\n            \"x\": 15152,\n            \"y\": 721,\n            \"mode\": \"move_press\",\n            \"cooldown\": 2\n        },\n        {\n            \"step\": 4,\n            \"navigator\": \"true\",\n            \"name\": \"Protect yourself from the winged beast\",\n            \"x\": 15209,\n            \"y\": 421,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 36,\n                \"attribute_power\": 60\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Protect yourself from the winged beast\",\n            \"x\": 15584,\n            \"y\": 753,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 36,\n                \"attribute_power\": 60\n            }\n        }\n    ]\n}'),
(53,23,17,1,1,18489,785,'[{\"text\":\"I have another task for you, adventurer. I need you to head to the Hedgetree to the southwest of here and speak with Hearer Leonnie.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"After tending to the Hedgetree, the Hearer was scheduled to board a boat from the Mirror Planks... Well, the vessel\'s departure time has come and gone, but there is still no sign of her.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Knowing how absorbed the Hearer becomes in her work, I am not unduly concerned. Armelle, however, was responsible for organizing Leonnie\'s transportation and is likely wondering if her wayward passenger is ever going to arrive. Perhaps a gentle reminder is in order?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(54,24,17,2,1,17456,1617,'[{\"text\":\"Yes, what troubles you, my de─? Ah. Yes. The boat. I had quite forgotten.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Hm? Oh, my task with the Hedgetree is well and finished, but the elementals murmur of a malevolent presence in the vicinity of the Tam-Tara Deepcroft.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I must abide a while longer, that I might better divine the source of the elementals\' distress. Please inform Armelle that I shall be late in arriving.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(55,25,17,3,1,20421,1041,'[{\"text\":\"You bear a message from Hearer Leonnie? An evil presence in the Deepcroft? That does sound grave, indeed.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I shall organize a vessel for a later time, then. Perhaps a bell from now? Two? Better make it three, just to be safe. Thank you for your trouble.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(56,25,18,1,1,20421,1041,'[{\"text\":\"Might you assist me with another matter, <player>? A wagon that departed from Quarrymill was overturned on the road when some large, ill-tempered forest beast chose that moment to defend its territory.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"By Nophica\'s grace, the driver escaped without serious injury, but the wagon\'s cargo was not so fortunate. With none willing to risk another encounter with the creature, I can only assume the goods remain strewn across the ground where the incident occurred.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Simply follow the road here to the south and you should come across the wreckage. Salvage what cargo you can, and deliver it to Keitha at Bentbranch Meadows if you would be so kind.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Find a place\",\n            \"completion_text\": \"I think there\'s a lost cargo around here somewhere\",\n            \"x\": 10059,\n            \"y\": 4497,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Salvage cargo\",\n            \"completion_text\": \"You picked up cargo.\",\n            \"x\": 8810,\n            \"y\": 4529,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 45,\n                \"value\": 1\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Salvage cargo\",\n            \"completion_text\": \"You picked up cargo.\",\n            \"x\": 8049,\n            \"y\": 4689,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 5,\n            \"pick_up_item\": {\n                \"id\": 45,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(57,16,18,2,1,15153,1393,'[{\"text\":\"You have a delivery for me?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Ah, this\'s the shipment I was waitin\' on from Quarrymill. I heard the wagon ran afoul o\' some great monstrosity just up the path from the Mirror Planks, but I see you\'ve managed to scrape together a few bits and pieces.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Seems I can\'t get through two bells these days without hearin\' some new tale o\' horrors in the Deepcroft or bandit cutthroats prowlin\' the woods hereabouts. Makes me wonder if me chocobos are safe at night, it does. If we\'re ever in need of a \'venturer\'s skills, I hope you\'ll be around to lend a hand.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"required_items\": [\n        {\n            \"id\": 45,\n            \"value\": 2,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(58,27,19,1,1,15888,1489,'[{\"text\":\"No! Oh, please gods, no! Leia\'s egg!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"<player>, you must help me! I stepped out of the stables for but a moment, and when I returned it was gone!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I— What was gone? An egg! Sorry? You are sure the chocobos will lay another!? Gah! You do not understand—the egg is extremely valuable! I must find it!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"You will help me, won\'t you, <player>? Oh, thank you, thank you!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Someone must have made off with it. There can be no other explanation. I shall scour every ilm of the stable once more just to be sure. While I do so, I should be very grateful if you would ask the others if they noticed anything unusual.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"Question the people of Bentbranch Meadows\",\n            \"x\": 16581,\n            \"y\": 1073,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 15\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Question the people of Bentbranch Meadows\",\n            \"x\": 15528,\n            \"y\": 1425,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 15\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Question the people of Bentbranch Meadows\",\n            \"completion_text\": \"You\'ve done a survey of the residents of Bentbranch Meadows.\",\n            \"x\": 17531,\n            \"y\": 1617,\n            \"world_id\": 1,\n            \"mode\": \"move_press\",\n            \"cooldown\": 15\n        },\n        {\n            \"step\": 2,\n            \"navigator\": \"true\",\n            \"name\": \"Defeat Janremi Blackheart\",\n            \"completion_text\": \"You defeated Janremi Blackheart. Return Leia\'s egg to Luquelot.\",\n            \"x\": 22313,\n            \"y\": 3313,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 37,\n                \"attribute_power\": 80\n            },\n            \"pick_up_item\": {\n                \"id\": 46,\n                \"value\": 1\n            }\n        }\n    ]\n}'),
(59,27,19,2,1,15453,1425,'[{\"text\":\"Leia\'s egg is irreplaceable. I cannot well express to you the depth of my gratitude.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And yet, I find that I am still troubled... While this whole regrettable episode unfolded, I bore witness to a sight that greatly concerned me..\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"move_to_completes_quest_step\": true,\n    \"required_items\": [\n        {\n            \"id\": 46,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(60,27,20,1,1,15888,1489,'[{\"text\":\"My thanks for your kindness earlier. I hate to impose again, but I have need of your assistance in another matter─one of grave import, I fear.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Of late, I have noticed that Ixali dirigibles have been appearing over the Twelveswood with increasing regularity. The frequency, however, does not bother me near so much as where they choose to fly─the patch of sky directly above the Guardian Tree.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The tree is a sacred site, or so I was given to believe when first I came to Gridania, and thus I naturally assumed that the elementals would not suffer the Ixal to profane it. Yet the birdmen have been coming and going as they please, with nary a sign of protest from the guardians of the Twelveswood.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And it was that which set me to thinking... Ever since the appearance of the much-talked-about “suspicious individual,” many and more strange things have been occurring in the forest. Could it be that he did something to the elementals?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In case it proved useful, I have committed the details of my sightings to parchment, and would ask that you deliver the document to Mother Miounne.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Please make all haste─I have an irrepressible feeling that something terrible is about to happen.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"reward_items\": [\n        {\n            \"id\": 47,\n            \"value\": 1\n        }\n    ]\n}'),
(61,6,20,2,1,22989,3281,'[{\"text\":\"I haven\'t the slightest inkling what this could be about...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Mother Miounne scans the letter, and her eyes widen.\",\"side\":\"author\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"text\":\"Gods be good...\",\"side\":\"author\",\"left_speaker_id\":-1,\"right_speaker_id\":0},{\"text\":\"I have a mission for you, <playerM. Suffice it to say, it is urgent.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I fear it may also prove dangerous, however, so you must be prepared.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Make what arrangements you can, and report back to me the moment you are ready.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"required_items\": [\n        {\n            \"id\": 47,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(62,6,21,1,1,22989,3281,'[{\"text\":\"Time is of the essence, so I shall speak plain.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Based on Luquelot\'s observations, the Ixal have designs on the Guardian Tree, and they mean to act soon.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The tree is the oldest living thing in this ancient forest, and it is held sacred by every forestborn Gridanian.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Should it come to any harm, the elementals would fly into a rage beyond pacifying. I dread to think of the chaos that would ensue.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"There isn\'t much time. We must act quickly.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"<player>, please see this letter to the hands of Bowlord Lewin, at the Seat of the First Bow in Quiver\'s Hold. Should the need arise, pray put yourself wholly at the man\'s disposal. I strongly suspect he will need all the able-bodied souls he can muster.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"The fate of Gridania hangs in the balance. Go swiftly, <player>.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"reward_items\": [\n        {\n            \"id\": 47,\n            \"value\": 1\n        }\n    ]\n}'),
(63,28,21,2,1,13738,3217,'[{\"text\":\"So you are <player>, the adventurer of whom I have heard so much. I understand you wish words with me.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Miounne has information on the Ixal, you say? Speak freely─you have both my ears.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"...Matron forfend! They mean to defile the Guardian Tree?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Before Nophica, I swear those filthy birdmen will not touch it─nay, not so much as a single leaf!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Twelve help me! How can it be that neither the Wood Wailers nor the Gods\' Quiver caught wind of this?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"I cannot help but think this plot bears the mark of the masked devil who has eluded us for so long. We must be wary─this incursion may be more than it seems.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Ah, would that the Warriors of Light were still with us...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"But this is no time for such idle thoughts. I thank you for delivering this message. You may assure Miounne that I will dispatch a unit of my best men to investi─\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"required_items\": [\n        {\n            \"id\": 47,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(64,28,22,1,1,13738,3217,'[{\"text\":\"None in Gridania can doubt your worth, <player>...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But to receive such praise from the Elder Seedseer herself!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"And nor is that the half of it! She chose you to play the role of Emissary, for gods\' sakes! You! An outsider! Do you have any idea what this means!?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"<sigh> But of course you don\'t. You are not forestborn...which is rather the point. Mistake me not, I think you worthy, but your selection is all but unprecedented. And I\'ll wager you have not the faintest inkling what is required of you...\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"There are preparations that the Emissary must needs complete ahead of time. I suggest you consult Miounne regarding the matter—she is overseeing the arrangements for the event.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Now, you had best get going—the ceremony cannot commence without the Emissary.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Oh, and, <player>...don\'t make a hash of this!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(65,6,22,2,1,22989,3281,'[{\"text\":\"Well, well, if it isn\'t the Emissary himself/herself! Had I known you were coming, I would have baked a pie!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"You truly are full of surprises, <player>. Next you\'ll be telling me you\'re one of the Warriors of Light, back from a half-decade long holiday!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"But let\'s speak of preparations. As you doubtless already know, Greenbliss is an age-old ceremony for strengthening the bond between man and elemental. These days, though, the name also refers to the festival at large.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In the ceremony, the Emissary serves as a conduit—a bridge between the people and the guardians of the Twelveswood. Suffice it to say, it is no small responsibility—nor does the Elder Seedseer choose mankind\'s representative on a whim.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Seldom in history have non-forestborn been chosen for the role—which should give you an idea of the magnitude of the honor being accorded you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"For your part, you are required to wear a ceremonial artifact, which is presently in the keeping of Timbermaster Beatin.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Go to the Carpenters\' Guild and collect it from the man, then return to me for further instructions.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(66,29,22,3,1,19278,3409,'[{\"text\":\"So you are the Emissary-to-be. Miounne sent word that you would be coming to collect the ceremonial artifact.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The good news is that it\'s ready—painstakingly crafted by these very hands, and from the rarest of materials.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Take it, along with this warning: get so much as a scratch on the thing, and I shall make an unceremonial artifact out of you.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"reward_items\": [\n        {\n            \"id\": 48,\n            \"value\": 1\n        }\n    ]\n}'),
(67,6,22,4,1,22989,3281,'[{\"text\":\"Back from your trip to the Carpenters\' Guild? Let\'s see what you have in that box.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Ahhh...this is by far the finest Monoa mask I have ever laid eyes upon. The timbermaster has truly outdone himself this time.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"In case he didn\'t mention, the mask is crafted from consecrated lumber rendered up by the Guardian Tree, solely for use in the ceremony. In other words, it is priceless—Mother bids you to handle it with care.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"And with that, your preparations are complete. The venue should just about be in order as well. If you have any questions, now\'s the time to ask them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"reward_items\": [\n        {\n            \"id\": 49,\n            \"value\": 1\n        }\n    ],\n    \"required_items\": [\n        {\n            \"id\": 48,\n            \"value\": 1,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(68,6,23,1,1,22989,3281,'[{\"text\":\"Now that you have the Monoa mask, all that\'s left is to participate in the ceremony.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"Perchance you are feeling nervous, but never fear—despite all the pomp surrounding the role, there really is nothing to being Emissary.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"All you are required to do is wear the mask, stand up straight, and look dignified. The more involved aspects of the proceedings will be handled by others. Simple, no?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"text\":\"The ceremony will be held at Mih Khetto\'s Amphitheatre. When you are ready, make yourself known to the caretaker there—a woman named Estaine.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"Oh, and be sure to wear your mask or she may not recognize you. Now, off you go, <player>, and good luck!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\n    \"required_items\": [\n        {\n            \"id\": 49,\n            \"value\": 1,\n            \"type\": \"show\"\n        }\n    ]\n}'),
(69,30,23,2,1,12526,2545,'[{\"text\":\"I have looked forward to your coming, <Player>. But tell me, are you recovered?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"I am most glad of that. Now, I hope you will not doubt the earnestness of my concern...but I would ask a favor of you. Nor can I deny that I summoned you here in part with this in mind. Know, however, that I proceed only upon the understanding that you are rested and well.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','',''),
(70,8,25,1,1,19396,3409,'[{\"action\":true,\"text\":\"Ah, there you are. I hear you\'re looking for a little adventure. Well, if you\'re really up for it, I\'ve got something for you. But I\'m warning you, it\'s no laughing matter. It\'s about Les. I\'m short on supplies again. Bring 50 anole carcasses and 50 chigoe carcasses and their eggs, 25 apiece.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"required_items\": [\n        {\n            \"id\": 22,\n            \"value\": 25,\n            \"type\": \"default\"\n        },\n        {\n            \"id\": 33,\n            \"value\": 25,\n            \"type\": \"default\"\n        }\n    ],\n    \"defeat_bots\": [\n        {\n            \"id\": 31,\n            \"value\": 50\n        },\n        {\n            \"id\": 32,\n            \"value\": 50\n        }\n    ]\n}'),
(71,8,26,1,1,19396,3409,'[{\"action\":true,\"text\":\"I really need feathers for my arrows. Bring me at least 50 of them.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"required_items\": [\n        {\n            \"id\": 116,\n            \"value\": 50,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(72,8,27,1,1,19396,3409,'[{\"action\":true,\"text\":\"Hi. Here\'s the thing. I can\'t handle a flying beast infestation on my own. If we don\'t do something now, it\'ll be too late. Why don\'t we reduce their population?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"defeat_bots\": [\n        {\n            \"id\": 36,\n            \"value\": 100\n        },\n        {\n            \"id\": 50,\n            \"value\": 15\n        }\n    ]\n}'),
(73,8,28,1,1,19396,3409,'[{\"action\":true,\"text\":\"Stranger, we all need your help. We\'re under attack by flying beasts. Please help us.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Repel an attack\",\n            \"x\": 16224,\n            \"y\": 1600,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 36,\n                \"attribute_power\": 60\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Repel an attack\",\n            \"x\": 17248,\n            \"y\": 1120,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 36,\n                \"attribute_power\": 60\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Repel an attack\",\n            \"x\": 22432,\n            \"y\": 1919,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 36,\n                \"attribute_power\": 60\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Repel an attack\",\n            \"x\": 22816,\n            \"y\": 2048,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 36,\n                \"attribute_power\": 60\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Repel an attack\",\n            \"x\": 17491,\n            \"y\": 4543,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 36,\n                \"attribute_power\": 60\n            }\n        },\n        {\n            \"navigator\": \"true\",\n            \"name\": \"Repel an attack\",\n            \"x\": 25696,\n            \"y\": 3296,\n            \"world_id\": 1,\n            \"mode\": \"defeat_bot\",\n            \"defeat_bot\": {\n                \"id\": 36,\n                \"attribute_power\": 60\n            }\n        }\n    ]\n}'),
(74,8,29,1,1,19396,3409,'[{\"action\":true,\"text\":\"Hi. I need iron, as always. Not many people are helping. Bring me 25 bars of iron. I really need them to make arrows.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"required_items\": [\n        {\n            \"id\": 72,\n            \"value\": 25,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(75,9,30,1,1,19862,3473,'[{\"action\":true,\"text\":\"Hello, stranger. Do you like to hunt? We should hunt wild animals. I think 50 carcasses of each critter will be enough.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"defeat_bots\": [\n        {\n            \"id\": 39,\n            \"value\": 50\n        },\n        {\n            \"id\": 42,\n            \"value\": 50\n        }\n    ]\n}'),
(76,9,31,1,1,19862,3473,'[{\"action\":true,\"text\":\"Hi. Did you hear there\'s a ghost living in the swamps? Get rid of him. Only for sure. They say it keeps showing up. But try it anyway.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"defeat_bots\": [\n        {\n            \"id\": 52,\n            \"value\": 25\n        }\n    ]\n}'),
(77,9,32,1,1,19862,3473,'[{\"action\":true,\"text\":\"Hi. There\'s an orc settlement on the left in the steppe. You know what scares me? There\'s an ogre that just showed up and he\'s got dynamite in his head, you know? Okay, one, but they keep coming. You should visit them like this. I hope you come back with some ogre trophies.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"defeat_bots\": [\n        {\n            \"id\": 41,\n            \"value\": 20\n        }\n    ]\n}'),
(78,9,33,1,1,19862,3473,'[{\"text\":\"Aapphpphpph. Hello stranger. You smell that foul odor. It\'s the mushrooms. They\'re getting big and plentiful. Please get rid of them. I think there\'s some kind of magic involved. Good luck, traveler.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"defeat_bots\": [\n        {\n            \"id\": 49,\n            \"value\": 5\n        },\n        {\n            \"id\": 34,\n            \"value\": 80\n        }\n    ]\n}'),
(79,9,34,1,1,19862,3473,'[{\"action\":true,\"text\":\"Hi. My guys need good armor and a great hammer. Bring them to me. I owe you one\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"required_items\": [\n        {\n            \"id\": 78,\n            \"value\": 5,\n            \"type\": \"default\"\n        },\n        {\n            \"id\": 82,\n            \"value\": 5,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(80,7,35,1,1,19475,3217,'[{\"text\":\"Hello stranger. I\'m studying the fragments of darkness, if you could bring them to me.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"required_items\": [\n        {\n            \"id\": 111,\n            \"value\": 50,\n            \"type\": \"default\"\n        }\n    ]\n}'),
(81,7,36,1,1,19475,3217,'[{\"action\":true,\"text\":\"Hey, stranger. I want you to see how our aethers works. Find this location in the swamps.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"move_to\": [\n        {\n            \"step\": 1,\n            \"navigator\": \"true\",\n            \"name\": \"To get to aether\",\n            \"x\": 8063,\n            \"y\": 3781,\n            \"world_id\": 1,\n            \"mode\": \"move\"\n        }\n    ]\n}'),
(82,7,37,1,1,19475,3217,'[{\"action\":true,\"text\":\"Hello, stranger. Between the prairie and the plain, there\'s some kind of creature. It changes color. It\'s very frightening to the people of Gridania. You can reduce their numbers. Destroy 25 of the creatures.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"defeat_bots\": [\n        {\n            \"id\": 51,\n            \"value\": 25\n        }\n    ]\n}'),
(83,7,38,1,1,19475,3217,'[{\"action\":true,\"text\":\"Hi. I\'m still learning the elements of dark creatures and substances. But there are still a lot of kinda dead and kinda not dead dolls walking around near the cemetery. You can make the world a more peaceful place. Destroy 50 dolls\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"defeat_bots\": [\n        {\n            \"id\": 43,\n            \"value\": 50\n        }\n    ]\n}'),
(84,7,39,1,1,19475,3217,'[{\"action\":true,\"text\":\"Hi. There\'s a dungeon behind the cemetery. I don\'t know where it came from, but there\'s too many zombies. Can you figure it out? I\'d appreciate it if you could kill 50 zombies.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"defeat_bots\": [\n        {\n            \"id\": 44,\n            \"value\": 50\n        }\n    ]\n}'),
(85,8,40,1,1,19396,3409,'[{\"action\":true,\"text\":\"We periodically exterminate populations of forest dwellers and borderlands to prevent such accidents, but these creatures have begun spawning out of season, and so it is becoming increasingly difficult to control their numbers.\\nIf you want to prove yourself, go to the forest and border zones and eliminate 20 pests each in all zones. Be careful and approach them one at a time so your adventurer career doesn\'t get cut short.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\n    \"move_to_completes_quest_step\": true,\n    \"defeat_bots\": [\n        {\n            \"id\": 31,\n            \"value\": 20\n        },\n        {\n            \"id\": 32,\n            \"value\": 20\n        },\n        {\n            \"id\": 33,\n            \"value\": 20\n        },\n        {\n            \"id\": 34,\n            \"value\": 20\n        },\n        {\n            \"id\": 36,\n            \"value\": 20\n        },\n        {\n            \"id\": 42,\n            \"value\": 20\n        }\n    ]\n}'),
(86,1,41,1,1,19548,4305,'[{\"text\":\"Wait, you left so quickly, I\'d listen now if I were you. Just listen. This world is unlike any other, it\'s very exciting and full of possibilities. So, step one! You need to open the menu where you normally vote. It\'s called the Voting Panel. \",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"This is a very important panel, because you will interact with it most often. Your first task is to select the player\'s class. How to do this: \\n1. Go to the voting menu\\n2. Click on upgrades and professions\\n3. There will be a section at the top. Select a class. Choose the one you like.\\n4. Don\'t miss the most important part! Familiarize yourself with the other buttons. Give it a try, you won\'t lose anything.\\n5.  The most important thing!!!! when you familiarize yourself. The heart or  shield next to you is the navigator. It will help you always find your way\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,''),
(87,1,41,2,1,21125,4433,'[{\"action\":true,\"text\":\"Where to start, actually. Remember we talked about the voting panel, we always interact through it, like if it\'s different stores, houses, craft places, etc. There are so many different interactions. \\nAnd so click on the panel. At the top will be information about it, also pay attention to it.\\nAt the bottom are all the crafting items. They are divided into groups. To craft something, you need to click on the item and it will say:\\nRequired items:\\nitem - quantity\\n\\nAnd then the graph of crafting (gold). Click on it. And we create an item. To avoid wasting time, you can specify the number of items in the field where you specify the reason.\",\"side\":\"default\",\"left_speaker_id\":-1,\"right_speaker_id\":0}]',NULL,''),
(88,1,41,3,1,22405,4721,'[{\"action\":true,\"text\":\"So we\'re the ones with the ore. I think you already know how to process it. This is for your practice.  We have 5 stones to mine. It\'s not that hard, but it\'ll be a lot of fun afterwards. Oh yes when you mine or farm you can get different items than just 1.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 173,\r\n            \"value\": 5,\r\n            \"type\": \"show\"\r\n        }\r\n    ]\r\n}'),
(89,1,41,4,1,21986,6001,'[{\"action\":true,\"text\":\"Okay. This is one of the stores where you can sell certain things. Give him the stones. Remember how we talked through our voting menu. Oh, I almost forgot. We have a little bit of an economy here. That is, each player contributes to the economy, he mines ore and the store in turn makes products from it. And these products you can transfer to other stores where you can buy something and get gold for it! Work as a courier and for money. Unfortunately, without products you can\'t buy anything, alas. And so your task is to sell 5 stones. Try it. I\'ll go somewhere else and you catch up with me. Follow your heart navigator . But sell first!\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,'{\r\n    \"move_to_completes_quest_step\": true,\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Go to the Store\",\r\n            \"x\": 21769,\r\n            \"y\": 6065,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"cooldown\": 1,\r\n            \"completion_text\": \"Sell the stones\"\r\n        }\r\n    ]\r\n}'),
(90,1,41,5,1,18254,3057,'[{\"text\":\"I think you sold the stones and didn\'t keep them in your pockets. Heh-heh-heh. Remember we talked about classes?\\nWell, look, each class has its own abilities. And then there\'s general abilities. I guess if you\'re interested, you can look it up and click on it. So to make a long story short. You pump your skills and then you can do a bind on a certain button with a certain skill that you pump and you can quickly use it during the battle. Use it! Useful thing\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1},{\"action\":true,\"text\":\"In the meantime, I\'ll move on. Study\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,''),
(91,1,41,6,1,17345,5265,'[{\"action\":true,\"text\":\"It\'s a quiet neighborhood. You can buy a house here. Or buy a guild house if you are a guild leader. You can decorate your house, add friends, open doors, grow crops in your house. There are certain places where you can buy a house. Look for them, and you will find them. Let\'s move on\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,''),
(92,1,41,7,1,16980,2385,'[{\"action\":true,\"text\":\"It\'s a bank. This is where you can deposit your money. It\'s important. First of all, you can lose some of it during battles. You need to enter the territory of the bank and press the respawn button. After you do this, a window will open where you will need to point the cursor at the tabs: Deposit / Withdraw. Try if you have sold stones put the money in the bank. And I\'ll move on.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,''),
(93,1,41,8,1,16051,2289,'[{\"action\":true,\"text\":\"And this is our farm. This is where the plants are grown. This is where you can harvest your crops and sell them. Let me show you\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,''),
(94,1,41,9,1,15352,1425,'[{\"action\":true,\"text\":\"Well, that\'s the last part. This is where you can sell your harvest. These crops are also processed into products like in the miner\'s store. Don\'t forget to carry food if you want to buy something. And don\'t forget to upgrade your skills, if you remember where. There you can pump the efficiency of the miner and farmer. And that\'s not all I told you! Find Bertennant. He will tell you. If anything, you can always find me\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]',NULL,''),
(95,16,42,1,1,15153,1393,'[{\"action\":true,\"text\":\"Hi. This is a difficult time in our community. I was wondering if I could ask you to harvest some of the stock. Bring me 500 wheat. I will reward you\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 70,\r\n            \"value\": 500,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(96,16,43,1,1,15153,1393,'[{\"action\":true,\"text\":\"Hi. My ranch needs building materials. Can you bring me 500 stones?  That\'d be great. Thanks.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 173,\r\n            \"value\": 500,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(97,16,44,1,1,15153,1393,'[{\"action\":true,\"text\":\"Hi. As you know, raw food doesn\'t taste very good. Get me some 200 coal so I can cook over a fire. Uh-huh. Berries on a coal fire.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 50,\r\n            \"value\": 200,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(98,16,45,1,1,15153,1393,'[{\"action\":true,\"text\":\"Hi. You already know we have a lot of breakdowns. Iron ingots can help us fix or replace our tools. Can you please bring me 100 iron ignot?\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 72,\r\n            \"value\": 100,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(99,16,46,1,1,15153,1393,'[{\"action\":true,\"text\":\"Hello, my friend. I want berries. Yeah. Bring me 250 berries. I\'d love it if you\'d do that for me.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 67,\r\n            \"value\": 250,\r\n            \"type\": \"default\"\r\n        }\r\n    ]\r\n}'),
(100,16,47,1,1,15153,1393,'[{\"action\":true,\"text\":\"Uh-huh. I need help cleaning up. You come to my ranch a lot.  Help me, please.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\r\n    \"move_to\": [\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Clean up the ranch\",\r\n            \"x\": 15057,\r\n            \"y\": 1425,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 20,\r\n            \"completion_text\": \"You cleared this place out\",\r\n            \"pick_up_item\": {\r\n                \"id\": 92,\r\n                \"value\": 1\r\n            }\r\n        },\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Clean up the ranch\",\r\n            \"x\": 14944,\r\n            \"y\": 1169,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 20,\r\n            \"completion_text\": \"You cleared this place out\",\r\n            \"pick_up_item\": {\r\n                \"id\": 92,\r\n                \"value\": 1\r\n            }\r\n        },\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Clean up the ranch\",\r\n            \"x\": 14899,\r\n            \"y\": 1361,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 20,\r\n            \"completion_text\": \"You cleared this place out\",\r\n            \"pick_up_item\": {\r\n                \"id\": 92,\r\n                \"value\": 1\r\n            }\r\n        },\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Clean up the ranch\",\r\n            \"x\": 15443,\r\n            \"y\": 1425,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 20,\r\n            \"completion_text\": \"You cleared this place out\"\r\n        }\r\n    ]\r\n}'),
(101,16,48,1,1,15153,1393,'[{\"action\":true,\"text\":\"Hey, stranger. I have an assignment for you to investigate the entire area. There might be some wild plants somewhere. We\'ll need that information in the future.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\r\n    \"move_to_completes_quest_step\": true,\r\n    \"move_to\": [\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Scout the area\",\r\n            \"x\": 9962,\r\n            \"y\": 1777,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"cooldown\": 5\r\n        },\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Scout the area\",\r\n            \"x\": 6329,\r\n            \"y\": 433,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"cooldown\": 5\r\n        },\r\n        {\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Scout the area\",\r\n            \"x\": 27729,\r\n            \"y\": 1137,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"cooldown\": 5\r\n        }\r\n    ]\r\n}'),
(102,16,49,1,1,15153,1393,'[{\"action\":true,\"text\":\"Hey, I need your help. Plant crops on all the farms\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\r\n    \"move_to_completes_quest_step\": true,\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Plant a crop\",\r\n            \"x\": 15590,\r\n            \"y\": 2321,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"cooldown\": 60,\r\n            \"completion_text\": \"You planted a crop\"\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Plant a crop\",\r\n            \"x\": 15169,\r\n            \"y\": 2289,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 60,\r\n            \"completion_text\": \"You planted a crop\"\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Plant a crop\",\r\n            \"x\": 14751,\r\n            \"y\": 2257,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 60,\r\n            \"completion_text\": \"You planted a crop\"\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Plant a crop\",\r\n            \"x\": 14325,\r\n            \"y\": 2289,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 60,\r\n            \"completion_text\": \"You planted a crop\"\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Plant a crop\",\r\n            \"x\": 13959,\r\n            \"y\": 2321,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 60,\r\n            \"completion_text\": \"You planted a crop\"\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Plant a crop\",\r\n            \"x\": 9693,\r\n            \"y\": 1681,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 60,\r\n            \"completion_text\": \"You planted a crop\"\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Plant a crop\",\r\n            \"x\": 9279,\r\n            \"y\": 1649,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 60,\r\n            \"completion_text\": \"You planted a crop\"\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Plant a crop\",\r\n            \"x\": 8878,\r\n            \"y\": 1617,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move_press\",\r\n            \"cooldown\": 60,\r\n            \"completion_text\": \"You planted a crop\"\r\n        }\r\n    ]\r\n}'),
(103,16,50,1,1,15153,1393,'[{\"action\":true,\"text\":\"Hi, I want to check out our fields and beds and see if you can harvest some crops.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\r\n    \"move_to_completes_quest_step\": true,\r\n    \"required_items\": [\r\n        {\r\n            \"id\": 70,\r\n            \"value\": 50,\r\n            \"type\": \"default\"\r\n        }\r\n    ],\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Come to the farm with the plantings\",\r\n            \"x\": 15573,\r\n            \"y\": 2321,\r\n            \"world_id\": 1,\r\n            \"mode\": \"move\",\r\n            \"completion_text\": \"You\'ve come to the farm. Collect 50 wheat\"\r\n        }\r\n    ]\r\n}'),
(104,16,51,1,1,15153,1393,'[{\"action\":true,\"text\":\"Hello, damn pests. Can you get rid of them or we\'ll be out of crops.\",\"side\":\"default\",\"left_speaker_id\":0,\"right_speaker_id\":-1}]','','{\r\n    \"move_to_completes_quest_step\": true,\r\n    \"move_to\": [\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Pest control\",\r\n            \"x\": 15170,\r\n            \"y\": 2289,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 33,\r\n                \"attribute_power\": 30,\r\n                \"world_id\": 1\r\n            }\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Pest control\",\r\n            \"x\": 15589,\r\n            \"y\": 2321,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 33,\r\n                \"attribute_power\": 30,\r\n                \"world_id\": 1\r\n            }\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Pest control\",\r\n            \"x\": 14730,\r\n            \"y\": 2257,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 33,\r\n                \"attribute_power\": 30,\r\n                \"world_id\": 1\r\n            }\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Pest control\",\r\n            \"x\": 14317,\r\n            \"y\": 2289,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 33,\r\n                \"attribute_power\": 30,\r\n                \"world_id\": 1\r\n            }\r\n        },\r\n        {\r\n            \"step\": 1,\r\n            \"navigator\": \"true\",\r\n            \"name\": \"Pest control\",\r\n            \"x\": 13956,\r\n            \"y\": 2321,\r\n            \"world_id\": 1,\r\n            \"mode\": \"defeat_bot\",\r\n            \"defeat_bot\": {\r\n                \"id\": 33,\r\n                \"attribute_power\": 30,\r\n                \"world_id\": 1\r\n            }\r\n        }\r\n    ]\r\n}');
/*!40000 ALTER TABLE `tw_bots_quest` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_crafts_list`
--

DROP TABLE IF EXISTS `tw_crafts_list`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_crafts_list` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `ItemID` int(11) DEFAULT NULL,
  `ItemValue` int(11) NOT NULL,
  `RequiredItems` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL CHECK (json_valid(`RequiredItems`)),
  `Price` int(11) NOT NULL DEFAULT 100,
  `WorldID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `CraftIID` (`ItemID`),
  KEY `WorldID` (`WorldID`),
  CONSTRAINT `tw_crafts_list_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_crafts_list_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=39 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_crafts_list`
--

LOCK TABLES `tw_crafts_list` WRITE;
/*!40000 ALTER TABLE `tw_crafts_list` DISABLE KEYS */;
INSERT INTO `tw_crafts_list` VALUES
(1,41,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 40,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',0,0),
(2,72,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 50,\r\n            \"value\": 50\r\n        },\r\n        {\r\n            \"id\": 54,\r\n            \"value\": 10\r\n        }\r\n\r\n    ]\r\n}',480,1),
(4,74,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 50,\r\n            \"value\": 200\r\n        },\r\n        {\r\n            \"id\": 56,\r\n            \"value\": 10\r\n        }\r\n\r\n    ]\r\n}',999999,1),
(9,78,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 72,\r\n            \"value\": 30\r\n        },\r\n        {\r\n            \"id\": 7,\r\n            \"value\": 50\r\n        }\r\n    ]\r\n}',11856,1),
(10,79,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 73,\r\n            \"value\": 150\r\n        },\r\n        {\r\n            \"id\": 78,\r\n            \"value\": 1\r\n        }\r\n\r\n    ]\r\n}',999999,1),
(11,81,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 74,\r\n            \"value\": 300\r\n        },\r\n        {\r\n            \"id\": 79,\r\n            \"value\": 1\r\n        }\r\n\r\n    ]\r\n}',99999,1),
(12,82,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 72,\r\n            \"value\": 50\r\n        },\r\n        {\r\n            \"id\": 108,\r\n            \"value\": 25\r\n        },\r\n        {\r\n            \"id\": 28,\r\n            \"value\": 1\r\n        },\r\n        {\r\n            \"id\": 7,\r\n            \"value\": 150\r\n        }\r\n    ]\r\n}',27408,1),
(13,83,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 82,\r\n            \"value\": 300\r\n        },\r\n        {\r\n            \"id\": 82,\r\n            \"value\": 1\r\n        }\r\n\r\n\r\n    ]\r\n}',999999,1),
(14,84,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 82,\r\n            \"value\": 500\r\n        },\r\n        {\r\n            \"id\": 83,\r\n            \"value\": 1\r\n        }\r\n\r\n\r\n    ]\r\n}',999999,1),
(15,26,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 72,\r\n            \"value\": 8\r\n        },\r\n        {\r\n            \"id\": 7,\r\n            \"value\": 50\r\n        }\r\n    ]\r\n}',2726,1),
(16,42,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 73,\r\n            \"value\": 100\r\n        },\r\n        {\r\n            \"id\": 26,\r\n            \"value\": 1\r\n        }\r\n\r\n    ]\r\n}',999999,1),
(17,62,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 74,\r\n            \"value\": 100\r\n        },\r\n        {\r\n            \"id\": 42,\r\n            \"value\": 1\r\n        }\r\n\r\n    ]\r\n}',999999,1),
(18,27,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 72,\r\n            \"value\": 10\r\n        },\r\n        {\r\n            \"id\": 7,\r\n            \"value\": 50\r\n        }\r\n    ]\r\n}',4176,1),
(19,43,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 73,\r\n            \"value\": 150\r\n        },\r\n        {\r\n            \"id\": 27,\r\n            \"value\": 1\r\n        }\r\n\r\n    ]\r\n}',99999,1),
(20,63,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 74,\r\n            \"value\": 150\r\n        },\r\n        {\r\n            \"id\": 43,\r\n            \"value\": 1\r\n        }\r\n\r\n    ]\r\n}',99999,1),
(21,104,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 92,\r\n            \"value\": 25\r\n        },\r\n        {\r\n            \"id\": 7,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',30,1),
(22,106,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 105,\r\n            \"value\": 10\r\n        }\r\n    ]\r\n}',576,1),
(23,108,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 107,\r\n            \"value\": 10\r\n        }\r\n    ]\r\n}',240,1),
(24,138,2,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 113,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',5,1),
(25,138,4,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 125,\r\n            \"value\": 1\r\n        }\r\n    ]\r\n}',10,1),
(26,119,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 108,\r\n            \"value\": 100\r\n        },\r\n        {\r\n            \"id\": 54,\r\n            \"value\": 5\r\n        },\r\n        {\r\n            \"id\": 104,\r\n            \"value\": 500\r\n        }\r\n    ]\r\n}',9999999,1),
(27,132,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 108,\r\n            \"value\": 150\r\n        },\r\n        {\r\n            \"id\": 121,\r\n            \"value\": 100\r\n        },\r\n        {\r\n            \"id\": 134,\r\n            \"value\": 10\r\n        }\r\n    ]\r\n}',99999,1),
(28,135,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 108,\r\n            \"value\": 150\r\n        },\r\n        {\r\n            \"id\": 104,\r\n            \"value\": 500\r\n        },\r\n        {\r\n            \"id\": 115,\r\n            \"value\": 800\r\n        }\r\n    ]\r\n}',999999,1),
(29,136,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 134,\r\n            \"value\": 10\r\n        },\r\n        {\r\n            \"id\": 115,\r\n            \"value\": 100\r\n        }\r\n    ]\r\n}',9999,1),
(30,64,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 106,\r\n            \"value\": 1000\r\n        },\r\n        {\r\n            \"id\": 117,\r\n            \"value\": 200\r\n        }\r\n    ]\r\n}',9999999,1),
(31,115,10,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 121,\r\n            \"value\": 1000\r\n        }\r\n    ]\r\n}',500,1),
(32,73,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 50,\r\n            \"value\": 150\r\n        },\r\n        {\r\n            \"id\": 55,\r\n            \"value\": 10\r\n        }\r\n\r\n    ]\r\n}',100,1),
(33,66,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 116,\r\n            \"value\": 1000\r\n        },\r\n        {\r\n            \"id\": 128,\r\n            \"value\": 500\r\n        },\r\n        {\r\n            \"id\": 117,\r\n            \"value\": 200\r\n        }\r\n    ]\r\n}',9999999,1),
(34,65,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 127,\r\n            \"value\": 3000\r\n        },\r\n        {\r\n            \"id\": 117,\r\n            \"value\": 200\r\n        }\r\n    ]\r\n}',9999999,1),
(35,28,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 108,\r\n            \"value\": 15\r\n        },\r\n        {\r\n            \"id\": 7,\r\n            \"value\": 50\r\n        }\r\n    ]\r\n}',3880,1),
(36,39,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 50,\r\n            \"value\": 50\r\n        },\r\n        {\r\n            \"id\": 54,\r\n            \"value\": 25\r\n        },\r\n        {\r\n            \"id\": 7,\r\n            \"value\": 50\r\n        }\r\n    ]\r\n}',576,1),
(37,127,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 123,\r\n            \"value\": 40\r\n        },\r\n        {\r\n            \"id\": 138,\r\n            \"value\": 150\r\n        }\r\n    ]\r\n}',1656,1),
(38,19,1,'{\r\n    \"items\": [\r\n        {\r\n            \"id\": 3,\r\n            \"value\": 1\r\n        },\r\n        {\r\n            \"id\": 127,\r\n            \"value\": 70\r\n        },\r\n        {\r\n            \"id\": 108,\r\n            \"value\": 10\r\n        },\r\n        {\r\n            \"id\": 171,\r\n            \"value\": 20\r\n        },\r\n        {\r\n            \"id\": 97,\r\n            \"value\": 1\r\n        },\r\n        {\r\n            \"id\": 152,\r\n            \"value\": 50\r\n        }\r\n    ]\r\n}',746784,1);
/*!40000 ALTER TABLE `tw_crafts_list` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_dungeons`
--

DROP TABLE IF EXISTS `tw_dungeons`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_dungeons` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(64) NOT NULL DEFAULT 'Unknown',
  `Level` int(11) NOT NULL DEFAULT 1,
  `DoorX` int(11) NOT NULL DEFAULT 0,
  `DoorY` int(11) NOT NULL DEFAULT 0,
  `RequiredQuestID` int(11) NOT NULL DEFAULT -1,
  `Story` tinyint(4) NOT NULL DEFAULT 0,
  `WorldID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  KEY `WorldID` (`WorldID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_dungeons`
--

LOCK TABLES `tw_dungeons` WRITE;
/*!40000 ALTER TABLE `tw_dungeons` DISABLE KEYS */;
/*!40000 ALTER TABLE `tw_dungeons` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_dungeons_door`
--

DROP TABLE IF EXISTS `tw_dungeons_door`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_dungeons_door` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(64) NOT NULL DEFAULT 'Write here name dungeon',
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `BotID` int(11) NOT NULL,
  `DungeonID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  KEY `tw_dungeons_door_ibfk_1` (`DungeonID`),
  KEY `tw_dungeons_door_ibfk_2` (`BotID`),
  CONSTRAINT `tw_dungeons_door_ibfk_1` FOREIGN KEY (`DungeonID`) REFERENCES `tw_dungeons` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_dungeons_door_ibfk_2` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_dungeons_door`
--

LOCK TABLES `tw_dungeons_door` WRITE;
/*!40000 ALTER TABLE `tw_dungeons_door` DISABLE KEYS */;
/*!40000 ALTER TABLE `tw_dungeons_door` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_dungeons_records`
--

DROP TABLE IF EXISTS `tw_dungeons_records`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_dungeons_records` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `UserID` int(11) NOT NULL,
  `DungeonID` int(11) NOT NULL,
  `Lifetime` int(11) NOT NULL,
  `PassageHelp` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`),
  KEY `tw_dungeons_records_ibfk_1` (`UserID`),
  KEY `DungeonID` (`DungeonID`),
  KEY `Seconds` (`Lifetime`),
  CONSTRAINT `tw_dungeons_records_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_dungeons_records_ibfk_2` FOREIGN KEY (`DungeonID`) REFERENCES `tw_dungeons` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_dungeons_records`
--

LOCK TABLES `tw_dungeons_records` WRITE;
/*!40000 ALTER TABLE `tw_dungeons_records` DISABLE KEYS */;
/*!40000 ALTER TABLE `tw_dungeons_records` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_groups`
--

DROP TABLE IF EXISTS `tw_groups`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_groups` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `OwnerUID` int(11) NOT NULL,
  `AccountIDs` varchar(64) DEFAULT NULL,
  PRIMARY KEY (`ID`),
  KEY `tw_groups_ibfk_1` (`OwnerUID`),
  CONSTRAINT `tw_groups_ibfk_1` FOREIGN KEY (`OwnerUID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE NO ACTION ON UPDATE NO ACTION
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_groups`
--

LOCK TABLES `tw_groups` WRITE;
/*!40000 ALTER TABLE `tw_groups` DISABLE KEYS */;
INSERT INTO `tw_groups` VALUES
(3,2,'2'),
(4,11,'11'),
(5,13,'13');
/*!40000 ALTER TABLE `tw_groups` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_guilds`
--

DROP TABLE IF EXISTS `tw_guilds`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_guilds` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
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
  `ChairExperience` int(11) NOT NULL DEFAULT 1,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `OwnerID` (`LeaderUID`),
  KEY `Bank` (`Bank`(768)),
  KEY `Level` (`Level`),
  KEY `Experience` (`Exp`),
  CONSTRAINT `tw_guilds_ibfk_1` FOREIGN KEY (`LeaderUID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_guilds`
--

LOCK TABLES `tw_guilds` WRITE;
/*!40000 ALTER TABLE `tw_guilds` DISABLE KEYS */;
INSERT INTO `tw_guilds` VALUES
(1,'PORNO','{\"members\":[{\"deposit\":\"0\",\"id\":3,\"rank_id\":1}]}',1,3,10,840,'246',0,0,2,1),
(2,'sex','{\"members\":[{\"deposit\":\"0\",\"id\":2,\"rank_id\":3}]}',3,2,6,579,'400',0,0,2,1),
(3,'Babylon','{\"members\":[{\"deposit\":\"11111\",\"id\":13,\"rank_id\":4}]}',4,13,4,213,'6611',0,0,2,1);
/*!40000 ALTER TABLE `tw_guilds` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_guilds_decorations`
--

DROP TABLE IF EXISTS `tw_guilds_decorations`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_guilds_decorations` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `HouseID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `WorldID` int(11) DEFAULT NULL,
  PRIMARY KEY (`ID`),
  KEY `tw_guilds_decorations_ibfk_2` (`ItemID`),
  KEY `tw_guilds_decorations_ibfk_3` (`WorldID`),
  KEY `HouseID` (`HouseID`),
  CONSTRAINT `tw_guilds_decorations_ibfk_2` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_guilds_decorations_ibfk_4` FOREIGN KEY (`HouseID`) REFERENCES `tw_guilds_houses` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_guilds_decorations_ibfk_5` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=58 DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_guilds_decorations`
--

LOCK TABLES `tw_guilds_decorations` WRITE;
/*!40000 ALTER TABLE `tw_guilds_decorations` DISABLE KEYS */;
INSERT INTO `tw_guilds_decorations` VALUES
(35,15056,3056,2,13,1),
(37,15120,3056,2,13,1),
(38,15120,3088,2,13,1),
(39,15120,3120,2,13,1),
(40,15152,3120,2,13,1),
(41,15184,3152,2,13,1),
(42,15152,3184,2,13,1),
(43,15120,3184,2,13,1),
(44,15088,3152,2,13,1),
(45,15056,3184,2,13,1),
(46,15024,3184,2,13,1),
(47,14992,3152,2,13,1),
(48,15024,3120,2,13,1),
(49,15056,3120,2,13,1),
(50,15056,3088,2,13,1),
(53,15056,3024,2,13,1),
(54,15056,2992,2,13,1),
(55,15120,2992,2,13,1),
(56,15120,3024,2,13,1),
(57,15088,2960,2,13,1);
/*!40000 ALTER TABLE `tw_guilds_decorations` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_guilds_history`
--

DROP TABLE IF EXISTS `tw_guilds_history`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_guilds_history` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `GuildID` int(11) NOT NULL,
  `Text` varchar(64) NOT NULL,
  `Time` datetime NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`ID`),
  KEY `MemberID` (`GuildID`)
) ENGINE=InnoDB AUTO_INCREMENT=14 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_guilds_history`
--

LOCK TABLES `tw_guilds_history` WRITE;
/*!40000 ALTER TABLE `tw_guilds_history` DISABLE KEYS */;
INSERT INTO `tw_guilds_history` VALUES
(1,1,'added rank \'Newbie\'','2025-01-10 21:54:00'),
(2,1,'Kurosio2 rank changed to Newbie','2025-01-10 21:54:00'),
(3,1,'\'Kurosio2\' deposit \'15666\' in the guild safe.','2025-01-10 21:55:00'),
(4,1,'Your guild has purchased a house!','2025-01-10 21:55:00'),
(5,2,'HaLLoWeeN`iPro rank changed to Newbie','2025-01-20 01:49:00'),
(6,2,'added rank \'Newbie\'','2025-01-20 01:49:00'),
(7,2,'\'HaLLoWeeN`iPro\' deposit \'2000\' in the guild safe.','2025-01-20 01:51:00'),
(8,2,'Your guild has purchased a house!','2025-01-20 01:51:00'),
(9,2,'House extended by 1 days.','2025-01-20 01:52:00'),
(10,3,'added rank \'Newbie\'','2025-02-01 20:37:00'),
(11,3,'Trisha rank changed to Newbie','2025-02-01 20:37:00'),
(12,3,'\'Trisha\' deposit \'11111\' in the guild safe.','2025-02-01 20:41:00'),
(13,3,'Your guild has purchased a house!','2025-02-01 20:41:00');
/*!40000 ALTER TABLE `tw_guilds_history` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_guilds_houses`
--

DROP TABLE IF EXISTS `tw_guilds_houses`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_guilds_houses` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `GuildID` int(11) DEFAULT NULL,
  `InitialFee` int(11) NOT NULL DEFAULT 0,
  `RentDays` int(11) NOT NULL DEFAULT 3,
  `Doors` longtext DEFAULT NULL,
  `Farmzones` longtext DEFAULT NULL,
  `Properties` longtext DEFAULT NULL,
  `WorldID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `OwnerMID` (`GuildID`),
  KEY `WorldID` (`WorldID`),
  CONSTRAINT `tw_guilds_houses_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_guilds_houses`
--

LOCK TABLES `tw_guilds_houses` WRITE;
/*!40000 ALTER TABLE `tw_guilds_houses` DISABLE KEYS */;
INSERT INTO `tw_guilds_houses` VALUES
(1,NULL,11664,4,'[\n    {\n        \"name\": \"Front Door\",\n        \"position\": {\"x\": 17824, \"y\": 1600}\n    },\n    {\n        \"name\": \"Back Door\",\n        \"position\": {\"x\": 19744, \"y\": 2240}\n    },\n    {\n        \"name\": \"Farm zone\",\n        \"position\": {\"x\": 18304, \"y\": 1888}\n    },\n    {\n        \"name\": \"Farm zone\",\n        \"position\": {\"x\": 19200, \"y\": 2240}\n    }\n]','[\n    {\n        \"name\": \"FarmZone 1\",\n        \"position\": {\n            \"x\": 18112,\n            \"y\": 1888\n        },\n        \"items\": \"70\",\n        \"radius\": 192\n    },\n    {\n        \"name\": \"FarmZone 2\",\n        \"position\": {\n            \"x\": 19008,\n            \"y\": 2240\n        },\n        \"items\": \"70\",\n        \"radius\": 192\n    }\n]','{\r\n    \"position\": {\"x\": 18720, \"y\": 1568},\r\n    \"text_position\": {\"x\": 19456, \"y\": 1648},\r\n    \"radius\": 1376\r\n}',1),
(2,1,9720,1,'[\r\n    {\r\n        \"name\": \"Door\",\r\n        \"position\": {\"x\": 15686, \"y\": 3313}\r\n    },\r\n    {\r\n        \"name\": \"Bedroom\",\r\n        \"position\": {\"x\": 15246, \"y\": 2961}\r\n    },\r\n    {\r\n        \"name\": \"Farm zone\",\r\n        \"position\": {\"x\": 14888, \"y\": 3313}\r\n    }\r\n]','[\n    {\n        \"name\": \"FarmZone 1\",\n        \"position\": {\n            \"x\": 14708,\n            \"y\": 3313\n        },\n        \"items\": \"70\",\n        \"radius\": 192\n    }\n]','{\r\n    \"position\": {\"x\": 15200, \"y\": 3008},\r\n    \"text_position\": {\"x\": 15663, \"y\": 2576},\r\n    \"radius\": 864\r\n}',1),
(3,NULL,8100,4,'[\r\n    {\r\n        \"name\": \"Front Door\",\r\n        \"position\": {\"x\": 12384, \"y\": 832}\r\n    },\r\n    {\r\n        \"name\": \"Back Door\",\r\n        \"position\": {\"x\": 10656, \"y\": 928}\r\n    },\r\n    {\r\n        \"name\": \"Farm zone\",\r\n        \"position\": {\"x\": 11200, \"y\": 928}\r\n    }\r\n]','[\n    {\n        \"name\": \"FarmZone\",\n        \"position\": {\n            \"x\": 11744,\n            \"y\": 928\n        },\n        \"items\": \"70\",\n        \"radius\": 256\n    }\n]','{\r\n    \"position\": {\"x\": 11520, \"y\": 544},\r\n    \"text_position\": {\"x\": 11528, \"y\": 1040},\r\n    \"radius\": 1120\r\n}',1),
(4,NULL,6750,4,'[\r\n    {\r\n        \"name\": \"Front Door\",\r\n        \"position\": {\"x\": 16224, \"y\": 5056}\r\n    },\r\n    {\r\n        \"name\": \"Farm zone\",\r\n        \"position\": {\"x\": 16832, \"y\": 6208}\r\n    }\r\n]','[\r\n    {\r\n        \"name\": \"FarmZone\",\r\n        \"position\": {\r\n            \"x\": 16928,\r\n            \"y\": 4768\r\n        },\r\n        \"item_id\": 70,\r\n        \"radius\": 128\r\n    }\r\n]','{\r\n    \"position\": {\"x\": 16352, \"y\": 4704},\r\n    \"text_position\": {\"x\": 16016, \"y\": 5185},\r\n    \"radius\": 736\r\n}',1),
(5,3,4500,1,'[\r\n    {\r\n        \"name\": \"Front Door\",\r\n        \"position\": {\"x\": 26240, \"y\": 2432}\r\n    },\r\n    {\r\n        \"name\": \"Farm zone\",\r\n        \"position\": {\"x\": 26432, \"y\": 2208}\r\n    }\r\n]','[\n    {\n        \"name\": \"FarmZone\",\n        \"position\": {\n            \"x\": 26368,\n            \"y\": 2208\n        },\n        \"items\": \"70\",\n        \"radius\": 96\n    }\n]','{\r\n    \"position\": {\"x\": 26592, \"y\": 2304},\r\n    \"text_position\": {\"x\": 26627, \"y\": 2642},\r\n    \"radius\": 512\r\n}',1),
(6,NULL,3000,4,'[\r\n    {\r\n        \"name\": \"Front Door\",\r\n        \"position\": {\"x\": 27744, \"y\": 1856}\r\n    },\r\n    {\r\n        \"name\": \"Farm zone\",\r\n        \"position\": {\"x\": 27872, \"y\": 1536}\r\n    }\r\n]','[\n    {\n        \"name\": \"FarmZone\",\n        \"position\": {\n            \"x\": 27968,\n            \"y\": 1536\n        },\n        \"items\": \"70\",\n        \"radius\": 96\n    }\n]','{\r\n    \"position\": {\"x\": 27712, \"y\": 1536},\r\n    \"text_position\": {\"x\": 28067, \"y\": 1248},\r\n    \"radius\": 840\r\n}',1);
/*!40000 ALTER TABLE `tw_guilds_houses` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_guilds_invites`
--

DROP TABLE IF EXISTS `tw_guilds_invites`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_guilds_invites` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `GuildID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `OwnerID` (`UserID`),
  KEY `MemberID` (`GuildID`),
  CONSTRAINT `tw_guilds_invites_ibfk_1` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_guilds_invites_ibfk_2` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_guilds_invites`
--

LOCK TABLES `tw_guilds_invites` WRITE;
/*!40000 ALTER TABLE `tw_guilds_invites` DISABLE KEYS */;
/*!40000 ALTER TABLE `tw_guilds_invites` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_guilds_ranks`
--

DROP TABLE IF EXISTS `tw_guilds_ranks`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_guilds_ranks` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Rights` int(11) NOT NULL DEFAULT 3,
  `Name` varchar(32) NOT NULL DEFAULT 'Rank name',
  `GuildID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `MemberID` (`GuildID`)
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_guilds_ranks`
--

LOCK TABLES `tw_guilds_ranks` WRITE;
/*!40000 ALTER TABLE `tw_guilds_ranks` DISABLE KEYS */;
INSERT INTO `tw_guilds_ranks` VALUES
(1,0,'Newbie',1),
(2,0,'PIDOR',1),
(3,0,'Newbie',2),
(4,0,'Newbie',3);
/*!40000 ALTER TABLE `tw_guilds_ranks` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_houses`
--

DROP TABLE IF EXISTS `tw_houses`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_houses` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
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
  `WorldID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `OwnerID` (`UserID`),
  KEY `WorldID` (`WorldID`),
  KEY `PlantID` (`PlantID`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_houses`
--

LOCK TABLES `tw_houses` WRITE;
/*!40000 ALTER TABLE `tw_houses` DISABLE KEYS */;
/*!40000 ALTER TABLE `tw_houses` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_houses_decorations`
--

DROP TABLE IF EXISTS `tw_houses_decorations`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_houses_decorations` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `HouseID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `WorldID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `WorldID` (`WorldID`),
  KEY `HouseID` (`HouseID`),
  KEY `DecoID` (`ItemID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_houses_decorations`
--

LOCK TABLES `tw_houses_decorations` WRITE;
/*!40000 ALTER TABLE `tw_houses_decorations` DISABLE KEYS */;
/*!40000 ALTER TABLE `tw_houses_decorations` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_items_list`
--

DROP TABLE IF EXISTS `tw_items_list`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_items_list` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(32) NOT NULL DEFAULT 'Item name',
  `Description` varchar(64) NOT NULL DEFAULT 'Item desc',
  `Group` enum('Other','Usable','Resource','Quest','Settings','Equipment','Decoration','Potion','Currency') NOT NULL DEFAULT 'Quest',
  `Type` enum('Default','Equip hammer','Equip gun','Equip shotgun','Equip grenade','Equip rifle','Equip pickaxe','Equip rake','Equip armor','Equip eidolon','Equip title','Equip potion HP','Equip potion MP','Single use x1','Multiple use x99','Resource harvestable','Resource mineable') NOT NULL DEFAULT 'Default',
  `InitialPrice` int(11) NOT NULL DEFAULT 100,
  `Desynthesis` int(11) NOT NULL DEFAULT 100,
  `AT1` int(11) DEFAULT NULL,
  `AT2` int(11) DEFAULT NULL,
  `ATValue1` int(11) DEFAULT NULL,
  `ATValue2` int(11) DEFAULT NULL,
  `Data` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL CHECK (json_valid(`Data`)),
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ItemID` (`ID`),
  KEY `ItemBonus` (`AT1`),
  KEY `ItemID_2` (`ID`),
  KEY `tw_items_list_ibfk_5` (`AT2`),
  CONSTRAINT `tw_items_list_ibfk_1` FOREIGN KEY (`AT1`) REFERENCES `tw_attributes` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `tw_items_list_ibfk_2` FOREIGN KEY (`AT2`) REFERENCES `tw_attributes` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=178 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_items_list`
--

LOCK TABLES `tw_items_list` WRITE;
/*!40000 ALTER TABLE `tw_items_list` DISABLE KEYS */;
INSERT INTO `tw_items_list` VALUES
(1,'Gold','Major currency','Currency','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(2,'Hammer','A normal hammer','Equipment','Equip hammer',0,0,13,3,3,5,NULL),
(3,'Gun','Conventional weapon','Equipment','Equip gun',1400,200,14,NULL,1,NULL,NULL),
(4,'Shotgun','Conventional weapon','Equipment','Equip shotgun',2800,400,15,NULL,1,NULL,NULL),
(5,'Grenade','Conventional weapon','Equipment','Equip grenade',3500,500,16,NULL,3,NULL,NULL),
(6,'Rifle','Conventional weapon','Equipment','Equip rifle',4200,600,17,NULL,5,NULL,NULL),
(7,'Material','Required to improve weapons','Currency','Default',7,0,NULL,NULL,NULL,NULL,NULL),
(8,'Product','Required to shop','Currency','Default',7,0,NULL,NULL,NULL,NULL,NULL),
(9,'Skill point','Skill point','Currency','Default',5,0,NULL,NULL,NULL,NULL,NULL),
(10,'Achievement point','Achievement Point','Currency','Default',5,0,NULL,NULL,NULL,NULL,NULL),
(11,'Pickup Shotgun','Decoration for house!','Decoration','Default',10000,10,NULL,NULL,NULL,NULL,NULL),
(12,'Pickup Grenade','Decoration for house!','Decoration','Default',10000,10,NULL,NULL,NULL,NULL,NULL),
(13,'Pickup Mana','Decoration for house!','Decoration','Default',10000,10,NULL,NULL,NULL,NULL,NULL),
(14,'Potion mana regen','Regenerate +5%, 15sec every sec.\n','Potion','Equip potion MP',1000,1,NULL,NULL,NULL,NULL,NULL),
(15,'Tiny HP Potion','Recovers 7HP per second for 10 seconds.','Potion','Equip potion HP',1000,1,NULL,NULL,NULL,NULL,'{\n  \"potion\": {\n    \"effect\": \"Tiny heal\",\n    \"value\": 5,\n    \"lifetime\": 10\n  }\n}'),
(16,'Capsule survival experience','You got 10-50 class experience','Usable','Multiple use x99',0,0,NULL,NULL,NULL,NULL,NULL),
(17,'Little bag of gold','You got 10-50 gold','Usable','Multiple use x99',0,0,NULL,NULL,NULL,NULL,NULL),
(18,'Pickup Health','Decoration for house!','Decoration','Default',10000,10,NULL,NULL,NULL,NULL,NULL),
(19,'Explosive module for gun','It happens sometimes','Equipment','Default',933480,5000,NULL,NULL,NULL,NULL,NULL),
(20,'Explosive module for shotgun','It happens sometimes','Equipment','Default',999999,10000,NULL,NULL,NULL,NULL,NULL),
(21,'Sworld','A regular sword','Quest','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(22,'Chigoe egg','The egg that Chigoe lays','Resource','Default',10,1,NULL,NULL,NULL,NULL,NULL),
(23,'Survey records','Strange incomprehensible records','Quest','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(24,'Toolbox','A drawer full of tools','Quest','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(25,'Show equipment description','Settings game.','Settings','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(26,'Iron rake','The usual rusty rake.','Equipment','Equip rake',3408,116,12,NULL,2,NULL,NULL),
(27,'Iron pickaxe','The usual iron pickaxe.','Equipment','Equip pickaxe',5220,136,11,NULL,2,NULL,NULL),
(28,'Leather armor','Lightweight armor','Equipment','Equip armor',5820,136,5,NULL,15,NULL,NULL),
(29,'Activity coin','Coins that are given out for activity','Currency','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(30,'Newbie','Newbie','Equipment','Equip title',0,0,NULL,NULL,NULL,NULL,NULL),
(31,'Crate','...','Resource','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(32,'Surveyor\'s rope','Ordinary surveyor\'s rope.','Quest','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(33,'Anole egg','It\'s a strange-looking and disgusting egg.','Resource','Default',10,1,NULL,NULL,NULL,NULL,NULL),
(34,'Show critical damage','Settings game.','Settings','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(35,'Althyk\'s Ring','Althyk\'s Ring is an item level 1.','Equipment','Default',9999999,30,5,7,20,10,NULL),
(36,'Empyrean Ring','Empyrean Ring is an item level 1.','Equipment','Default',99999999,30,5,7,20,10,NULL),
(37,'Ring of Fidelity','Ring of Fidelity is an item level 1.','Equipment','Default',999999,30,4,1,15,1,NULL),
(38,'Eversharp Ring','Eversharp Ring is an item level 1.','Equipment','Default',9999999,32,5,NULL,20,NULL,NULL),
(39,'Trash armor','Pieces of armor from the junk','Equipment','Equip armor',720,100,5,NULL,10,NULL,NULL),
(40,'Green grass','It looks like ordinary green grass.','Resource','Resource harvestable',0,0,NULL,NULL,NULL,NULL,NULL),
(41,'Treated green grass','Compressed processed grass','Resource','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(42,'Titanium rake','The usual titanium rake.','Equipment','Equip rake',99999,50,12,NULL,3,NULL,NULL),
(43,'Titanium Pickaxe','The usual titanium pickaxe.','Equipment','Equip pickaxe',9999999,50,11,NULL,3,NULL,NULL),
(44,'Leather bag','Just leather bag.','Quest','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(45,'Cargo','Very valuable cargo.','Quest','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(46,'Leia\'s egg','The egg that Leia needs','Quest','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(47,'Letter ','Stamped letter','Quest','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(48,'Artifact','A magical thing','Quest','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(49,'Monoa Mask','It\'s a beautiful mask','Equipment','Equip armor',99999,0,5,7,7,13,NULL),
(50,'Coal','Black fuel','Resource','Resource mineable',2,1,NULL,NULL,NULL,NULL,NULL),
(51,'Vampire ring','The ring that draws out life','Equipment','Default',900000,1,8,NULL,50,NULL,NULL),
(52,'Vampire armor','Armor that sucks the life','Equipment','Equip armor',999999,1,5,8,50,50,NULL),
(53,'Vampire hammer','The hammer that pulls life','Equipment','Equip hammer',9999999,1,8,2,50,10,NULL),
(54,'Iron ore','Ordinary iron ore','Resource','Resource mineable',6,1,NULL,NULL,NULL,NULL,NULL),
(55,'Titanium ore','Solid titanium ore','Resource','Resource mineable',60,1,NULL,NULL,NULL,NULL,NULL),
(56,'Adamantite ore','The strongest adamantite ore','Resource','Resource mineable',120,1,NULL,NULL,NULL,NULL,NULL),
(57,'EIDOL #1','','Equipment','Equip eidolon',0,0,19,NULL,25,NULL,NULL),
(58,'Corn','Farm-grown Corn','Resource','Resource harvestable',6,0,NULL,NULL,NULL,NULL,NULL),
(59,'EIDOL #2','','Equipment','Equip eidolon',0,0,19,NULL,25,NULL,NULL),
(60,'Blue Light Armor','Base armor','Equipment','Equip armor',9999999,0,5,NULL,30,NULL,NULL),
(61,'Tomato','Farm-grown Tomato','Resource','Resource harvestable',8,0,NULL,NULL,NULL,NULL,NULL),
(62,'Adamantium Rake','The usual adamantium rake.','Equipment','Equip rake',50000,50,12,NULL,5,NULL,NULL),
(63,'Adamantium pickaxe','The usual adamantium pickaxe.','Equipment','Equip pickaxe',50000,60,11,NULL,5,NULL,NULL),
(64,'Poison Hook','Inflicts gradual damage.','Equipment','Default',999999,4000,NULL,NULL,NULL,NULL,NULL),
(65,'Explosive impulse hook','Inflicts gradual explode damage.','Equipment','Default',999999,8000,NULL,NULL,NULL,NULL,NULL),
(66,'Magic Spider Hook','It\'s sticky to the air.','Equipment','Default',999999,4000,NULL,NULL,NULL,NULL,NULL),
(67,'Strawberries','Farm-grown Strawberries','Resource','Resource harvestable',10,0,NULL,NULL,NULL,NULL,NULL),
(68,'Cabbage','Farm-grown Cabbage','Resource','Resource harvestable',12,0,NULL,NULL,NULL,NULL,NULL),
(69,'Bestial Light Armor','Armor for Tank.','Equipment','Equip armor',5,100,5,6,30,30,NULL),
(70,'Wheat','Farm-grown Wheat','Resource','Resource harvestable',4,0,NULL,NULL,NULL,NULL,NULL),
(71,'Pumpkin','Farm-grown Pumpkin','Resource','Resource harvestable',14,0,NULL,NULL,NULL,NULL,NULL),
(72,'Iron ingot','Iron ingot','Resource','Default',400,12,NULL,NULL,NULL,NULL,NULL),
(73,'Titanium ingot','Titanium ingot','Resource','Default',999999,0,NULL,NULL,NULL,NULL,NULL),
(74,'Adamantite ingot','Adamantite ingot','Resource','Default',99999999,0,NULL,NULL,NULL,NULL,NULL),
(75,'Eternal Sun Belt','Eternal Sun Belt is an item level 1.','Equipment','Default',9999999,0,6,2,20,10,NULL),
(76,'Shadower Mantle','Mantle for Healer.','Equipment','Equip armor',999999,0,7,8,30,50,NULL),
(77,'Mercenary Armor','Lightweight armor for DPS.','Equipment','Equip armor',99999,0,1,2,2,30,NULL),
(78,'Iron hammer','The universal iron hammer','Equipment','Equip hammer',14820,328,13,4,6,5,NULL),
(79,'Titanium hammer','The universal titanium hammer','Equipment','Equip hammer',99999,0,13,12,20,5,NULL),
(80,'EIDOL #3','','Equipment','Equip eidolon',0,0,19,NULL,25,NULL,NULL),
(81,'Adamantium hammer','The adamantium universal hammer','Equipment','Equip hammer',999999,0,13,11,30,15,NULL),
(82,'Iron armor','Universal iron armor','Equipment','Equip armor',37260,720,5,NULL,20,NULL,NULL),
(83,'Titanium armor','Universal titanium armor','Equipment','Equip armor',999999,0,5,NULL,50,NULL,NULL),
(84,'Adamantium armor','Universal adamantium armor','Equipment','Equip armor',9999999,0,5,NULL,100,NULL,NULL),
(85,'Thorny Ring','Thorny Ring is item level 1.','Equipment','Default',99999,0,9,5,25,10,NULL),
(86,'Thorny Necklace','Thorny Necklace is an item level 1.','Equipment','Default',999999,0,5,7,10,10,NULL),
(87,'Bone Armillae','Bone Armillae is an item level 1.','Equipment','Default',9999999,0,1,NULL,1,NULL,NULL),
(88,'EIDOL #4','','Equipment','Equip eidolon',0,0,19,NULL,25,NULL,NULL),
(89,'Eidolon Crystal','Required to improve eidolons','Currency','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(90,'Ammo Box','Box of bullets','Equipment','Default',70000,5000,10,NULL,50,NULL,NULL),
(91,'Ammo Generator','Generates ammunition','Equipment','Default',9999999,0,9,NULL,50,NULL,NULL),
(92,'Web','A spider\'s web','Resource','Default',1,0,NULL,NULL,NULL,NULL,NULL),
(93,'Show quest star navigation','Settings game.','Settings','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(94,'Pickup Laser','Decoration for house!','Decoration','Default',10000,10,NULL,NULL,NULL,NULL,NULL),
(95,'Ticket guild','Command: /gcreate <name>','Other','Default',0,800,NULL,NULL,NULL,NULL,NULL),
(96,'Customizer','Customizer for personal skins','Equipment','Default',0,500,NULL,NULL,NULL,NULL,NULL),
(97,'Damage Equalizer','Disabled self dmg.','Equipment','Default',80000,7000,NULL,NULL,NULL,NULL,NULL),
(98,'Show detail gain messages','Settings game.','Settings','Default',0,0,NULL,NULL,NULL,NULL,NULL),
(99,'Hammer Lamp','Hammer Lamp','Equipment','Equip hammer',9999999,0,13,3,3,5,NULL),
(100,'Pizdamet','Pizdamet','Equipment','Equip grenade',9999999,0,16,NULL,3,NULL,NULL),
(101,'Wall Pusher','Plazma wall','Equipment','Equip rifle',999999,0,17,NULL,3,NULL,NULL),
(102,'Hammer Blast','Hammer Blast','Equipment','Equip hammer',999999,0,13,3,3,5,NULL),
(103,'Magnetic Pulse','Conventional weapon','Equipment','Equip rifle',999999,0,17,NULL,2,NULL,NULL),
(104,'Thread','A strand of spider web','Resource','Default',38,1,NULL,NULL,NULL,NULL,NULL),
(105,'Weak poison','A little weak poison','Resource','Default',60,4,NULL,NULL,NULL,NULL,NULL),
(106,'Poison','Concentrated poison','Resource','Default',720,32,NULL,NULL,NULL,NULL,NULL),
(107,'Untreated Leather','Untreated Leather','Resource','Default',20,1,NULL,NULL,NULL,NULL,NULL),
(108,'Leather','Treated leather','Resource','Default',300,8,NULL,NULL,NULL,NULL,NULL),
(109,'Teeth','The teeth','Resource','Default',15,1,NULL,NULL,NULL,NULL,NULL),
(110,'Claws ','The claws of the beast','Resource','Default',10,1,NULL,NULL,NULL,NULL,NULL),
(111,'Fragments of Darkness','Very dark fragments','Resource','Default',9999999,1,NULL,NULL,NULL,NULL,NULL),
(112,'Mushroom extract','Mushroom substance','Resource','Default',8,1,NULL,NULL,NULL,NULL,NULL),
(113,'Bone','Someone\'s bone','Resource','Default',10,1,NULL,NULL,NULL,NULL,NULL),
(114,'Rotten meat','Meat that\'s gone bad','Resource','Default',999999,100,NULL,NULL,NULL,NULL,NULL),
(115,'Magic Matter','The matter that radiates magic','Resource','Default',999999,100,NULL,NULL,NULL,NULL,NULL),
(116,'Feather','The feather of the beast','Resource','Default',5,1,NULL,NULL,NULL,NULL,NULL),
(117,'Hook parts','Hook parts','Resource','Default',1500,150,NULL,NULL,NULL,NULL,NULL),
(118,'A ring of greed','This ring is so gold','Equipment','Default',9999999,100,20,NULL,NULL,NULL,NULL),
(119,'Miner\'s clothes','Miner\'s clothes','Equipment','Equip armor',9999999,100,12,NULL,NULL,NULL,NULL),
(120,'Banner','One of the items to create a guild','Resource','Default',100,2,NULL,NULL,NULL,NULL,NULL),
(121,'Mana Flower','Magic pours out of this flower','Resource','Default',9999999,100,NULL,NULL,NULL,NULL,NULL),
(122,'Life Flower','Life pours out of this flower','Resource','Default',999999,100,NULL,NULL,NULL,NULL,NULL),
(123,'Explosives','A lot of explosive substances','Resource','Default',35,2,NULL,NULL,NULL,NULL,NULL),
(124,'Adhesive Bandage','Adhesive Bandage','Potion','Equip potion HP',999999,100,NULL,NULL,NULL,NULL,NULL),
(125,'Skull','Someone\'s skull','Resource','Default',20,2,NULL,NULL,NULL,NULL,NULL),
(126,'Rifle Scope','Rifle Scope','Equipment','Default',50000,6000,17,NULL,50,NULL,NULL),
(127,'Explosive Powder','Very explosive','Resource','Default',2070,8,NULL,NULL,NULL,NULL,NULL),
(128,'Spider Egg','Spider Egg','Resource','Default',20,1,NULL,NULL,NULL,NULL,NULL),
(129,'Wooden Crate','Wooden Crate','Resource','Single use x1',99999,100,NULL,NULL,NULL,NULL,NULL),
(130,'Iron Crate','Iron Crate','Resource','Single use x1',9999,100,NULL,NULL,NULL,NULL,NULL),
(131,'Golden Crate','Golden Crate','Resource','Single use x1',9999999,100,NULL,NULL,NULL,NULL,NULL),
(132,'Mana Cloak','A cloak that emits mana','Equipment','Equip armor',999999,100,7,NULL,100,NULL,NULL),
(133,'Advanced Combat Techniques','Advanced Combat Techniques','Equipment','Single use x1',999999,100,NULL,NULL,100,NULL,NULL),
(134,'Life crystal','The life-giving crystal','Resource','Default',999999,100,NULL,NULL,NULL,NULL,NULL),
(135,'Hoarder\'s bag','Hoarder\'s bag','Equipment','Default',999999,100,6,18,50,50,NULL),
(136,'Aether crystal','Aether crystal','Resource','Default',99999,100,NULL,NULL,NULL,NULL,NULL),
(137,'Slime','A piece of slime','Resource','Default',25,5,NULL,NULL,NULL,NULL,NULL),
(138,'Bone meal','Bone meal','Resource','Default',1,1,NULL,NULL,NULL,NULL,NULL),
(139,'WarLord','You already have combat experience','Equipment','Equip title',0,0,1,NULL,10,NULL,NULL),
(140,'Legend of the DM','You\'ve been through the whole ordeal of murder.','Equipment','Equip title',0,0,1,2,50,50,NULL),
(141,'The Legendary Traveler','You already know and feel this place','Equipment','Equip title',0,0,5,NULL,80,NULL,NULL),
(142,'Dungeon King','You\'re a pain to the dark world','Equipment','Equip title',0,0,5,7,200,200,NULL),
(143,'Tank Lord','The armor is a part of you.','Equipment','Equip title',0,0,5,8,500,200,NULL),
(144,'DMG Lord','One touch, destruction','Equipment','Equip title',0,0,1,2,500,200,NULL),
(145,'The Lord Healer','The earth before you blooms','Equipment','Equip title',0,0,5,7,500,700,NULL),
(146,'Lord of the Ore','You make everything out of nothing','Equipment','Equip title',0,0,18,12,500,500,NULL),
(147,'Harvest King','You know nature better than anyone','Equipment','Equip title',0,0,11,6,500,500,NULL),
(148,'The Overlord of Damage','Nothing but nothing','Equipment','Equip title',0,0,1,3,1000,500,NULL),
(149,'King of Gold','You\'re respected. And they give you a discount','Equipment','Equip title',0,0,20,NULL,3000,NULL,NULL),
(150,'Tracked Plazma','Tracked Plazma','Equipment','Equip rifle',0,0,17,NULL,2,NULL,NULL),
(151,'Gun Pulse','Gun Pulse','Equipment','Equip gun',0,0,14,NULL,1,NULL,NULL),
(152,'Module parts','Assembly components','Resource','Default',5000,600,NULL,NULL,NULL,NULL,NULL),
(153,'Weapons parts','Weapons parts','Resource','Default',500,60,NULL,NULL,NULL,NULL,NULL),
(154,'Beer','Which gives you a little strength, but you get drunk.','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(155,'A small potion of health','Gives you some HP.','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(156,'Health potion','Gives you some HP.','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(157,'A small potion of mana','Gives you some MP.','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(158,'Mana potion','Gives you some MP.','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(159,'Vampirism potion','You\'re sucking the life','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(160,'Luck potion','You\'re very lucky','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(161,'Damage potion','You do more damage','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(162,'Speed potion','You\'re moving faster','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(163,'Gravity potion','You float like a leaf','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(164,'Potion resistance','You\'re part of the damage','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(165,'Hedgehog potion','You get damage, but you also give back in return','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(166,'Invisibility potion','You\'re invisible','Potion','Equip potion HP',999999,999999,NULL,NULL,NULL,NULL,'{\r\n  \"potion\": {\r\n    \"effect\": \"Tiny heal\",\r\n    \"value\": 5,\r\n    \"lifetime\": 10\r\n  }\r\n}'),
(167,'Ancient remains','The ancient remains of someone','Other','Default',1000,1,NULL,NULL,NULL,NULL,NULL),
(168,'Ancient coin','The ancient coin','Other','Default',5000,1,NULL,NULL,NULL,NULL,NULL),
(169,'Ancient adornment','The ancient adornment','Other','Default',7000,1,NULL,NULL,NULL,NULL,NULL),
(170,'Ancient relic','The ancient relic','Other','Default',10000,1,NULL,NULL,NULL,NULL,NULL),
(171,'Ancient component','The ancient component','Other','Default',15000,1,NULL,NULL,NULL,NULL,NULL),
(172,'Ancient artifact','The ancient artifact','Other','Default',50000,1,NULL,NULL,NULL,NULL,NULL),
(173,'Stone','Just stone','Resource','Resource mineable',5,1,NULL,NULL,NULL,NULL,NULL);
/*!40000 ALTER TABLE `tw_items_list` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_logics_worlds`
--

DROP TABLE IF EXISTS `tw_logics_worlds`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_logics_worlds` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `MobID` int(11) NOT NULL,
  `Mode` int(11) NOT NULL DEFAULT 0 COMMENT '(1,3) 0 up 1 left',
  `ParseInt` int(11) NOT NULL COMMENT '(2) health (3)itemid key',
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `WorldID` int(11) NOT NULL,
  `Comment` varchar(64) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `MobID` (`MobID`),
  KEY `WorldID` (`WorldID`),
  KEY `ParseInt` (`ParseInt`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_logics_worlds`
--

LOCK TABLES `tw_logics_worlds` WRITE;
/*!40000 ALTER TABLE `tw_logics_worlds` DISABLE KEYS */;
/*!40000 ALTER TABLE `tw_logics_worlds` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_positions_farming`
--

DROP TABLE IF EXISTS `tw_positions_farming`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_positions_farming` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `ItemID` int(11) NOT NULL,
  `PositionX` int(11) NOT NULL,
  `PositionY` int(11) NOT NULL,
  `Distance` int(11) NOT NULL DEFAULT 300 COMMENT 'Range of unit spreading',
  `WorldID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `ItemID` (`ItemID`),
  KEY `WorldID` (`WorldID`)
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_positions_farming`
--

LOCK TABLES `tw_positions_farming` WRITE;
/*!40000 ALTER TABLE `tw_positions_farming` DISABLE KEYS */;
INSERT INTO `tw_positions_farming` VALUES
(1,40,3507,2257,300,0),
(2,70,15575,2321,170,1),
(3,61,15165,2289,170,1),
(4,67,14742,2257,170,1),
(5,61,14330,2289,170,1),
(6,70,13969,2321,100,1),
(7,67,9700,1681,150,1),
(8,68,9276,1649,150,1),
(9,71,8853,1617,150,1);
/*!40000 ALTER TABLE `tw_positions_farming` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_positions_mining`
--

DROP TABLE IF EXISTS `tw_positions_mining`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_positions_mining` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `ItemID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Health` int(11) NOT NULL DEFAULT 100,
  `PositionX` int(11) NOT NULL,
  `PositionY` int(11) NOT NULL,
  `Distance` int(11) NOT NULL DEFAULT 300 COMMENT 'Range of unit spreading',
  `WorldID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `ItemID` (`ItemID`),
  KEY `WorldID` (`WorldID`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_positions_mining`
--

LOCK TABLES `tw_positions_mining` WRITE;
/*!40000 ALTER TABLE `tw_positions_mining` DISABLE KEYS */;
INSERT INTO `tw_positions_mining` VALUES
(1,50,1,100,23268,4465,250,1),
(2,50,1,100,22561,4721,300,1),
(3,50,1,100,23543,5105,300,1),
(4,54,1,100,22144,5297,600,1),
(5,55,1,100,23261,5713,600,1),
(6,56,1,100,20590,5745,600,1);
/*!40000 ALTER TABLE `tw_positions_mining` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_quest_boards`
--

DROP TABLE IF EXISTS `tw_quest_boards`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_quest_boards` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(32) NOT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `WorldID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  KEY `WorldID` (`WorldID`)
) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_quest_boards`
--

LOCK TABLES `tw_quest_boards` WRITE;
/*!40000 ALTER TABLE `tw_quest_boards` DISABLE KEYS */;
INSERT INTO `tw_quest_boards` VALUES
(1,'Guild Tasks',19665,3185,1),
(2,'Miner Tasks',22432,5856,1),
(3,'Farmer Tasks',15200,1056,1);
/*!40000 ALTER TABLE `tw_quest_boards` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_quests_board_list`
--

DROP TABLE IF EXISTS `tw_quests_board_list`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_quests_board_list` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `QuestID` int(11) NOT NULL,
  `DailyBoardID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  KEY `tw_quests_board_list_ibfk_1` (`DailyBoardID`),
  KEY `tw_quests_board_list_ibfk_2` (`QuestID`)
) ENGINE=InnoDB AUTO_INCREMENT=27 DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_quests_board_list`
--

LOCK TABLES `tw_quests_board_list` WRITE;
/*!40000 ALTER TABLE `tw_quests_board_list` DISABLE KEYS */;
INSERT INTO `tw_quests_board_list` VALUES
(1,25,1),
(2,26,1),
(3,27,1),
(4,28,1),
(5,29,1),
(6,30,1),
(7,31,1),
(8,32,1),
(9,33,1),
(10,34,1),
(11,35,1),
(12,36,1),
(13,37,1),
(14,38,1),
(15,39,1),
(16,40,1),
(17,42,3),
(18,43,3),
(19,44,3),
(20,45,3),
(21,46,3),
(22,47,3),
(23,48,3),
(24,49,3),
(25,50,3),
(26,51,3);
/*!40000 ALTER TABLE `tw_quests_board_list` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_quests_list`
--

DROP TABLE IF EXISTS `tw_quests_list`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_quests_list` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `NextQuestID` int(11) DEFAULT NULL,
  `Name` varchar(24) NOT NULL DEFAULT 'Quest name',
  `Money` int(11) NOT NULL,
  `Exp` int(11) NOT NULL,
  `Flags` set('Type main','Type side','Type daily','Type weekly','Type repeatable','Can''t refuse') NOT NULL DEFAULT 'Type main',
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `NextQuestID` (`NextQuestID`)
) ENGINE=InnoDB AUTO_INCREMENT=52 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_quests_list`
--

LOCK TABLES `tw_quests_list` WRITE;
/*!40000 ALTER TABLE `tw_quests_list` DISABLE KEYS */;
INSERT INTO `tw_quests_list` VALUES
(1,41,'First quest',0,0,'Type side,Can\'t refuse'),
(2,3,'Coming to Gridania',5,10,'Type main,Can\'t refuse'),
(3,4,'Close to Home',10,50,'Type main,Can\'t refuse'),
(4,5,'To the Bannock',10,10,'Type main,Can\'t refuse'),
(5,6,'Passing Muster',0,10,'Type main,Can\'t refuse'),
(6,7,'Chasing Shadows',50,100,'Type main,Can\'t refuse'),
(7,8,'Eggs over Queasy',100,80,'Type main,Can\'t refuse'),
(8,9,'Surveying the Damage',200,300,'Type main,Can\'t refuse'),
(9,10,'A Soldier\'s Breakfast',500,300,'Type main,Can\'t refuse'),
(10,11,'Spirithold Broken',10,20,'Type main,Can\'t refuse'),
(11,12,'On to Bentbranch',50,150,'Type main,Can\'t refuse'),
(12,13,'You Shall Not Trespass',60,170,'Type main,Can\'t refuse'),
(13,14,'Don\'t Look Down',0,0,'Type main,Can\'t refuse'),
(14,15,'Darkness of the Forest',250,250,'Type main,Can\'t refuse'),
(15,16,'Threat Level Elevated',25,50,'Type main,Can\'t refuse'),
(16,17,'Migrant Marauders',500,700,'Type main,Can\'t refuse'),
(17,18,'A Hearer Is Often Late',25,50,'Type main,Can\'t refuse'),
(18,19,'Salvaging the Scene',800,300,'Type main,Can\'t refuse'),
(19,20,'Leia\'s Legacy',400,800,'Type main,Can\'t refuse'),
(20,21,'Dread Is in the Air',50,30,'Type main,Can\'t refuse'),
(21,22,'To Guard a Guardian',100,50,'Type main,Can\'t refuse'),
(22,23,'Festive Endeavors',50,50,'Type main,Can\'t refuse'),
(23,24,'Renewing the Covenant',20,50,'Type main,Can\'t refuse'),
(24,NULL,'The Gridanian Envoy',0,0,'Type main,Can\'t refuse'),
(25,NULL,'Archer | Need eggs!',50,50,'Type weekly,Type repeatable'),
(26,NULL,'Archer | The feathers?',50,50,'Type weekly,Type repeatable'),
(27,NULL,'Archer | Beast mortality',50,50,'Type weekly,Type repeatable'),
(28,NULL,'Archer | Protect us',50,50,'Type weekly,Type repeatable'),
(29,NULL,'Archer | Iron tip',50,50,'Type weekly,Type repeatable'),
(30,NULL,'Lancer | Wild hunt',50,50,'Type weekly,Type repeatable'),
(31,NULL,'Lancer | Ghost vs',50,50,'Type weekly,Type repeatable'),
(32,NULL,'Lancer | Dynamite hunt',50,50,'Type weekly,Type repeatable'),
(33,NULL,'Lancer | Hate mushrooms',50,50,'Type weekly,Type repeatable'),
(34,NULL,'Lancer | Armor boy',50,50,'Type weekly,Type repeatable'),
(35,NULL,'Wizard | Dark shards',50,50,'Type weekly,Type repeatable'),
(36,NULL,'Wizard | Teleport power',50,50,'Type side'),
(37,NULL,'Wizard | Illusion tee?',50,50,'Type weekly,Type repeatable'),
(38,NULL,'Wizard | Puppeteer?',50,50,'Type weekly,Type repeatable'),
(39,NULL,'Wizard | Zombie killer',50,50,'Type weekly,Type repeatable'),
(40,NULL,'S | Population Control',50,50,'Type daily,Type repeatable'),
(41,2,'Intro to MMORPG',10,10,'Type side'),
(42,NULL,'Farmer | Need more wheat',200,20,'Type weekly,Type repeatable'),
(43,NULL,'Farmer | And the stone?',500,150,'Type weekly,Type repeatable'),
(44,NULL,'Farmer | Bring coal',300,50,'Type weekly,Type repeatable'),
(45,NULL,'Farmer | Need for iron',1500,300,'Type weekly,Type repeatable'),
(46,NULL,'Farmer | I want berries',600,200,'Type weekly,Type repeatable'),
(47,NULL,'Farmer | Put it in order',10,30,'Type daily,Type repeatable'),
(48,NULL,'Farmer | Reconnaissance',10,10,'Type side'),
(49,NULL,'Farmer | Plant a crop',50,10,'Type weekly,Type repeatable'),
(50,NULL,'Farmer | Harvest',10,10,'Type side'),
(51,NULL,'Farmer | Pests',50,100,'Type daily,Type repeatable');
/*!40000 ALTER TABLE `tw_quests_list` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_skills_list`
--

DROP TABLE IF EXISTS `tw_skills_list`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_skills_list` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(64) NOT NULL,
  `Description` varchar(64) NOT NULL,
  `Type` int(11) NOT NULL DEFAULT 0 COMMENT '0-Improvements\r\n1-Healer\r\n2-Dps\r\n3-Tank',
  `BoostName` varchar(64) NOT NULL DEFAULT '''name''',
  `BoostValue` int(11) NOT NULL DEFAULT 1,
  `PercentageCost` int(11) NOT NULL DEFAULT 10,
  `PriceSP` int(11) NOT NULL,
  `MaxLevel` int(11) NOT NULL,
  `Passive` tinyint(1) NOT NULL DEFAULT 0,
  `ProfessionID` int(11) NOT NULL DEFAULT -1,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `ProfessionID` (`ProfessionID`)
) ENGINE=InnoDB AUTO_INCREMENT=13 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_skills_list`
--

LOCK TABLES `tw_skills_list` WRITE;
/*!40000 ALTER TABLE `tw_skills_list` DISABLE KEYS */;
INSERT INTO `tw_skills_list` VALUES
(1,'Health turret','Creates turret a recovery health ',1,'+ life span',1,25,28,6,0,2),
(2,'Sleepy Gravity','Magnet mobs to itself',3,'radius',20,25,28,10,0,0),
(3,'Craft Discount','Will give discount on the price of craft items',0,'% discount gold for craft item',1,0,28,50,1,-1),
(4,'Proficiency with weapons','You can perform an automatic fire',0,'can perform an auto fire with all types of weapons',1,0,120,1,1,-1),
(5,'Blessing of God of war','The blessing restores ammo',3,'% recovers ammo within a radius of 800',25,50,28,4,0,1),
(6,'Attack Teleport','An attacking teleport that deals damage to all mobs radius',2,'% your strength',25,10,100,4,0,1),
(7,'Cure','Restores HP all nearby target\'s.',1,'% adds a health bonus',3,5,10,5,0,-1),
(8,'Provoke','Aggresses mobs in case of weak aggression',3,'power of aggression',150,30,40,2,0,0),
(9,'Last Stand','Enters mana damage dampening mode',3,'reduces mana consumption',5,25,40,2,0,0),
(10,'Magic Bow','Entering magic bow mode',1,'+ additional shot',1,30,40,5,0,2),
(11,'Healing aura','Creates an aura that restores health.',1,'+ 15 aura radius',15,30,80,5,0,2),
(12,'Flame Wall','Slows down and deals damage.',2,'+ 20 aura radius',20,30,80,5,0,1);
/*!40000 ALTER TABLE `tw_skills_list` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_voucher`
--

DROP TABLE IF EXISTS `tw_voucher`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_voucher` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Code` varchar(32) NOT NULL,
  `Data` text NOT NULL,
  `Multiple` tinyint(1) NOT NULL DEFAULT 0,
  `ValidUntil` bigint(20) NOT NULL DEFAULT 0,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_voucher`
--

LOCK TABLES `tw_voucher` WRITE;
/*!40000 ALTER TABLE `tw_voucher` DISABLE KEYS */;
/*!40000 ALTER TABLE `tw_voucher` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_voucher_redeemed`
--

DROP TABLE IF EXISTS `tw_voucher_redeemed`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_voucher_redeemed` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `VoucherID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `TimeCreated` int(11) NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_voucher_redeemed`
--

LOCK TABLES `tw_voucher_redeemed` WRITE;
/*!40000 ALTER TABLE `tw_voucher_redeemed` DISABLE KEYS */;
/*!40000 ALTER TABLE `tw_voucher_redeemed` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_warehouses`
--

DROP TABLE IF EXISTS `tw_warehouses`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_warehouses` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(32) NOT NULL DEFAULT '''Bussines name''',
  `Type` set('buying','selling','storage') NOT NULL DEFAULT 'buying',
  `Data` longtext DEFAULT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `Currency` int(11) NOT NULL DEFAULT 1,
  `WorldID` int(11) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `WorldID` (`WorldID`),
  KEY `Currency` (`Currency`),
  CONSTRAINT `tw_warehouses_ibfk_1` FOREIGN KEY (`Currency`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=12 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_warehouses`
--

LOCK TABLES `tw_warehouses` WRITE;
/*!40000 ALTER TABLE `tw_warehouses` DISABLE KEYS */;
INSERT INTO `tw_warehouses` VALUES
(1,'Weapons shop','buying,storage','{\"items\":[{\"enchant\":0,\"id\":3,\"price\":1400,\"value\":1},{\"enchant\":0,\"id\":4,\"price\":2800,\"value\":1},{\"enchant\":0,\"id\":5,\"price\":3500,\"value\":1},{\"enchant\":0,\"id\":6,\"price\":4200,\"value\":1},{\"enchant\":0,\"id\":153,\"price\":500,\"value\":1},{\"enchant\":0,\"id\":117,\"price\":1500,\"value\":1},{\"enchant\":0,\"id\":152,\"price\":5000,\"value\":1},{\"enchant\":0,\"id\":126,\"price\":50000,\"value\":1},\r\n{\"enchant\":0,\"id\":97,\"price\":80000,\"value\":1},\r\n{\"enchant\":0,\"id\":90,\"price\":70000,\"value\":1}],\"storage\":{\"position\":{\"x\":21067,\"y\":3606},\"value\":\"516\"}}',21003,3883,1,1),
(2,'Tavern','buying,storage','{\r\n    \"items_collections\": [\r\n        {\r\n            \"group\": \"Potion\",\r\n            \"type\": \"Equip potion HP\"\r\n        },\r\n        {\r\n            \"group\": \"Potion\",\r\n            \"type\": \"Equip potion MP\"\r\n        }\r\n    ],\r\n    \"storage\": {\r\n        \"value\": \"1\",\r\n        \"position\": {\r\n            \"x\": 23053,\r\n            \"y\": 2970\r\n        }\r\n    }\r\n}',22528,2880,1,1),
(3,'Mining Store','selling,storage','{\"items_collections\":[{\"group\":\"Resource\",\"type\":\"Resource mineable\"}],\"storage\":{\"position\":{\"x\":21895,\"y\":5616},\"value\":\"358\"}}',21728,5984,1,1),
(4,'Farm Store','selling,storage','{\"items_collections\":[{\"group\":\"Resource\",\"type\":\"Resource harvestable\"}],\"storage\":{\"position\":{\"x\":15696,\"y\":1137},\"value\":\"3129\"}}',15040,1280,1,1),
(9,'Buyer','selling','{\r\n    \"items_collections\": [\r\n        {\r\n            \"group\": \"Resource\",\r\n            \"type\": \"Default\"\r\n        },\r\n        {\r\n            \"group\": \"Equipment\",\r\n            \"type\": \"Equip armor\"\r\n        },\r\n        {\r\n            \"group\": \"Equipment\",\r\n            \"type\": \"Equip gun\"\r\n        },\r\n        {\r\n            \"group\": \"Equipment\",\r\n            \"type\": \"Equip shotgun\"\r\n        },\r\n        {\r\n            \"group\": \"Equipment\",\r\n            \"type\": \"Equip rifle\"\r\n        },\r\n        {\r\n            \"group\": \"Equipment\",\r\n            \"type\": \"Equip rake\"\r\n        },\r\n        {\r\n            \"group\": \"Equipment\",\r\n            \"type\": \"Equip pickaxe\"\r\n        }\r\n    ]\r\n}',19831,3921,1,1),
(10,'Activity Shop','buying,storage','{\r\n  \"items\": [\r\n    {\"id\": 95, \"price\": 2500, \"value\": 1},\r\n    {\"id\": 96, \"price\": 1000, \"value\": 1},\r\n    {\"id\": 16, \"price\": 20, \"value\": 1},\r\n    {\"id\": 129, \"price\": 100, \"value\": 1},\r\n    {\"id\": 130, \"price\": 1000, \"value\": 1},\r\n    {\"id\": 131, \"price\": 10000, \"value\": 1},\r\n    {\"id\": 117, \"price\": 300, \"value\": 1}\r\n  ],\r\n  \"storage\": {\r\n    \"position\": {\"x\": 16928, \"y\": 3408},\r\n    \"value\": \"145\"\r\n  }\r\n}',16960,3697,29,1),
(11,'Achievement Shop','buying,storage','{\r\n  \"items\": [\r\n    {\r\n      \"id\": 1,\r\n      \"price\": 1,\r\n      \"value\": 10\r\n    },\r\n    {\r\n      \"enchant\": 0,\r\n      \"id\": 7,\r\n      \"price\": 1,\r\n      \"value\": 2\r\n    },\r\n    {\r\n      \"enchant\": 0,\r\n      \"id\": 16,\r\n      \"price\": 20,\r\n      \"value\": 1\r\n    },\r\n    {\r\n      \"enchant\": 0,\r\n      \"id\": 129,\r\n      \"price\": 100,\r\n      \"value\": 1\r\n    },\r\n    {\r\n      \"enchant\": 0,\r\n      \"id\": 130,\r\n      \"price\": 1000,\r\n      \"value\": 1\r\n    },\r\n    {\r\n      \"enchant\": 0,\r\n      \"id\": 131,\r\n      \"price\": 10000,\r\n      \"value\": 1\r\n    }\r\n  ],\r\n    \"storage\": {\r\n        \"value\": \"1\",\r\n        \"position\": {\r\n            \"x\": 26912,\r\n            \"y\": 1776\r\n        }\r\n    }\r\n}',26560,1600,10,1);
/*!40000 ALTER TABLE `tw_warehouses` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_world_swap`
--

DROP TABLE IF EXISTS `tw_world_swap`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_world_swap` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `WorldID` int(11) DEFAULT NULL,
  `PositionX` int(11) DEFAULT NULL,
  `PositionY` int(11) DEFAULT NULL,
  `TwoWorldID` int(11) DEFAULT NULL,
  `TwoPositionX` int(11) DEFAULT NULL,
  `TwoPositionY` int(11) DEFAULT NULL,
  PRIMARY KEY (`ID`),
  KEY `WorldID` (`WorldID`),
  KEY `TwoWorldID` (`TwoWorldID`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_world_swap`
--

LOCK TABLES `tw_world_swap` WRITE;
/*!40000 ALTER TABLE `tw_world_swap` DISABLE KEYS */;
INSERT INTO `tw_world_swap` VALUES
(1,0,3057,1157,1,19699,4309);
/*!40000 ALTER TABLE `tw_world_swap` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `tw_worlds`
--

DROP TABLE IF EXISTS `tw_worlds`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_worlds` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(256) NOT NULL,
  `Path` varchar(256) NOT NULL,
  `Type` enum('default','dungeon','tutorial','') NOT NULL,
  `RespawnWorldID` int(11) NOT NULL,
  `JailWorldID` int(11) NOT NULL,
  `RequiredLevel` int(11) NOT NULL DEFAULT 1,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_worlds`
--

LOCK TABLES `tw_worlds` WRITE;
/*!40000 ALTER TABLE `tw_worlds` DISABLE KEYS */;
INSERT INTO `tw_worlds` VALUES
(0,'Tutorilishe','tutorial.map','tutorial',0,0,1),
(1,'main','main.map','default',1,1,1);
/*!40000 ALTER TABLE `tw_worlds` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*M!100616 SET NOTE_VERBOSITY=@OLD_NOTE_VERBOSITY */;

-- Dump completed on 2025-02-11 12:55:31
