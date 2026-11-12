"""
Tests for the Teeworlds <-> Discord bridge (standard library unittest).

    cd web-tools/tools/discord && python3 -m unittest discover -s tests -v

Config, Econ parsing/escaping, the Econ client and the multi-server hub are
tested against a fake Econ server (tests/fake_econ.py) and need nothing but the
standard library. The Discord side (bridge and status cogs) is tested with a
real, never logged in bot object and mocked Discord objects; those tests are
skipped when discord.py is not installed.
"""
import asyncio
import configparser
import dataclasses
import logging
import os
import sys
import tempfile
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

logging.basicConfig(level=os.environ.get("TEST_LOG", "CRITICAL"))

import config as cfg  # noqa: E402
from fake_econ import FakeEconServer, TIMESTAMP, parse_command, split_commands  # noqa: E402
from models.econ import EconBridgeToggle, EconChatMessage  # noqa: E402
from services.econ_client import EconClient, escape_console_string, parse_econ_line  # noqa: E402
from services.server_hub import ServerHub  # noqa: E402
from utils.helpers import format_discord_username, format_game_lines, split_message  # noqa: E402

try:
    import discord  # noqa: E402
except ImportError:  # pragma: no cover
    discord = None

BASE_CONFIG = """
[discord]
token = test-token
channel_id = 100

[status]
channel_id = 200

[logging]
status_level = CRITICAL

[econ]
password = secret
"""


def make_config(servers_ini: str, base: str = BASE_CONFIG, env=None) -> cfg.BridgeConfig:
    parser = configparser.ConfigParser(interpolation=None)
    parser.read_string(base + servers_ini)
    return cfg.parse_config(parser, env or {})


def config_for(servers, **econ) -> cfg.BridgeConfig:
    """Config for fake servers: {tag: FakeEconServer or port}."""
    econ_options = {"connect_timeout": 2, "auth_timeout": 2, "read_timeout": 30, "reconnect_delay": 1}
    econ_options.update(econ)
    base = BASE_CONFIG + "".join(f"{key} = {value}\n" for key, value in econ_options.items())
    ini = ""
    for index, (tag, server) in enumerate(servers.items()):
        port = server if isinstance(server, int) else server.port
        ini += f"\n[server:{tag}]\necon_port = {port}\ngame_address = 10.0.0.{index + 1}:8303\n"
    return make_config(ini, base)


async def wait_until(predicate, timeout: float = 3.0):
    deadline = asyncio.get_running_loop().time() + timeout
    while not predicate():
        if asyncio.get_running_loop().time() > deadline:
            raise AssertionError("condition not met in time")
        await asyncio.sleep(0.01)


# --- config ----------------------------------------------------------------------------
class ConfigTests(unittest.TestCase):
    def problems(self, servers_ini, base=BASE_CONFIG, env=None):
        with self.assertRaises(cfg.ConfigError) as caught:
            make_config(servers_ini, base, env)
        return caught.exception.problems

    def test_many_servers(self):
        config = make_config("""
[server:EU]
name = Europe
econ_port = 7310
game_address = 1.2.3.4:8310

[server:RU]
econ_host = 10.0.0.2
econ_port = 7310
econ_password = other

[server:CN]
econ_port = 7311
enabled = false
""")
        self.assertEqual([server.tag for server in config.servers], ["EU", "RU"])
        eu, ru = config.servers
        self.assertEqual((eu.name, eu.econ_host, eu.econ_port, eu.econ_password, eu.game_address),
                         ("Europe", "127.0.0.1", 7310, "secret", "1.2.3.4:8310"))
        self.assertEqual((ru.name, ru.econ_host, ru.econ_password, ru.game_address),
                         ("RU", "10.0.0.2", "other", None))
        self.assertEqual(eu.label, "[EU]")
        self.assertIs(config.get_server("eu"), eu)
        self.assertEqual(config.discord.channel_id, 100)
        self.assertEqual(config.status.channel_id, 200)
        self.assertEqual(config.bridge.discord_tag, "[DS]")

    def test_passwords_are_not_in_repr(self):
        config = make_config("[server:EU]\necon_port = 7310\n")
        self.assertNotIn("secret", repr(config))
        self.assertNotIn("test-token", repr(config))

    def test_environment_overrides(self):
        base = BASE_CONFIG.replace("token = test-token", "token = YOUR_DISCORD_BOT_TOKEN")
        config = make_config("[server:EU]\necon_port = 7310\n", base,
                             env={"DISCORD_BOT_TOKEN": "from-env", "ECON_PASSWORD": "env-pass"})
        self.assertEqual(config.discord.token, "from-env")
        self.assertEqual(config.servers[0].econ_password, "env-pass")

    def test_password_may_contain_comment_characters(self):
        config = make_config("[server:EU]\necon_port = 7310\necon_password = a#b;c%d\n")
        self.assertEqual(config.servers[0].econ_password, "a#b;c%d")

    def test_placeholder_token_rejected(self):
        base = BASE_CONFIG.replace("token = test-token", "token = YOUR_DISCORD_BOT_TOKEN")
        self.assertTrue(any("token" in p for p in self.problems("[server:EU]\necon_port = 1\n", base)))

    def test_no_servers(self):
        self.assertTrue(any("no game servers" in p for p in self.problems("")))

    def test_all_servers_disabled(self):
        problems = self.problems("[server:EU]\necon_port = 7310\nenabled = no\n")
        self.assertTrue(any("disabled" in p for p in problems))

    def test_duplicate_tag_is_case_insensitive(self):
        problems = self.problems("[server:EU]\necon_port = 1\n[server:eu]\necon_port = 2\n")
        self.assertTrue(any("duplicate tag" in p for p in problems))

    def test_duplicate_econ_endpoint(self):
        problems = self.problems("[server:EU]\necon_port = 7310\n[server:RU]\necon_port = 7310\n")
        self.assertTrue(any("already used by [server:EU]" in p for p in problems))

    def test_invalid_values_are_all_reported(self):
        problems = self.problems("""
[server:has space]
econ_port = 1

[server:EU]
econ_port = 70000
game_address = not-an-address
econ_prot = 5

[server:RU]
""")
        text = "\n".join(problems)
        self.assertIn("invalid tag 'has space'", text)
        self.assertIn("econ_port must be <= 65535", text)
        self.assertIn("game_address must look like", text)
        self.assertIn("unknown option 'econ_prot'", text)
        self.assertIn("[server:RU] econ_port is required", text)

    def test_missing_password(self):
        base = BASE_CONFIG.replace("password = secret", "password = CHANGE_ME")
        problems = self.problems("[server:EU]\necon_port = 7310\n", base)
        self.assertTrue(any("econ_password is not set" in p for p in problems))

    def test_unknown_section(self):
        problems = self.problems("[server:EU]\necon_port = 1\n[servers]\nx = 1\n")
        self.assertTrue(any("unknown section" in p for p in problems))

    def test_example_config_is_valid(self):
        example = os.path.join(os.path.dirname(HERE), "bridge.cfg.example")
        config = cfg.load_config(example, env={"DISCORD_BOT_TOKEN": "t", "ECON_PASSWORD": "p"})
        self.assertEqual([server.tag for server in config.servers], ["EU"])
        # as shipped the example must be refused: it has no token and no password
        with self.assertRaises(cfg.ConfigError) as caught:
            cfg.load_config(example, env={})
        self.assertEqual(len(caught.exception.problems), 2)

    def test_missing_file_and_relative_log_path(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "bridge.cfg")
            with self.assertRaises(cfg.ConfigError):
                cfg.load_config(path, env={})
            with open(path, "w", encoding="utf-8") as config_file:
                config_file.write(BASE_CONFIG.replace("[logging]", "[logging]\nfile = logs.txt")
                                  + "[server:EU]\necon_port = 7310\n")
            config = cfg.load_config(path, env={})
            self.assertEqual(config.logging.file, os.path.join(directory, "logs.txt"))


