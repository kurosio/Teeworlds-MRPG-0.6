# mysql_bridge.py — central database + replica guide

One central MariaDB, game servers in several locations. Each location runs a
local **read replica** plus the bridge: reads of the shared database are local,
everything else goes to the primary over the WAN.

```
game server (A) --\
game server (B) ---+--> local bridge --> [local replica] --\
game server (C) --/    (mysql_bridge.py)                    +--> replication --> CENTRAL PRIMARY
                                                            --/
```

The bridge is a stdlib asyncio MySQL/MariaDB proxy for the shared database
only: plain `SELECT`s go to the replica, everything else (writes, DDL,
`SHOW`, transactions, anything uncertain) to the primary; see
[How routing works](#how-routing-works).

The game server has two connections and picks one per query by the table it
touches: `mrpg_shared` tables go through the bridge, `mrpg_local` tables go
straight to the local MariaDB. The bridge never sees the local database.

**Files:** `mysql_bridge.py` (the bridge), `bridge.cfg.example` (template),
`replica_setup.sql` (primary accounts), `tests/` (tests, fake server, benchmark).

**Ports:** 3306 on the primary — replicas and bridges; 3306 on a replica —
its local bridge only; 33060 on a bridge — the local game server only.

**Before you start**

* Debian/Ubuntu, MariaDB 10.6+ (tuning: [`docs/mariadb.md`](../../docs/mariadb.md));
  `mariadb`/`mariadb-dump` are `mysql`/`mysqldump` on older installs.
* `mrpg_shared` is the name used throughout; another name must be changed in
  the dump import, the `GRANT`s, `bridge.cfg` and `sv_sql_database_shared`.
* Replace `<PLACEHOLDER>`s; no `'` or `\` in passwords; unique strong ones.
* TLS: the bridge encrypts primary/replica hops (`use_tls = true`), **not** the
  game server ↔ bridge hop (`sv_sql_use_tls 0`, localhost/LAN). Replication
  uses `MASTER_SSL=1`; a VPN between locations is still recommended.

---

## Quick automated setup (`setup_replication.sh`)

The steps below (Steps 1–2) can be automated on a fresh Debian/Ubuntu host with
`setup_replication.sh` in this directory. The script takes no arguments — it is
fully interactive: pick what to set up in the menu (`1` = master, `2` =
replica, `3` = status, `4` = reset), then answer the questions one by one (Enter accepts
the value in `[brackets]`). Before changing anything it shows the plan and
asks to confirm: `Y` = apply, `t` = print the commands without running them
(dry-run, no root needed), `n` = abort. If replication does not come up, the
verification step prints the relevant `SHOW SLAVE STATUS` fields (`Last_IO_Error` /
`Last_SQL_Error`) instead of a bare warning. Menu option `4` (reset) undoes the
setup on the local host: stops/resets replication, moves the configs aside to
`*.removed.<date>` (nothing is deleted silently), and optionally drops the
accounts and databases the script created — dropping the shared database
requires typing `yes` explicitly.

```bash
# on the central primary (master) — step 1: choose 1 in the menu,
# answer the questions (server_id, local-DBs for the game account, ...)
sudo sh setup_replication.sh

# on each location (replica) — step 2: choose 2, enter the primary host,
# the mrpg_replica password from the master output; the snapshot is either
# a ready file on the host or pulled over SSH (sudo mariadb-dump on the
# primary) by the script itself
sudo sh setup_replication.sh

# check on either host: choose 3
sudo sh setup_replication.sh
```

SSH for the snapshot is interactive: a password/passphrase is asked on the
terminal, and the primary's host key is added automatically on first use
(`StrictHostKeyChecking=accept-new` — a changed key is still rejected; fix it
with `ssh-keygen -R <primary-host>`). Password login as root is often disabled
(`PermitRootLogin prohibit-password`) — use an ssh key
(`ssh-keygen && ssh-copy-id root@<primary-host>`).

Passwords are asked without echo; an empty answer generates a random one
(except the `mrpg_replica` password on a replica — that one comes from the
master). Generated passwords are saved to `/root/mrpg-replication-<date>.cred`
(mode 600). The script is idempotent: old configs are backed up next to the
new ones, an existing dump is not re-imported into a non-empty database
(it asks first), and accounts are created with `CREATE USER IF NOT EXISTS`.
On the master it also verifies after the restart that the requested
`server_id` and `log_bin = ON` actually took effect (so a config that was
not loaded — e.g. an `.cnf` written into the wrong directory or an include
order problem — stops the run instead of producing a silently broken
replication setup), and `RESET SLAVE`/`CHANGE MASTER` failures abort with a
clear error instead of continuing half-configured. Temp snapshot files are
removed even on Ctrl+C, and the SSH pull is interactive from the real
terminal (falling back to `/dev/null` stdin when there is none).

The replica flow also sets up the **local** side: it optionally creates the
per-server database `mrpg_local_<location>` (e.g. `mrpg_local_eu`), imports
`mrpg_local.sql`, verifies the table count (expected 10), and creates a
dedicated account for the game server's `sv_sql_local_*` connection (default
`mrpg_local@127.0.0.1`). It receives `ALL PRIVILEGES` on its own local
database and MariaDB's `READ ONLY ADMIN` privilege: the latter is needed to
write local state while the replica keeps `read_only = 1`; it does not grant
access to `mrpg_shared`. This connection is direct to `127.0.0.1:3306`,
never through the bridge. Menu option `4` (reset) can drop that account and
database too, with the same typed-`yes` safeguard for data.

After the replication steps are applied, the replica flow additionally offers:

* The post-setup steps take their **defaults from what was created earlier in
  the same run** (the local `mrpg_local_*` database/user for `sv_sql_local_*`,
  the `mrpg_bridge` account and password for `bridge.cfg`, the entered primary
  host/port). When the local side was not created in this run, the step offers
  the `mrpg_local%` databases/users it finds on this host instead of a guessed
  name, and `mysql_bridge.py` defaults to the directory the script itself
  lives in (where it ships in this repository). Config files are written
  atomically (temp file → rename, the previous file is backed up first), and
  the `bridge.cfg` is validated with `mysql_bridge.py --check-config` and with
  an actual TCP login to the local replica — if the password or grants are
  wrong, the script says so and offers to fix them instead of leaving the
  bridge to fail at startup.
* **Step 4 (game server):** asks for the directory containing
  `autoexec_server.cfg` (e.g. `~/.teeworlds` — it must already exist and contain
  the file). It removes **every** old `sv_sql*` line (commented-out ones too)
  and the `#sql` marker, backs up the file to `autoexec_server.cfg.bak.<date>`,
  and appends a fresh `#sql` block: `sv_sql_*` → the bridge on `127.0.0.1:33060`,
  `sv_sql_local_*` → the local MariaDB on `127.0.0.1:3306` with the local
  database created in this run.
* **Step 3 (bridge):** asks for the directory containing `mysql_bridge.py`
  (e.g. `~/Teeworlds-MRPG-0.6/other/sql_bridge`) and writes `bridge.cfg` there
  (chmod 600, backs up a previous one to `bridge.cfg.bak.<date>`): listen
  `127.0.0.1:33060`, `[primary]` = the values you entered, `[replica]` =
  `mrpg_bridge` with the local replica `127.0.0.1:3306`. If the file validates,
  the script runs `python3 mysql_bridge.py bridge.cfg --check-config`. `bridge.cfg`
  is git-ignored — never commit it.
* **Systemd unit:** offers to install the bridge as a service
  (`/etc/systemd/system/mrpg-mysql-bridge.service`, unit `mrpg-mysql-bridge`,
  user `mrpg` — created with `useradd -r -s /usr/sbin/nologin mrpg` if missing),
  then asks separately whether to **enable it on boot** and whether to **start it
  now**. When the bridge runs as the `mrpg` user, the script grants only
  `bridge.cfg` itself to it (not the whole directory). If `systemctl` is
  missing or any answer is `n`, the unit is skipped and
  the manual example in step 3 below applies. If the service fails to start,
  `journalctl -xeu mrpg-mysql-bridge` has the reason (the sandbox-friendly
  checks below pinpoint the usual one: the replica login).

  **Final verification (step 4):** before "Replica is ready" is printed the
  script re-runs, as usable proof, the exact connections the game server
  makes — `SHOW SLAVE STATUS` (IO/SQL threads), a TCP login with the
  `sv_sql_local_*` account against the local MariaDB, and a login with the
  `sv_sql_*` account **through the bridge** (which the bridge relays to the
  primary). It prints a per-item ✅/⚠ list and either **"The server is ready
to work."** or **"The server is NOT fully ready"** with the cause and the
  exact fix for each failed item (wrong `mrpg_user` password, primary
  unreachable, missing grants, …). A green checkmark there is the same test
  the game server would run at startup.

**Which `MASTER_USE_GTID` to use — `CURRENT_POS` or `slave_pos`?**
For a snapshot provisioned by this script use **`MASTER_USE_GTID=slave_pos`**.
`mariadb-dump --master-data=2 --gtid` records the primary point as
`SET GLOBAL gtid_slave_pos='…'`, but comments that statement in the dump to
avoid changing replication settings implicitly on import. The script extracts
that exact position, applies it explicitly after importing the fresh snapshot,
and then starts in `slave_pos` mode. The position is persisted in the
InnoDB-backed `mysql.gtid_slave_pos` table and survives a restart.

`CURRENT_POS` is only for the exceptional case where no snapshot is imported
and the administrator already knows the replica's data and GTID state exactly
match the primary. Do not use it to recover a replica after error 1236: it can
reuse the stale pre-import position and request a purged binlog.

**`Last_IO_Error` 1236 — "connecting slave requested to start from GTID … which
is not in the master's binlog".** The replica asked the primary for a GTID that
no longer exists in its binlog: the snapshot is **stale** (the binlog with that
GTID was purged by `expire_logs_days`/`PURGE BINARY LOGS`, or the dump came from
a different or rebuilt primary). No position "skip" is safe — the replica would
silently miss the transactions in the gap. The script detects this exact error
after `START SLAVE` (and on every later config run it prints the full
`Last_IO_Error`/`Last_SQL_Error`) and offers to **re-provision with a fresh
snapshot**: drop `mrpg_shared`, pull a brand-new dump over SSH, reimport,
explicitly set the snapshot's `gtid_slave_pos`, then re-run `CHANGE MASTER` +
`START SLAVE` in `slave_pos` mode. Before importing any snapshot the script
also checks it records a GTID position at all (a plain dump without `--gtid`
cannot provision GTID replication) and **verifies the dump against the
primary's current position over SSH** (it asks the primary for
`@@global.gtid_binlog_pos` and compares component by component): a dump whose
position **ahead of or foreign to** that server (e.g. one taken from a
different/rebuilt primary) is refused outright; a position **behind** it may be
purged and gets a warning — take a fresh dump (snapshot option `2`) in that
case. To compare by hand on the primary: `mariadb -e "SELECT @@global.gtid_binlog_pos"`
vs the dump's `SET GLOBAL gtid_slave_pos='…'` line. The line is normally
commented because the dump uses `--master-data=2`, which is why the script
applies it explicitly. To recover: answer `y` to the script's offer (or menu
`4` reset → menu `2` with snapshot option `2` — SSH pull of a fresh dump,
never the stale file).

