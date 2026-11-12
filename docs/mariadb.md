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
| `mrpg_local` | `tw_groups`, `tw_guilds_decorations`, `tw_guilds_history`, `tw_guilds_invites`, `tw_houses_decorations`, `tw_links` |
| `mrpg_shared` | every other table |

The six local tables are a fixed list in the server (`LOCAL_TABLES` in `src/engine/server/sql_connect_pool.cpp`). Every query names its table, and the first name decides the connection: a name from that list uses the local connection, every other name the shared one. The game code never names a database, and nothing is parsed from the SQL text.

Notes:

* One statement uses the tables of one database only. A statement that joins a table of each database is sent to the shared connection and MariaDB rejects it, because the local table is not there.
* The tables of `mrpg_local` have no foreign keys to the tables of `mrpg_shared`. Their rows are not removed automatically when the item, account or world they point to is deleted.
* Moving a table to the other database means editing that list and rebuilding the server.
* A query for a table that exists in neither database goes to the shared database, so the error names it.

### Server settings

```
# shared: through the bridge (or straight to the primary without one)
sv_sql_host "127.0.0.1"
sv_sql_port "33060"
sv_sql_login "mrpg_user"
sv_sql_password "CHANGE_ME_SHARED_PASSWORD"
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

For split-database deployments, every game server needs a local database of its own; give each of them another name, for example `sv_sql_local_database "mrpg_local_eu"`.

At startup the server opens both databases. It does not start if:

* the shared database can not be opened, or the local one is configured and can not be opened;
* a local table is missing in the local database (import `mrpg_local.sql`);
* a local table sits in the shared database (a half-finished split would be read from the wrong side).

### Moving an existing database to two databases

The current server routes exactly these six tables to the local database:
`tw_groups`, `tw_guilds_decorations`, `tw_guilds_history`,
`tw_guilds_invites`, `tw_houses_decorations`, and `tw_links`. This list is
version-specific; older instructions that move `tw_guilds`, auction, rank, or
war tables describe an earlier schema and must not be followed for this
version. `tw_guilds` and the other unlisted tables stay in the shared database.

Migration is data-sensitive and legacy schemas differ. Before changing
anything, stop every game server that can write to the database and take a
verified backup. Create the local schema from the current `mrpg_local.sql`
file, then migrate data for the six listed tables only, checking each table's
columns and row counts against the source. Do not blindly use `CREATE TABLE …
LIKE` against an old schema: the table definitions may no longer match the
current dump. If the old schema stores house ownership in `tw_houses.UserID`
or `tw_guilds_houses.GuildID` rather than in `tw_links`, convert those rows to
the current `tw_links` format as a separate, reviewed step. Keep the source
tables and backup until the new installation has been checked.

Before starting the server, ensure the local database contains all six tables
and that none of those six tables remains in the shared database. The server
checks both conditions at startup and refuses an ambiguous split. Grant the
game account access to the new local database, set `sv_sql_database_shared`
to the shared database and `sv_sql_local_database` to the per-server local
database, then start the server and inspect its SQL startup log. For
multi-server replicas, follow the current [SQL bridge upgrade guide](../other/sql_bridge/README.md)
instead of manually moving replicated tables on a live installation.
