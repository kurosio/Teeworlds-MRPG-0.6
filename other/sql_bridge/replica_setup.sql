-- Run on the CENTRAL PRIMARY database once, to create the accounts the
-- bridges/replicas use, and to enable binary logging if not already on.
--
-- Replication is shared-only: mrpg_shared reaches the replicas, the local
-- databases do not (replicate_wild_ignore_table = mrpg_local%.% on every
-- replica, see README.md). No local grant is needed here.

CREATE USER IF NOT EXISTS 'mrpg_replica'@'%' IDENTIFIED BY 'changeme';
GRANT REPLICATION SLAVE, REPLICATION CLIENT ON *.* TO 'mrpg_replica'@'%';

-- Read-only account of the bridge's replica connections: mrpg_shared only.
CREATE USER IF NOT EXISTS 'mrpg_bridge'@'%' IDENTIFIED BY 'changeme';
GRANT SELECT ON mrpg_shared.* TO 'mrpg_bridge'@'%';

FLUSH PRIVILEGES;

-- On each LOCATION replica, after importing a fresh dump made with
-- --master-data=2 --gtid, disable/reset any leftover local binary log
-- (RESET MASTER on this replica only), explicitly apply the dump's commented
-- SET GLOBAL gtid_slave_pos='…' value, then run (MASTER_SSL=1 encrypts the
-- replication stream over the WAN):
--   CHANGE MASTER TO
--     MASTER_HOST='db-central.example.com',
--     MASTER_USER='mrpg_replica',
--     MASTER_PASSWORD='changeme',
--     MASTER_USE_GTID=slave_pos,
--     MASTER_SSL=1;
--   START SLAVE;
-- Check replication lag with: SHOW SLAVE STATUS\G  (Seconds_Behind_Master)
-- The location's intentionally created mrpg_local_<location> database is
-- local state and is not modified by replication.
