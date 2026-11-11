# MariaDB for MRPG: Configuration and Recommendations

## 1) Recommended MariaDB settings 

```ini
[mysqld]
# Basic Network Settings
bind-address = 127.0.0.1
port = 3306
skip-name-resolve = 1

# Encoding
character-set-server = utf8mb4
collation-server = utf8mb4_unicode_ci

# InnoDB (profile for a game server)
innodb_buffer_pool_size = 1G
innodb_log_file_size = 256M
innodb_flush_log_at_trx_commit = 2
innodb_flush_method = O_DIRECT
innodb_file_per_table = 1

# Connections
max_connections = 200
thread_cache_size = 100

# Timetables / Schedules
tmp_table_size = 64M
max_heap_table_size = 64M
sort_buffer_size = 2M
join_buffer_size = 2M

# Logs
slow_query_log = 1
slow_query_log_file = /var/log/mysql/mariadb-slow.log
long_query_time = 1
```

## 2) MariaDB configuration application commands (Ubuntu, Debian)

```bash
# 1. Create a separate configuration for game mode
sudo tee /etc/mysql/mariadb.conf.d/60-mrpg.cnf >/dev/null <<'CNF'
[mysqld]
bind-address = 127.0.0.1
port = 3306
skip-name-resolve = 1
character-set-server = utf8mb4
collation-server = utf8mb4_unicode_ci
innodb_buffer_pool_size = 1G
innodb_log_file_size = 256M
innodb_flush_log_at_trx_commit = 2
innodb_flush_method = O_DIRECT
innodb_file_per_table = 1
max_connections = 200
thread_cache_size = 100
tmp_table_size = 64M
max_heap_table_size = 64M
sort_buffer_size = 2M
join_buffer_size = 2M
slow_query_log = 1
slow_query_log_file = /var/log/mysql/mariadb-slow.log
long_query_time = 1
CNF

# 2. Check the syntax and restart
sudo mariadbd --help >/dev/null
sudo systemctl restart mariadb
sudo systemctl status mariadb --no-pager

# 3. Check the values used
mysql -u root -p -e "SHOW VARIABLES LIKE 'innodb_buffer_pool_size';"
mysql -u root -p -e "SHOW VARIABLES LIKE 'max_connections';"
mysql -u root -p -e "SHOW VARIABLES LIKE 'character_set_server';"
```

## 3) Database and user commands for MRPG

The server opens two connections, one per database. Each query uses one of them, chosen by its table:

* `mrpg_shared` — content, accounts and records. When several game servers work together, this database is replicated between them and the server reaches it through [`other/sql_bridge`](../other/sql_bridge/README.md) (`SELECT` from the local replica, writes to the primary over the WAN).
* `mrpg_local` — the state of one game server (guilds, groups, auction, owners of houses). Every game server has its own. It is **never replicated** and it never goes through the bridge: it is a plain direct connection to the MariaDB next to the server.

Both may live in one MariaDB (single server) or in two (bridge deployments).

```sql
CREATE DATABASE IF NOT EXISTS mrpg_shared
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

CREATE DATABASE IF NOT EXISTS mrpg_local
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

CREATE USER IF NOT EXISTS 'mrpg_user'@'127.0.0.1' IDENTIFIED BY 'CHANGE_ME_SHARED_PASSWORD';
CREATE USER IF NOT EXISTS 'mrpg_local'@'127.0.0.1' IDENTIFIED BY 'CHANGE_ME_LOCAL_PASSWORD';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX ON mrpg_shared.* TO 'mrpg_user'@'127.0.0.1';
-- Local game state has a separate, database-scoped account with full access.
GRANT ALL PRIVILEGES ON mrpg_local.* TO 'mrpg_local'@'127.0.0.1';
FLUSH PRIVILEGES;
```

Importing databases:

```bash
mysql -u root -p mrpg_shared < mrpg_shared.sql
# on the game server's own MariaDB, one per server: mrpg_local_eu, mrpg_local_us, ...
mysql -u root -p mrpg_local < mrpg_local.sql
```

## 4) Shared and local database

### Tables

| Database | Tables |
|---|---|
| `mrpg_local` | `tw_auction_slots`, `tw_groups`, `tw_guilds`, `tw_guilds_decorations`, `tw_guilds_history`, `tw_guilds_invites`, `tw_guilds_ranks`, `tw_guilds_wars`, `tw_houses_decorations`, `tw_links` |
| `mrpg_shared` | every other table |

The ten local tables are a fixed list in the server (`LOCAL_TABLES` in `src/engine/server/sql_connect_pool.cpp`). Every query names its table, and the first name decides the connection: a name from that list uses the local connection, every other name the shared one. The game code never names a database, and nothing is parsed from the SQL text.

Notes:

