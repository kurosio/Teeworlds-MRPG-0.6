-- phpMyAdmin SQL Dump
-- version 5.2.1deb3
-- https://www.phpmyadmin.net/
--
-- Host: localhost:3306
-- Generation Time: May 13, 2026 at 08:23 PM
-- Server version: 10.11.14-MariaDB-0ubuntu0.24.04.1-log
-- PHP Version: 8.3.6

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `mrpg_local`
--

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
  `Rights` bigint(20) NOT NULL DEFAULT 0,
  `Name` varchar(32) NOT NULL DEFAULT 'Rank name',
  `GuildID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `tw_guilds_wars`
--

CREATE TABLE `tw_guilds_wars` (
  `GuildID1` int(11) NOT NULL,
  `GuildID2` int(11) NOT NULL,
  `TimeUntilEnd` bigint(20) NOT NULL,
  `Score1` int(11) NOT NULL DEFAULT 0,
  `Score2` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

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
-- Table structure for table `tw_links`
--

CREATE TABLE `tw_links` (
  `ID` int(11) NOT NULL,
  `Type` varchar(32) NOT NULL,
  `OwnerID` bigint(20) NOT NULL,
  `TargetID` bigint(20) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Indexes for dumped tables
--

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
-- Indexes for table `tw_guilds_wars`
--
ALTER TABLE `tw_guilds_wars`
  ADD PRIMARY KEY (`GuildID1`,`GuildID2`),
  ADD KEY `GuildID2` (`GuildID2`);

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
-- Indexes for table `tw_links`
--
ALTER TABLE `tw_links`
  ADD PRIMARY KEY (`ID`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `tw_auction_slots`
--
ALTER TABLE `tw_auction_slots`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_groups`
--
ALTER TABLE `tw_groups`
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
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_guilds_history`
--
ALTER TABLE `tw_guilds_history`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

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
-- AUTO_INCREMENT for table `tw_houses_decorations`
--
ALTER TABLE `tw_houses_decorations`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tw_links`
--
ALTER TABLE `tw_links`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- Constraints for dumped tables
--

--
-- Constraints for table `tw_guilds_invites`
--
ALTER TABLE `tw_guilds_invites`
  ADD CONSTRAINT `tw_guilds_invites_ibfk_1` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `tw_guilds_wars`
--
ALTER TABLE `tw_guilds_wars`
  ADD CONSTRAINT `tw_guilds_wars_ibfk_1` FOREIGN KEY (`GuildID1`) REFERENCES `tw_guilds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_wars_ibfk_2` FOREIGN KEY (`GuildID2`) REFERENCES `tw_guilds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
