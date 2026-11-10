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

The server uses two databases on the same MariaDB server, with the same account. Every connection of the server is opened to one of them:

* `mrpg_shared` — content, accounts and records. When several game servers work together, this is the database that is replicated between them.
* `mrpg_local` — the state of one game server (guilds, groups, auction, owners of houses). Every game server has its own. It is **never replicated**: replication copies `mrpg_shared` only, and every replica excludes the local databases (`replicate_wild_ignore_table = mrpg_local%.%`, see [`other/sql_bridge/README.md`](../other/sql_bridge/README.md)).

```sql
CREATE DATABASE IF NOT EXISTS mrpg_shared
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

CREATE DATABASE IF NOT EXISTS mrpg_local
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

CREATE USER IF NOT EXISTS 'mrpg_user'@'127.0.0.1' IDENTIFIED BY 'CHANGE_ME_STRONG_PASSWORD';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX ON mrpg_shared.* TO 'mrpg_user'@'127.0.0.1';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX ON mrpg_local.* TO 'mrpg_user'@'127.0.0.1';
FLUSH PRIVILEGES;
```

Importing databases:

```bash
mysql -u root -p mrpg_shared < mrpg_shared.sql
mysql -u root -p mrpg_local < mrpg_local.sql
```

## 4) Shared and local database

### Tables

| Database | Tables |
|---|---|
| `mrpg_local` | `tw_auction_slots`, `tw_groups`, `tw_guilds`, `tw_guilds_decorations`, `tw_guilds_history`, `tw_guilds_invites`, `tw_guilds_ranks`, `tw_guilds_wars`, `tw_houses_decorations`, `tw_links` |
| `mrpg_shared` | every other table |

The server has no list that assigns the tables to the databases. At startup it reads the structure of the shared database (its list of tables, `SHOW TABLES`). For every query it then looks for the table of the query in this list:

* the shared database has the table — the query uses a connection to the shared database;
* the shared database does not have it — the query uses a connection to the local database.

The game code never names a database. To move a table to the other database, move the table itself (it must exist in one database only) and restart the server.

Notes:

* One statement uses the tables of one database only. A statement with a table of each database (`JOIN`, subquery) is sent to the shared database and MariaDB rejects it, because the local table is not there.
* The tables of `mrpg_local` have no foreign keys to the tables of `mrpg_shared`. Their rows are not removed automatically when the item, account or world they point to is deleted.
* The list of tables is read once, at startup. Restart the game server after a table is added to the shared database or removed from it.
* A query for a table that exists in neither database is sent to the local database, so the error names the local database.

### Server settings

```
sv_sql_host "localhost"
sv_sql_login "root"
sv_sql_port "3306"
sv_sql_password ""
sv_sql_database_shared "mrpg_shared"
sv_sql_database_local "mrpg_local"
sv_sql_pool_size 3
sv_sql_use_tls 0
```

Both databases use the same host, port and account, so they must be on the same MariaDB server and the account needs access to both of them. `sv_sql_database_shared` replaces the older `sv_sql_database`.

`sv_sql_pool_size` is the number of worker connections for both databases together. It is divided between them, and the shared database gets the larger part, because most of the queries use it: one third of the connections (at least one) is opened to the local database, the rest to the shared one.

| `sv_sql_pool_size` | Shared | Local |
|---|---|---|
| 2 | 1 | 1 |
| 3 | 2 | 1 |
| 6 | 4 | 2 |
| 12 | 8 | 4 |

Synchronous queries use one more connection for each database.

`sv_sql_database_local ""` (or the name of the shared database) turns the split off: all tables are read from `sv_sql_database_shared` and all connections are opened to it, as in the older versions. This is the default of the server binary; the shipped `autoexec_server.cfg` sets `mrpg_local`. Set both names in the configuration file, not while the server runs.

Every game server needs a local database of its own. If several game servers use the same MariaDB server (directly or through [`other/sql_bridge`](../other/sql_bridge/README.md)), give each of them another name, for example `sv_sql_database_local "mrpg_local_eu"`. Keep the `mrpg_local` prefix: with the bridge, one replication rule (`mrpg_local%.%`) excludes all local databases from the replicas, and the reads of a local database are then served by the central database.

At startup the server reads the tables of both databases. It does not start if:

* one of the databases can not be opened;
* a table exists in both databases (the queries would use the one in the shared database and ignore the rows of the local one);
* a table of `mrpg_local.sql` exists in neither database.

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

Then remove the copied tables from the old database, before the game server starts. The server chooses the database of a query by the tables of the shared database: while the old database still has these tables, the server does not start (the tables with foreign keys to `tw_guilds` go first):

```sql
USE old_db;
DROP TABLE tw_guilds_invites, tw_guilds_wars;
DROP TABLE tw_auction_slots, tw_groups, tw_guilds, tw_guilds_decorations, tw_guilds_history, tw_guilds_ranks, tw_houses_decorations;
DROP TABLE IF EXISTS tw_links;

-- only if the owners of houses were still stored in these columns
ALTER TABLE tw_houses DROP COLUMN UserID;
ALTER TABLE tw_guilds_houses DROP COLUMN GuildID;
```

Give the account of the game server access to the new database (see section 3), set `sv_sql_database_shared` and `sv_sql_database_local` and start the server.
