# The database layer: pool, asynchronous API, registration and authentication

The server has exactly two required MariaDB endpoints, each with its own pool:
the **remote** shared authority (accounts and data shared across servers) and
the **local** database owned by this game server. The local schema contains six
tables: `tw_groups`, `tw_guilds_decorations`, `tw_guilds_history`,
`tw_guilds_invites`, `tw_houses_decorations`, and `tw_links`. A statement is
routed explicitly by its `Db::EKind`, normally selected from the table contract;
there is no one-endpoint fallback or SQL-comment routing.

Everything below lives in the code this document describes:

| File | What it is |
| --- | --- |
| `src/engine/server/db/db_types.h` | `EKind`, `SError`, `SEndpoint`, bound values, query options |
| `src/engine/server/db/db_result.h` | `CResultSet`: rows copied out of the client, safe to keep |
| `src/engine/server/db/db_driver.h` | the seam to the client: `IDbDriver` / `IDbConnection` |
| `src/engine/server/db/db_mariadb.cpp` | the only file that knows MariaDB Connector/C++ |
| `src/engine/server/db/db_pool.h` | connections, leases, limits, timeouts, shutdown |
| `src/engine/server/db/db_async.h` | workers, `CRequest`, `Db::Query`, `Db::Transaction` |
| `src/engine/server/db/db_startup.h` | `Db::StartFromConfig` / `Db::Shutdown`: the settings, both databases, the schema check |
| `src/game/server/core/components/accounts/account_db.h` | every statement of the registration and authentication pipeline |

## Using the asynchronous API

Two ways to run a statement, one result type, no resource to release.

```cpp
#include <engine/server/db/db_async.h>

// 1. A read, answered on the game thread. The first row is readable without
//    Next(); the result says what happened (Ok / HasRows / Changed / InsertId).
Db::Query(Db::EKind::Remote,
    "SELECT Password, PasswordSalt FROM tw_accounts WHERE Username = ?",
    Db::Bind(Login))
    .Then([pContext](Db::CResultSet&& Result)
    {
        if(!Result.Ok())
            return Fail(pContext, Result.Error().ToString()); // code, sqlstate, message
        if(!Result.HasRows())
            return Fail(pContext, "account not found");
        if(Result.GetString("Password") != Hash(Password, Result.GetString("PasswordSalt")))
            return Fail(pContext, "wrong password");
        Continue(pContext);
    });

// 2. A write; Changed() is "the statement matched a row", which is what a
//    compare-and-swap update has to know.
Db::Query(Db::EKind::Remote,
    "UPDATE tw_account_sessions SET Heartbeat = UNIX_TIMESTAMP() WHERE AccountID = ? AND SessionToken = ?",
    Db::Bind(AccountID, Token))
    .Then([pContext](Db::CResultSet&& Result) { OnActivated(pContext, Result.Changed()); });

// 3. Several statements, all or nothing: return true to commit. The generated
//    id of the first INSERT is the id of the whole transaction.
Db::Transaction(Db::EKind::Remote, [Data](Db::CTransaction& Tx)
{
    auto Account = Tx.Execute("INSERT INTO tw_accounts (Username, Password) VALUES (?, ?)",
        Db::Bind(Data.m_Login, Data.m_Hash));
    return Account.Changed() && Tx.Execute("INSERT INTO tw_accounts_data (ID, Nick) VALUES (?, ?)",
        Db::Bind(Account.InsertId(), Data.m_Nick)).Changed();
}).Then([](Db::CResultSet&& Result) { Finish(Result); });

// 4. Startup code that must have the answer now (never from a callback).
auto Tables = Db::Query(Db::EKind::Remote, "SHOW TABLES").Result();
```

That is the whole common surface: `Db::Query`, `Db::Transaction`,
`CRequest::Then/Cancel/Result`, and the getters of `CResultSet`. The same
`Query` request can be handled asynchronously with `Then()` or waited on with
`Result()` for startup-only code.

### SQL safety rules

- Statements with bindings use server-side prepared statements. Bound values
  are owned by the request and are never concatenated into SQL. The MariaDB
  driver checks the SQL placeholder count against the binding count before
  execution and returns an error on a mismatch.
- SQL structure must remain static. Table or column identifiers cannot be
  bound, so code that needs one maps inputs through a fixed allowlist (house
  tables, decoration table, guild upgrade fields). Dynamic `IN` lists contain
  only generated `?` placeholders; an empty list takes a valid no-op path.
