# Teeworlds MRPG 0.6

This repository contains the **server-only** MRPG modification based on Teeworlds 0.6/DDNet. It is not the upstream `teeworlds/teeworlds` repository: use this repository URL when cloning and use the `mmoteeworlds_srv` executable for the game server. Cross-server synchronization is provided by the separate `mmoteeworlds_relay` executable.

Development is currently infrequent. The project is not frozen, but this repository does not promise compatibility with every current Teeworlds client or operating-system release. The build configuration currently supports Linux and Windows; macOS configuration is explicitly rejected by CMake. The old upstream client, CircleCI/AppVeyor badges, and instructions for building the client have been removed because they do not describe this fork.

## Clone

The `libraries` submodule is required. Initialize it when cloning, or update submodules after an existing clone:

```sh
git clone --recurse-submodules https://github.com/kurosio/Teeworlds-MRPG-0.6.git
cd Teeworlds-MRPG-0.6

# For an existing checkout:
git submodule update --init --recursive
```

## Build on Linux

Requirements: CMake 3.16 or newer, a C/C++ compiler and build tools, Git, Python 3, MariaDB Connector/C development files, SQLite development files, and libcurl development files. OpenSSL development files are optional (they enable TLS support where configured). On Debian/Ubuntu:

```sh
sudo apt update
sudo apt install build-essential cmake git python3 libmariadb-dev libsqlite3-dev libcurl4-openssl-dev libssl-dev
```

Configure and build the server:

```sh
cmake -S . -B build -DSERVER_COMPILE=ON
cmake --build build --target mmoteeworlds_srv --parallel
```

The executable is `build/mmoteeworlds_srv` (or `build/mmoteeworlds_srv.exe` on Windows). The build does not start the server. To build the optional relay separately, use `cmake --build build --target mmoteeworlds_relay --parallel`.

On Windows, install Visual Studio with the C++ desktop workload, CMake, Python 3, MariaDB Connector/C, SQLite, and libcurl, then configure with CMake and build the `mmoteeworlds_srv` target. See the project CMake configuration for dependency detection and available build targets.

## Server setup

1. Install MariaDB and import `mrpg_shared.sql` and `mrpg_local.sql` as described in [docs/mariadb.md](docs/mariadb.md). A single-database setup is supported by leaving `sv_sql_local_database` empty; multi-server deployments should read the [SQL bridge guide](other/sql_bridge/README.md).
2. Review `autoexec_server.cfg`. It is an example configuration, **not a production-ready secure configuration**: change the RCON passwords, database credentials, server name, map, and other values for your deployment. Do not expose database or RCON passwords in public repositories or logs.
3. Place the required game data and map files in the paths expected by `storage.cfg`. The default server configuration uses `maps/main.map`.
4. Start the server from its deployment directory so that it can find its configuration, data, and maps. The server reads `autoexec_server.cfg` and accepts console command lines as command-line arguments; it does not use the relay's `-f <config>` option.

For optional cross-server event synchronization, build and configure `mmoteeworlds_relay` and set the `sv_sync_*` options on each game server. Read [docs/server_sync.md](docs/server_sync.md) before enabling it. Synchronization is disabled by default.

## Project documentation

- [MariaDB setup and schema layout](docs/mariadb.md)
- [Cross-server synchronization relay](docs/server_sync.md)
- [SQL bridge and replicated shared database](other/sql_bridge/README.md)
- [Multiworld map notes](maps/README.md)
- [Translations](server_lang/README.txt)

## License and history

This project is distributed under the terms in [license.txt](license.txt). The code is derived from Teeworlds; see the license file for the applicable copyright and license notices.
