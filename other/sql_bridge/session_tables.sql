-- Shared tables required by account takeover.
-- Run once on the central primary database:
--   mariadb -u root -p mrpg_shared < other/sql_bridge/session_tables.sql
--
-- The table belongs in mrpg_shared and is replicated to every location.
-- The relay carries the immediate takeover event; the table is the
-- authoritative owner used by asynchronous primary checks and fallback
-- recovery when the relay is unavailable.

CREATE TABLE IF NOT EXISTS `tw_account_sessions` (
  `AccountID` int(11) NOT NULL,
  `ServerID` int(11) NOT NULL,
  `ClientID` int(11) NOT NULL,
  `SessionToken` varchar(64) NOT NULL,
  `Heartbeat` bigint(20) NOT NULL DEFAULT 0,
  `UpdatedAt` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`AccountID`),
  KEY `tw_account_sessions_server` (`ServerID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- Upgrade a table created by an earlier version of this migration. MariaDB
-- supports IF NOT EXISTS for ADD COLUMN, so the file stays repeatable.
ALTER TABLE `tw_account_sessions`
  ADD COLUMN IF NOT EXISTS `Heartbeat` bigint(20) NOT NULL DEFAULT 0 AFTER `SessionToken`;