**Fresh import always starts from a clean `mrpg_shared`.** Before importing a
snapshot the script drops and re-creates `mrpg_shared` on the replica (asks
first, default `y` — the snapshot is the source of truth; if replication is
currently running, it requires typing `yes`). The replica copy is by
definition stale; after the import the script explicitly sets the dump's
`gtid_slave_pos` as the start position. This leaves an empty database with no
`ERROR 1050`, duplicated rows, or stale pre-import GTID position.
Declining keeps the old behavior (the import then asks about recreating the
non-empty database). Drops run with `SET sql_log_bin = 0` only when this
server actually has binary logging (binlog-less replicas run a plain
`DROP DATABASE`).

The SSH snapshot download prints progress every 15 seconds (`… downloaded N MB
so far`) — a multi-GB dump is a long silent transfer otherwise. If the local
dump import fails (e.g. `ERROR 1050 (42S01): Table 'tw_auction_slots' already
exists` in a `mrpg_local_<location>` that already has tables), the script
offers to **drop and recreate** the database and re-import it — you must
type `yes` to confirm; declining keeps the database untouched. The same
drop-and-recreate offer is available for the **shared** database import on
the master (step 1) and after a failed re-import.

---

## What is replicated

| Database | Holds | Primary | Replica |
|---|---|---|---|
| `mrpg_shared` | content, accounts, records | yes | yes — **the only replicated DB** |
| `mrpg_local_<location>` | state of one server: guilds, groups, auction, house owners | yes | **no** |

