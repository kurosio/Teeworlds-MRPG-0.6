-- Run on the CENTRAL PRIMARY database once, to create the account the
-- bridges/replicas use, and to enable binary logging if not already on.

CREATE USER IF NOT EXISTS 'mrpg_replica'@'%' IDENTIFIED BY 'changeme';
GRANT REPLICATION SLAVE, REPLICATION CLIENT ON *.* TO 'mrpg_replica'@'%';

CREATE USER IF NOT EXISTS 'mrpg_bridge'@'%' IDENTIFIED BY 'changeme';
GRANT SELECT ON mrpg_shared.* TO 'mrpg_bridge'@'%';
-- one line per local database (every game server has its own, see README.md)
GRANT SELECT ON mrpg_local_eu.* TO 'mrpg_bridge'@'%';

FLUSH PRIVILEGES;

-- On each LOCATION replica, after importing a dump of the primary, run
-- (MASTER_SSL=1 encrypts the replication stream over the WAN):
--   CHANGE MASTER TO
--     MASTER_HOST='db-central.example.com',
--     MASTER_USER='mrpg_replica',
--     MASTER_PASSWORD='changeme',
--     MASTER_USE_GTID=CURRENT_POS,
--     MASTER_SSL=1;
--   START SLAVE;
-- Check replication lag with: SHOW SLAVE STATUS\G  (Seconds_Behind_Master)