# --- econ output parsing ---------------------------------------------------------------
class ParseTests(unittest.TestCase):
    def parse(self, system, text):
        return parse_econ_line(f"{TIMESTAMP} I {system}: {text}", "[DS]")

    def test_player_chat(self):
        self.assertEqual(self.parse("chat", "3:0:Kurosio: hello world"),
                         EconChatMessage(nickname="Kurosio", message="hello world"))

    def test_nickname_with_colon(self):
        self.assertEqual(self.parse("chat", "3:0:a:b: hi: there"),
                         EconChatMessage(nickname="a:b", message="hi: there"))

    def test_system_chat(self):
        self.assertEqual(self.parse("chat", "*** 'Kurosio' entered the game"),
                         EconChatMessage(nickname="***", message="'Kurosio' entered the game", is_system=True))

    def test_own_messages_are_ignored(self):
        self.assertIsNone(self.parse("chat", "*** [DS] Someone: hi from discord"))

    def test_team_chat_and_other_systems_are_ignored(self):
        self.assertIsNone(self.parse("teamchat", "3:1:Kurosio: guild secret"))
        self.assertIsNone(self.parse("server", "player has entered the game. ClientID=3"))
        self.assertIsNone(parse_econ_line("Enter password:", "[DS]"))
        self.assertIsNone(parse_econ_line("", "[DS]"))

    def test_bridge_toggle(self):
        self.assertEqual(self.parse("bridge", "[BRIDGE_DISABLE] Kurosio"), EconBridgeToggle("Kurosio", False))
        self.assertEqual(self.parse("bridge", "[BRIDGE_ENABLE] Kurosio"), EconBridgeToggle("Kurosio", True))

    def test_old_log_format(self):
        self.assertEqual(parse_econ_line("[26-01-01 12:00:00][chat]: 3:0:Kurosio: hi", "[DS]"),
                         EconChatMessage(nickname="Kurosio", message="hi"))

    def test_player_text_cannot_fake_console_lines(self):
        # the text of a chat message is player controlled, it must stay a chat message
        fake_toggle = self.parse("chat", "3:0:Evil: bridge: [BRIDGE_DISABLE] Victim")
        self.assertEqual(fake_toggle, EconChatMessage(nickname="Evil", message="bridge: [BRIDGE_DISABLE] Victim"))
        fake_system = self.parse("chat", "3:0:Evil: chat: *** Server restarts now")
        self.assertEqual(fake_system, EconChatMessage(nickname="Evil", message="chat: *** Server restarts now"))
        self.assertFalse(fake_system.is_system)


