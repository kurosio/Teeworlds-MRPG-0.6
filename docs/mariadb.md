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

The server requires exactly two database endpoints, each with an independent connection pool. Every query is routed by its declared table kind:

* `mrpg_shared` — the remote authoritative database for content, accounts and data shared by game servers. The game server connects to MariaDB directly; reads and writes use the same endpoint.
* `mrpg_local` — this game server's local state (groups, guild decorations/history/invites, house decorations and links). It connects directly to the MariaDB local to this server.

For a development setup both endpoints may be on one MariaDB host, but they remain separate schemas, endpoint configurations and pools. Production deployments should point the remote endpoint at the shared database host and the local endpoint at `127.0.0.1`.

```sql
CREATE DATABASE IF NOT EXISTS mrpg_shared
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

CREATE DATABASE IF NOT EXISTS mrpg_local
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

-- Run on the remote MariaDB; restrict the host to the game server's address.
CREATE USER IF NOT EXISTS 'mrpg_user'@'GAME_SERVER_IP' IDENTIFIED BY 'CHANGE_ME_SHARED_PASSWORD';
GRANT SELECT, INSERT, UPDATE, DELETE ON mrpg_shared.* TO 'mrpg_user'@'GAME_SERVER_IP';

-- Run on this game server's local MariaDB.
CREATE USER IF NOT EXISTS 'mrpg_local'@'127.0.0.1' IDENTIFIED BY 'CHANGE_ME_LOCAL_PASSWORD';
GRANT SELECT, INSERT, UPDATE, DELETE ON mrpg_local.* TO 'mrpg_local'@'127.0.0.1';
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

The six local tables are a fixed list in the database layer (`LOCAL_TABLES` in `src/engine/server/db/db_types.h`). A module names its endpoint once, at compile time, from the table it writes:

```cpp
static constexpr Db::EKind KIND = Db::KindOfTable(TW_GUILDS_TABLE); // EKind::Local for the six, EKind::Remote otherwise
Db::Query(KIND, "UPDATE tw_guilds SET Bank = ? WHERE ID = ?", Db::Bind(Bank, ID));
```

A name from that list uses the local pool, every other name uses the remote pool, and routing never parses the SQL text. Both endpoints and database names are required at startup; a missing local endpoint is an error, not a fallback to remote.

Notes:

* One statement uses the tables of one database only. A statement that joins a table of each database is sent to the shared connection and MariaDB rejects it, because the local table is not there.
* The tables of `mrpg_local` have no foreign keys to the tables of `mrpg_shared`. Their rows are not removed automatically when the item, account or world they point to is deleted.
* Moving a table to the other database means editing that list and rebuilding the server.
* A query for a table that exists in neither database goes to the shared database, so the error names it.

### json columns

`tw_bots_info.JsonTeeInfo` holds one tee-info document. The column was
`varchar(128)` with a default that was not valid JSON (`color_feer`, no closing
brace), so a row written without an explicit value, or a value a little longer
than the seeded ones, produced a document the parser rejects. The dump declares
`varchar(256)` and a valid default; an installation created earlier needs the
same two changes once, with every game server stopped:

```sql
ALTER TABLE tw_bots_info
    MODIFY `JsonTeeInfo` varchar(256) NOT NULL
    DEFAULT '{\"color_body\":-1,\"color_feet\":-1,\"custom_color\":0,\"skin\":\"default\"}';
-- rows written with the old default, and rows cut by the old width:
UPDATE tw_bots_info SET JsonTeeInfo = '{\"color_body\":-1,\"color_feet\":-1,\"custom_color\":0,\"skin\":\"default\"}'
    WHERE JSON_VALID(JsonTeeInfo) = 0;
```

A column that stores a json document is sized for the largest document the code
writes, not for the seeded ones: MariaDB truncates an overlong value in a
non-strict `sql_mode`, and a truncated document is a parse error every time it
is read.

### Server settings

```
# remote: direct connection to the shared authoritative MariaDB
sv_sql_host "127.0.0.1"
sv_sql_port "3306"
sv_sql_login "mrpg_user"
sv_sql_password "CHANGE_ME_SHARED_PASSWORD"
sv_sql_database_shared "mrpg_shared"
sv_sql_pool_size 3
sv_sql_use_tls 0

# local: direct game-server → its own local MariaDB
sv_sql_local_host "127.0.0.1"
sv_sql_local_port "3306"
sv_sql_local_login "mrpg_local"
sv_sql_local_password "CHANGE_ME_LOCAL_PASSWORD"
sv_sql_local_database "mrpg_local"
sv_sql_local_pool_size 1
sv_sql_local_use_tls 0
```

`sv_sql_*` configures the remote connection, and `sv_sql_local_*` configures the local connection. Each independent pool opens only its endpoint's connections: `sv_sql_pool_size` to the remote database and `sv_sql_local_pool_size` to the local one. There are no synchronous/ad-hoc database connections.

For development on one MariaDB host, use that host address for both endpoints but keep the separate schema names and credentials. In production, configure `sv_sql_host` with the remote MariaDB address and `sv_sql_local_host` as `127.0.0.1` (or the local MariaDB address). Both database names and both complete endpoint configurations are mandatory; an empty `sv_sql_local_database` is invalid. The remote and local logins may differ.

For split-database deployments, every game server needs a local database of its own; give each of them another name, for example `sv_sql_local_database "mrpg_local_eu"`.

At startup the server opens and checks both databases. It does not start if:

* either required endpoint is incomplete or cannot be opened;
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
checks both conditions at startup and refuses an ambiguous split. Grant the local account access to the new local database, set
`sv_sql_database_shared` to the remote shared database and
`sv_sql_local_database` to the per-server local database, then start the server
and inspect its SQL startup log. Keep all game servers stopped until the
migration and endpoint checks are complete.
