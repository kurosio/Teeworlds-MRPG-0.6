# Discord bridge

One bridge process connects **any number of game servers** to one Discord
channel through their external consoles (Econ).

```
game server [EU] --econ--\
game server [RU] --econ---+--> bridge (main.py) <--> Discord channel
game server [CN] --econ--/
```

- **Game → Discord:** the public chat of every server is posted to the channel
  under the tag of its server: `[EU] nickname (🇩🇪)`, with the player's skin as
  the avatar. Server messages are posted as `[EU] Server`.
- **Discord → game:** a message written in the channel is said on **all**
  servers at once as `[DS] Author: text`.
- **Status:** one message in the status channel with an embed and a join button
  per server.

Servers are added in the config only, the code does not change.

**Files:** `main.py` (entry point), `bridge.cfg.example` (config template),
`config.py` (config loader), `services/` (Econ client, server hub, master
server status), `discord_bot/` (bot, cogs, webhook), `tests/`.

## Setup

1. Enable the external console on every game server (autoexec):

   ```
   ec_port 7310
   ec_password "a-long-random-password"
   ec_bindaddr "localhost"
   ```

   The Econ password travels in plain text and gives full console access. For a
   server on another machine keep `ec_bindaddr` on a private address (VPN, SSH
   tunnel) or firewall the port to the bridge host. Never open it to the internet.

2. Create the Discord bot: enable **Message Content Intent**, invite it with the
   `bot` and `applications.commands` scopes, and give it *View Channel*, *Send
   Messages*, *Manage Webhooks* and *Add Reactions* in the bridge channel
   (*Embed Links*, *Read Message History* and *Manage Messages* in the status
   channel).

3. Install and configure:

   ```sh
   pip install -r requires.txt
   cp bridge.cfg.example bridge.cfg && chmod 600 bridge.cfg
   python3 main.py --check-config   # validates the file and lists the servers
   python3 main.py
   ```

   `bridge.cfg` is git-ignored. The path can be given as an argument
   (`python3 main.py /etc/mrpg/bridge.cfg`) or in `DISCORD_BRIDGE_CONFIG`; the
   token and the default Econ password can come from `DISCORD_BOT_TOKEN` and
   `ECON_PASSWORD`. `../run_bridge_stack.sh` starts the bridge together with
   skins_api and checks the config first.

## Adding a server

Add a section to `bridge.cfg` and restart the bridge:

```ini
[econ]
host = 127.0.0.1
password = a-long-random-password

[server:EU]
name = Europe
econ_port = 7310
game_address = 103.27.156.253:8310

[server:RU]
name = Russia
econ_host = 10.0.0.2
econ_port = 7310
econ_password = another-password
game_address = 203.0.113.10:8310
```

| Option | Meaning |
| --- | --- |
| `[server:TAG]` | The tag shown on Discord in front of the nickname (`[TAG] nickname`). 1–16 characters, no spaces, unique. |
| `econ_port` | `ec_port` of the server. Required. |
| `econ_host`, `econ_password` | Default to `host` and `password` of `[econ]`. |
| `game_address` | Public `ip:port` as registered on the master server. Gives the status embed and the flags/skins of the players; without it the server is bridged with default avatars. |
| `name` | Name in the status message and in `/bridge_servers`. Defaults to the tag. |
| `enabled` | `false` keeps the section but does not connect. |

Every server has its own connection: a server that is down is retried in the
background (`reconnect_delay`) and does not affect the others. All other
options are described in `bridge.cfg.example`; a typo in an option name is
reported at start.

## Behaviour

- A Discord message is delivered to every connected server. It gets a ❌
  reaction only when no server received it; servers that are offline are
  written to the log and shown by `/bridge_servers`.
- Long Discord messages are split into several chat lines; each line starts
  with `discord_tag`, which is also how the bridge ignores its own messages
  coming back from the servers.
- Messages starting with `command_prefix` and messages of bots are not relayed.
- Team (guild) chat is never relayed.
- Game chat is queued for Discord (`queue_size`); if Discord cannot keep up, the
  oldest messages are dropped instead of delaying the newer ones.

Slash commands (role `admin_role_id`, or the Administrator permission):

| Command | Action |
| --- | --- |
| `/bridge_servers` | List the servers and the state of their links. |
| `/bridge_mute name`, `/bridge_unmute name`, `/bridge_mutelist` | Hide the messages of a player from Discord (on all servers). |

In game, `/bridge_disable` and `/bridge_enable` let a player hide his own
messages from Discord; the choice applies to all bridged servers. Both lists are
stored next to `main.py` (`muted_players.json`, `bridge_disabled.json`).

## Migrating from `config.py`

Settings used to be constants in `config.py`; they are now in `bridge.cfg`.

| Old constant | New option |
| --- | --- |
| `DISCORD_BOT_TOKEN`, `DISCORD_CHANNEL_ID`, `COMMAND_PREFIX`, `WEBHOOK_NAME`, `ADMIN_ROLE_ID` | `[discord]` `token`, `channel_id`, `command_prefix`, `webhook_name`, `admin_role_id` |
| `TEESERVER_IP`, `TEESERVER_ECON_PORT`, `ECON_PASSWORD` | `[server:TAG]` `econ_host`, `econ_port`, `econ_password` (or the `[econ]` defaults) |
| `GAME_SERVER_ADDRESS` | `[server:TAG]` `game_address` |
| `SERVER_TAG_TEEWORLDS`, `MAX_MESSAGE_LENGTH`, `SKIN_API_URL`, `WEBHOOK_SERVER_SKIN`, `WEBHOOK_UNKNOWN_SKIN`, `WEBHOOK_SEND_TIMEOUT`, `PLAYER_CACHE_TIMEOUT` | `[bridge]` `discord_tag`, `max_message_length`, `skin_api_url`, `server_skin`, `unknown_skin`, `webhook_send_timeout`, `player_cache_timeout` |
| `STATUS_CHANNEL_ID`, `STATUS_MESSAGE_ID`, `STATUS_UPDATE_INTERVAL_MINUTES`, `MASTER_SERVER_API_URL`, `SHOW_JOIN_BUTTON` | `[status]` `channel_id`, `message_id`, `update_interval_minutes`, `master_url`, `show_join_button` |
| `ECON_CONNECT_TIMEOUT`, `ECON_AUTH_TIMEOUT`, `ECON_READ_TIMEOUT`, `RECONNECT_DELAY` | `[econ]` `connect_timeout`, `auth_timeout`, `read_timeout`, `reconnect_delay` |
| `LOG_FILE`, `LOG_LEVEL`, `STATUS_UPDATE_LOG_LEVEL` | `[logging]` `file`, `level`, `status_level` |

`POLL_INTERVAL` is gone (the connections are read as data arrives). To keep
using the webhook the old bridge created, set `webhook_name` to its name.

## Tests

```sh
python3 -m unittest discover -s tests -v
```

The tests run the Econ client and the multi-server hub against a fake Econ
server (`tests/fake_econ.py`) that parses commands the way the game console
does. The Discord side is tested with mocked Discord objects and is skipped
when `discord.py` is not installed.