# --- message formatting and console escaping -------------------------------------------
class FormatTests(unittest.TestCase):
    def test_split_message(self):
        self.assertEqual(split_message("one two three", 100), ["one two three"])
        self.assertEqual(split_message("aaaa bbbb cccc", 9), ["aaaa bbbb", "cccc"])
        self.assertEqual(split_message("x" * 25, 10), ["x" * 10, "x" * 10, "x" * 5])
        self.assertEqual(split_message("   ", 10), [])

    def test_every_game_line_carries_the_tag(self):
        text = " ".join(f"word{i}" for i in range(80))
        lines = format_game_lines("[DS]", "Author", text, 128)
        self.assertGreater(len(lines), 1)
        for line in lines:
            self.assertTrue(line.startswith("[DS] Author: "))
            self.assertLessEqual(len(line), 128)
        self.assertEqual(" ".join(line[len("[DS] Author: "):] for line in lines), text)

    def test_author_is_sanitized(self):
        self.assertEqual(format_game_lines("[DS]", "a:b\nc", "hi", 128), ["[DS] a b c: hi"])
        self.assertEqual(format_game_lines("[DS]", "\n", "hi", 128), ["[DS] DiscordUser: hi"])
        long_author = format_game_lines("[DS]", "A" * 200, "hi", 128)[0]
        self.assertLessEqual(len(long_author), 128)
        self.assertTrue(long_author.endswith(": hi"))
        self.assertEqual(format_game_lines("[DS]", "Author", "   ", 128), [])

    def test_discord_username(self):
        self.assertEqual(format_discord_username("[EU]", "Kurosio", "🇷🇺"), "[EU] Kurosio (🇷🇺)")
        self.assertEqual(format_discord_username("[EU]", "Server"), "[EU] Server")
        long_name = format_discord_username("[EU]", "N" * 200, "🌐")
        self.assertEqual(len(long_name), 80)
        self.assertTrue(long_name.startswith("[EU] NNN") and long_name.endswith("… (🌐)"))

    def test_escaping_survives_the_console_parser(self):
        cases = [
            'plain text',
            'quote " inside',
            'x"; shutdown; say "pwned',
            'ends with backslash \\',
            'escaped quote \\" ; shutdown',
            '\\\\"; shutdown #',
            'new\nline; shutdown\r\nsay hi',
            '# not a comment',
            'tab\there \x00 nul',
        ]
        for text in cases:
            with self.subTest(text=text):
                line = f'say "{escape_console_string(text)}"'
                self.assertNotIn("\n", line)
                commands = split_commands(line)
                self.assertEqual(len(commands), 1, commands)
                name, argument = parse_command(commands[0])
                self.assertEqual(name, "say")
                self.assertIsNotNone(argument)
                self.assertNotIn('"', argument)
                # nothing but the quote replacement and whitespace cleanup changes the text
                expected = " ".join(text.replace("\x00", "").replace('"', "'").split())
                self.assertEqual(" ".join(argument.split()), expected)


