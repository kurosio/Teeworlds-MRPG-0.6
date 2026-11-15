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
An opt-in automatic resnapshot is described below. It does not merge divergent data.

## Install the guard with the game service

The supplied root `deploy.sh` is based on the provided deployment script.
`service install` **stops/recreates services** and its bootstrap can kill game
processes. Review it and schedule downtime first. It writes systemd units and
requires a working local root MariaDB socket login, `mariadb`, and GNU `timeout`.
All database commands explicitly use a local Unix socket; configure the local
socket and credentials in root's MariaDB client settings.
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
MAX_IO_RETRIES=5
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
more often than every 120 seconds, up to five attempts per unhealthy period.
A healthy check or guard restart resets this in-memory budget. `Connecting`
already retries itself. The next poll checks the result. It never skips events. When opt-in resnapshot is disabled, it does not edit GTIDs or change replication settings. Unknown errors, named/multiple channels, SQL errors,
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

## Experimental automatic replica resnapshot (opt-in)

**Do not enable in production until a restore drill with your exact MariaDB
versions, topology, bridge, dataset, and service setup has passed.** This is a
new implementation tested with mocks, not a guarantee of unattended recovery.
It destroys the replica's `mrpg_shared` and resets its global `gtid_slave_pos`.
It never runs modifying SQL on the primary, but `mariadb-dump --master-data`
briefly takes a global read lock there. Backups consume disk and contain player
data. No replica-only shared writes are merged. The bridge must use primary
reads, including during recovery, and stale game caches must be handled separately.

Supported topology: one primary, one default replica channel, no downstream
replicas or other replicated databases/domains (the snapshot and existing replica position
may contain only one matching GTID domain), local `log_bin=0`, `read_only=1`,
`event_scheduler=OFF` or `DISABLED`, and exactly
`replicate_wild_do_table=mrpg_shared.%`. Other do/ignore database or exact-table
filters are rejected (wild-ignore filters may remain). This restriction matters
because `gtid_slave_pos` applies to the entire replica, not just one database.
The primary's shared base tables must all be InnoDB. Do not run concurrent DDL
or operations affecting other databases during snapshots. Ensure adequate disk
space for both dumps and imported data; the 1 GiB check is only a floor, not a
size estimate. Review triggers, routines and events before using this procedure.

### Guided one-time authorization

After meeting the topology and safety requirements above, run on each **replica**:

```sh
cd /path/to/Teeworlds-MRPG-0.6/other/sql_bridge
sudo ./setup_resnapshot.sh
```

The script detects the local replica ID and `Master_Host`, checks replica
preconditions, generates a dedicated key if needed, and asks for a restricted
primary SSH account. It displays the scanned ED25519 host-key fingerprint: enter
the fingerprint obtained independently from the primary administrator. A scan
alone is not proof of identity. If the key is not yet authorized, you may opt
to run `ssh-copy-id`; this **changes the primary account's authorized_keys** and
may prompt for an SSH password locally. It does not save the password or modify
primary database contents. Prefer a restricted backup account with only the
needed MariaDB read/dump privileges; an unrestricted root SSH key is high risk.
The dedicated private key is unencrypted so an unattended service can use it;
protect the replica host and `/etc/mrpg` accordingly.

After SSH login and basic source checks, the script shows the destructive
recovery warning and requires the explicit word `ENABLE`. It backs up and
atomically updates the root-only guard configuration and restarts an active
guard. It does **not** create database accounts, change MariaDB configuration,
prove that full dump privileges are present, test a full restore, or guarantee
recovery. Those still require one-time setup and a staging drill. It currently
supports SSH port 22, an ED25519 host key, and an SSH hostname identical to
`Master_Host`; use the manual configuration below for other deployments.

### Manual configuration (alternative)

One-time setup on each replica (example values only):

1. Configure the above replication filter and server settings using your normal
   MariaDB maintenance process; restart if required. Ensure local databases are
   separately backed up and verify all services read shared data from primary.
2. Create a dedicated SSH key for root on the replica, and a restricted backup
   account on primary. Install the public key according to your security policy.
   Verify the primary SSH host-key fingerprint out of band and add it to root's
   `known_hosts` on the replica. Batch mode and strict host-key checking are
   required; no password is stored in the guard. SSH target must reach the same
   server as replication `Master_Host`. Allow only read/dump permissions on its
   local MariaDB client account (including permissions required by your MariaDB
   version for `--master-data`, table data, views, routines, triggers and events).
   Test a snapshot and lock impact on a staging primary first.
3. In root-owned mode-0600 `/etc/mrpg/replication-guard.conf`, add:

```sh
AUTO_RESNAPSHOT=1
RESYNC_REPLICA_ID=2
RESYNC_PRIMARY_ID=1
RESYNC_MASTER_HOST=db-primary
RESYNC_SSH_TARGET=backup@db-primary
RESYNC_RETRY_SECONDS=3600
```

`RESYNC_MASTER_HOST` must exactly match `Master_Host` from `SHOW ALL SLAVES
STATUS`; IDs must match actual MariaDB IDs and be distinct. The host must use
the same socket-authenticated account for the non-interactive SSH `mariadb` and
`mariadb-dump` commands. Restart the guard by restarting the game service.

On `1062`, `1032`, `1146`, IO error `1236`, or an interrupted import marker, the
guard attempts a resnapshot, at most once per cooldown in a running guard
process. A service restart resets that in-memory cooldown. The script checks
server IDs, local-only connection, source identity, channel, filters, read-only,
binlog and table engines. It first saves a local replica dump under
`/var/lib/mrpg-recovery` and downloads an SSH snapshot with GTID; only then does
it stop replication, replace `mrpg_shared`, restore GTID and start the existing
channel. It never issues `RESET MASTER`. The root-only pending marker enables
retry after an interrupted import, using the original saved replica backup.
Backups are retained, not automatically deleted; monitor capacity and protect
and rotate them under your backup policy. If any precondition, SSH, backup,
import or restart fails, the guard logs the failure and retries after cooldown;
it does not claim success. `--once` remains read-only and never resnapshots.
Primary unavailability, privilege errors, DDL races and exhausted disk cannot
be fixed automatically. Monitor the guard externally and perform recovery drills.