- Text and binary bindings are distinct. `Db::Blob(...)` uses the driver's
  binary stream binding and preserves embedded NUL bytes; callers must not
  hex-encode data into SQL or interpolate it into a statement.
- Do not log or return bound values with SQL errors. The failed-query log stores
  the statement template, which contains placeholders rather than credentials
  or player-provided values.
- Writes are not retried automatically because a disconnect can leave the
  commit outcome unknown. Only an explicitly opted-in idempotent statement may
  use `SQueryOptions::Retries(n)`; transactions ignore that option and never
  replay their body.

Everything else has a default:

| What | Default | How to change it |
| --- | --- | --- |
| connection, pooling, reconnect, idle ping | handled inside | `Db::SExecutorConfig` at `Db::Start` |
| retries of a lost connection | reads 3, writes 0 by default; transactions never retry | `SQueryOptions().Retries(n)` only for idempotent writes |
| timeout of one request | 30 s | `SQueryOptions().Timeout(ms)` |
| wait before a write lands (delayed writes) | none | `SQueryOptions().Delay(ms)` |
| answering a read from an identical read in flight | off | `SQueryOptions().Coalesce()` |
| reading the generated id | only for `INSERT`/`REPLACE`, decided from the text | nothing to do |
| thread, lease, cancellation, cleanup | handled inside | nothing to do |

Properties that the API guarantees, and that the tests check:

* **The `Db::Query` / `Db::Transaction` callback runs on the game thread**, from
  `Async::RunMainThreadTasks()` in the server tick. A worker never touches game
  state. `CRequest::Result()` is the explicit blocking exception; remaining
  call sites are migration debt and must not be added to runtime gameplay paths.
* **A request owns its state.** Dropping the `CRequest` does not leak: the
  worker finishes on its own, the connection returns to the pool and the result
  is released. `Request.Cancel()` cancels delivery, not an SQL call already in
  progress; the callback receives a cancellation error, but a write may still
  have committed. Do not blindly retry a cancelled or timed-out write.
* **Timeouts are answers, not hangs.** `SQueryOptions().Timeout(ms)` bounds one
  request; a statement that is still running when the deadline passes is
  reported as a timeout instead of being waited for. Work whose deadline passed
  while it stood in the queue or waited for a connection is dropped before it
  runs. Opening a connection (including waiting for the connector's global
  connect lock) is bounded by `sv_sql_connect_timeout_ms` and the remaining
  request/acquire budget.
  A client deadline cannot undo SQL already executing: a timed-out write may
  have committed and its outcome is unknown. The server side limit
  `sv_sql_statement_timeout_ms` reduces that window by aborting long statements.
* **Identical reads share one statement.** With `SQueryOptions().Coalesce()` a
  read that is already in flight answers every later identical read (same text,
  same parameters, same endpoint), so a menu that every player opens at once
  costs one statement, not one per player. It is off by default and never used
  for a read that must see our own writes. Note that `Db::CVoteLoad` already
  single-flights the menu loads at the game level; the option is the safety net
  below it.
* **Connection loss is handled conservatively.** A statement that fails with
  2006/2013/2014 or SQLSTATE `08*` causes the broken connection to be discarded.
  Reads may retry on a fresh connection; writes do not retry by default because
  the server may have committed before the connection was lost. Transactions
  are never replayed, even if their commit response is ambiguous. The MariaDB
  client never transparently reconnects: only the pool may create a replacement
  session, so a reconnect cannot silently discard transaction state.
* **Parameters are bound, never spliced.** `Db::Bind(...)` owns its strings and
  the MariaDB connection uses server-side prepared statements. `Db::MaskQuery(...)`
  is the log form (literals become `?`), so credentials, nicknames and IPs never
  reach the console.
* **Both endpoints behave the same way.** `EKind::Remote` and `EKind::Local` are
  two executors with their own pools, workers and limits.
* **Independent requests may execute concurrently and complete out of submission
  order.** Use one `Transaction` for dependent writes, or submit the next step
  from the previous request's callback. Do not assume that FIFO queue insertion
  makes commits ordered when an endpoint has multiple workers. The transaction
  body itself runs sequentially on one worker/connection and must use only
  `Tx.Execute()`; waiting on another query to the same executor from that body
  can deadlock when its workers/connections are occupied.