# --- econ client -----------------------------------------------------------------------
class EconTestCase(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self._servers = []
        self._cleanup = []

    async def asyncTearDown(self):
        for cleanup in reversed(self._cleanup):
            await cleanup()
        for server in self._servers:
            await server.stop()

    async def fake_server(self, **kwargs) -> FakeEconServer:
        server = await FakeEconServer(**kwargs).start()
        self._servers.append(server)
        return server

    def run_client(self, client: EconClient, events: list) -> asyncio.Task:
        async def handler(event):
            events.append(event)

        self._cleanup.append(client.stop)
        return client.start(handler)


class EconClientTests(EconTestCase):
    def client_for(self, server, **econ) -> EconClient:
        config = config_for({"EU": server}, **econ)
        return EconClient(config.servers[0], config.econ, config.bridge.discord_tag)

    async def test_connect_and_receive_chat(self):
        server = await self.fake_server()
        client = self.client_for(server)
        events = []
        self.run_client(client, events)
        await server.wait_for(lambda: server.authed_clients == 1)
        await wait_until(lambda: client.is_connected)

        server.player_chat("Kurosio", "hello")
        server.emit("server", "noise the bridge ignores")
        server.system_chat("'Kurosio' entered the game")
        await wait_until(lambda: len(events) == 2)
        self.assertEqual(events[0], EconChatMessage("Kurosio", "hello"))
        self.assertEqual(events[1], EconChatMessage("***", "'Kurosio' entered the game", is_system=True))
        self.assertIsNone(client.last_error)
        self.assertIsNotNone(client.uptime)

    async def test_say_reaches_the_server_and_its_echo_is_ignored(self):
        server = await self.fake_server()
        client = self.client_for(server)
        events = []
        self.run_client(client, events)
        await wait_until(lambda: client.is_connected)

        self.assertTrue(await client.say(['[DS] Author: hi "there"; shutdown', "[DS] Author: second line"]))
        await server.wait_for(lambda: len(server.executed) == 2)
        self.assertEqual(server.executed, [("say", "[DS] Author: hi 'there'; shutdown"),
                                           ("say", "[DS] Author: second line")])

        # the server echoes `say` as a "***" chat line: it must not come back as an event
        server.player_chat("Kurosio", "after")
        await wait_until(lambda: len(events) >= 1)
        self.assertEqual(events, [EconChatMessage("Kurosio", "after")])

    async def test_wrong_password(self):
        server = await self.fake_server(password="other")
        client = self.client_for(server)
        self.assertFalse(await client.connect())
        self.assertFalse(client.is_connected)
        self.assertIn("wrong econ password", client.last_error)
        self.assertFalse(await client.say(["[DS] a: b"]))
        self.assertEqual(server.raw_lines, [])

    async def test_wrong_password_backs_off_but_a_down_server_does_not(self):
        delays = []
        real_sleep = asyncio.sleep

        async def fake_sleep(delay):
            if delay < 1:           # the polling of this test
                return await real_sleep(delay)
            delays.append(delay)    # a reconnect delay of the client: do not really wait
            await real_sleep(0)

        rejecting = await self.fake_server(password="other")
        down = await self.fake_server()
        down_port = down.port
        await down.stop()

        for target, expected in ((rejecting, [2, 4, 8, 10]), (down_port, [1, 1, 1, 1])):
            client = self.client_for(target, reconnect_delay=1, reconnect_delay_max=10)
            delays.clear()
            with mock.patch("services.econ_client.asyncio.sleep", fake_sleep):
                client.start(lambda event: None)
                await wait_until(lambda: len(delays) >= 4)
                await client.stop()
            self.assertEqual(delays[:4], expected)

    async def test_server_down(self):
        server = await self.fake_server()
        port = server.port
        await server.stop()
        client = self.client_for(port)
        self.assertFalse(await client.connect())
        self.assertIn("connection failed", client.last_error)

    async def test_silent_server_times_out_during_auth(self):
        async def silent(reader, writer):
            await reader.read()
            writer.close()

        listener = await asyncio.start_server(silent, "127.0.0.1", 0)
        self.addAsyncCleanup(listener.wait_closed)
        self.addCleanup(listener.close)
        client = self.client_for(listener.sockets[0].getsockname()[1], auth_timeout=0.5)
        self.assertFalse(await client.connect())
        self.assertIn("timed out", client.last_error)

    async def test_reconnects_after_the_server_drops_the_connection(self):
        server = await self.fake_server()
        client = self.client_for(server)
        events = []
        self.run_client(client, events)
        await wait_until(lambda: client.is_connected)

        await server.drop_clients()
        await wait_until(lambda: not client.is_connected)
        self.assertFalse(await client.say(["[DS] a: lost"]))

        await server.wait_for(lambda: server.authed_clients == 1, timeout=5)
        await wait_until(lambda: client.is_connected)
        server.player_chat("Kurosio", "back")
        await wait_until(lambda: events == [EconChatMessage("Kurosio", "back")])

    async def test_idle_connection_is_probed(self):
        server = await self.fake_server()
        client = self.client_for(server, read_timeout=1)
        events = []
        self.run_client(client, events)
        await wait_until(lambda: client.is_connected)
        await asyncio.sleep(1.3)
        # the probe is an empty line: the console ignores it and the link stays up
        self.assertTrue(client.is_connected)
        self.assertEqual(server.raw_lines, [])
        self.assertEqual(server.connections, 1)

    async def test_handler_errors_do_not_kill_the_link(self):
        server = await self.fake_server()
        client = self.client_for(server)
        seen = []

        async def handler(event):
            seen.append(event)
            if len(seen) == 1:
                raise RuntimeError("boom")

        client.start(handler)
        self._cleanup.append(client.stop)
        await wait_until(lambda: client.is_connected)
        server.player_chat("A", "first")
        server.player_chat("B", "second")
        await wait_until(lambda: len(seen) == 2)
        self.assertTrue(client.is_connected)


# --- hub: many servers -----------------------------------------------------------------
class ServerHubTests(EconTestCase):
    async def start_hub(self, servers, **econ):
        hub = ServerHub(config_for(servers, **econ))
        events = []

        async def handler(server, event):
            events.append((server.tag, event))

        hub.start(handler)
        self._cleanup.append(hub.stop)
        return hub, events

    async def test_events_are_tagged_with_their_server(self):
        eu, ru = await self.fake_server(), await self.fake_server()
        hub, events = await self.start_hub({"EU": eu, "RU": ru})
        await wait_until(lambda: hub.connected_count() == 2)

        eu.player_chat("Anna", "hi from eu")
        await wait_until(lambda: len(events) == 1)
        ru.player_chat("Boris", "hi from ru")
        await wait_until(lambda: len(events) == 2)
        self.assertEqual(events, [("EU", EconChatMessage("Anna", "hi from eu")),
                                  ("RU", EconChatMessage("Boris", "hi from ru"))])

    async def test_broadcast_reaches_every_server(self):
        servers = {tag: await self.fake_server() for tag in ("EU", "RU", "CN")}
        hub, events = await self.start_hub(servers)
        await wait_until(lambda: hub.connected_count() == 3)

        results = await hub.broadcast(["[DS] Author: hello all"])
        self.assertEqual(results, {"EU": True, "RU": True, "CN": True})
        for server in servers.values():
            await server.wait_for(lambda s=server: s.said == ["[DS] Author: hello all"])

        # every server echoes the line back; none of the echoes may become an event
        await asyncio.sleep(0.1)
        self.assertEqual(events, [])

    async def test_broadcast_with_an_offline_server(self):
        eu, ru = await self.fake_server(), await self.fake_server()
        ru_port = ru.port
        await ru.stop()
        hub, _ = await self.start_hub({"EU": eu, "RU": ru_port})
        await wait_until(lambda: hub.connected_count() == 1)

        self.assertEqual(await hub.broadcast(["[DS] Author: hi"]), {"EU": True, "RU": False})
        await eu.wait_for(lambda: eu.said == ["[DS] Author: hi"])
        states = {state.server.tag: state for state in hub.states()}
        self.assertTrue(states["EU"].connected)
        self.assertFalse(states["RU"].connected)
        self.assertIn("connection failed", states["RU"].last_error)

        self.assertEqual(await hub.broadcast(["[DS] Author: only eu"], tags=["eu"]), {"EU": True})

    async def test_offline_server_joins_later(self):
        eu = await self.fake_server()
        late = FakeEconServer()
        await late.start()
        late_port = late.port
        await late.stop()
        hub, events = await self.start_hub({"EU": eu, "RU": late_port})
        await wait_until(lambda: hub.connected_count() == 1)

        late.port = late_port
        await late.start()
        self._servers.append(late)
        await wait_until(lambda: hub.connected_count() == 2, timeout=6)
        late.player_chat("Boris", "now online")
        await wait_until(lambda: events == [("RU", EconChatMessage("Boris", "now online"))])

    async def test_stop_closes_all_links(self):
        eu, ru = await self.fake_server(), await self.fake_server()
        hub, _ = await self.start_hub({"EU": eu, "RU": ru})
        await wait_until(lambda: hub.connected_count() == 2)
        await hub.stop()
        self.assertFalse(hub.is_running)
        self.assertEqual(hub.connected_count(), 0)
        await eu.wait_for(lambda: eu.authed_clients == 0 and ru.authed_clients == 0)
        self.assertEqual(await hub.broadcast(["[DS] a: b"]), {"EU": False, "RU": False})


# --- discord side ----------------------------------------------------------------------
def fake_discord_message(content, channel_id=100, author="Author", bot=False, attachments=()):
    message = mock.MagicMock()
    message.content = content
    message.clean_content = content
    message.author.bot = bot
    message.author.display_name = author
    message.channel.id = channel_id
    message.attachments = list(attachments)
    message.add_reaction = mock.AsyncMock()
    return message


MASTER_DATA = {"servers": [
    {"addresses": ["tw-0.6+udp://10.0.0.1:8303"],
     "info": {"name": "MRPG Europe", "max_clients": 64, "gametype": "MRPG", "map": {"name": "main"},
              "clients": [{"name": "Anna", "country": 276, "skin": {"name": "pinky", "color_body": 1, "color_feet": 2}},
                          {"name": "[CLAN] Tagged", "country": -1, "skin": {"name": "default"}}]}},
    {"addresses": ["tw-0.6+udp://10.0.0.2:8303", "tw-0.7+udp://10.0.0.2:8303"],
     "info": {"name": "MRPG Russia", "max_clients": 64, "gametype": "MRPG", "map": "main",
              "clients": [{"name": "Anna", "country": 643, "skin": {"name": "bluekitty"}}]}},
    {"addresses": ["tw-0.6+udp://192.0.2.1:8303"], "info": {"name": "someone else", "clients": []}},
]}


@unittest.skipIf(discord is None, "discord.py is not installed")
class DiscordTestCase(EconTestCase):
    async def make_bot(self, servers, master_data=MASTER_DATA, overrides=None, **econ):
        """A real bot object that never logs in; `overrides` is {section: {option: value}}."""
        from discord_bot.bot import DiscordBridgeBot
        from services.server_status import ServerStatusService

        config = config_for(servers, **econ)
        for section, values in (overrides or {}).items():
            config = dataclasses.replace(config, **{section: dataclasses.replace(getattr(config, section), **values)})
        bot = DiscordBridgeBot(config)
        bot.status_service = ServerStatusService(session=None, config=config)
        bot.status_service._fetch_master_server_data = mock.AsyncMock(return_value=master_data)
        bot.webhook_manager = mock.MagicMock()
        bot.webhook_manager.send = mock.AsyncMock()
        self._cleanup.append(bot.server_hub.stop)
        return bot


class BridgeCogTests(DiscordTestCase):
    async def make_cog(self, servers, **kwargs):
        from discord_bot.cogs.bridge import BridgeCog

        bot = await self.make_bot(servers, **kwargs)
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        with mock.patch("discord_bot.cogs.bridge.BASE_DIR", directory.name):
            cog = BridgeCog(bot)
        self.state_dir = directory.name

        # what on_ready does once the channel and the webhook are resolved
        cog.target_channel = mock.MagicMock()
        cog.target_channel.id = bot.config.discord.channel_id
        cog._sender_task = asyncio.create_task(cog._discord_sender_loop())
        self.addAsyncCleanup(asyncio.gather, cog._sender_task, return_exceptions=True)
        self.addCleanup(cog._sender_task.cancel)
        cog.hub.start(cog._on_server_event)
        cog._ready_event.set()
        await wait_until(lambda: cog.hub.connected_count() == len([s for s in servers.values()
                                                                   if not isinstance(s, int)]))
        return bot, cog

    @staticmethod
    def sent(bot):
        return [(call.kwargs["username"], call.kwargs["content"]) for call in bot.webhook_manager.send.call_args_list]

    async def test_game_chat_is_posted_under_the_server_tag(self):
        eu, ru = await self.fake_server(), await self.fake_server()
        bot, cog = await self.make_cog({"EU": eu, "RU": ru})
        await bot.status_service.update_status()

        eu.player_chat("Anna", "hello from europe")
        await wait_until(lambda: len(self.sent(bot)) == 1)
        ru.player_chat("Anna", "привет из россии")
        await wait_until(lambda: len(self.sent(bot)) == 2)
        ru.player_chat("Unknown", "not on the master yet")
        await wait_until(lambda: len(self.sent(bot)) == 3)
        eu.system_chat("'Anna' left the game")
        await wait_until(lambda: len(self.sent(bot)) == 4)

        self.assertEqual(self.sent(bot), [
            ("[EU] Anna (🇩🇪)", "hello from europe"),
            ("[RU] Anna (🇷🇺)", "привет из россии"),
            ("[RU] Unknown (🌐)", "not on the master yet"),
            ("[EU] Server", "'Anna' left the game"),
        ])
        # the same nickname on two servers gets the skin of the right server
        avatars = [call.kwargs["avatar_url"] for call in bot.webhook_manager.send.call_args_list]
        self.assertIn("name=pinky", avatars[0])
        self.assertIn("body=1", avatars[0])
        self.assertIn("name=bluekitty", avatars[1])
        self.assertIn("name=default", avatars[2])
        self.assertIn("name=bowlingball", avatars[3])

    async def test_discord_message_goes_to_all_servers(self):
        servers = {tag: await self.fake_server() for tag in ("EU", "RU", "CN")}
        bot, cog = await self.make_cog(servers)

        message = fake_discord_message("hello <:pepe:123456> everyone")
        await cog.on_message(message)
        for server in servers.values():
            await server.wait_for(lambda s=server: len(s.said) == 1)
            self.assertEqual(server.said, ["[DS] Author: hello :pepe: everyone"])
        message.add_reaction.assert_not_called()

        # the echoes of the three servers are not posted back to Discord
        await asyncio.sleep(0.1)
        self.assertEqual(self.sent(bot), [])

    async def test_long_discord_message_is_split_and_not_echoed(self):
        eu = await self.fake_server()
        bot, cog = await self.make_cog({"EU": eu})

        text = " ".join(f"word{i}" for i in range(60))
        await cog.on_message(fake_discord_message(text))
        await eu.wait_for(lambda: len(eu.said) >= 3)
        for line in eu.said:
            self.assertTrue(line.startswith("[DS] Author: "))
            self.assertLessEqual(len(line), 128)
        relayed = " ".join(line[len("[DS] Author: "):] for line in eu.said)
        self.assertEqual(relayed, text[:300] + "...")

        await asyncio.sleep(0.1)
        self.assertEqual(self.sent(bot), [])

    async def test_messages_that_are_not_relayed(self):
        eu = await self.fake_server()
        bot, cog = await self.make_cog({"EU": eu})

        await cog.on_message(fake_discord_message("from another channel", channel_id=999))
        await cog.on_message(fake_discord_message("from a bot", bot=True))
        await cog.on_message(fake_discord_message("!command"))
        await cog.on_message(fake_discord_message("   "))
        await cog.on_message(fake_discord_message("this one is relayed"))
        await eu.wait_for(lambda: len(eu.said) == 1)
        self.assertEqual(eu.said, ["[DS] Author: this one is relayed"])

    async def test_injection_attempt_from_discord(self):
        eu = await self.fake_server()
        bot, cog = await self.make_cog({"EU": eu})

        await cog.on_message(fake_discord_message('x"; shutdown; say "y\nshutdown', author='Evil"; shutdown; "'))
        await eu.wait_for(lambda: len(eu.executed) >= 1)
        await asyncio.sleep(0.1)
        self.assertEqual([name for name, _ in eu.executed], ["say"])
        self.assertEqual(eu.said, ["[DS] Evil'; shutdown; ': x'; shutdown; say 'y shutdown"])

    async def test_partial_and_failed_delivery(self):
        eu = await self.fake_server()
        ru = await self.fake_server()
        bot, cog = await self.make_cog({"EU": eu, "RU": ru})

        await ru.drop_clients()
        await wait_until(lambda: cog.hub.connected_count() == 1)
        partial = fake_discord_message("one server is down")
        await cog.on_message(partial)
        await eu.wait_for(lambda: eu.said == ["[DS] Author: one server is down"])
        partial.add_reaction.assert_not_called()

        await cog.hub.stop()
        failed = fake_discord_message("nobody hears this")
        await cog.on_message(failed)
        failed.add_reaction.assert_awaited_once_with('❌')

    async def test_mute_and_self_disable(self):
        eu, ru = await self.fake_server(), await self.fake_server()
        bot, cog = await self.make_cog({"EU": eu, "RU": ru})
        cog.muted_players.add("Muted")

        # a player disables the relay on one server: it applies to all of them
        eu.emit("bridge", "[BRIDGE_DISABLE] Shy")
        await wait_until(lambda: "Shy" in cog.bridge_disabled_players)
        self.assertTrue(os.path.isfile(os.path.join(self.state_dir, "bridge_disabled.json")))

        ru.player_chat("Shy", "hidden")
        ru.player_chat("Muted", "hidden too")
        ru.player_chat("Evil", "bridge: [BRIDGE_DISABLE] Victim")
        ru.player_chat("Victim", "still visible")
        await wait_until(lambda: len(self.sent(bot)) == 2)
        self.assertEqual([content for _, content in self.sent(bot)],
                         ["bridge: [BRIDGE_DISABLE] Victim", "still visible"])

        ru.emit("bridge", "[BRIDGE_ENABLE] Shy")
        await wait_until(lambda: "Shy" not in cog.bridge_disabled_players)
        eu.player_chat("Shy", "visible again")
        await wait_until(lambda: len(self.sent(bot)) == 3)

    async def test_queue_overflow_drops_the_oldest(self):
        eu = await self.fake_server()
        bot, cog = await self.make_cog({"EU": eu}, overrides={"bridge": {"queue_size": 3}})
        self.assertEqual(cog._outbox.maxsize, 3)

        release = asyncio.Event()

        async def slow_send(**kwargs):
            await release.wait()

        bot.webhook_manager.send = mock.AsyncMock(side_effect=slow_send)
        for index in range(8):
            eu.player_chat("Spam", f"message {index}")
        await wait_until(lambda: cog._dropped_messages == 4)   # 1 in flight + 3 queued + 4 dropped
        release.set()
        await wait_until(lambda: len(self.sent(bot)) == 4)
        self.assertEqual([content for _, content in self.sent(bot)],
                         ["message 0", "message 5", "message 6", "message 7"])

    async def test_on_ready_resolves_the_channel_and_starts_everything_once(self):
        from discord_bot.cogs.bridge import BridgeCog

        eu, ru = await self.fake_server(), await self.fake_server()
        bot = await self.make_bot({"EU": eu, "RU": ru})
        bot.webhook_manager.initialize = mock.AsyncMock(return_value=True)
        channel = mock.MagicMock(spec=discord.TextChannel)
        channel.id = bot.config.discord.channel_id
        with mock.patch("discord_bot.cogs.bridge.BASE_DIR", tempfile.gettempdir()):
            cog = BridgeCog(bot)
        self.addCleanup(lambda: cog._sender_task and cog._sender_task.cancel())

        with mock.patch.object(bot, "get_channel", return_value=channel) as get_channel:
            await cog.on_ready()
            get_channel.assert_called_once_with(100)
            bot.webhook_manager.initialize.assert_awaited_once_with(channel)
            self.assertIs(cog.target_channel, channel)
            await wait_until(lambda: cog.hub.connected_count() == 2)
            sender = cog._sender_task
            tasks = [client._task for client in cog.hub.clients.values()]

            # Discord reconnected: on_ready fires again and must not duplicate the links
            await cog.on_ready()
            self.assertIs(cog._sender_task, sender)
            self.assertEqual([client._task for client in cog.hub.clients.values()], tasks)
            self.assertEqual((eu.connections, ru.connections), (1, 1))

        eu.player_chat("Anna", "works")
        await wait_until(lambda: len(self.sent(bot)) == 1)
        await cog.cog_unload()
        self.assertFalse(cog.hub.is_running)

    async def test_on_ready_without_the_channel_disables_the_bridge(self):
        from discord_bot.cogs.bridge import BridgeCog

        eu = await self.fake_server()
        bot = await self.make_bot({"EU": eu})
        with mock.patch("discord_bot.cogs.bridge.BASE_DIR", tempfile.gettempdir()):
            cog = BridgeCog(bot)
        with mock.patch.object(bot, "get_channel", return_value=None), \
                mock.patch.object(bot, "fetch_channel", mock.AsyncMock(side_effect=discord.NotFound(
                    mock.MagicMock(status=404, reason="Not Found"), "unknown channel"))):
            await cog.on_ready()
        self.assertIsNone(cog.target_channel)
        self.assertFalse(cog.hub.is_running)
        await cog.on_message(fake_discord_message("nobody is listening"))
        self.assertEqual(eu.connections, 0)

    async def test_describe_servers(self):
        eu = await self.fake_server()
        down = await self.fake_server()
        down_port = down.port
        await down.stop()
        bot, cog = await self.make_cog({"EU": eu, "RU": down_port})
        await wait_until(lambda: cog.hub.states()[1].last_error is not None)
        text = cog.describe_servers()
        self.assertIn("1/2 connected", text)
        self.assertIn("🟢 **[EU]** EU — connected for", text)
        self.assertIn("🔴 **[RU]** RU — offline (connection failed", text)


class StatusTests(DiscordTestCase):
    async def make_status_cog(self, servers, **kwargs):
        from discord_bot.cogs.status import StatusCog

        bot = await self.make_bot(servers, **kwargs)
        return bot, StatusCog(bot)

    async def test_status_of_every_server_from_one_master_fetch(self):
        bot, cog = await self.make_status_cog({"EU": 1, "RU": 2, "CN": 3})
        statuses = await bot.status_service.update_status()
        bot.status_service._fetch_master_server_data.assert_awaited_once()

        self.assertEqual([(s.server.tag, s.found, len(s.players)) for s in statuses],
                         [("EU", True, 2), ("RU", True, 1), ("CN", False, 0)])
        self.assertEqual((await bot.status_service.get_player("EU", "Anna"))["country"], 276)
        self.assertEqual((await bot.status_service.get_player("RU", "Anna"))["country"], 643)
        self.assertIsNotNone(await bot.status_service.get_player("EU", "[CLAN] Tagged"))
        self.assertIsNone(await bot.status_service.get_player("CN", "Anna"))

        embeds = cog.build_status_embeds(statuses)
        self.assertEqual([embed.title for embed in embeds], [
            "📊 Server Status: [EU] MRPG Europe", "📊 Server Status: [RU] MRPG Russia", "📊 Server Status: [CN] CN"])
        self.assertIn("**Online:** 2 / 64", embeds[0].description)
        self.assertIn("10.0.0.1:8303", embeds[0].description)
        self.assertEqual(embeds[0].fields[0].value,
                         "[Anna](https://ddnet.org/players/Anna), "
                         "[\\[CLAN\\] Tagged](https://ddnet.org/players/%5BCLAN%5D%20Tagged)")
        self.assertEqual(embeds[0].fields[0].name, "Players Online (2)")
        self.assertIn("not found on the master server list", embeds[2].description)
        self.assertEqual(len(embeds[2].fields), 0)
        self.assertIsNotNone(embeds[-1].footer.text)

        view = cog.build_join_view(statuses)
        self.assertEqual([(item.label, item.url) for item in view.children], [
            ("Join EU", "https://ddnet.org/connect-to/?addr=10.0.0.1:8303"),
            ("Join RU", "https://ddnet.org/connect-to/?addr=10.0.0.2:8303")])

    async def test_master_failure_keeps_the_player_cache(self):
        bot, cog = await self.make_status_cog({"EU": 1})
        await bot.status_service.update_status()
        bot.status_service._fetch_master_server_data = mock.AsyncMock(return_value=None)
        statuses = await bot.status_service.update_status()
        self.assertFalse(statuses[0].master_available)
        self.assertIsNotNone(await bot.status_service.get_player("EU", "Anna"))
        embeds = cog.build_status_embeds(statuses)
        self.assertIn("master server list is unavailable", embeds[0].description)
        self.assertIsNone(cog.build_join_view(statuses))

    async def test_full_servers_fit_into_one_message(self):
        tags = [f"S{i}" for i in range(10)]
        master = {"servers": [
            {"addresses": [f"tw-0.6+udp://10.0.0.{i + 1}:8303"],
             "info": {"name": f"MRPG server number {i} with a long name", "max_clients": 64, "gametype": "MRPG",
                      "map": {"name": "main"},
                      "clients": [{"name": f"player_{i}_{n:02d}*_"} for n in range(64)]}}
            for i in range(10)]}
        bot, cog = await self.make_status_cog({tag: i + 1 for i, tag in enumerate(tags)}, master_data=master)
        statuses = await bot.status_service.update_status()
        embeds = cog.build_status_embeds(statuses)

        self.assertEqual(len(embeds), 10)
        self.assertLessEqual(sum(len(embed) for embed in embeds), 6000)
        for embed in embeds:
            self.assertLessEqual(len(embed.fields[0].value), 1024)
            self.assertIn("more", embed.fields[0].value)
        self.assertEqual(len(cog.build_join_view(statuses).children), 10)

    async def test_more_servers_than_embeds_use_the_compact_overview(self):
        servers = {f"S{i}": i + 1 for i in range(12)}
        bot, cog = await self.make_status_cog(servers)
        statuses = await bot.status_service.update_status()
        embeds = cog.build_status_embeds(statuses)
        self.assertEqual(len(embeds), 1)
        self.assertEqual(embeds[0].title, "📊 Server Status: 12 servers, 3 players online")
        self.assertEqual(embeds[0].description.count("\n"), 12)
        self.assertIn("**[S0]** [MRPG Europe]", embeds[0].description)
        self.assertIn("**[S5]** S5 — not listed", embeds[0].description)

    async def test_status_message_is_sent_once_and_then_edited(self):
        bot, cog = await self.make_status_cog({"EU": 1, "RU": 2})
        channel = mock.MagicMock(spec=discord.TextChannel)
        channel.pins = mock.AsyncMock(return_value=[])
        sent_message = mock.MagicMock(spec=discord.Message)
        channel.send.return_value = sent_message
        cog.status_channel = channel

        await cog.update_server_status_loop()
        channel.send.assert_awaited_once()
        embeds = channel.send.call_args.kwargs["embeds"]
        self.assertEqual([embed.title for embed in embeds],
                         ["📊 Server Status: [EU] MRPG Europe", "📊 Server Status: [RU] MRPG Russia"])
        self.assertEqual(len(channel.send.call_args.kwargs["view"].children), 2)
        sent_message.pin.assert_awaited_once()
        self.assertIs(cog.status_message, sent_message)

        await cog.update_server_status_loop()
        channel.send.assert_awaited_once()
        sent_message.edit.assert_awaited_once()
        self.assertEqual(len(sent_message.edit.call_args.kwargs["embeds"]), 2)

    async def test_without_status_channel_only_the_player_cache_is_refreshed(self):
        bot, cog = await self.make_status_cog({"EU": 1}, overrides={"status": {"channel_id": None}})
        await cog.update_server_status_loop()
        self.assertIsNotNone(await bot.status_service.get_player("EU", "Anna"))
        self.assertIsNone(cog.status_message)

    async def test_single_server_and_plain_names(self):
        bot, cog = await self.make_status_cog({"EU": 1}, overrides={"status": {"player_url": ""}})
        statuses = await bot.status_service.update_status()
        embeds = cog.build_status_embeds(statuses)
        self.assertEqual(embeds[0].fields[0].value, "Anna, [CLAN] Tagged")
        self.assertEqual([item.label for item in cog.build_join_view(statuses).children], ["Join Server"])


@unittest.skipIf(discord is None, "discord.py is not installed")
class BotSetupTests(unittest.IsolatedAsyncioTestCase):
    async def test_setup_hook_loads_the_cogs_and_registers_the_commands(self):
        from discord_bot.bot import DiscordBridgeBot

        bot = DiscordBridgeBot(config_for({"EU": 1, "RU": 2}))
        with mock.patch.object(bot.tree, "sync", mock.AsyncMock(return_value=[])) as sync:
            await bot.setup_hook()
            sync.assert_awaited_once()
        try:
            self.assertEqual(sorted(bot.cogs), ["BridgeCog", "StatusCog"])
            self.assertEqual(sorted(command.name for command in bot.tree.get_commands()),
                             ["bridge_mute", "bridge_mutelist", "bridge_servers", "bridge_unmute"])
            self.assertEqual(sorted(bot.server_hub.clients), ["eu", "ru"])
            self.assertIs(bot.cogs["BridgeCog"].hub, bot.server_hub)
            self.assertEqual([server.tag for server in bot.status_service.servers], ["EU", "RU"])
        finally:
            await bot.close()
        self.assertEqual(bot.cogs, {})
        self.assertTrue(bot.http_session.closed)


@unittest.skipIf(discord is None, "discord.py is not installed")
class WebhookManagerTests(unittest.IsolatedAsyncioTestCase):
    async def test_webhook_is_created_with_the_configured_name_and_reused(self):
        from discord_bot.webhook_manager import WebhookManager

        bot = mock.MagicMock()
        channel = mock.MagicMock(spec=discord.TextChannel)
        channel.id = 100
        channel.webhooks.return_value = []
        webhook = mock.MagicMock(spec=discord.Webhook)
        channel.create_webhook.return_value = webhook

        manager = WebhookManager(bot=bot, http_session=None, webhook_name="MRPG Bridge")
        self.assertTrue(await manager.initialize(channel))
        self.assertTrue(await manager.initialize(channel))
        channel.create_webhook.assert_awaited_once()
        self.assertEqual(channel.create_webhook.call_args.kwargs["name"], "MRPG Bridge")
        self.assertTrue(manager.is_ready())

        await manager.send(username="[EU] Anna (🇩🇪)", content="hello", avatar_url="http://skins/render?name=pinky")
        webhook.send.assert_awaited_once()
        self.assertEqual(webhook.send.call_args.kwargs["username"], "[EU] Anna (🇩🇪)")
        self.assertEqual(webhook.send.call_args.kwargs["content"], "hello")

    async def test_existing_webhook_is_found(self):
        from discord_bot.webhook_manager import WebhookManager

        bot = mock.MagicMock()
        channel = mock.MagicMock(spec=discord.TextChannel)
        channel.id = 100
        other = mock.MagicMock(spec=discord.Webhook)
        other.name, other.user = "MRPG Bridge", object()
        mine = mock.MagicMock(spec=discord.Webhook)
        mine.name, mine.user = "MRPG Bridge", bot.user
        channel.webhooks.return_value = [other, mine]

        manager = WebhookManager(bot=bot, http_session=None, webhook_name="MRPG Bridge")
        self.assertTrue(await manager.initialize(channel))
        self.assertIs(manager.webhook, mine)
        channel.create_webhook.assert_not_called()


if __name__ == "__main__":
    unittest.main()