* One statement uses the tables of one database only. A statement that joins a table of each database is sent to the shared connection and MariaDB rejects it, because the local table is not there.
* The tables of `mrpg_local` have no foreign keys to the tables of `mrpg_shared`. Their rows are not removed automatically when the item, account or world they point to is deleted.
* Moving a table to the other database means editing that list and rebuilding the server.
* A query for a table that exists in neither database goes to the shared database, so the error names it.

### Server settings

```
# shared: through the bridge (or straight to the primary without one)
sv_sql_host "localhost"
sv_sql_port "33060"
sv_sql_login "root"
sv_sql_password ""
sv_sql_database_shared "mrpg_shared"
sv_sql_pool_size 3
sv_sql_use_tls 0

# local: direct game-server → local MariaDB, never the bridge
sv_sql_local_host "127.0.0.1"
sv_sql_local_port "3306"
sv_sql_local_login "mrpg_local"
sv_sql_local_password "CHANGE_ME_LOCAL_PASSWORD"
sv_sql_local_database "mrpg_local"
sv_sql_local_pool_size 1
sv_sql_local_use_tls 0
```

`sv_sql_*` is the shared connection, `sv_sql_local_*` the local one. Each pool opens exactly its own connections: `sv_sql_pool_size` to the shared database, `sv_sql_local_pool_size` to the local one (plus one synchronous connection per database and thread that needs it).

Where the two point:

| Setup | `sv_sql_host` | `sv_sql_local_host` |
|---|---|---|
| one MariaDB, no bridge | its address | empty (same server) |
| bridge for the shared database | the bridge (33060) | the MariaDB next to the server (3306) — **not** the bridge |

The local SQL port is always MariaDB's own (`sv_sql_local_port`), never the bridge port: the bridge serves `mrpg_shared` only and would not know a local database. When the local database lives on the primary instead, set `sv_sql_local_host` to the primary — correct, but every local query then pays a WAN round trip.

On a replica configured with `read_only = 1`, normal database grants alone still reject local writes with MariaDB error 1290. Grant the direct local account `READ ONLY ADMIN` in addition to its database-scoped `ALL PRIVILEGES`; this bypasses the global read-only guard but does not grant it access to `mrpg_shared`. `other/sql_bridge/setup_replication.sh` creates this exact account and grant automatically.

`sv_sql_local_database ""` leaves the local connection unused: every query goes to the shared database, as in older versions. Set both names in the configuration file, not while the server runs. The shared login and the local login may differ; each connection is opened with its own account.

Every game server needs a local database of its own; give each of them another name, for example `sv_sql_local_database "mrpg_local_eu"`.

At startup the server opens both databases. It does not start if:

* the shared database can not be opened, or the local one is configured and can not be opened;
* a local table is missing in the local database (import `mrpg_local.sql`);
* a local table sits in the shared database (a half-finished split would be read from the wrong side).

### Guild rank integrity

`tw_guilds` **and** `tw_guilds_ranks` are local tables. Guilds, their rank
records, member JSON (`Members`) and `DefaultRankID` must therefore come from
the same `mrpg_local_<location>` database. They are neither read through the
bridge nor replicated from `mrpg_shared`.

A crash at `CGuild::CRank::GetID(this=0x0)` means a member JSON rank ID has no
matching row in the local `tw_guilds_ranks` table. This is data left by a
partial old split (for example, copying `tw_guilds` but not
`tw_guilds_ranks`), not a replication delay. Inspect the local database while
the game server is stopped:

```sql
SELECT g.ID, g.Name, g.DefaultRankID,
       r.ID AS default_rank_found, r.Name AS default_rank_name
FROM mrpg_local_eu.tw_guilds AS g
LEFT JOIN mrpg_local_eu.tw_guilds_ranks AS r
  ON r.ID = g.DefaultRankID AND r.GuildID = g.ID
WHERE r.ID IS NULL;

SELECT GuildID, ID, Name
FROM mrpg_local_eu.tw_guilds_ranks
ORDER BY GuildID, ID;
```

Replace `mrpg_local_eu` with the configured `sv_sql_local_database`. Restore
or move the matching rank rows from a backup of the **same location** before
starting the server. The migration below copies both tables. The server also
repairs unresolved members to the guild default as a last-resort startup
repair and no longer dereferences a null rank during that repair; this avoids
the crash, but it cannot reconstruct lost custom rank names or rights.

### Moving an existing database to two databases

The existing database becomes the shared one. It may keep its name (set `sv_sql_database_shared` to it); only the local tables move out.

If the existing database is replicated between several game servers, follow "Upgrading a single database" in [`other/sql_bridge/README.md`](../other/sql_bridge/README.md): it says where to run the statements below.

Stop the game server and make a backup of the existing database (`mysqldump -u root -p old_db > old_db.sql`). Then run as `root` on the MariaDB server of the game (replace `old_db` with the name of the existing database):

