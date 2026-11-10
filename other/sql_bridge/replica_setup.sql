-- Run on the CENTRAL PRIMARY database once, to create the accounts the
-- bridges/replicas use, and to enable binary logging if not already on.
--
-- Replication is SHARED ONLY: mrpg_shared is the only database that reaches
-- the replicas. Every local database (mrpg_local_eu, mrpg_local_us, ...) is
-- excluded on each replica with
--   replicate_wild_ignore_table = mrpg_local%.%
-- (README.md, steps 2.1 and "Excluding a local database from replication
-- later"). The local databases therefore need no account on the replicas and
-- none is granted here.

CREATE USER IF NOT EXISTS 'mrpg_replica'@'%' IDENTIFIED BY 'changeme';
GRANT REPLICATION SLAVE, REPLICATION CLIENT ON *.* TO 'mrpg_replica'@'%';

-- Read-only account of the bridge's replica connections: mrpg_shared only.
CREATE USER IF NOT EXISTS 'mrpg_bridge'@'%' IDENTIFIED BY 'changeme';
GRANT SELECT ON mrpg_shared.* TO 'mrpg_bridge'@'%';

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
-- and confirm that only mrpg_shared exists there:
--   SHOW DATABASES LIKE 'mrpg%';
