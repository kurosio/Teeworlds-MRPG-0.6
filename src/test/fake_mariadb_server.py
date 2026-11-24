#!/usr/bin/env python3
"""
Starts one fake MariaDB server (see fake_mariadb.py) and prints "READY <port>".

The C++ database tests (src/test/db.cpp) use this to talk to a real MariaDB
client over a real socket without needing a MariaDB installation. The process
serves until it is killed; SIGTERM ends it cleanly.

    python3 fake_mariadb_server.py --name shared --database mrpg_shared
"""

import argparse
import asyncio
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from fake_mariadb import FakeMySQLServer  # noqa: E402


async def run(args):
    server = FakeMySQLServer(
        args.name,
        databases=tuple(args.database),
        deprecate_eof=not args.no_deprecate_eof,
        real_last_insert_id=True,
    )
    server.delay = args.delay
    await server.start(args.host, args.port)
    print("READY %d" % server.port, flush=True)
    try:
        while True:
            await asyncio.sleep(3600)
    except asyncio.CancelledError:
        pass
    finally:
        await server.stop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", default="shared")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--database", action="append", default=None)
    parser.add_argument("--no-deprecate-eof", action="store_true")
    parser.add_argument("--delay", type=float, default=0.0,
                        help="seconds every statement waits before it is answered")
    args = parser.parse_args()
    if args.database is None:
        args.database = ["mrpg_shared", "mrpg_local"]
    asyncio.run(run(args))


if __name__ == "__main__":
    main()
