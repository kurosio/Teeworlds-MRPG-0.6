# Cross-server synchronization through the relay

The game servers of MRPG no longer synchronize through the `tw_server_sync`
table. Instead there is a separate small program, the **relay**
(`mmoteeworlds_relay`, `src/relay/`), which only retransmits messages:

```
   game server 1 ──┐
   game server 2 ──┼── one outgoing TCP connection each ──▶  relay
   game server N ──┘                                          │
                                                              ▼
                              event of one server is forwarded to all others
```

* Connection direction: **the game servers connect out to the relay**. The relay
  needs no list of servers and the servers need no open port for it, so the same
  setup works behind NAT. One listening port on the relay side is enough.
* The relay is a **plain retransmitter**: it does not read event payloads, does
  not execute anything and stores nothing on disk. Restarting it only costs the
  connections, the servers reconnect on their own.
* A server only processes an event of *another* server (`server` field differs)
  and drops duplicates (`id` per sender).

## Trust boundary

The relay listens on a TCP port, so **anyone who can reach it** can send frames.
Both sides therefore treat every received byte as hostile input:

* a frame is at most 8 MiB (`FRAME_MAX_LENGTH`), a longer one closes the
  connection instead of being buffered;
* every JSON field is read with an explicit type check and a default
  (`relay/relay_json.h`), so a wrong type or a number that does not fit into an
  `int` never throws and never aborts a server;
* server names, language tags and rejection reasons are cut to a fixed length
  and stripped of control characters before they reach the log;
* an event name is limited to 64 characters and must not contain control
  characters, and the guild ID is range-checked, because both end up in the
  database code of the receiving server;
* a client that does not read its messages is dropped after
  `MAX_SEND_BUFFER` (4 MiB) instead of growing the memory of the relay;
* `relay_password` is compared as a whole string and protects the same way a
  server password does. It is not encryption: over the internet put the relay
  behind a tunnel (WireGuard, SSH) or bind it to a private address.
* both programs survive a defect in the handling of a message: the relay catches
  what escapes `CRelay::Update()` and keeps serving the other servers,
  `CServerSync::OnTick()` drops the connection instead of ending the game.

### Starting problems

| symptom | cause |
| --- | --- |
| `could not create the listening socket ...` | the port is taken by another process, or `bindaddr` is not an address of this host. The engine logs the exact `errno` line above it. |
| relay runs but no server connects | wrong `relay_port`, a firewall, or `bindaddr` limited to loopback while the servers are remote. |
| `::1` or `::` as `bindaddr` | write the address of one family; `bindaddr ::1` and `bindaddr "[::1]"` both work, and only that family is bound. |

## What a message carries

The message carries **only the event and the IDs** — no guild data:

```json
{ "id": 17, "event": "guild_create", "server": 1, "world": 0, "value": 12 }
```

`id` is the message ID (unique per sender, used to drop duplicates), `server` is
`sv_sync_server_id` of the sender, `world` the world that produced the event and
`value` the ID of the changed object — for the guild events, the guild ID.

The event names are constants of `server_sync.h` (`syncevents::GUILD_CREATE`,
`syncevents::GUILD_DISBAND`), so a sender and the `OnSyncEvent()` handler of the
receivers cannot drift apart.

The receiving server applies the event with the same handler as before
(`OnSyncEvent`): for `guild_create` it selects the row from its own `tw_guilds`
by `value` and calls `InitGuildFromRow()`, for `guild_disband` it finds the guild
by ID and removes it. The relay does not look into the payload.

This means the guild tables must be shared between the servers (the same
database, or a replicated one) — otherwise the `SELECT` on the receiving server
finds nothing. That is the assumption the synchronization has always made.

Delivery is **at least once**: a message is kept until its bytes are on the
socket, so a connection that dies mid-write re-sends it after the reconnect. The
handlers are idempotent (a `guild_create` for a guild that is already loaded is
ignored, a `guild_disband` for an unknown ID does nothing) and the `id` of the
sender drops the repeats.

## Source layout

Three small files are shared by the relay and the game server
(`src/relay`), so neither side re-implements the same thing:

| file | contents |
| --- | --- |
| `relay_protocol.h` | frame format, message types, `CFrameReader`, limits |
| `relay_json.h/.cpp` | throwing-free JSON helpers for untrusted payloads |
| `relay_stream.h/.cpp` | non-blocking send/receive of frames and the shared clock |

Everything else is one program: `relay.cpp`/`relay.h`/`main.cpp` for the relay
and `src/engine/server/server_sync.cpp` for the game server.

## Adding a new event

1. Add the name to `syncevents` in `src/engine/server/server_sync.h`.
2. Send it from the component that changes the data:
   `Sync->Push(syncevents::MY_EVENT, GS()->GetWorldID(), ObjectID);`
3. Add a branch to `OnSyncEvent()` of that same component. It must load the
   object **from the database by `Req.Value`** and must be safe to run twice:
   the delivery is at-least-once and the event can arrive after the local state
   already matches the database.

No change in `src/relay` is needed: the relay forwards unknown event names
unchanged, it only knows the envelope.

## Configuration

Relay (`relay.cfg` next to the binary, or `-f <file>`):

```
relay_port 8383
relay_max_clients 64
relay_password ""        # must match sv_sync_relay_password
relay_name "mrpg-relay"
#bindaddr 127.0.0.1      # optional
```

Every game server (`autoexec_server.cfg`):