`CRequest::Result()` blocks the calling thread. It remains for bootstrap/content
loaders while they are migrated to callback-driven stages; the remaining
production call sites must be removed before claiming a fully nonblocking game
loop. New work should use only `Db::Query` / `Db::Transaction` and handle errors
in the callback. An empty
`CRequest` is a no-op - it runs nothing and answers with an empty result - which
is how `AccountDb::RefreshSessionHeartbeats({})` avoids a statement.

The old `Database->Execute<...>` facade, `ResultPtr` and `CSqlString` are gone.
The SQL runtime has exactly two endpoint pools, and its only connection driver
is `CMariaDbDriver`; there is no implicit third or synchronous connection.

A statement that fails for good (after its retries, and unless it was cancelled)
is recorded once by the worker in `sv_sql_failed_log_file`, so a write the game
gave up on can still be found later.

## The pool

`Db::CDbPool` holds at most `m_MaxConnections` connections (idle + busy). A
connection lives in exactly one `CDbPool::CLease`; the lease returns it in its
destructor, so no code path can leak one or return it twice, and `Discard()`
drops a broken connection instead of pooling it.

* `Acquire(timeout)` never blocks forever: a full pool answers "no connection"
  after the configured wait, and a failing database is not hammered (connections
  are retried after `m_ConnectRetryDelayMs`).
* Idle connections older than `m_IdlePingMs` are pinged before reuse and dropped
  when dead: a MariaDB restart does not leave the server with dead connections.
* `Shutdown()` refuses new leases, waits for active ones up to a limit, closes
  everything and is idempotent. A lease that outlives the pool is still safe.
* `Stats()` reports open/idle/busy/created/reused/discarded/failed/timeouts; the
  tests use it to prove the limits.
* The pool and workers are sized together per endpoint: `sv_sql_pool_size` for
  remote and `sv_sql_local_pool_size` for local. Each pool owns only its
  endpoint's connections; no statement opens an ad-hoc connection. Remote
  latency and concurrent account loads determine remote pool sizing, while
  local state has an independent queue and capacity.

Configuration (`sv_sql_*` in `src/game/variables.h`): `sv_sql_pool_size`,
`sv_sql_queue_wait_timeout_ms`, `sv_sql_queue_max_size`,
`sv_sql_connect_timeout_ms`, `sv_sql_connect_retry_delay_ms`,
`sv_sql_query_timeout_ms`, `sv_sql_statement_timeout_ms`,
`sv_sql_socket_timeout_ms`, `sv_sql_slow_statement_warn_ms`,
`sv_sql_idle_ping_ms`, `sv_sql_debug`, and the endpoint settings (`sv_sql_host/port/user/
password/database`, `sv_sql_local_*`). Writes are never retried by global
configuration; a caller may opt an idempotent single statement into retries
with `SQueryOptions::Retries(n)`. Transactions never retry.

### Detailed SQL diagnostics

`sv_sql_debug 1` enables per-request diagnostics for both endpoints: startup
configuration and schema routing, queue admission/refusal and wait time,
connection attempts/acquisition, statement duration and outcome, pool counts,
retry details, and each transaction's BEGIN/statements/COMMIT or ROLLBACK. Set
`sv_sql_debug 0` (the default) to turn the extra output off. This is intentionally
verbose; enable it while reproducing an issue, then disable it.

Diagnostic SQL is a shortened query shape. Bound parameter values are never
written, and quoted literals, numeric literals and SQL comments are redacted;
row contents and credentials are not logged. Error diagnostics prefer the
server error code and SQLSTATE so duplicate-key and connection failures can be
correlated without emitting parameter values.

## Reading a busy queue

The workers own the diagnostics, so a slow database leaves a trace of itself
instead of a silent backlog:

| Message | Meaning | What to do |
| --- | --- | --- |
| `Database queue is long: N requests waiting (...)` | more work is queued than `sv_sql_queue_warn_size`; printed once per 10 s with the busy/open connections, the high water mark and the slowest statement so far | raise `sv_sql_pool_size` (each connection is a worker), or find the statement in the next line |
| `A statement took Xms: '...'` | a statement is slower than `sv_sql_slow_statement_warn_ms` (default 1 s); printed once per second | the text names the query: add the missing index, narrow the `SELECT`, add a `LIMIT` |
| `A statement finished after its request timed out: Xms, '...'` | the statement ran past its deadline (the request was already answered with a timeout) | usually the same query as above; with `sv_sql_statement_timeout_ms` the server aborts it instead |
| `The database queue is back to normal: ...` | the backlog drained; the line carries the high water mark and the longest wait of the burst | nothing, it is the "it recovered" marker |
| `The <endpoint> database ran N statements (...)` | the summary printed on shutdown: timeouts, dropped requests, coalesced reads, longest wait and slowest statement of the whole run | the numbers to compare two configurations with |