* Only `mrpg_shared` is copied. Every replica excludes local databases with
  `replicate_wild_ignore_table = mrpg_local%.%` (step 2.1); snapshots contain
  `mrpg_shared` only (step 2.2). Replication never creates or changes a local
  database: `setup_replication.sh` (or the administrator) creates the one for
  this location. Only a local schema copied from the primary before the filter
  is stale; remove it as described [below](#excluding-a-local-database-from-replication-later).
* The game server opens two connections: `sv_sql_*` for the shared database
  (through the bridge, login relayed to the primary) and `sv_sql_local_*` for
  the local one (direct, no bridge, no replica). A query names its table, and
  the table decides the connection; a statement never mixes tables of both.
* One local database per game server, named `mrpg_local_<location>` (keep the
  prefix); two servers must never share one. It lives on the game server's own
  MariaDB; if you keep it on the primary instead, the ignore rule of step 2.1
  keeps it out of the replicas (as before).

---

## Step 0 — Prerequisites

One central MariaDB host reachable over a private network/VPN; per location one
host for replica + bridge (may be the game-server host); Python 3.8+ on bridge
hosts (no packages needed).

---

## Step 1 — Central primary database

### 1.1 Install MariaDB

```bash
sudo apt update
sudo apt install -y mariadb-server
sudo mysql_secure_installation
```

### 1.2 Binary logging (required for replication)

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

Apply the tuning from [`docs/mariadb.md`](../../docs/mariadb.md) and set
`bind-address` to the private/VPN address.

### 1.3 Create the databases

```bash
mariadb -u root -p -e "CREATE DATABASE IF NOT EXISTS mrpg_shared CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
mariadb -u root -p mrpg_shared < mrpg_shared.sql
```

`mrpg_local.sql` goes into `mrpg_local_<location>` on each game server (step 4);
import it on the primary too if you want to keep the local databases there.

### 1.4 Accounts

Set real passwords in `replica_setup.sql` (it creates `mrpg_replica` for the
replication stream and `mrpg_bridge` for read connections) and run it on the
primary:

```bash
cd other/sql_bridge
cp replica_setup.sql replica_setup.local.sql
sed -i "/mrpg_replica/s/'changeme'/'<REPLICA_REPL_PASSWORD>'/" replica_setup.local.sql
sed -i "/mrpg_bridge/s/'changeme'/'<BRIDGE_READ_PASSWORD>'/"   replica_setup.local.sql
mariadb -u root -p < replica_setup.local.sql
```

The game server logs in through the bridge for the **shared** database, so the
primary needs this account only for `mrpg_shared`:

```bash
mariadb -u root -p <<'SQL'
CREATE USER IF NOT EXISTS 'mrpg_user'@'%' IDENTIFIED BY '<GAME_PASSWORD>';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX ON mrpg_shared.* TO 'mrpg_user'@'%';
FLUSH PRIVILEGES;
SQL
```

The per-location local account is created on that location's MariaDB in step
2.5; it is `mrpg_local`, not `mrpg_user`, and never connects through the
bridge. In production, replace `@'%'` with the VPN IPs of your bridges and
replicas.

### 1.5 Firewall

Allow 3306 from the private network only; never expose it to the internet:

```bash
sudo ufw allow from 10.0.0.0/24 to any port 3306 proto tcp
```

---

## Step 2 — Replica at each location

Repeat this step once per location.

### 2.1 Install and configure MariaDB

```bash
sudo apt install -y mariadb-server
sudo tee /etc/mysql/mariadb.conf.d/61-mrpg-replica.cnf >/dev/null <<'CNF'
[mysqld]
server_id = 2                       # unique per replica: 2, 3, 4, ...
bind-address = 127.0.0.1            # only the local bridge needs it
read_only = 1                       # the bridge never writes to a replica
gtid_strict_mode = 1
# replication is shared-only: keep the local databases out
replicate_wild_ignore_table = mrpg_local%.%
CNF
sudo systemctl restart mariadb
```

* `mrpg_local%.%` covers every table of every database starting with
  `mrpg_local` (`LIKE` wildcards: `%` any text, `_` one character).
* The table part is exactly `%`, so `CREATE/ALTER/DROP DATABASE` of matching
  databases is filtered too: replication cannot create even an empty local
  database. Create the per-location database locally instead. Do not narrow
  the pattern (`mrpg_local%.tw_%` would let it through).
* Skipped events still advance the GTID position, so local changes never
  break replication. Put the rule on replicas only, never on the primary.
* One line per value in `my.cnf` (no comma list); another database name needs
  its own line. Routines/events are not filtered — keep them out of local DBs.

### 2.2 Snapshot the primary

On the **primary**, preferably at low traffic (`--single-transaction` keeps
InnoDB consistent; `--gtid` stamps the dump with the GTID position):

```bash
# MariaDB 10.x: the option is called --master-data (= --source-data on MySQL 8);
# either works for this flow (it only stamps binlog coordinates as a comment)
mariadb-dump -u root -p --single-transaction --quick --routines --triggers --events \
  --master-data=2 --gtid mrpg_shared > mrpg_shared_snapshot.sql
```

`setup_replication.sh` auto-detects which of the two spellings your dump
supports when it pulls the snapshot over SSH.

Copy the dump to the replica host, then on the **replica** reset the old
connection, import into a clean shared database, and apply the position the
dump recorded. With `--master-data=2` that `SET GLOBAL gtid_slave_pos` line is
commented in the dump, so importing it does **not** set the position itself:

```bash
DUMP=mrpg_shared_snapshot.sql
DUMP_GTID=$(grep -aiEo "gtid_slave_pos[[:space:]]*=[[:space:]]*'[^']*'" "$DUMP" | tail -1 | sed -e "s/^.*'\([^']*\)'.*$/\1/")
test -n "$DUMP_GTID" || { echo "Dump has no GTID position; recreate it with --gtid" >&2; exit 1; }

mariadb -u root -p <<'SQL'
STOP SLAVE;
RESET SLAVE ALL;
SET SESSION foreign_key_checks = 0;
DROP DATABASE IF EXISTS mrpg_shared;
CREATE DATABASE mrpg_shared CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
SET SESSION foreign_key_checks = 1;
SQL
mariadb -u root -p mrpg_shared < "$DUMP"
mariadb -u root -p -e "SET GLOBAL gtid_slave_pos = '$DUMP_GTID';"
```

Only `mrpg_shared` is dumped: the replica ignores local databases and must
not get a copy.

### 2.3 Start replication

On the **replica** (values from step 1.4):

```bash
mariadb -u root -p <<'SQL'
CHANGE MASTER TO
  MASTER_HOST='db-central.example.com',
  MASTER_PORT=3306,
  MASTER_USER='mrpg_replica',
  MASTER_PASSWORD='<REPLICA_REPL_PASSWORD>',
  MASTER_USE_GTID=slave_pos,
  MASTER_SSL=1;
START SLAVE;
SQL
```

### 2.4 Bridge read account

The snapshot starts at the current GTID position, so the accounts of step 1.4
are not in it. On the **replica**:

```bash
mariadb -u root -p <<'SQL'
CREATE USER IF NOT EXISTS 'mrpg_bridge'@'127.0.0.1' IDENTIFIED BY '<BRIDGE_READ_PASSWORD>';
GRANT SELECT ON mrpg_shared.* TO 'mrpg_bridge'@'127.0.0.1';
FLUSH PRIVILEGES;
SQL
```

`mrpg_shared` only — that is all the bridge needs (it never touches another
database and does not read the replication status).

### 2.5 Direct local game database and account

The local game state lives on this location host. It must be reached directly
by the game server, not by `mysql_bridge.py`. On the **replica/location host**:

```bash
mariadb -u root -p <<'SQL'
CREATE DATABASE IF NOT EXISTS mrpg_local_eu CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS 'mrpg_local'@'127.0.0.1' IDENTIFIED BY '<LOCAL_GAME_PASSWORD>';
GRANT ALL PRIVILEGES ON mrpg_local_eu.* TO 'mrpg_local'@'127.0.0.1';
-- read_only=1 protects mrpg_shared; this permits writes only where the
-- account has schema privileges, namely the local database above.
GRANT READ ONLY ADMIN ON *.* TO 'mrpg_local'@'127.0.0.1';
FLUSH PRIVILEGES;
SQL
mariadb -u root -p mrpg_local_eu < mrpg_local.sql
```

`READ ONLY ADMIN` is required because a normal account receives MariaDB error
1290 for an `INSERT` while `read_only = 1` is set, even if it has `INSERT` on
the local schema. It bypasses the global read-only check; `ALL PRIVILEGES` is
still limited to `mrpg_local_eu.*`, so this account has no access to
`mrpg_shared`. `setup_replication.sh` performs these commands and also tests
an `INSERT` followed by `ROLLBACK` with the exact `sv_sql_local_*` account.

### 2.6 Verify

```bash
mariadb -u root -p -e "SHOW SLAVE STATUS\G" | grep -E "Slave_IO_Running|Slave_SQL_Running|Seconds_Behind|Replicate_Wild_Ignore_Table"
mariadb -u root -p -e "SHOW DATABASES LIKE 'mrpg%';"
```

Both threads are `Yes` and the ignore list shows `mrpg_local%.%`. The local
`mrpg_local_<location>` database is expected to appear because it was created
on this host; replication never imports or modifies it.

---

## Excluding a local database from replication later

The rule is per replica and is not replicated itself. Either edit the config
and restart:

```bash
sudo nano /etc/mysql/mariadb.conf.d/61-mrpg-replica.cnf
# under [mysqld]:
#   replicate_wild_ignore_table = mrpg_local%.%
sudo systemctl restart mariadb
```

or online (not persisted — still add the line):
`STOP SLAVE; SET GLOBAL replicate_wild_ignore_table = 'mrpg_local%.%'; START SLAVE;`
Other database names are appended (`'mrpg_local%.%,guilds_eu.%'`).

The rule only stops future events. A local database copied from the primary
before this rule was added is stale; drop that **copied** database on the
replica (the primary keeps its copy). Do not drop this location's deliberately
created `mrpg_local_<location>` database:

```bash
# Replace mrpg_local_copied with the stale schema's actual name, never this
# location's active mrpg_local_<location> database.
mariadb -u root -p <<'SQL'
STOP SLAVE;
DROP DATABASE IF EXISTS mrpg_local_copied;
START SLAVE;
SQL
mariadb -u root -p -e "SHOW DATABASES LIKE 'mrpg%';"   # mrpg_shared + this location's local DB
```

One-off primary statements that must not reach the replicas (e.g. the
`GRANT`s below) go in a session with `SET sql_log_bin = 0;`.

---

## Step 3 — Run the bridge at each location

### 3.1 Get the script

```bash
python3 --version            # needs 3.8+
pip3 install uvloop          # optional: 10-30% more throughput
```

### 3.2 Write `bridge.cfg`

```bash
cd other/sql_bridge
cp bridge.cfg.example bridge.cfg
chmod 600 bridge.cfg
nano bridge.cfg
```

Minimal file; everything else has a default:

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

`[primary]` has no user/password on purpose: the game server's login is
relayed and checked there. The bridge routes by statement only — plain
`SELECT`s to the replica, everything else to the primary. It serves
`mrpg_shared`; the local database does not go through it.

Key settings (defaults in brackets; the rest is commented in
`bridge.cfg.example`):

| Key | Default | Effect |
|---|---|---|
| `[bridge] max_clients` | 256 | max simultaneous game-server connections |
| `[bridge] allowed_clients` | all | source IP/CIDR allowlist; set it if `listen_host` is not loopback |
| `[bridge] replica_pool_size` | 16 | persistent replica connections; keep above the sum of all `sv_sql_pool_size` |
| `[bridge] route_selects_to_replica` | true | `false` = send everything to the primary (incident mode) |
| `[bridge] read_your_writes_grace_sec` | 0 | reads use the primary this long after any write; set above the lag, e.g. 2 |
| `[bridge] log_level` | `INFO` | `DEBUG` logs every routing decision |
| `[primary] use_tls` | false | TLS to the primary; with `tls_ca_file` the certificate is verified |

The template also sets `max_clients = 64` and
`read_your_writes_grace_sec = 2.0`. Timeouts and buffers
(`connect_timeout_sec`, `replica_acquire_timeout_sec`,
`replica_query_timeout_sec`, `replica_buffer_kb`, `client_idle_timeout_sec`,
`auth_timeout_sec`, `max_packet_mb`) are already right; change only with a
reason.

TLS to the primary: `use_tls = true` and
`tls_ca_file = /etc/mrpg/primary-ca.pem` in `[primary]` (plus
`tls_server_name` if it differs from `host`). Needs `ssl` on the primary; the
certificate is verified and clear-text login is refused.

Secrets: `password_file = /etc/mrpg/bridge_replica_password` (or
`BRIDGE_REPLICA_PASSWORD`). Overrides: `BRIDGE_LISTEN_HOST`,
`BRIDGE_LISTEN_PORT`, `BRIDGE_PRIMARY_HOST`, `BRIDGE_PRIMARY_PORT`,
`BRIDGE_REPLICA_HOST`, `BRIDGE_REPLICA_PORT`, `BRIDGE_REPLICA_USER`,
`BRIDGE_LOG_LEVEL`.

Validate (0 = OK, 2 = invalid):

```bash
python3 mysql_bridge.py bridge.cfg --check-config
```

**No replica yet?** Point `[replica]` at the primary with an account that has
`SELECT` on `mrpg_shared` (e.g. `mrpg_bridge`) — the bridge becomes a plain
proxy.

### 3.3 Run it

```bash
python3 mysql_bridge.py bridge.cfg
```

The config path is optional (default: `bridge.cfg` next to the script). The
log shows `bridge 3.0 listening on ...` and a `stats replica=... primary=...
fallback=... pool=n/m replica=up` line every 30 s; `SIGTERM`/`SIGINT` stop it
cleanly.

As a service:

```bash
sudo useradd -r -s /usr/sbin/nologin mrpg && sudo chown -R mrpg:mrpg /opt/teeworlds-mrpg/other/sql_bridge
sudo tee /etc/systemd/system/mrpg-mysql-bridge.service >/dev/null <<'UNIT'
[Unit]
Description=Teeworlds-MRPG MySQL bridge
After=network-online.target mariadb.service

[Service]
User=mrpg
WorkingDirectory=/opt/teeworlds-mrpg/other/sql_bridge
ExecStart=/usr/bin/python3 /opt/teeworlds-mrpg/other/sql_bridge/mysql_bridge.py /opt/teeworlds-mrpg/other/sql_bridge/bridge.cfg
Restart=always

[Install]
WantedBy=multi-user.target
UNIT
sudo systemctl daemon-reload
sudo systemctl enable --now mrpg-mysql-bridge
sudo journalctl -u mrpg-mysql-bridge -f
```

---

## Step 4 — Point the game server at the bridge

```ini
#sql
# shared database: through the bridge
sv_sql_host "127.0.0.1"
sv_sql_port 33060
sv_sql_login "mrpg_user"
sv_sql_password "<GAME_PASSWORD>"
sv_sql_database_shared "mrpg_shared"
sv_sql_pool_size 8
sv_sql_use_tls 0

# local database: direct game-server → local MariaDB, never the bridge
sv_sql_local_host "127.0.0.1"
sv_sql_local_port 3306
sv_sql_local_login "mrpg_local"
sv_sql_local_password "<LOCAL_GAME_PASSWORD>"
sv_sql_local_database "mrpg_local_eu"
sv_sql_local_pool_size 2
sv_sql_local_use_tls 0
```

* `mrpg_user`, not `mrpg_bridge`, is used for `sv_sql_*` (the shared login is
  relayed to the primary); `sv_sql_password` is limited to 32 characters.
  `sv_sql_local_login` is the independent local `mrpg_local` account.
* `sv_sql_pool_size` is the shared pool (through the bridge),
  `sv_sql_local_pool_size` the local one. Both count their own connections.
* `sv_sql_local_database` is the only per-server line; without a complete
  local database the server does not start. `sv_sql_local_host` must point at
  the MariaDB that holds it, **not** at the bridge.
* Empty `sv_sql_local_database` = no local database, everything uses the
  shared connection (single-database installs). Empty `sv_sql_local_host` =
  reuse the shared endpoint — fine only without a bridge, because the bridge
  does not serve the local database.
* Bridge on another host: `sv_sql_host` = its LAN IP, matching
  `listen_host`, firewall 33060 to the game-server host.

---

## Step 5 — Verify end-to-end

```bash
# shared database through the bridge
mariadb -h 127.0.0.1 -P 33060 -u mrpg_user -p mrpg_shared \
  -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='mrpg_shared';"

# local database directly on the local MariaDB (10 tables, none in mrpg_shared)
mariadb -h 127.0.0.1 -P 3306 -u mrpg_local -p mrpg_local_eu -e "SHOW TABLES;"

# replica health
mariadb -u root -p -e "SHOW SLAVE STATUS\G" | grep -E "Slave_IO_Running|Slave_SQL_Running|Seconds_Behind"

# bridge log
journalctl -u mrpg-mysql-bridge -f
```

Start the game server and check its log:

```
[SQL] Shared database 'mrpg_shared' (127.0.0.1:33060, 8 connections), local database 'mrpg_local_eu' (127.0.0.1:3306, 2 connections).
```

The server refuses to start when a database is unreachable, when a local
table is missing in the local database, or when one of them still sits in the
shared database.

---

## Adding a game server later

Create the local database on that server's own MariaDB — no replica, no
bridge and no primary change is needed:

```bash
mariadb -u root -p <<'SQL'
CREATE DATABASE IF NOT EXISTS mrpg_local_us CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS 'mrpg_local'@'127.0.0.1' IDENTIFIED BY '<LOCAL_GAME_PASSWORD>';
GRANT ALL PRIVILEGES ON mrpg_local_us.* TO 'mrpg_local'@'127.0.0.1';
GRANT READ ONLY ADMIN ON *.* TO 'mrpg_local'@'127.0.0.1';
FLUSH PRIVILEGES;
SQL
mariadb -u root -p mrpg_local_us < mrpg_local.sql
```

* It is reached only by `sv_sql_local_*` and is visible only on this location
  host's MariaDB; replication never creates or copies it. It needs no
  `mrpg_bridge` grant.
* Keeping it on the primary also works: the ignore rule of step 2.1 keeps it
  out of the replicas, and `sv_sql_local_host` then points at the primary (one
  WAN round trip per local query — prefer the game-server host).
* A new location still needs its own replica and bridge (steps 2–3) for the
  shared database.

---

## Upgrading a single database

Older versions kept everything in one replicated database (`mmotee`). That
database becomes the shared one and may keep its name (set
`sv_sql_database_shared`, formerly `sv_sql_database`); only the local tables
move out. In this guide it is called `mrpg_shared`.

1. Stop every game server; back up the database on the **primary**.
2. On the **primary**, create one local database per game server (step 1.3).
   The server that keeps the existing local rows gets them via "Moving an
   existing database to two databases" in
   [`docs/mariadb.md`](../../docs/mariadb.md), including the final
   `DROP`/`ALTER` — it does not start while the shared database still has
   those tables. Others import `mrpg_local.sql` (duplicating rows duplicates
   auction slots and guild banks). Only the removal of the old tables from
   the shared database replicates.
3. On each location host, create the separate `mrpg_local` account and grant
   it access only to that host's local database (the `GRANT` in [Adding a game
   server later](#adding-a-game-server-later)); with `read_only = 1` it also
   needs `READ ONLY ADMIN`. No `mrpg_bridge` grant is needed.
4. Wait until every replica has applied the shared-database changes (step 2.6).
   `SHOW DATABASES` must show `mrpg_shared` plus only the local database
   intentionally created for that location.
5. In every game server configuration replace `sv_sql_database` by
   `sv_sql_database_shared` and add the `sv_sql_local_*` block (step 4); start
   and check the logs.

---

## Troubleshooting

* **Access denied through the bridge** — the account must exist on the
  *primary* (login is relayed); check `mrpg_user` and its host mask.
* **`replica ... unavailable` / `fallback` / `replica returned error`** — the
  replica is unreachable, rejects `mrpg_bridge`, or errored; the query is
  served by the primary and the replica retried with backoff.
* **`client ... requested TLS` / `primary ... does not offer TLS`** — set
  `sv_sql_use_tls 0`; the bridge does not terminate TLS. If the primary lacks
  TLS, configure `ssl` there or disable `[primary] use_tls` behind a VPN.
* **Stale reads right after a write** — raise
  `read_your_writes_grace_sec`; see [Consistency](#consistency).
* **`replica cannot use database '...'`** — the replica is missing
  `mrpg_shared`, or `mrpg_bridge` lacks `SELECT` on it. A local database name
  here means `sv_sql_local_host` still points at the bridge.
* **An unexpected local database appears in `SHOW DATABASES` on a replica** —
  the one created for this location is expected; another copy imported from the
  primary is stale and should be removed (section above).
* **`The local database '...' is unavailable`** — the database named by
  `sv_sql_local_database` must exist on the host named by `sv_sql_local_host`
  with the tables of `mrpg_local.sql`, and that login needs a `GRANT`.
* **`Tables not found in the local database`** — import `mrpg_local.sql`
  there; the server checks all ten tables at startup.
* **`Local tables found in the shared database`** — the split is incomplete;
  remove them from `mrpg_shared` and wait for the replicas (step 2.6).
* **Replication broken** — `SHOW SLAVE STATUS\G` shows `Last_IO_Error` /
  `Last_SQL_Error`; fix, then `START SLAVE;`.
* **Debugging** — `route_selects_to_replica = false` sends everything to the
  primary; `BRIDGE_LOG_LEVEL=DEBUG` logs every route.

---

## How routing works

Only statements that are certainly side-effect free and session independent go
to the replica:

| Replica | Primary |
|---|---|
| a single plain `SELECT ...` / `(SELECT ...)` | everything else: writes, DDL, `SHOW`, `DESCRIBE`, `EXPLAIN`, `CALL` |
| | `SELECT ... FOR UPDATE / LOCK IN SHARE MODE / FOR SHARE`, `SELECT ... INTO` |
| | `LAST_INSERT_ID()`, `FOUND_ROWS()`, `ROW_COUNT()`, `GET_LOCK()`, `@variables`, `DATABASE()`, `LOAD_FILE()` |
| | multi-statements and anything after a `SET` / `USE` / `BEGIN` / `LOCK TABLES` / `CREATE TEMPORARY TABLE` on that session — the Connector/C++ connect-time bootstrap (`set autocommit=1, session_track_schema=1[, sql_mode = concat(@@sql_mode,',STRICT_TRANS_TABLES')]`) is the one exemption |
| | multi-result prepared statements (`COM_STMT_*` are relayed, never split) |

The first word decides the route, and string literals are blanked out first,
so a `SELECT`'s values can neither change the route nor appear in the log.
The rule can only be too careful (a `SELECT` served by the primary), never
too loose.

If the replica is down, short of pool slots, slow or erroring, the query goes
to the primary. Responses up to `replica_buffer_kb` are buffered for that; a
bigger stream cut by a replica failure closes that client connection (the
game server reconnects). The replica connections use the charset of the first
game-server session; a session with another charset goes to the primary.

Sessions that negotiate `MARIADB_CLIENT_CACHE_METADATA` (MariaDB
Connector/C++ sets it on **every** connection) are *not* pinned to the
primary: the bridge never negotiates the flag with the replica, so replica
responses always carry full inline metadata — which a client that knows the
flag parses as `has_metadata = 1` (bare result headers are always valid for
such clients; the flag only lets a server omit metadata for repeated
statements). Routing plain SELECTs from those clients to the replica is
byte-safe.

### Consistency

* After any write, **all** reads use the primary for
  `read_your_writes_grace_sec` (one timestamp for the whole bridge) — the
  game writes and reads back on different pooled connections, so a per-table
  or per-connection rule would miss it. Set it above the replication lag.
* A transaction stays on the primary from `BEGIN` to `COMMIT`/`ROLLBACK`.
* Not covered: writes through another location's bridge appear when
  replication catches up; keep the grace above the lag.

## Testing

Standard library only; runs the bridge against strict test clients and fake
MariaDB servers (routing, framing, failover, pool, TLS, timeouts, limits):

```bash
cd other/sql_bridge
python3 -m unittest discover -s tests -v
```

With `pymysql`, `mariadb`, `mysql-connector-python` or `mysql-mimic` installed,
interoperability tests run too. `tests/bench_bridge.py` is a micro-benchmark
(`--compare old_bridge.py`).

## Security checklist

* `bridge.cfg` holds a password: `chmod 600`, or use `password_file` /
  `BRIDGE_REPLICA_*`.
* Bridge and replica ports stay on `127.0.0.1` (or LAN + `allowed_clients` +
  firewall), never on the internet; the game server ↔ bridge hop is plain.
* `mrpg_bridge` has only `SELECT` on `mrpg_shared`.
* `mrpg_local` has `ALL PRIVILEGES` only on its own `mrpg_local_<location>`
  schema. Its global `READ ONLY ADMIN` bypass is solely what permits those
  local writes while the shared replica uses `read_only = 1`.
* `[primary] use_tls` + `tls_ca_file` when the WAN is not already encrypted.
* Unauthenticated peers cannot reach the replica (login is checked by the
  primary); central 3306 only via VPN, `@'%'` masks tightened; `MASTER_SSL=1`
  encrypts replication.

### Which account goes where (the automation prints this card at the end)

| Setting | Value | Account lives on | For |
|---|---|---|---|
| `sv_sql_host` / `sv_sql_port` | `127.0.0.1` / `33060` | — | the **bridge** on the same host (not the WAN primary) |
| `sv_sql_login` / `sv_sql_password` | `mrpg_user` / `<GAME_PASSWORD>` | primary | the game; the bridge relays the login to the primary |
| `sv_sql_database_shared` | `mrpg_shared` | primary + every replica | replicated account data |
| `sv_sql_local_*` | `127.0.0.1:3306`, local account (automation: `mrpg_local`) | the location host | this server's local database (direct, no bridge) |
| bridge `[primary]` | `<primary>:3306` | primary | writes and fallback |
| bridge `[replica]` | `127.0.0.1:3306`, `mrpg_bridge` | the location host | reads from the local replica |

Rules of thumb: `sv_sql_login` is **never** `root`, `mrpg_bridge` or
`mrpg_replica`; the bridge's `[replica]` section uses the **local** SELECT-only
account; replication credentials never reach the game. Both `menu 1` (master)
and `menu 2` (replica) end with a **FINAL CARD** summary of exactly these
values for the host type.
