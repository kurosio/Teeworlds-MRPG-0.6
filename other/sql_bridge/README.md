# mysql_bridge.py — central database + replica setup guide

`mysql_bridge.py` lets **one central database** serve game servers in several
locations with minimal added latency. Each game-server location runs a local
MariaDB **read replica** plus the bridge; reads are served locally, writes are
forwarded to the central primary over the WAN:

```
game server (location A) --\
game server (location B) ---+--> local bridge --> [local replica] --\
game server (location C) --/    (mysql_bridge.py)                    +--> replication --> CENTRAL PRIMARY
                                                                    --/
```

The bridge is a pure Python 3 (stdlib, asyncio) MySQL/MariaDB proxy: it routes
plain `SELECT`/`SHOW`/`DESCRIBE`/`EXPLAIN` statements to the local replica and
everything else (`INSERT`/`UPDATE`/`DELETE`/DDL/transactions/anything it is not
sure about) to the central primary. See [How routing works](#how-routing-works)
for the exact rules and the consistency guarantees.

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
* The database name used everywhere is `mmotee` (as in `mmprpg_clean.sql`,
  `replica_setup.sql`, `bridge.cfg.example`). If you use a different name,
  change it consistently in the dump import, the `GRANT`s, `bridge.cfg` and
  `sv_sql_database`.
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

### 1.3 Create the database and import the game schema

```bash
mariadb -u root -p -e "CREATE DATABASE IF NOT EXISTS mmotee CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
mariadb -u root -p mmotee < mmprpg_clean.sql
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
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX ON mmotee.* TO 'mrpg_user'@'%';
FLUSH PRIVILEGES;
SQL
```

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
CNF
sudo systemctl restart mariadb
```

### 2.2 Take a snapshot of the primary and import it

On the **central primary** (prefer a low-traffic moment; `--single-transaction`
keeps InnoDB tables consistent):

```bash
mariadb-dump -u root -p --single-transaction --quick --routines --triggers --events \
  --source-data=2 --gtid mmotee > mmotee_snapshot.sql
```

Copy `mmotee_snapshot.sql` to the replica host (e.g. `scp`), then on the
**replica**:

```bash
mariadb -u root -p -e "CREATE DATABASE IF NOT EXISTS mmotee CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
mariadb -u root -p mmotee < mmotee_snapshot.sql
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
GRANT SELECT ON mmotee.* TO 'mrpg_bridge'@'127.0.0.1';
FLUSH PRIVILEGES;
SQL
```

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
mariadb -u root -p -e "SHOW SLAVE STATUS\G" | grep -E "Slave_IO_Running|Slave_SQL_Running|Seconds_Behind"
```

Expect `Slave_IO_Running=Yes` and `Slave_SQL_Running=Yes`.
`Seconds_Behind_Master` is your replication lag — watch it in production.

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

```bash
cd other/sql_bridge
cp bridge.cfg.example bridge.cfg
chmod 600 bridge.cfg
nano bridge.cfg
```

The important values:

```ini
[bridge]
listen_host = 127.0.0.1            # LAN IP instead, if the game server is on another host
listen_port = 33060

[primary]
host = db-central.example.com      # central primary from step 1
port = 3306

[replica]
host = 127.0.0.1                   # this location's replica from step 2
port = 3306
user = mrpg_bridge
password = <BRIDGE_READ_PASSWORD>
database = mmotee
use_tls = false                    # local replica: no TLS needed
```

To encrypt the WAN hop to the primary (recommended unless a VPN already
does), enable TLS in `[primary]`. This needs `ssl` configured on the
primary (`ssl_cert`/`ssl_key` in its `my.cnf`); the bridge verifies the
server certificate and refuses to log in in clear text if TLS is missing:

```ini
[primary]
host = db-central.example.com
port = 3306
use_tls = true
tls_ca_file = /etc/mrpg/primary-ca.pem    # CA that signed the primary's certificate
#tls_server_name = db-central.example.com # if it differs from host
```

Useful `[bridge]` knobs (all documented in `bridge.cfg.example`):

* `max_clients`, `replica_pool_size`, `allowed_clients` (source IP/CIDR allowlist)
* `route_selects_to_replica = false` — incident mode: send *everything* to the primary
* `read_your_writes_grace_sec` / `read_your_writes_scope` — see [Consistency](#consistency)
* `max_replica_lag_sec` — stop reading from a replica that is behind
* `client_idle_timeout_sec`, `auth_timeout_sec`, `replica_query_timeout_sec`,
  `replica_acquire_timeout_sec`, `replica_buffer_kb`, `max_packet_mb`

The values `BRIDGE_LISTEN_HOST`, `BRIDGE_LISTEN_PORT`, `BRIDGE_PRIMARY_HOST`,
`BRIDGE_PRIMARY_PORT`, `BRIDGE_REPLICA_HOST`, `BRIDGE_REPLICA_PORT`,
`BRIDGE_REPLICA_USER`, `BRIDGE_REPLICA_PASSWORD`, `BRIDGE_LOG_LEVEL` can also
be set via environment variable, and `password_file` /
`BRIDGE_REPLICA_PASSWORD_FILE` read the replica password from a file, so
secrets don't have to live in `bridge.cfg`.

Check the file before starting (exit code 0 = OK, 2 = error):

```bash
python3 mysql_bridge.py bridge.cfg --check-config
```

**No replica at this location yet?** Point the `[replica]` section at the
primary (its host/port, plus any account with `SELECT` on `mmotee`, e.g.
`mrpg_bridge`) — the bridge then works as a plain proxy, and you can add the
replica later.

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
sv_sql_database "mmotee"
sv_sql_pool_size 8
sv_sql_use_tls 0
```

> **Important:** the bridge forwards the login to the **central primary**, so
> the game server must use the primary's account (`mrpg_user` from step 1.4) —
> **not** `mrpg_bridge` (that one is the bridge's own read-only replica
> connection). `sv_sql_password` is limited to 32 characters.

If the bridge runs on a different host than the game server, set
`sv_sql_host` to the bridge host's LAN IP and `listen_host` in `bridge.cfg`
accordingly (firewall 33060 to the game-server host only).

---

## Step 5 — Verify end-to-end

```bash
# through the bridge (on the bridge host):
mariadb -h 127.0.0.1 -P 33060 -u mrpg_user -p mmotee \
  -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='mmotee';"

# replica health:
mariadb -u root -p -e "SHOW SLAVE STATUS\G" | grep -E "Slave_IO_Running|Slave_SQL_Running|Seconds_Behind"

# bridge log: client connects and per-30s stats
journalctl -u mrpg-mysql-bridge -f
```

Finally start the game server and check its log for a successful SQL
connection.

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
* `mrpg_bridge` has only `SELECT` on `mmotee` (+ optional `REPLICA MONITOR`).
* Enable `[primary] use_tls` with a `tls_ca_file` when the WAN link is not
  already encrypted by a VPN.
* Unauthenticated connections never reach the replica: the game server's
  login is checked by the primary first, and `auth_timeout_sec` /
  `max_clients` bound what an unauthenticated peer can hold open.
* Central 3306 reachable only via the VPN/private network; restrict the
  `@'%'` account masks to concrete IPs where possible.
* All location-to-location traffic goes through your verified TLS
  tunnel/VPN; `MASTER_SSL=1` additionally encrypts the replication stream.
