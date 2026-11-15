# MariaDB replication recovery and monitoring

A replica with `Slave_IO_Running: Yes`, `Slave_SQL_Running: No` and
`Seconds_Behind_Master: NULL` is **not healthy**. The cause cannot be identified
without `Last_SQL_Errno`, `Last_SQL_Error`, `Last_IO_Errno` and `Last_IO_Error`.
No script can guarantee that replication will never stop or safely repair every
failure without knowing the topology and the data involved.

## Immediate response

1. Route game-server shared reads to the primary on **every** bridge. The
   updated `mysql_bridge.py` does this by default; deploy and restart all
   bridges and verify their routing. A running old bridge retains its old
   behavior. Direct replica connections, local caches and values already loaded
   by game servers are not protected. If the primary is unavailable, do not
   fall back to stale reads. Consider pausing game writes until stale in-memory
   values have been reconciled.
2. Before changing replication, save the complete status and consistent backups
   of the primary and replica, including every `mrpg_local*` database on each
   host. Local databases are not replicated. Coordinate a maintenance window
   or quiesce writes for consistent backups. Store diagnostic output securely;
   SQL errors may contain player data, hosts or query values.
3. Check whether any clients or privileged accounts wrote directly to the
   replica's `mrpg_shared`. Preserve and reconcile those changes before any
   resnapshot: replacing the shared database discards replica-only writes.

Run the bounded, read-only diagnostic on the affected host:

```sh
cd /path/to/Teeworlds-MRPG-0.6/other/sql_bridge
sudo sh fix_replication.sh auto
# Explicit alternatives: sudo sh fix_replication.sh replica
#                        sudo sh fix_replication.sh primary
```

`auto` reports all channels. No channel does **not** prove that a server is a
primary; it may be a misconfigured replica. For manual investigation retain the
full `SHOW ALL SLAVES STATUS\G` and GTID information. Redact credentials and
private information before sharing logs.

## Recovery

* Errors `1062` and `1032` may indicate divergent data; skipping an event does
  not reconcile it. `1146` can indicate schema or filter drift; creating an
  empty table does not restore missing rows. Diagnose and compare data first.
* For `1236`, read the complete IO error. If required binlogs have been purged,
  a fresh, consistent primary snapshot and replica reprovisioning are needed.
* Resolve network, authentication or TLS errors from `Last_IO_Error`. For a
  single **default** channel, after fixing the cause, the optional command
  `sudo sh fix_replication.sh restart-replica` asks for `START` and starts the
  threads. It does not prove data equality. Named or multiple channels are
  rejected and require a topology-specific procedure.

The existing `sudo sh setup_replication.sh` replica workflow and [README.md](README.md)
describe resnapshotting. Review its plan before applying: shared data may be
replaced and GTID state changed. Never perform replica reset operations on the
primary. If other databases share GTID domains, there are multiple channels, or
the replica feeds downstream servers, arrange a topology-specific recovery
first. Preserve `mrpg_local*`. Validate both threads, lag, schema, critical
rows, local databases, permissions and application behavior after recovery.
There is no automatic full resnapshot or merge of divergent data.

## Install the guard with the game service

The supplied root `deploy.sh` is based on the provided deployment script.
`service install` **stops/recreates services** and its bootstrap can kill game
processes. Review it and schedule downtime first. It writes systemd units and
requires a working local root MariaDB socket login, `mariadb`, and GNU `timeout`.
For an initial installation on each machine, specify the *actual* role:

```sh
cd /path/to/Teeworlds-MRPG-0.6
# On a replica (use GUARD_ROLE=primary on the primary instead):
sudo GUARD_ROLE=replica PROJECT_DIR="$PWD" ./deploy.sh service install
sudo systemctl start mmoteeworlds-server.service
sudo systemctl status mmoteeworlds-replication-guard.service
sudo journalctl -u mmoteeworlds-replication-guard.service -f
```

When creating the guard configuration for the first time, `GUARD_ROLE` tells the
installer which role to associate with the local MariaDB `server_id`; it does
not change the database role. IDs alone do not determine roles. The installer
never overwrites an existing `/etc/mrpg/replication-guard.conf`: inspect and
update it manually if the topology changes. Without `GUARD_ROLE` it creates an
empty mapping; the guard reports an error until configured. Configuration is a
trusted executable shell file; keep it root-owned, mode 0600. Example:

```sh
PRIMARY_IDS=1
REPLICA_IDS=2,3
INTERVAL=15
MAX_LAG=300
IO_RETRY_SECONDS=120
REPORT_REPEAT_SECONDS=300
AUTO_START_IO=1
```

Comma-separated IDs have no spaces and must match the actual unique IDs. The
guard is a root systemd companion bound to the game service; it stops when the
game service stops. It checks the primary's binlog or a single replica channel,
reports changes and repeats persistent status every five minutes in the journal.
It suggests likely causes from error numbers without logging SQL error text.
When SQL is running and the **default** channel's IO thread is stopped with a
network error 2003, 2006, 2013 or 2055, it retries `START SLAVE IO_THREAD`, no
more often than every 120 seconds. `Connecting` already retries itself. The
next poll checks the result. It never skips events, edits GTIDs or changes
replication settings. Unknown errors, named/multiple channels, SQL errors,
authentication errors and missing binlogs require investigation. Automatic
recovery cannot resolve all causes or guarantee consistent data.

For a read-only health check, run `sudo sh other/sql_bridge/replication_guard.sh --once`:
exit 0 means healthy, 1 unhealthy, 2 invalid configuration/dependency. This
mode never starts a thread. `replication_watchdog.sh` is a separate optional
read-only cron check; see `replication_watchdog.conf.example`. Avoid installing
both unless both are needed.

Journal entries are not guaranteed notifications. Configure external alerts
and host availability monitoring, test delivery, and independently monitor disk
space, backup restoration and binlog retention. The guard does not stop the
bridge, detect stale application caches or prevent all direct replica reads.

## Configuration examples and consistency

`mrpg_primary_ideal.cnf` and `mrpg_replica_ideal.cnf` are **examples**, not
universal settings. Check your MariaDB version, existing configuration,
unique IDs, resources, TLS and topology on a staging system before changing or
restarting production. `expire_logs_days=10` is only an example: size retention
for your maximum outage and available disk. `read_only` can be bypassed by
privileged users. See [MariaDB settings](../../docs/mariadb.md) and the main
[bridge guide](README.md).

Replica reads are disabled by default even for an existing configuration that
sets `route_selects_to_replica = true`. To explicitly accept stale reads, both
that option and `allow_stale_replica_reads = true` must be set. Even with both
replication threads running and zero reported lag, asynchronous replication can
return stale values; a later write may overwrite newer primary data. The lag
monitor and grace period do not provide linearizability. Do not opt in for
player read-modify-write flows unless a separate concurrency control mechanism
protects them. A replica monitor account needs `REPLICA MONITOR` (or the
version-appropriate equivalent) if lag monitoring is enabled.
