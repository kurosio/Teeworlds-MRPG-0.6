"""Safety checks use fakes; real MariaDB integration is still required."""
import importlib.util
import os
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

HERE = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('resnapshot', HERE / 'replica_resnapshot.py')
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)


class ResnapshotTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.calls = []
        self.s = {'Connection_name': '', 'Master_Host': 'db-primary', 'Master_Server_Id': '1',
                  'Replicate_Wild_Do_Table': 'mrpg_shared.%', 'Slave_SQL_Running': 'No',
                  'Slave_IO_Running': 'Yes'}
        self.primary_id = '1'
        self.gtid = '0-1-123'
        env = {'RESYNC_REPLICA_ID': '2', 'RESYNC_PRIMARY_ID': '1',
               'RESYNC_MASTER_HOST': 'db-primary', 'RESYNC_SSH_TARGET': 'backup@db-primary'}
        self.patches = [mock.patch.object(r, 'BASE', Path(self.tmp.name)), mock.patch.object(r, 'secure_directory', return_value=True),
                        mock.patch.dict(os.environ, env), mock.patch.object(r.os, 'geteuid', return_value=0),
                        mock.patch.object(r, 'status', side_effect=lambda: dict(self.s)),
                        mock.patch.object(r, 'local', side_effect=self.local),
                        mock.patch.object(r, 'run', side_effect=self.fake_run),
                        mock.patch.object(r.shutil, 'disk_usage', return_value=SimpleNamespace(free=5 * 1024**3)),
                        mock.patch.object(r.subprocess, 'run')]
        for patch in self.patches:
            patch.start()
            self.addCleanup(patch.stop)

    def local(self, sql):
        self.calls.append(sql)
        if sql.startswith('SELECT @@global.server_id'):
            return '2\t0\t1\tOFF'
        return ''

    def fake_run(self, args, output=None, timeout=3600):
        self.calls.append(args)
        if args[0] == 'ssh' and 'SELECT @@global.server_id' in args[-1]:
            return SimpleNamespace(stdout=(self.primary_id + '\t1\t0\t2\tutf8mb4\tutf8mb4_unicode_ci').encode())
        if args[0] == 'ssh':
            output.write(("-- SET GLOBAL gtid_slave_pos='%s';\n" % self.gtid).encode())
        else:
            output.write(b'-- local backup\n')
        return SimpleNamespace(stdout=b'')

    def test_success_only_mutates_replica_after_backup_and_valid_snapshot(self):
        r.main()
        changes = [x for x in self.calls if isinstance(x, str)]
        self.assertTrue(any('DROP DATABASE' in x for x in changes))
        self.assertIn("SET GLOBAL gtid_slave_pos='0-1-123'", changes)
        self.assertFalse(any('RESET MASTER' in x for x in changes))
        self.assertFalse((r.BASE / 'resnapshot-pending').exists())
        self.assertEqual(len(list(r.BASE.glob('replica-*.sql'))), 1)
        for cmd in (x for x in self.calls if isinstance(x, list) and x[0] == 'ssh'):
            self.assertNotIn('DROP', cmd[-1])
            self.assertIn('StrictHostKeyChecking=yes', cmd)

    def test_configured_ssh_files_require_secure_ownership_and_modes(self):
        env = {'RESYNC_SSH_IDENTITY': '/etc/mrpg/key',
               'RESYNC_SSH_KNOWN_HOSTS': '/etc/mrpg/known_hosts'}
        path = mock.Mock()
        path.is_absolute.return_value = True
        path.is_symlink.return_value = False
        path.is_file.return_value = True
        path.stat.return_value = SimpleNamespace(st_uid=0, st_mode=0o600)
        with mock.patch.dict(os.environ, env), mock.patch.object(r, 'Path', return_value=path):
            options = r.ssh_credentials()
            self.assertIn('IdentitiesOnly=yes', options)
            self.assertIn('UserKnownHostsFile=/etc/mrpg/known_hosts', options)
            path.stat.return_value = SimpleNamespace(st_uid=0, st_mode=0o644)
            with self.assertRaises(ValueError):
                r.ssh_credentials()
        with mock.patch.dict(os.environ, {'RESYNC_SSH_IDENTITY': '/etc/key',
                                         'RESYNC_SSH_KNOWN_HOSTS': ''}):
            with self.assertRaises(ValueError):
                r.ssh_credentials()

    def test_bad_gtid_does_not_modify_replica(self):
        self.gtid = "bad';DROP DATABASE x;--"
        with self.assertRaises(ValueError):
            r.main()
        self.assertFalse(any(isinstance(x, str) and 'DROP' in x for x in self.calls))

    def test_multiple_gtid_domains_abort_before_drop(self):
        self.gtid = '0-1-123,1-1-23'
        with self.assertRaises(ValueError):
            r.main()
        self.assertFalse(any(isinstance(x, str) and 'DROP' in x for x in self.calls))

    def test_source_identity_mismatch_does_not_modify_replica(self):
        self.primary_id = '9'
        with self.assertRaises(ValueError):
            r.main()
        self.assertFalse(any(isinstance(x, str) and 'DROP' in x for x in self.calls))

    def test_wrong_filter_does_not_modify_replica(self):
        self.s['Replicate_Wild_Do_Table'] = ''
        with self.assertRaises(ValueError):
            r.main()
        self.assertFalse(any(isinstance(x, str) and 'DROP' in x for x in self.calls))

    def test_failed_import_keeps_marker_and_backup_for_retry(self):
        r.subprocess.run.side_effect = r.subprocess.TimeoutExpired('import', 1)
        with self.assertRaises(r.subprocess.TimeoutExpired):
            r.main()
        self.assertTrue((r.BASE / 'resnapshot-pending').exists())
        self.assertEqual(len(list(r.BASE.glob('replica-*.sql'))), 1)
        r.subprocess.run.side_effect = None
        r.main()
        self.assertEqual(len(list(r.BASE.glob('replica-*.sql'))), 1)
        self.assertFalse((r.BASE / 'resnapshot-pending').exists())


if __name__ == '__main__':
    unittest.main()
