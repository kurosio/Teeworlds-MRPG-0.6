#!/usr/bin/env python3
"""Opt-in single-channel resnapshot; primary queries and dump are read-only."""
import fcntl
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

DB = "mrpg_shared"
BASE = Path('/var/lib/mrpg-recovery')
ENV = os.environ.copy()
ENV['LC_ALL'] = 'C'


def run(args, *, output=None, timeout=3600):
    return subprocess.run(args, stdin=subprocess.DEVNULL, stdout=output or subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=timeout, check=True, env=ENV)


def local(sql):
    return run(['mariadb', '--protocol=socket', '--connect-timeout=5', '-N', '-e', sql], timeout=15).stdout.decode().strip()


def status():
    raw = local('SHOW ALL SLAVES STATUS\\G')
    fields = {}
    for line in raw.splitlines():
        if ':' not in line:
            continue
        name, value = line.strip().split(':', 1)
        if name in fields:
            raise ValueError('multiple channels or ambiguous fields')
        fields[name] = value.strip()
    return fields


def check_replica(expected_id, expected_primary, master_host):
    own = local('SELECT @@global.server_id, @@global.log_bin, @@global.read_only, @@global.event_scheduler').split('\t')
    if len(own) != 4 or own[:3] != [expected_id, '0', '1'] or own[3] not in ('OFF', 'DISABLED', '0'):
        raise ValueError('replica ID, binlog, read_only or event_scheduler precondition failed')
    s = status()
    if s.get('Connection_name') != '' or s.get('Master_Host') != master_host:
        raise ValueError('expected one default channel with matching Master_Host')
    if s.get('Master_Server_Id') not in (expected_primary, '0'):
        raise ValueError('replication source ID mismatch')
    if s.get('Replicate_Wild_Do_Table') != DB + '.%':
        raise ValueError('replica must replicate only mrpg_shared.% via wild-do filter')
    for field in ('Replicate_Do_DB', 'Replicate_Ignore_DB', 'Replicate_Do_Table', 'Replicate_Ignore_Table'):
        if s.get(field, ''):
            raise ValueError('unsupported replication filter: ' + field)
    if not s.get('Slave_SQL_Running') or not s.get('Slave_IO_Running'):
        raise ValueError('incomplete replication status')
    return s


def secure_directory(path):
    return not path.is_symlink() and path.stat().st_uid == 0 and not path.stat().st_mode & 0o077


def ssh_credentials():
    """Use the provisioned dedicated key and verified host list when configured."""
    key = os.environ.get('RESYNC_SSH_IDENTITY', '')
    known = os.environ.get('RESYNC_SSH_KNOWN_HOSTS', '')
    if not key and not known:
        return []  # Backwards-compatible operator-managed root SSH configuration.
    if not key or not known:
        raise ValueError('both SSH identity and known-hosts paths are required')
    for path in (Path(key), Path(known)):
        if (not path.is_absolute() or path.is_symlink() or not path.is_file()
                or path.stat().st_uid != 0 or path.stat().st_mode & 0o077):
            raise ValueError('SSH key and known-hosts must be root-owned mode 0600 files')
    return ['-i', key, '-o', 'IdentitiesOnly=yes', '-o', 'UserKnownHostsFile=' + known,
            '-o', 'GlobalKnownHostsFile=/dev/null']