```sql
CREATE DATABASE IF NOT EXISTS mrpg_local
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;
USE mrpg_local;
SET SESSION sql_mode = 'NO_AUTO_VALUE_ON_ZERO';

CREATE TABLE tw_auction_slots      LIKE old_db.tw_auction_slots;
CREATE TABLE tw_groups             LIKE old_db.tw_groups;
CREATE TABLE tw_guilds             LIKE old_db.tw_guilds;
CREATE TABLE tw_guilds_decorations LIKE old_db.tw_guilds_decorations;
CREATE TABLE tw_guilds_history     LIKE old_db.tw_guilds_history;
CREATE TABLE tw_guilds_invites     LIKE old_db.tw_guilds_invites;
CREATE TABLE tw_guilds_ranks       LIKE old_db.tw_guilds_ranks;
CREATE TABLE tw_guilds_wars        LIKE old_db.tw_guilds_wars;
CREATE TABLE tw_houses_decorations LIKE old_db.tw_houses_decorations;

INSERT INTO tw_auction_slots      SELECT * FROM old_db.tw_auction_slots;
INSERT INTO tw_groups             SELECT * FROM old_db.tw_groups;
INSERT INTO tw_guilds             SELECT * FROM old_db.tw_guilds;
INSERT INTO tw_guilds_decorations SELECT * FROM old_db.tw_guilds_decorations;
INSERT INTO tw_guilds_history     SELECT * FROM old_db.tw_guilds_history;
INSERT INTO tw_guilds_invites     SELECT * FROM old_db.tw_guilds_invites;
INSERT INTO tw_guilds_ranks       SELECT * FROM old_db.tw_guilds_ranks;
INSERT INTO tw_guilds_wars        SELECT * FROM old_db.tw_guilds_wars;
INSERT INTO tw_houses_decorations SELECT * FROM old_db.tw_houses_decorations;

-- CREATE TABLE ... LIKE does not copy foreign keys
ALTER TABLE tw_guilds_invites
  ADD CONSTRAINT tw_guilds_invites_ibfk_1 FOREIGN KEY (GuildID) REFERENCES tw_guilds (ID) ON DELETE CASCADE ON UPDATE CASCADE;
ALTER TABLE tw_guilds_wars
  ADD CONSTRAINT tw_guilds_wars_ibfk_1 FOREIGN KEY (GuildID1) REFERENCES tw_guilds (ID) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT tw_guilds_wars_ibfk_2 FOREIGN KEY (GuildID2) REFERENCES tw_guilds (ID) ON DELETE CASCADE ON UPDATE CASCADE;
```

The owners of houses are stored in `tw_links`. If the existing database already has this table, copy it like the other ones:

```sql
CREATE TABLE tw_links LIKE old_db.tw_links;
INSERT INTO tw_links SELECT * FROM old_db.tw_links;
```

If it does not (the owners are still in the columns `tw_houses.UserID` and `tw_guilds_houses.GuildID`), create the table from these columns:

```sql
CREATE TABLE tw_links (
  ID int(11) NOT NULL AUTO_INCREMENT,
  Type varchar(32) NOT NULL,
  OwnerID bigint(20) NOT NULL,
  TargetID bigint(20) NOT NULL,
  PRIMARY KEY (ID)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

INSERT INTO tw_links (Type, OwnerID, TargetID)
  SELECT 'player_house', UserID, ID FROM old_db.tw_houses WHERE UserID IS NOT NULL;
INSERT INTO tw_links (Type, OwnerID, TargetID)
  SELECT 'guild_house', GuildID, ID FROM old_db.tw_guilds_houses WHERE GuildID IS NOT NULL;
```

Compare the number of rows of every table, for example:

```sql
SELECT (SELECT COUNT(*) FROM old_db.tw_guilds) AS old_rows, (SELECT COUNT(*) FROM mrpg_local.tw_guilds) AS new_rows;
```

Then remove the copied tables from the old database, before the game server starts: while the shared database still has a local table, the server refuses to start (the tables with foreign keys to `tw_guilds` go first):

```sql
USE old_db;
DROP TABLE tw_guilds_invites, tw_guilds_wars;
DROP TABLE tw_auction_slots, tw_groups, tw_guilds, tw_guilds_decorations, tw_guilds_history, tw_guilds_ranks, tw_houses_decorations;
DROP TABLE IF EXISTS tw_links;

-- only if the owners of houses were still stored in these columns
ALTER TABLE tw_houses DROP COLUMN UserID;
ALTER TABLE tw_guilds_houses DROP COLUMN GuildID;
```

Give the account of the game server access to the new database (see section 3), then set `sv_sql_database_shared` (the old database) and `sv_sql_local_database` (`mrpg_local`) — plus `sv_sql_local_host` and `sv_sql_local_login` if the local database is on another MariaDB — and start the server.
