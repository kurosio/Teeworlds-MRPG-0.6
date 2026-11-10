# mysql_bridge.py — central database + replica setup guide

`mysql_bridge.py` lets **one central database** serve game servers in several
locations with minimal added latency. Each game-server location runs a local
MariaDB **read replica** plus the bridge; reads of the shared database are
served locally, writes are forwarded to the central primary over the WAN:

```
game server (location A) --\
game server (location B) ---+--> local bridge --> [local replica] --\
game server (location C) --/    (mysql_bridge.py)                    +--> replication --> CENTRAL PRIMARY
                                                                    --/
```

The bridge is a pure Python 3 (stdlib, asyncio) MySQL/MariaDB proxy: it routes
plain `SELECT`/`SHOW`/`DESCRIBE`/`EXPLAIN` statements to the local replica and
everything else (`INSERT`/`UPDATE`/`DELETE`/DDL/transactions/anything it is not
sure about) to the central primary. Only the shared database `mrpg_shared` is
replicated, so a read of a local database is re-run on the primary. See
[How routing works](#how-routing-works) for the exact rules and the consistency
guarantees.

**Files in this folder**

| File | Purpose |
|---|---|
| `mysql_bridge.py` | the bridge itself |
| `bridge.cfg.example` | bridge config template (copy to `bridge.cfg`) |
| `replica_setup.sql` | accounts to create on the central primary |
| `tests/` | test suite, fake MariaDB server and benchmark (see [Testing](#testing)) |

**Ports / who connects to what**

| Port | Runs on | Connects to it |
|---|---|---|
| 3306 | central primary | replicas (replication) + bridges (writes) |
| 3306 | each location's replica | local bridge only (`127.0.0.1`) |
| 33060 | each location's bridge | local game server only (`127.0.0.1`) |

Notes before you start:

* Commands below assume Debian/Ubuntu with MariaDB 10.6+ (see also
  [`docs/mariadb.md`](../../docs/mariadb.md) for server tuning). Client tools
  are `mariadb` / `mariadb-dump`; on older installs they are called
  `mysql` / `mysqldump`.
* The shared database is called `mrpg_shared` everywhere (as in
  `mrpg_shared.sql`, `replica_setup.sql`, `bridge.cfg.example`). If you use a
  different name, change it consistently in the dump import, the `GRANT`s,
  `bridge.cfg` and `sv_sql_database_shared`.
* Every game server has **two** databases: the replicated `mrpg_shared` and a
  local database of its own that is **never replicated**. The model and its
  consequences are described under
  [What is replicated and what is not](#what-is-replicated-and-what-is-not).
* Replace every `<PLACEHOLDER>` (hostnames, passwords) with your own values.
  Avoid `'` and `\` inside passwords. Pick **strong, unique** passwords for
  `mrpg_replica`, `mrpg_bridge` and `mrpg_user`.
* Encryption: the bridge can encrypt its own connections to the primary and
  the replica with standard MySQL TLS (`use_tls = true`, certificate
  verified). It does **not** terminate TLS from the game server, so the game
  server ↔ bridge hop is plain text: keep it on `127.0.0.1` or a trusted LAN
  and set `sv_sql_use_tls 0`. A WireGuard/Tailscale (or similar) private
  network between locations is still recommended (it also keeps the
  central port 3306 off the internet). Replication is encrypted separately
  via `MASTER_SSL=1`.

---

## What is replicated and what is not

| Database | Holds | On the central primary | On each location's replica |
|---|---|---|---|
| `mrpg_shared` | content, accounts, records | yes | yes — **the only replicated database** |
| `mrpg_local_<location>` | the state of one game server: guilds, groups, auction, house owners (see [`docs/mariadb.md`](../../docs/mariadb.md)) | yes | **no** — excluded from replication, never created there |

Only `mrpg_shared` is copied to the replicas. The exclusion is one rule on
every replica, `replicate_wild_ignore_table = mrpg_local%.%` (step 2.1), and
the local databases are never part of a snapshot (step 2.2). With every local
database matching that pattern, a replica never creates, changes or drops one;
`mrpg_shared` is the only MRPG database on it.

What follows from that:

* **The game server reaches both databases through the bridge**, with the same
  host, port and account. Login is relayed to the primary, so the account and
  its privileges are the primary's. Writes of both databases go to the
  primary.
* **Reads of `mrpg_shared`** are served by the local replica.
* **Reads of a local database always go to the primary**, because the replica
  has no copy of it. They cost one WAN round trip: that is the price of local
  state, and there is nothing to replicate or tune. The bridge tries the
  replica once for such a database, logs a `replica-db` warning (at most every
  30 s) and otherwise sends that database straight to the primary.
* A connection is opened to one database, and one statement never uses tables
  of both databases.
* **Two game servers must never share a local database.** Name them after the
  location: `mrpg_local_eu`, `mrpg_local_us`, ... (one per game server if a
  location runs several). Keep the `mrpg_local` prefix: the exclusion rule is
  written for it.
* Upgrading from a single database: see
  [Upgrading a single database](#upgrading-a-single-database).
* A replica that already holds a local database (from a setup without the
  exclusion rule) keeps a stale copy and must be cleaned: see
  [Excluding a local database from replication later](#excluding-a-local-database-from-replication-later).

---

## Step 0 — Prerequisites

* One central MariaDB host ("primary"), reachable from every location over the
  private network/VPN.
* Per game-server location: one host for the local replica + bridge
  (may be the game-server host itself).
* On bridge hosts: Python 3.8+ (`python3 --version`). No packages needed.

---

## Step 1 — Central primary database

### 1.1 Install MariaDB

```bash
sudo apt update
sudo apt install -y mariadb-server
sudo mysql_secure_installation
```

### 1.2 Enable binary logging (required for replication)

MariaDB GTID is on by default; replication needs the binlog and a unique
`server_id`:

```bash
sudo tee /etc/mysql/mariadb.conf.d/61-mrpg-replication.cnf >/dev/null <<'CNF'
[mysqld]
server_id = 1                       # unique in the cluster; replicas use 2, 3, ...
log_bin = /var/log/mysql/mariadb-bin
binlog_format = ROW
gtid_strict_mode = 1
CNF
sudo systemctl restart mariadb
mariadb -u root -p -e "SHOW VARIABLES LIKE 'log_bin'; SHOW VARIABLES LIKE 'server_id';"
```

Also apply the tuning from [`docs/mariadb.md`](../../docs/mariadb.md), and set
`bind-address` to the private/VPN address so replicas and bridges can connect.

### 1.3 Create the databases and import the game schema

The shared database:

```bash
mariadb -u root -p -e "CREATE DATABASE IF NOT EXISTS mrpg_shared CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
mariadb -u root -p mrpg_shared < mrpg_shared.sql
```

One local database per game server, all from the same `mrpg_local.sql`
(repeat with `mrpg_local_us`, ...). They exist on the primary only and are
excluded from replication (step 2.1): a replica never gets them.

```bash
mariadb -u root -p -e "CREATE DATABASE IF NOT EXISTS mrpg_local_eu CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
mariadb -u root -p mrpg_local_eu < mrpg_local.sql
```

### 1.4 Create replication, bridge and game-server accounts

Set real passwords in `replica_setup.sql` (it creates `mrpg_replica` for the
replication stream and `mrpg_bridge` for the bridges' read connections), then
run it **on the primary**:

```bash
cd other/sql_bridge
cp replica_setup.sql replica_setup.local.sql
sed -i "/mrpg_replica/s/'changeme'/'<REPLICA_REPL_PASSWORD>'/" replica_setup.local.sql
sed -i "/mrpg_bridge/s/'changeme'/'<BRIDGE_READ_PASSWORD>'/"   replica_setup.local.sql
mariadb -u root -p < replica_setup.local.sql
```

The game server logs in **through the bridge to the primary**, so it needs a
write-capable account on the primary (auth is verified by the primary only):

```bash
mariadb -u root -p <<'SQL'
CREATE USER IF NOT EXISTS 'mrpg_user'@'%' IDENTIFIED BY '<GAME_PASSWORD>';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX ON mrpg_shared.* TO 'mrpg_user'@'%';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX ON mrpg_local_eu.* TO 'mrpg_user'@'%';
FLUSH PRIVILEGES;
SQL
```

Repeat the second `GRANT` for every local database.

> In production, tighten the `@'%'` host masks to the VPN IPs of your bridges
> and replicas.

### 1.5 Firewall

Allow 3306 only from the private network (example with ufw, adjust subnet):

```bash
sudo ufw allow from 10.0.0.0/24 to any port 3306 proto tcp
```

Never expose 3306 to the internet.

---

## Step 2 — Read replica at each game-server location

Repeat this whole step **once per location**.

### 2.1 Install and configure MariaDB

```bash
sudo apt install -y mariadb-server
sudo tee /etc/mysql/mariadb.conf.d/61-mrpg-replica.cnf >/dev/null <<'CNF'
[mysqld]
server_id = 2                       # unique per replica: 2, 3, 4, ...
bind-address = 127.0.0.1            # only the local bridge needs it
read_only = 1                       # the bridge never writes to a replica
gtid_strict_mode = 1
# exclusion list of replication: the local databases of the game servers
replicate_wild_ignore_table = mrpg_local%.%
CNF
sudo systemctl restart mariadb
```

This line **is** the "shared only" part of replication: without it the
replica would copy every database of the primary, local ones included. A few
details matter:

* A value is `database.table`, matched with the wildcards of `LIKE`: `%` is
  any text, `_` is any one character. `mrpg_local%.%` covers every table of
  `mrpg_local`, `mrpg_local_eu`, `mrpg_local_us`, ... — one rule for all
  current and future local databases.
* The table part of that pattern is exactly `%`, so MariaDB also applies the
  filter to database-level statements (`CREATE DATABASE`, `ALTER DATABASE`,
  `DROP DATABASE`) of matching databases. The replica therefore does not even
  create an empty `mrpg_local_...` when the database is created on the
  primary. A narrower pattern such as `mrpg_local%.tw_%` would filter the
  tables but let the empty database through — do not narrow it.
* Filtering happens on the replica, and the skipped events still advance its
  GTID position: creating, moving or dropping a local database never stops
  replication and never breaks its consistency.
* Never put this rule on the primary: it keeps every local database and must
  serve them.
* The rule does not filter stored procedures, stored functions and events
  (`replicate_ignore_db` would). Local databases contain tables only
  (`mrpg_local.sql`), so the table rule is sufficient; do not put routines
  into a local database.
* In `my.cnf` every value is a separate line — a comma-separated list is not
  accepted there. The active list is shown by
  `SHOW SLAVE STATUS\G` (`Replicate_Wild_Ignore_Table`).

A local database with another name needs a value of its own:

```ini
replicate_wild_ignore_table = mrpg_local%.%
replicate_wild_ignore_table = guilds_eu.%
```

For a replica that already runs, or that already holds a local database, see
[Excluding a local database from replication later](#excluding-a-local-database-from-replication-later).

### 2.2 Take a snapshot of the primary and import it

On the **central primary** (prefer a low-traffic moment; `--single-transaction`
keeps InnoDB tables consistent):

```bash
mariadb-dump -u root -p --single-transaction --quick --routines --triggers --events \
  --source-data=2 --gtid mrpg_shared > mrpg_shared_snapshot.sql
```

Only `mrpg_shared` goes into the snapshot. The local databases stay on the
primary: the replica ignores their changes (step 2.1), so it must not get a
copy of them either.

Copy `mrpg_shared_snapshot.sql` to the replica host (e.g. `scp`), then on the
**replica**:

```bash
mariadb -u root -p -e "CREATE DATABASE IF NOT EXISTS mrpg_shared CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
mariadb -u root -p mrpg_shared < mrpg_shared_snapshot.sql
```

(`--gtid` stamps the dump with the primary's GTID position, so replication
continues exactly where the snapshot ends.)

### 2.3 Start replicating from the central primary

On the **replica**, run (values from step 1.4):

```bash
mariadb -u root -p <<'SQL'
CHANGE MASTER TO
  MASTER_HOST='db-central.example.com',
  MASTER_PORT=3306,
  MASTER_USER='mrpg_replica',
  MASTER_PASSWORD='<REPLICA_REPL_PASSWORD>',
  MASTER_USE_GTID=CURRENT_POS,
  MASTER_SSL=1;
START SLAVE;
SQL
```

`MASTER_SSL=1` encrypts the replication stream over the WAN.

### 2.4 Create the bridge's local read account

The snapshot starts at the current GTID position, so the accounts created on
the primary in step 1.4 are *not* part of it. Create the read-only account the
local bridge will use:

```bash
mariadb -u root -p <<'SQL'
CREATE USER IF NOT EXISTS 'mrpg_bridge'@'127.0.0.1' IDENTIFIED BY '<BRIDGE_READ_PASSWORD>';
GRANT SELECT ON mrpg_shared.* TO 'mrpg_bridge'@'127.0.0.1';
FLUSH PRIVILEGES;
SQL
```

The account gets access to `mrpg_shared` only and needs nothing else: the
local databases are not on the replica (step 2.1), and their reads are served
by the primary.

Optional, for the replication-lag guard (`max_replica_lag_sec` in
`bridge.cfg`), allow the account to read the replication status:

```bash
mariadb -u root -p -e "GRANT REPLICA MONITOR ON *.* TO 'mrpg_bridge'@'127.0.0.1';"
# MariaDB < 10.5: GRANT REPLICATION CLIENT ON *.* TO ...
```

Give the account **only** these privileges: the bridge replays a few
session statements (`SET NAMES ...`, `SET sql_mode = ...`) of the game
server on its replica connections, and reads run under this account.

### 2.5 Verify replication

```bash
mariadb -u root -p -e "SHOW SLAVE STATUS\G" | grep -E "Slave_IO_Running|Slave_SQL_Running|Seconds_Behind|Replicate_Wild_Ignore_Table"
mariadb -u root -p -e "SHOW DATABASES LIKE 'mrpg%';"
```

Expect `Slave_IO_Running=Yes` and `Slave_SQL_Running=Yes`.
`Seconds_Behind_Master` is your replication lag — watch it in production.
`Replicate_Wild_Ignore_Table` must show `mrpg_local%.%`, and
`SHOW DATABASES LIKE 'mrpg%'` must return `mrpg_shared` and nothing else: a
local database on a replica is a leftover of a setup without the rule, it is
stale, and it has to be removed (next section).

---

## Excluding a local database from replication later

Replication is shared-only: **every replica** lists `mrpg_local%.%` in
`replicate_wild_ignore_table`, so `mrpg_shared` is the only MRPG database on
it. The exclusion is configured per replica and can be added to a running
cluster without stopping the primary or the game servers. Do this when a
replica was set up before the rule existed, or when a local database with a
name outside the `mrpg_local` prefix appears.

### Add the rule on every replica

Either edit the config and restart (survives reboots):

```bash
sudo nano /etc/mysql/mariadb.conf.d/61-mrpg-replica.cnf
# under [mysqld]:
#   replicate_wild_ignore_table = mrpg_local%.%
sudo systemctl restart mariadb
```

or change it online — the replica threads must be stopped, and `SET GLOBAL`
lasts until the next restart, so still add the line to the config file:

```bash
mariadb -u root -p <<'SQL'
STOP SLAVE;
SET GLOBAL replicate_wild_ignore_table = 'mrpg_local%.%';
START SLAVE;
SQL
```

For a local database with another name, add its value to the list
(`'mrpg_local%.%,guilds_eu.%'`). `SET GLOBAL` accepts a comma-separated list;
`my.cnf` needs one line per value (step 2.1). Run this on **each** replica:
the filter is not replicated itself.

### Remove a copy that the replica already holds

The filter stops future events; it does not delete anything that was replicated
before. A local database that is already on a replica is a stale copy — the
game servers never read it (the bridge reads local tables from the primary),
but it wastes space, shows up in `SHOW DATABASES` and hides the mistake. Remove
it on each such replica:

```bash
mariadb -u root -p <<'SQL'
STOP SLAVE;
DROP DATABASE IF EXISTS mrpg_local_eu;
START SLAVE;
SQL
mariadb -u root -p -e "SHOW DATABASES LIKE 'mrpg%';"   # expect mrpg_shared only
```

The `DROP` runs on the replica only; the primary keeps its database, and the
replica's GTID position is untouched.

### Verify

```bash
mariadb -u root -p -e "SHOW SLAVE STATUS\G" \
  | grep -E "Slave_IO_Running|Slave_SQL_Running|Replicate_Wild_Ignore_Table"
```

Both threads must be `Yes` and the ignore list must contain
`mrpg_local%.%` (plus one value per local database with another name).

### If a statement of the primary must stay out of the binlog

`SET sql_log_bin = 0` in a session of the primary keeps that session's
statements out of the binary log entirely — for example the `GRANT`s of
[Adding a game server later](#adding-a-game-server-later), because the accounts
of the primary do not exist on the replicas. It is per session and has to be
repeated every time; it is not a replacement for the replica-side rule. Keep
the game server's own writes in the binary log: the rule above decides what
the replicas apply (`mrpg_shared`) and what they skip (local databases).

---

## Step 3 — Run the bridge at each location

Repeat once per location.

### 3.1 Get the script

Copy `other/sql_bridge/` to the host (or clone the repo) and optionally install
uvloop for extra throughput:

```bash
python3 --version            # needs 3.8+
pip3 install uvloop          # optional: typically 10-30% more throughput
```

### 3.2 Write `bridge.cfg`

`bridge.cfg` is an INI file with three sections: `[bridge]` (where the game
servers connect), `[primary]` (the central database) and `[replica]` (this
location's read replica). Copy the template and edit it:

```bash
cd other/sql_bridge
cp bridge.cfg.example bridge.cfg
chmod 600 bridge.cfg
nano bridge.cfg
```

**Minimal file — this is enough to run.** Every other setting has a working
default:

```ini
[bridge]
# LAN IP instead of 127.0.0.1, if the game server is on another host
listen_host = 127.0.0.1
listen_port = 33060

[primary]
# central primary from step 1
host = db-central.example.com
port = 3306

[replica]
# this location's replica from step 2
host = 127.0.0.1
port = 3306
# read-only account, step 2.4
user = mrpg_bridge
password = <BRIDGE_READ_PASSWORD>
# database used for reads that arrive without a default one
database = mrpg_shared
```

`[primary]` has no user or password on purpose: the game server's login is
relayed to the primary, so its own account is used and checked there. The two
databases need no separate configuration either: the game server sends its
`USE`/login database with every connection and the bridge routes by statement.
Reads of `mrpg_shared` go to the replica; reads of a local database go to the
primary, because the replica does not have it
([why](#what-is-replicated-and-what-is-not)).

**Settings worth knowing** (default in parentheses; the rest is commented in
`bridge.cfg.example`):

| Key | Default | What it does |
|---|---|---|
| `[bridge] max_clients` | 256 | maximum simultaneous game-server connections |
| `[bridge] allowed_clients` | all | source IP/CIDR allowlist; set it whenever `listen_host` is not loopback |
| `[bridge] replica_pool_size` | 16 | persistent replica connections shared by all game servers; keep it above the sum of their `sv_sql_pool_size` |
| `[bridge] route_selects_to_replica` | true | `false` sends *everything* to the primary (incident mode, zero-overhead relay) |
| `[bridge] read_your_writes_grace_sec` | 0 | after a write, matching reads use the primary for this long; set it above the replication lag, e.g. 2 |
| `[bridge] read_your_writes_scope` | `table` | which reads that covers: `table`, `global` or `connection` ([Consistency](#consistency)) |
| `[bridge] max_replica_lag_sec` | 0 (off) | bypass the replica while it is behind by more than this; needs `REPLICA MONITOR`, step 2.4 |
| `[bridge] log_level` | `INFO` | `DEBUG` logs the route of every query (without string literals) |
| `[bridge] stats_interval_sec` | 30 | period of the `stats replica=... primary=...` log line; 0 disables it |
| `[primary] use_tls` | false | TLS to the primary; with `tls_ca_file` the certificate is verified |
| `[replica] use_tls` | false | TLS to the replica; needed only if it is not on localhost |

The defaults above are those of the script; `bridge.cfg.example` already sets
two recommended values of its own (`max_clients = 64`,
`read_your_writes_grace_sec = 2.0`).

Timeouts and buffers (`connect_timeout_sec`, `replica_acquire_timeout_sec`,
`replica_query_timeout_sec`, `replica_buffer_kb`, `client_idle_timeout_sec`,
`auth_timeout_sec`, `max_packet_mb`) are already right for the game server;
change them only with a reason, and see the comments in `bridge.cfg.example`.

**Encrypting the WAN hop to the primary** (recommended unless a VPN already
does it): set `use_tls` in `[primary]`. This needs `ssl` configured on the
primary (`ssl_cert`/`ssl_key` in its `my.cnf`); the bridge verifies the server
certificate and refuses to log in in clear text if TLS is missing:

```ini
[primary]
host = db-central.example.com
port = 3306
use_tls = true
# CA that signed the primary's certificate
tls_ca_file = /etc/mrpg/primary-ca.pem
# only if the certificate's name differs from host
#tls_server_name = db-central.example.com
```

**Secrets.** The bridge's own credentials are only the `[replica]` ones; the
game server logs in with its own account, which the primary checks. Keep the
replica password out of the file with `password_file =
/etc/mrpg/bridge_replica_password` (or `BRIDGE_REPLICA_PASSWORD_FILE`), or set
it as `BRIDGE_REPLICA_PASSWORD`. The other supported variables are
`BRIDGE_LISTEN_HOST`, `BRIDGE_LISTEN_PORT`, `BRIDGE_PRIMARY_HOST`,
`BRIDGE_PRIMARY_PORT`, `BRIDGE_REPLICA_HOST`, `BRIDGE_REPLICA_PORT`,
`BRIDGE_REPLICA_USER` and `BRIDGE_LOG_LEVEL`; they override the file.

**Validate before starting** (exit code 0 = OK, 2 = invalid):

```bash
python3 mysql_bridge.py bridge.cfg --check-config
```

**No replica at this location yet?** Point `[replica]` at the primary (its
host/port, plus an account with `SELECT` on `mrpg_shared`, e.g. `mrpg_bridge`)
— the bridge then works as a plain proxy, and you can add the replica later.
Keep `database = mrpg_shared` in that case as well.

### 3.3 Run it

```bash
python3 mysql_bridge.py bridge.cfg
```

(The config path argument is optional; it defaults to `bridge.cfg` next to the
script.) The log shows `bridge 2.0 listening on 127.0.0.1:33060 ...` plus a
`stats replica=... primary=... fallback=... pool=n/m replica=up` line every 30
seconds. `SIGTERM`/`SIGINT` shut it down cleanly.

Run it as a service (example systemd unit, adjust the install path):

```bash
sudo useradd -r -s /usr/sbin/nologin mrpg
sudo chown -R mrpg:mrpg /opt/teeworlds-mrpg/other/sql_bridge
sudo tee /etc/systemd/system/mrpg-mysql-bridge.service >/dev/null <<'UNIT'
[Unit]
Description=Teeworlds-MRPG MySQL bridge
After=network-online.target mariadb.service
Wants=network-online.target

[Service]
Type=simple
User=mrpg
WorkingDirectory=/opt/teeworlds-mrpg/other/sql_bridge
ExecStart=/usr/bin/python3 /opt/teeworlds-mrpg/other/sql_bridge/mysql_bridge.py /opt/teeworlds-mrpg/other/sql_bridge/bridge.cfg
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
UNIT
sudo systemctl daemon-reload
sudo systemctl enable --now mrpg-mysql-bridge
sudo journalctl -u mrpg-mysql-bridge -f
```

---

## Step 4 — Point the game server at the bridge

In `autoexec_server.cfg` (or `custom.cfg`) of each game server:

```ini
#sql
sv_sql_host "127.0.0.1"
sv_sql_port 33060
sv_sql_login "mrpg_user"
sv_sql_password "<GAME_PASSWORD>"
sv_sql_database_shared "mrpg_shared"
sv_sql_database_local "mrpg_local_eu"
sv_sql_pool_size 8
sv_sql_use_tls 0
```

> **Important:** the bridge forwards the login to the **central primary**, so
> the game server must use the primary's account (`mrpg_user` from step 1.4) —
> **not** `mrpg_bridge` (that one is the bridge's own read-only replica
> connection). `sv_sql_password` is limited to 32 characters.

> **Important:** `sv_sql_database_local` is the only line that differs
> between the game servers: each one names its own local database
> (`mrpg_local_eu`, `mrpg_local_us`, ...). Two game servers on the same local
> database overwrite each other's guilds, groups, auction and house owners.
> The game server does not start when its local database is missing or
> incomplete. The database exists on the primary only; the bridge sends all of
> its queries there ([why](#what-is-replicated-and-what-is-not)).

`sv_sql_pool_size` is the number of worker connections for both databases
together; the game server divides it, the shared database gets the larger
part (8 = 6 to the shared database + 2 to the local one).

If the bridge runs on a different host than the game server, set
`sv_sql_host` to the bridge host's LAN IP and `listen_host` in `bridge.cfg`
accordingly (firewall 33060 to the game-server host only).

---

## Step 5 — Verify end-to-end

```bash
# through the bridge (on the bridge host):
mariadb -h 127.0.0.1 -P 33060 -u mrpg_user -p mrpg_shared \
  -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='mrpg_shared';"

# local database of this game server, through the bridge too (expect 10 tables,
# and none of them in the shared database; the bridge serves it from the
# primary, because the replica does not have it):
mariadb -h 127.0.0.1 -P 33060 -u mrpg_user -p mrpg_local_eu -e "SHOW TABLES;"

# replica health:
mariadb -u root -p -e "SHOW SLAVE STATUS\G" | grep -E "Slave_IO_Running|Slave_SQL_Running|Seconds_Behind"

# bridge log: client connects and per-30s stats
journalctl -u mrpg-mysql-bridge -f
```

Finally start the game server and check its log for a successful SQL
connection. It prints the databases it uses and how the pool is divided, for
example:

```
Shared database 'mrpg_shared', local database 'mrpg_local_eu' (127.0.0.1:33060).
The pool is divided: 6 connections to the shared database (40 tables), 2 to the local one (10 tables).
```

---

## Adding a game server later

A new game server needs a new local database. Create it on the **central
primary** only; replication never carries it (step 2.1), so no replica needs a
change:

```bash
mariadb -u root -p -e "CREATE DATABASE IF NOT EXISTS mrpg_local_us CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
mariadb -u root -p mrpg_local_us < mrpg_local.sql
mariadb -u root -p <<'SQL'
SET sql_log_bin = 0;
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX ON mrpg_local_us.* TO 'mrpg_user'@'%';
SQL
```

* The `CREATE DATABASE` and the tables do enter the binary log, and the
  replicas skip them through `replicate_wild_ignore_table = mrpg_local%.%`
  (step 2.1). Their `SHOW DATABASES` keeps showing `mrpg_shared` only.
* `SET sql_log_bin = 0` keeps the `GRANT` out of the binary log: the account
  `mrpg_user` exists on the primary only (step 2.4), so a replicated `GRANT`
  would fail on the replicas and stop replication.
* Nothing is granted to `mrpg_bridge`: it reads `mrpg_shared` from the replica
  and needs no access to a database that is not there.
* A local database named outside the `mrpg_local` prefix needs an extra value
  of the rule on **every** replica — see
  [Excluding a local database from replication later](#excluding-a-local-database-from-replication-later).

A new location also needs its own replica and bridge (steps 2 and 3); the
snapshot of step 2.2 still covers `mrpg_shared` only. Finish with step 4.

---

## Upgrading a single database

Older versions kept all tables in one replicated database (`mmotee` in the
older version of this guide). That database becomes the shared one and may
keep its name: set `sv_sql_database_shared` to it (the setting was called
`sv_sql_database`), leave the `GRANT`s and `database` in `bridge.cfg` as they
are, and read `mrpg_shared` in this guide as the name of your database. Only
the local tables move out, into one new local database per game server — and
only the shared database stays replicated.

1. Stop every game server and make a backup of the database on the
   **central primary**.
2. On the **central primary** create the local databases. The local tables
   (guilds, groups, auction, owners of houses) existed once, so choose the
   game server that keeps their rows:
   * for that game server do not import `mrpg_local.sql`; fill its database
     with the statements of "Moving an existing database to two databases" in
     [`docs/mariadb.md`](../../docs/mariadb.md), with its name in place of
     `mrpg_local`. The final `DROP`/`ALTER` part belongs to this step: a game
     server looks for the table of a query in the shared database first, so
     it does not start while the shared database still has the local tables;
   * for every other game server create the database from `mrpg_local.sql`
     (first two commands of [Adding a game server later](#adding-a-game-server-later)).
     The same rows for several game servers would duplicate the auction slots
     and the guild banks.

   The new local databases stay on the primary: the replicas skip their
   creation and all their tables (step 2.1). Replication does carry one part
   of this step — the removal of the old local tables from the shared
   database — because that is a change of `mrpg_shared`.
3. Give `mrpg_user` access to every new database: the `GRANT` of
   [Adding a game server later](#adding-a-game-server-later), with
   `SET sql_log_bin = 0` on the primary. No `GRANT` for `mrpg_bridge` is
   needed anywhere: the local databases are not on the replicas.
4. Wait until every replica has applied the changes to the shared database
   (step 2.5): a game server reads the list of tables through the bridge,
   from the replica. Its `SHOW DATABASES` must still show `mrpg_shared` only.
5. Replace `sv_sql_database` by `sv_sql_database_shared` and add
   `sv_sql_database_local` in the configuration of every game server
   (step 4), start the game servers and check their logs.


---

## Troubleshooting

* **"Access denied" through the bridge** — the account must exist on the
  *primary* (login is relayed there); check `mrpg_user` and its host mask.
* **`replica ... unavailable` / `fallback` in the bridge log** — the local
  replica is unreachable, rejects `mrpg_bridge`, or returned an error; the
  bridge automatically uses the primary (retrying the replica with backoff)
  so the game keeps running. The log line says which.
* **`client ... requested TLS`** — set `sv_sql_use_tls 0` on the game server;
  the bridge does not terminate TLS (it encrypts towards the primary itself
  with `[primary] use_tls`).
* **`primary ... does not offer TLS but use_tls = true`** — configure `ssl`
  on the primary, or set `use_tls = false` if a VPN encrypts the link.
* **`replica cannot serve a session profile`** — the replica lacks a protocol
  feature or rejected a session `SET` the game server uses; those reads use
  the primary. Usually a replica on a much older MariaDB version.
* **`replica lag check failed`** — grant `REPLICA MONITOR` (step 2.4) or set
  `max_replica_lag_sec = 0`.
* **Reads look stale right after a write** — raise
  `read_your_writes_grace_sec` above your replication lag; see
  [Consistency](#consistency).
* **Database incident / debugging** — set `route_selects_to_replica = false`
  to route all traffic to the primary, and/or `BRIDGE_LOG_LEVEL=DEBUG`.
* **Replication broken** — `SHOW SLAVE STATUS\G` on the replica shows
  `Last_IO_Error` / `Last_SQL_Error`; fix, then `START SLAVE;` again.
* **Game server exits with `The local database '...' is unavailable` or
  `Tables found neither in the shared database '...' nor in the local
  database '...'`** — it does not start without its local database. The
  database named by `sv_sql_database_local` must exist on the **primary**
  with the tables of `mrpg_local.sql` (step 1.3), and `mrpg_user` needs the
  `GRANT` of step 1.4 on it.
* **Game server exits with `Tables found in both databases '...' and
  '...'`** — the shared database still has tables of the local one, and the
  game server would use them instead of the local ones. Remove them from the
  shared database on the **primary**
  ([Upgrading a single database](#upgrading-a-single-database)) and wait
  until the replica has applied it (step 2.5).
* **`replica cannot use database 'mrpg_local_...'` in the bridge log** —
  expected: the replica has no local database
  ([by design](#what-is-replicated-and-what-is-not)), so the bridge sends the
  reads of that database to the primary and repeats the probe at most every
  30 s. The game keeps running; these reads cross the WAN. If the message
  names `mrpg_shared` instead, the replica is missing its copy or
  `mrpg_bridge` lacks `SELECT` (steps 2.2 and 2.4) — that one is a real
  problem.
* **A local database appears in `SHOW DATABASES` on a replica** — the replica
  has a stale copy from a setup without the exclusion rule. The game never
  reads it, but it must be removed:
  [Excluding a local database from replication later](#excluding-a-local-database-from-replication-later).

## How routing works

Only statements that are certain to be side-effect free and session
independent go to the replica:

| Goes to the **replica** | Always goes to the **primary** |
|---|---|
| `SELECT ...` / `WITH ... SELECT` / `(SELECT ...)` | every write, DDL, `CALL`, `DO`, `LOAD DATA` |
| `SHOW TABLES / COLUMNS / INDEX / CREATE TABLE / TABLE STATUS ...` | `SELECT ... FOR UPDATE / LOCK IN SHARE MODE / FOR SHARE` |
| `DESCRIBE` / `EXPLAIN SELECT` | `LAST_INSERT_ID()`, `FOUND_ROWS()`, `ROW_COUNT()`, `GET_LOCK()`, sequences, `@variables`, `INTO`, `SHOW WARNINGS/VARIABLES/STATUS/GRANTS/PROCESSLIST ...` |
| | multi-statements (`SELECT 1; DELETE ...`), executable comments (`/*! ... */`), anything unparsable |
| | everything inside a transaction or with `autocommit = 0`, after `LOCK TABLES` or `CREATE TEMPORARY TABLE`, and prepared statements |

String literals and comments are stripped before classification, so a
keyword inside a value (`WHERE name = 'for update'`) does not change the
route, and literals are never written to the debug log.

Routing depends on the statement, not on the database name. A read of a
database the replica does not have — a local database — is answered there
with an error, and the bridge immediately re-runs it on the primary. It
remembers that database for 30 s and skips the replica for it meanwhile, so at
most one read per 30 s pays for the probe. This is also why a replica needs no
copy of any local database.

If the replica is down, slow (`replica_query_timeout_sec`), out of pool
connections (`replica_acquire_timeout_sec`), lagging (`max_replica_lag_sec`),
or answers with an error, the query is transparently re-run on the primary.
Responses up to `replica_buffer_kb` are buffered for that purpose; if a
replica dies in the middle of streaming a bigger response, that one client
connection is closed (the game server's auto-reconnect retries).

The game server's session settings (`SET NAMES`, `SET sql_mode`, the
character set, the current database from `USE`) are mirrored on the replica
connections, so results are byte-for-byte what the primary would send.

### Consistency

Replication is asynchronous, so a row written on the primary appears on the
replica a little later. The bridge hides this within a location:

* **Same connection:** after a write, that connection reads from the primary
  for `read_your_writes_grace_sec`.
* **Other connections of the same bridge** (`read_your_writes_scope = table`,
  default): reads that mention a table written within the window also go to
  the primary. This matters for MRPG because its SQL worker pool may write on
  one connection and read back on another (e.g. account registration inserts
  into `tw_accounts`, then selects the new ID). `global` sends all reads to
  the primary during the window; `connection` restores the pre-2.0
  behaviour.
* **Writes inside a transaction** start the window at `COMMIT`.

Not covered: a write made through **another location's** bridge (or directly
on the primary) is only visible here once replication catches up. Keep
`read_your_writes_grace_sec` above the normal `Seconds_Behind_Master` and use
`max_replica_lag_sec` so a lagging replica is bypassed entirely. Gameplay
that requires cross-location read-after-write (e.g. the same account logging
in at two locations within a second) needs the primary.

## Testing

The test suite (standard library only) runs the bridge between a strict test
client and fake MariaDB servers and covers routing, protocol framing,
failover, pool behaviour, TLS, timeouts and the security limits:

```bash
cd other/sql_bridge
python3 -m unittest discover -s tests -v
```

If `pymysql`, `mariadb`, `mysql-connector-python` or `mysql-mimic` are
installed, interoperability tests with those clients/servers run too.
`tests/bench_bridge.py` is a micro-benchmark (`--compare old_bridge.py` to
compare two versions on the same machine).

## Security checklist

* `bridge.cfg` contains a password — keep `chmod 600` (the bridge warns
  otherwise), or use `password_file` / the `BRIDGE_REPLICA_*` environment
  variables.
* 33060 (bridge) and the replica's 3306 stay on `127.0.0.1` (or LAN +
  firewall, plus `allowed_clients`) — never on the internet. The game
  server ↔ bridge hop is not encrypted.
* `mrpg_bridge` has only `SELECT` on `mrpg_shared` (+ optional
  `REPLICA MONITOR`); the local databases are neither on the replica nor
  readable by it.
* Enable `[primary] use_tls` with a `tls_ca_file` when the WAN link is not
  already encrypted by a VPN.
* Unauthenticated connections never reach the replica: the game server's
  login is checked by the primary first, and `auth_timeout_sec` /
  `max_clients` bound what an unauthenticated peer can hold open.
* Central 3306 reachable only via the VPN/private network; restrict the
  `@'%'` account masks to concrete IPs where possible.
* All location-to-location traffic goes through your verified TLS
  tunnel/VPN; `MASTER_SSL=1` additionally encrypts the replication stream.