```
sv_sync_server_id 1               # unique per server, 0 = disabled
sv_sync_relay_host "relay.local"  # empty = disabled
sv_sync_relay_port 8383
sv_sync_relay_password ""
sv_sync_relay_reconnect_ms 5000
```

The console of the relay accepts these settings on the command line as whole
config lines, so a value has to be quoted:
`mmoteeworlds_relay "relay_port 8383" "relay_password secret"`.

Messages produced while the relay is unreachable stay in the queue (up to 10000)
and are sent after the reconnect. `Sync->IsConnected()` and
`Sync->PendingCount()` are available for console output or metrics.

## Wire protocol

Frames, both directions (`src/relay/relay_protocol.h`):

```
+---------------------+------------+---------------------------+
| length: uint32 (BE) | type: u8   | payload: length-1 bytes    |
+---------------------+------------+---------------------------+
```

`length` counts the type byte plus the payload, so the minimum is 1. The payload
of every message is UTF-8 JSON; the limit of a frame is 8 MiB.

| type | name | direction | payload |
| --- | --- | --- | --- |
| 1 | `HELLO` | server → relay | `{"protocol":1,"server_id":1,"password":"...","name":"..."}` |
| 2 | `WELCOME` | relay → server | `{"server_id":1,"relay":"mrpg-relay","peers":[2,3]}` |
| 3 | `REJECTED` | relay → server | `{"reason":"wrong password"}` (connection is closed) |
| 4 | `EVENT` | server ↔ relay | see above; forwarded byte for byte to all other servers |
| 5/6 | `PING`/`PONG` | both | `{"t":<ms>}` every 15 s, a silent connection is dropped after 60 s |
| 16 | `LOCALIZATION_HASH` | server → relay | `{"language":"ru","hash":"<sha256>"}` (groundwork) |
| 17 | `LOCALIZATION_REQUEST` | relay → server | `{"language":"ru"}` (not implemented) |
| 18 | `LOCALIZATION_DATA` | server ↔ relay | raw `.txt` file (not implemented) |

The relay rejects a duplicate `server_id`, a wrong password and anything sent
before the handshake, and drops a connection on a malformed frame. The server
uses a non-blocking socket and does all the work in `Sync->OnTick()`, which the
game calls once per `sv_sync_server_interval` seconds, so the game loop never
waits on the network.

## Localization groundwork

Not implemented, but the ground is prepared:

* `CMmoController::CollectLocalizationHashes()` (`mmo_controller.cpp`) returns
  the SHA-256 of every `./server_lang/<language>.txt` of the language index; the
  mother language `en` has no file and is skipped.
* The game registers it with `Sync->SetLocalizationHashProvider(...)`; on a
  successful handshake `CServerSync` sends one `LOCALIZATION_HASH` frame per
  language, so the relay sees the real hashes of every server.
* The relay stores the last hash of every server per language in
  `m_LocalizationHashes` and logs it, so the comparison can be added without
  touching the protocol.
* Planned flow: every server sends its hashes → the relay compares them and
  picks the newest file as the source → `LOCALIZATION_REQUEST` asks that server
  for the file → it answers with `LOCALIZATION_DATA` → the relay sends the file
  to every server whose hash differs → those servers replace their local file
  and reload the language. This removes the need to run `sync_lines_for_translate`
  (`CMmoController::SyncLocalizationFiles()`, a detached thread on every server)
  just to keep the files equal.

## Tests

`scripts/test_server_sync.py` starts the relay and checks:

1. the relay itself with raw TCP sockets: handshake, forwarding only to other
   servers, byte-for-byte payloads, `PING`/`PONG`, wrong password, duplicate
   server id, event before the handshake, malformed frame, unknown type, a
   mistyped HELLO field and a full relay;
2. the lifecycle of the relay itself: a clean SIGTERM shutdown (a double free
   used to crash it on every exit), a busy port that ends with an error instead of
   a relay that listens nowhere, `bindaddr` of a single family, and numbers a peer
   may send that do not fit into an integer;
3. the game server side with `test_server_sync`, a small program around the real
   `CServerSync`: an event queued with `Push()` before the connection exists is
   delivered through the relay to another server with its event name and value;
   without a relay it stays in the queue, with a relay that starts later it is
   sent as soon as the connection exists, and after a relay restart the server
   reconnects and keeps working.

```bash
cmake --build build --target mmoteeworlds_relay test_server_sync
python3 scripts/test_server_sync.py build/mmoteeworlds_relay build/test_server_sync
```

`test_server_sync` is a test target only (`EXCLUDE_FROM_ALL`), it is not part of
the packages.

## Removed

* `CServerSync::Push` no longer writes to a table; `TW_SERVER_SYNC_TABLE` is
  gone. The table itself can be dropped from existing databases:
  `DROP TABLE IF EXISTS tw_server_sync;`
* The `tw_server_sync` code is gone; the connection handles the timing itself.
  `sv_sync_server_interval` still decides how often `CServerSync::OnTick()` runs
  and `sv_sync_relay_reconnect_ms` the delay between the reconnection attempts.
* `CServerSync::PushEvent()` was folded into `Push()`, which now returns the
  message ID itself.
* The same socket, clock and JSON plumbing no longer exists twice:
  `relay_stream.h/.cpp` owns the non-blocking send/receive and the millisecond
  clock, `relay_json.h/.cpp` the defensive payload parsing, both shared by the
  relay and the game server. Event names and deduplication are validated in one
  place as well (`CServerSync::IsValidEventName()`, `CServerSync::IsDuplicate()`).