A queue that stays long on a healthy but distant remote database usually points
to network round-trip latency, insufficient remote pool capacity, or queries
that can be batched. The local queue is independent and should not be enlarged
to compensate for remote latency.

## Keeping the leaderboards cheap

The four top lists and the two record lists are the statements that run while
everybody is online, and they were the expensive ones:

* they select `*` from `tw_accounts_data`, which carries `EquippedSlots`,
  `EquipmentPresets` and `Achievements` (`longtext`) - a leaderboard read used to
  copy two blobs per row. They now select only the columns the menu shows
  (`Nick`, `Rating`, `Bank`, `Name`, `Level`, `Exp`, `UserID`, `Time`, `Score`).
* the rating leaderboard and record lists sort/group without an index. The
  schema dump includes targeted indexes for those access paths. On a database
  created from an older dump, apply this migration once (the individual
  `DungeonID`, `Seconds`, and `WorldID` indexes are replaced by composites;
  replace the existing `tw_dungeons_records_ibfk_1` index with a covering
  `(UserID, Time)` index for the grouped best-player query):

```sql
ALTER TABLE tw_accounts_data
  ADD INDEX idx_accounts_data_rating (Rating);
ALTER TABLE tw_accounts_professions
  ADD INDEX idx_accounts_professions_owner_profession (UserID, ProfessionID);
ALTER TABLE tw_dungeons_records
  ADD INDEX idx_dungeons_records_dungeon_time (DungeonID, Time),
  ADD INDEX idx_dungeons_records_user_time (UserID, Time);
ALTER TABLE tw_dungeons_records
  DROP INDEX tw_dungeons_records_ibfk_1,
  DROP INDEX DungeonID,
  DROP INDEX Seconds;
ALTER TABLE tw_rhythm_records
  ADD INDEX idx_rhythm_records_leaderboard (WorldID, Difficulty, Score),
  ADD INDEX idx_rhythm_records_lookup (UserID, WorldID, Difficulty, Score);
ALTER TABLE tw_rhythm_records
  DROP INDEX WorldID;
```

The professions index serves both per-account loading and the `(UserID,
ProfessionID)` update predicate. The rhythm indexes serve the leaderboard and
per-player lookup; their trailing `Score` columns also cover the grouped score
aggregate. The dungeon composites cover the filtered leaderboard and grouped
best-player aggregate without fetching table rows. These are non-unique
indexes: they do not change behavior or reject existing duplicate records.

`ORDER BY Bank + 0` cannot use an index at all: `Bank` is a `varchar`, and the
`+ 0` casts every row. If the wealth leaderboards are the slow ones, store the
number instead of formatting it - a generated, indexed column keeps the menu
cheap and the column readable:

```sql
ALTER TABLE tw_accounts_data
  ADD COLUMN BankValue DECIMAL(20, 0) AS (CAST(Bank AS DECIMAL(20, 0))) STORED,
  ADD INDEX idx_accounts_data_bank (BankValue);
-- and ORDER BY BankValue DESC in the wealth query
```

## The registration and authentication pipeline

`account_db.h` holds every statement of the pipeline, with the parameters bound:

| Function | Statement |
| --- | --- |
| `LookupNickname` | is the nickname taken? (remote canonical database) |
| `Authenticate` | nickname + credentials + profile + active ban in one row |
| `LoadAccount` | the account row of `tw_accounts_data` from the remote database |
| `Register` | create `tw_accounts` and `tw_accounts_data` in one transaction |
| `DeleteAccount`, `DeleteAccountByCredentials` | remove an account that a failed registration left behind |
| `UpdateCredentials`, `UpdateLoginStamp` | claim an existing account, stamp the login |
| `ReadSession`, `ClaimSession`, `ActivateSession`, `ReleaseSession`, `RemoveSession`, `RefreshSessionHeartbeats`, `FetchSessionTokens` | who may play an account |

The flow (`account_auth.cpp`, then `account_session.cpp`), all of it asynchronous:

1. `AccountAuth::Login` → `AccountDb::Authenticate` (one statement, one row).
2. the row is checked once: nickname exists, credentials match the stored hash,
   guest timeout code, ban.