def main():
    if os.geteuid() != 0:
        raise ValueError('must run as root')
    rid = os.environ.get('RESYNC_REPLICA_ID', '')
    pid = os.environ.get('RESYNC_PRIMARY_ID', '')
    host = os.environ.get('RESYNC_MASTER_HOST', '')
    target = os.environ.get('RESYNC_SSH_TARGET', '')
    if not re.fullmatch(r'[1-9][0-9]{0,9}', rid) or not re.fullmatch(r'[1-9][0-9]{0,9}', pid) or rid == pid:
        raise ValueError('configure distinct numeric primary and replica IDs')
    if not re.fullmatch(r'[A-Za-z0-9_.:-]+', host) or not re.fullmatch(r'[A-Za-z0-9_.-]+@[A-Za-z0-9_.-]+', target):
        raise ValueError('configure exact Master_Host and SSH user@host')
    BASE.mkdir(mode=0o700, parents=True, exist_ok=True)
    if not secure_directory(BASE):
        raise ValueError('recovery directory must be root-owned and mode 0700')
    os.umask(0o077)
    with (BASE / 'resnapshot.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        s = check_replica(rid, pid, host)
        marker = BASE / 'resnapshot-pending'
        if s['Slave_SQL_Running'] == 'Yes' and s['Slave_IO_Running'] == 'Yes':
            if marker.exists() and marker.read_text().strip().endswith('|complete'):
                marker.unlink()
                print('Previously imported snapshot is running; cleared recovery marker.', flush=True)
                return
            if not marker.exists():
                raise ValueError('replication is running; refusing resnapshot')
        ssh = ['ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes', '-o', 'ConnectTimeout=15',
               '-o', 'ServerAliveInterval=15', '-o', 'ServerAliveCountMax=4'] + ssh_credentials() + [target]
        probe = "mariadb --protocol=socket -N -e \"SELECT @@global.server_id, @@global.log_bin, (SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='mrpg_shared' AND table_type='BASE TABLE' AND (ENGINE IS NULL OR ENGINE <> 'InnoDB')), (SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='mrpg_shared' AND table_type='BASE TABLE'), (SELECT DEFAULT_CHARACTER_SET_NAME FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='mrpg_shared'), (SELECT DEFAULT_COLLATION_NAME FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='mrpg_shared')\""
        actual = run(ssh + [probe], timeout=30).stdout.decode().strip().split('\t')
        if len(actual) != 6 or actual[0] != pid or actual[1:3] != ['1', '0'] or not actual[3].isdigit() or int(actual[3]) == 0:
            raise ValueError('primary ID, binlog or InnoDB table precondition failed')
        if not all(re.fullmatch(r'[A-Za-z0-9_]+', value) for value in actual[4:]):
            raise ValueError('invalid primary character set or collation')
        if shutil.disk_usage(BASE).free < 1024**3:
            raise ValueError('less than 1 GiB free in recovery directory')
        stamp = time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())
        marker = BASE / 'resnapshot-pending'
        if marker.exists():
            name = marker.read_text().strip().split('|', 1)[0]
            if not re.fullmatch(r'replica-[0-9TZ-]+\.sql', name):
                raise ValueError('invalid pending recovery marker')
            backup = BASE / name
            if not backup.is_file() or backup.is_symlink() or backup.stat().st_size == 0:
                raise ValueError('original replica backup unavailable')
        else:
            backup = BASE / ('replica-' + stamp + '-' + str(os.getpid()) + '.sql')
            try:
                with backup.open('xb') as out:
                    run(['mariadb-dump', '--protocol=socket', '--single-transaction', '--quick', '--routines', '--triggers', '--events', DB], output=out)
                    out.flush(); os.fsync(out.fileno())
            except Exception:
                if backup.exists():
                    backup.unlink()
                raise
        # Retain replica backup even if later steps fail. Never purge automatically.
        with tempfile.NamedTemporaryFile(prefix='primary-', suffix='.sql', dir=BASE) as snap:
            command = 'mariadb-dump --protocol=socket --single-transaction --quick --master-data=2 --gtid --routines --triggers --events mrpg_shared'
            run(ssh + [command], output=snap)
            snap.flush(); os.fsync(snap.fileno())
            snap.seek(0)
            head = snap.read(1024 * 1024).decode('utf-8', 'replace')
            found = re.findall(r"gtid_slave_pos\s*=\s*'([^']+)'", head, re.I)
            if len(found) != 1 or not re.fullmatch(r'[0-9]+-[0-9]+-[0-9]+', found[0]):
                raise ValueError('snapshot GTID missing or ambiguous')
            gtid = found[0]
            current_gtid = local('SELECT @@global.gtid_slave_pos')
            if current_gtid and (not re.fullmatch(r'[0-9]+-[0-9]+-[0-9]+', current_gtid)
                                 or current_gtid.split('-', 1)[0] != gtid.split('-', 1)[0]):
                raise ValueError('replica GTID position has an unsupported or different domain')
            # Recheck immediately before destructive operations; never RESET MASTER.
            s = check_replica(rid, pid, host)
            if s['Slave_SQL_Running'] == 'Yes' and s['Slave_IO_Running'] == 'Yes' and not marker.exists():
                raise ValueError('replication recovered during snapshot; aborting')
            print('Snapshot ready; saved replica backup at %s; stopping replica' % backup, flush=True)
            if not marker.exists():
                with tempfile.NamedTemporaryFile(mode='w', dir=BASE, delete=False) as pending:
                    pending.write(backup.name + '\n')
                    pending.flush(); os.fsync(pending.fileno())
                os.replace(pending.name, marker)
                directory = os.open(BASE, os.O_RDONLY | os.O_DIRECTORY)
                try:
                    os.fsync(directory)
                finally:
                    os.close(directory)
            local('STOP SLAVE')
            local('RESET SLAVE')
            local('SET SESSION foreign_key_checks=0; DROP DATABASE IF EXISTS `mrpg_shared`; CREATE DATABASE `mrpg_shared` CHARACTER SET %s COLLATE %s' % (actual[4], actual[5]))
            snap.seek(0)
            subprocess.run(['mariadb', '--protocol=socket', DB], stdin=snap, stdout=subprocess.DEVNULL,
                           stderr=subprocess.PIPE, timeout=3600, check=True, env=ENV)
            local("SET GLOBAL gtid_slave_pos='%s'" % gtid)
            local('CHANGE MASTER TO MASTER_USE_GTID=slave_pos')
            with tempfile.NamedTemporaryFile(mode='w', dir=BASE, delete=False) as pending:
                pending.write(backup.name + '|complete\n')
                pending.flush(); os.fsync(pending.fileno())
            os.replace(pending.name, marker)
            directory = os.open(BASE, os.O_RDONLY | os.O_DIRECTORY)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
            local('START SLAVE')
            marker.unlink()
            print('Resnapshot imported; replication started. Next guard checks verify health.', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        print('Resnapshot failed: %s. Replica may need another resnapshot; backups retained.' % exc, file=sys.stderr)
        sys.exit(1)
