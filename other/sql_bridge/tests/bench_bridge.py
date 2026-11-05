"""
Micro-benchmark for mysql_bridge.py (fake backends, loopback TCP).

    python3 tests/bench_bridge.py                       # new bridge only
    python3 tests/bench_bridge.py --compare old.py      # old vs new
    git show <rev>:other/sql_bridge/mysql_bridge.py > /tmp/old.py

The fake primary/replica, the bridge, and the load generator run as separate
processes.  Absolute numbers depend on the machine (and the Python fake
servers are part of every measurement); compare bridges on the same host.
A "direct" row (client -> fake replica, no bridge) is the ceiling.
"""

import argparse
import asyncio
import os
import statistics
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from fake_mysql import MiniClient  # noqa: E402

NEW = os.path.join(os.path.dirname(HERE), "mysql_bridge.py")

CFG = """[bridge]
listen_host = 127.0.0.1
listen_port = {listen}
max_clients = 512
replica_pool_size = 16
route_selects_to_replica = true
read_your_writes_grace_sec = 0
stats_interval_sec = 0

[primary]
host = 127.0.0.1
port = {primary}

[replica]
host = 127.0.0.1
port = {replica}
user = bridge
password = bridgepw
database = mmotee
"""


def free_port():
    import socket
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


async def wait_port(port, timeout=10):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            r, w = await asyncio.open_connection("127.0.0.1", port)
            w.close()
            return
        except OSError:
            await asyncio.sleep(0.05)
    raise RuntimeError("port %d did not open" % port)


async def connect(port, n):
    clients = []
    for _ in range(n):
        c = MiniClient()
        await c.connect(port)
        clients.append(c)
    return clients


async def bench_latency(port, n=2000):
    (c,) = await connect(port, 1)
    sql = "SELECT * FROM tw_accounts WHERE ID = 1"
    for _ in range(100):
        await c.rows(sql)
    lat = []
    for _ in range(n):
        t = time.perf_counter()
        await c.rows(sql)
        lat.append(time.perf_counter() - t)
    await c.close()
    lat.sort()
    return {"p50_us": statistics.median(lat) * 1e6, "p99_us": lat[int(len(lat) * 0.99)] * 1e6}


async def bench_throughput(port, nclients=32, seconds=4.0, write_every=0):
    clients = await connect(port, nclients)
    count = 0
    stop = time.monotonic() + seconds

    async def worker(c):
        nonlocal count
        i = 0
        while time.monotonic() < stop:
            i += 1
            if write_every and i % write_every == 0:
                await c.query("UPDATE tw_accounts_data SET Level = 2 WHERE ID = 1")
            else:
                await c.rows("SELECT * FROM tw_accounts WHERE ID = 1")
            count += 1
    t = time.monotonic()
    await asyncio.gather(*(worker(c) for c in clients))
    el = time.monotonic() - t
    for c in clients:
        await c.close()
    return {"qps": count / el}


async def bench_bulk(port, rounds=8, rows=50000, width=100):
    (c,) = await connect(port, 1)
    await c.rows("SELECT ROWS %d %d" % (rows, width))            # warm the fake's cache
    t = time.monotonic()
    for _ in range(rounds):
        got = await c.rows("SELECT ROWS %d %d" % (rows, width))
        assert len(got) == rows
    el = time.monotonic() - t
    await c.close()
    size = rounds * rows * (width + 12)
    return {"MB_s": size / el / 1e6}


async def run_suite(port, label):
    res = {"label": label}
    res.update(await bench_latency(port))
    res["read_qps_32c"] = (await bench_throughput(port))["qps"]
    res["mixed_qps_32c"] = (await bench_throughput(port, write_every=5))["qps"]
    res["bulk_MB_s"] = (await bench_bulk(port))["MB_s"]
    return res


def start_bridge(path, primary, replica, python):
    listen = free_port()
    fd, cfg = tempfile.mkstemp(suffix=".cfg")
    with os.fdopen(fd, "w") as fh:
        fh.write(CFG.format(listen=listen, primary=primary, replica=replica))
    env = dict(os.environ, BRIDGE_LOG_LEVEL="WARNING")
    proc = subprocess.Popen([python, path, cfg], env=env, stdout=subprocess.DEVNULL,
                            stderr=subprocess.PIPE)
    return proc, listen, cfg


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--compare", help="path to another mysql_bridge.py (e.g. the old version)")
    ap.add_argument("--python", default=sys.executable, help="interpreter for the bridge process")
    args = ap.parse_args()
    fake = subprocess.Popen([sys.executable, os.path.join(HERE, "fake_mysql.py"), "primary", "replica"],
                            stdout=subprocess.PIPE, text=True)
    try:
        primary, replica = map(int, fake.stdout.readline().split())
        results = [await run_suite(replica, "direct (no bridge)")]
        targets = []
        if args.compare:
            targets.append(("old: " + os.path.basename(args.compare), args.compare))
        targets.append(("new: mysql_bridge.py", NEW))
        for label, path in targets:
            proc, listen, cfg = start_bridge(path, primary, replica, args.python)
            try:
                await wait_port(listen)
                results.append(await run_suite(listen, label))
            finally:
                proc.terminate()
                try:
                    proc.wait(5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                os.unlink(cfg)
    finally:
        fake.terminate()
    cols = ["p50_us", "p99_us", "read_qps_32c", "mixed_qps_32c", "bulk_MB_s"]
    print("%-28s" % "target" + "".join("%15s" % c for c in cols))
    for r in results:
        print("%-28s" % r["label"] + "".join("%15.0f" % r[c] for c in cols))


if __name__ == "__main__":
    asyncio.run(main())