3. `AccountSession::ReadRecord` → `TryClaim` (compare-and-swap on the token) →
   `AccountDb::LoadAccount` → `CAccountData::Init` →
   `AccountSession::BeginTakeover` → the account load → the session row is
   activated (`AccountDb::ActivateSession`).
4. registration: `AccountDb::LookupNickname` → `AccountDb::Register` (one
   transaction, id from the first insert) → the same authorization path.
   A player that disappears mid-registration has their account deleted with
   `AccountDb::DeleteAccount`.
5. disconnect and logout: `AccountDb::RemoveSession` / `AccountSession::Release`
   (only the token that owns the row matches), `AccountDb::UpdateLoginStamp` was
   fire and forget.

Session ownership is a compare-and-swap in the database, not a decision taken on
a read: `ClaimSession` matches only while the row still carries the token that
was read, so two servers cannot both take an account, and a decision taken on an
outdated read is refused instead of overwriting a newer owner. A failed read is
never mistaken for "no session".

## Tests

```sh
ninja testrunner
./testrunner --gtest_filter='CDb*:CAccount*'
```

* `src/test/db.cpp` — the real MariaDB client against the standalone fake server
  that speaks the wire protocol (`src/test/fake_mariadb.py`, started by
  `src/test/fake_mariadb_server.py`): pool reuse and hard limits, acquire timeout, shutdown,
  a lease that outlives the pool, callbacks on the game thread, cancellation,
  request timeout, a full queue, statement errors, server restart and recovery,
  bound parameters, commit/rollback, 40 concurrent requests on 3 connections,
  delayed writes, the first row being readable without `Next()`, an empty result
  being safe to read, graceful stop, and the two endpoints being independent.
  Tests skip themselves (with a message) where no `python3` is available.
* `src/test/db_account.cpp` — a scripted driver records every statement and the
  values that were bound to it: registration in one transaction, the data row
  linked to the generated id, rollback on failure, cleanup, nickname lookup and
  login parsing, ban and timeout-code handling, compare-and-swap session claims,
  batched heartbeats, concurrent logins, cancellation, connection loss, and that
  no value ever appears inside a statement text.

`testrunner` builds these sources itself (`TESTS_DB_SRC` in `CMakeLists.txt`), so
the runner needs no server build to test the layer.

## Database and generic background work

Database statements use the two endpoint-specific pools. Generic background
work (including file I/O) uses a separate process-wide worker pool and does not
consume a database connection or queue slot. Both deliver completions through
the game-thread task pump:

```cpp
// background work without a statement; work runs on a worker, Done on the tick
Async::Run([] { HeavyButBounded(); }, [] { ReportOnTheGameThread(); });

// whole-file I/O; the answer arrives on the game thread
AsyncFile::Read(Storage(), "server_data/x.txt", IStorageEngine::TYPE_ABSOLUTE,
    [](AsyncFile::SReadResult&& Result) {
        if(Result.Ok()) Apply(Result.m_Content);
        else if(!Result.Missing()) ReportReadError(Result.m_Error);
    });
AsyncFile::Write(Storage(), "server_data/x.txt", IStorageEngine::TYPE_ABSOLUTE, Build());
AsyncFile::Write("server_data/quests/42.json", Build());   // plain path
AsyncFile::Remove("server_data/prison/42.txt");
```

Generic-work lifecycle and delivery:

* **Coordinated shutdown.** `Db::Shutdown()` stops the database executors and
  drains generic work before server-owned storage is destroyed, then drops
  pending completions with `Async::StopMainThreadTasks()`.
* **One delivery rule.** Generic background completions are queued for the game
  thread and run from `Async::RunMainThreadTasks()`. `Async::Run` never executes
  rejected or unavailable work inline; it is independent of either DB endpoint.
* **Ordering per file.** Writes and removals of the same file cannot overtake
  each other: while one runs, the newest request waits and runs next. The file
  always ends with the last content passed and no write is dropped.
* **Worker ownership.** Generic work is accepted by its own process-wide
  scheduler and is drained during orderly server shutdown. Exceptions are
  logged; a supplied completion is still posted to the main-thread pump.

## Menus never wait

A menu is drawn inside a tick, so it cannot wait for anything. Lists that need a
read (mailbox, letters, best players, dungeon and rhythm records, top lists,
rhythm difficulties) are drawn from a cache:

1. the first draw starts the read and is rendered without the missing rows;
2. the answer fills the cache on the game thread and re-sends the menu
   (`m_VotesData.UpdateVotesIf(...)`);
3. entries have a short lifetime (20 s for record lists) or are invalidated by
   the operation that changes them (mail claim, delete).

`CVotePlayerData::UpdateVotes` only requests a redraw; the redraw itself runs
from the tick in `ApplyVoteUpdaterData()`, so a menu is never built while it is
being read and no thread is needed to defer it.

## Migrated state

Every SQL statement of the server goes through this layer: accounts, sessions,
inventory, quests, guilds, houses, mails, auctions, warehouse, crafting,
professions, achievements, bots, worlds, aethernet, duties, rhythm, dungeon and
the engine's own startup/shutdown.

What a player triggers while playing is asynchronous - login, registration,
profile and equipment saves, item moves and claims, mails (list, read, claim,
delete), voucher redemption, password/PIN/nickname changes, auction slots,
group and guild creation, guild ranks, dungeon and rhythm records, ban
administration: the game thread queues the work and the completion arrives from
`Async::RunMainThreadTasks()`.

Per-account loading is coordinated by `CAccountData::AddLoadStage()` and
`RunLoadStages()`. Each loader registers a named asynchronous stage and may name
its prerequisites; independent stages (professions, achievements, inventory,
quests, skills, aethers, bonuses, prison term and rating) run concurrently, while
dependent stages wait for their prerequisites. The coordinator joins every
stage, reports a failed read once, and only then calls
`CAccountManager::FinishAccountLoad()`. `CPlayer::Tick()` gates on
`IsReadyToPlay()`, so no menu, packet or match is served from half-loaded data.
If the claim deadline passes first, the session module (`account_session.cpp`)
releases the reservation and kicks the player. A new account-data loader should register a
stage during `OnPlayerLogin()` or `CAccountData::RegisterLoadStages()`, name any real
ordering requirements in its dependency list, and call its completion exactly
once after applying its result.

The current loading boundaries, findings, and extension rules are documented in
[`account_loading_architecture.md`](account_loading_architecture.md).

Per-account data files (`server_data/account_bonuses`, `account_prison`, rating
history), the wiki content, the period stamps, the leveling/attribute trackers
and the trackers' periodic saves no longer block: reads answer on the game
thread, writes leave it at once.

### Intentionally synchronous

* **Startup and content loading.** `Db::Query(...).Result()` in `OnPreInit`,
  `OnInitWorld`, `Init*`, world/guild/house/warehouse/bot loading and
  `Db::StartFromConfig()` - the server is not accepting players yet and the rows
  build the object graph the world starts from. The same code runs when a guild
  arrives through a sync event, where the read is a few milliseconds against a
  database whose connection is already validated.
* **The rhythm map of a round.** `LoadDanceMapData` reads one small JSON when a
  rhythm world is created (`OnInit`, part of the synchronous map loading of this
  engine) and when a player joins with another difficulty. The note timings have
  to be in place before the warmup ends, and an asynchronous load would have to
  gate the round start on the answer - a gameplay change that the reading of a
  few milliseconds does not justify.
* **Language tables.** `CLocalization::Init()` creates every language from
  `server_lang/index.json` and loads its file once, at start-up; `Reload()` does
  the same on the restart path. Translating a message never touches the disk any
  more - the lookup used to retry the file read for every message when a
  language had failed to load.
* **Quest progress files** (`server_data/quests/*.json`). The quest step treats a
  failed write as "the step did not happen" and reverts itself
  (`quest_step_data.cpp`), so the write has to answer before the step is
  finished. The files are small; the same code path is the only place that
  depends on the answer of a write.
* **The start-up file reads that remain.** `CLevelingTracker::LoadTrackingData()`
  and `CAttributesTracker::LoadTrackingData()` (called from
  `CAccountListener::Initialize()`), `CGameControllerTutorial::OnInit()` and
  `CLocalization::Init()` read their files once while the server is coming up;
  their contents are in memory before the first player can connect, and every
  later write of the same file is asynchronous.
* **`reload_localization`.** The command rewrites the language files and rebuilds
  the language table while chat messages read it. Moving it off the game thread
  needs a snapshot-and-swap of the whole localization subsystem; until then the
  command stays on the tick and is serialized by `gs_LocalizationSyncMutex`
  against the periodic `sync_lines` maintenance, which does run on the workers.
* **The path finder.** A dedicated long-lived worker with its own queue, because
  a path search is unbounded work that must not occupy a database worker.
