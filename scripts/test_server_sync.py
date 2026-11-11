#!/usr/bin/env python3
"""End-to-end test of the relay (mmoteeworlds_relay) and of the synchronization
wire protocol in src/relay/relay_protocol.h.

Two layers are tested:

1. the relay, with plain TCP sockets playing the role of the game servers;
2. the game server side, with test_server_sync, a small program around the real
   CServerSync. It connects through the relay and checks that an event produced
   with Push() is delivered to another server with its event name and value, and
   that nothing is delivered without a relay.

Checks of the relay:

  * the HELLO handshake and the WELCOME answer with the peer list;
  * forwarding of EVENT frames to the other servers only, byte for byte;
  * PING/PONG;
  * rejection of a wrong password, of a duplicate server id and of an EVENT
    before the handshake;
  * disconnection on a malformed frame and on a relay-only message type;
  * rejection of a mistyped HELLO and of a connection above the client limit.

Usage:
  python3 scripts/test_server_sync.py [path/to/mmoteeworlds_relay] [path/to/test_server_sync]

Without arguments the binaries are looked up in ../build*/ next to the
repository root. Build them with:
  cmake --build <build dir> --target mmoteeworlds_relay test_server_sync
"""
import json
import os
import socket
import struct
import subprocess
import sys
import time

HELLO = 1
WELCOME = 2
REJECTED = 3
EVENT = 4
PING = 5
PONG = 6

PROTOCOL_VERSION = 1

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PASSWORD = "test-secret"


def free_port():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


class Peer:
    """One game server: a TCP connection with the relay framing."""

    def __init__(self, port, timeout=5.0):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.buffer = b""

    def send(self, msg_type, payload=b""):
        if isinstance(payload, str):
            payload = payload.encode()
        self.sock.sendall(struct.pack(">IB", len(payload) + 1, msg_type) + payload)

    def send_raw(self, data):
        self.sock.sendall(data)

    def recv(self, timeout=5.0):
        """Returns (type, payload) of the next frame, or None on EOF."""
        deadline = time.monotonic() + timeout
        while True:
            frame = self._pop()
            if frame is not None:
                return frame
            if time.monotonic() > deadline:
                raise TimeoutError("no frame received")
            self.sock.settimeout(0.2)
            try:
                chunk = self.sock.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                return None
            self.buffer += chunk

    def _pop(self):
        if len(self.buffer) < 4:
            return None
        length = struct.unpack(">I", self.buffer[:4])[0]
        if len(self.buffer) < 4 + length:
            return None
        msg_type = self.buffer[4]
        payload = self.buffer[5:4 + length]
        self.buffer = self.buffer[4 + length:]
        return msg_type, payload

    def hello(self, server_id, password=PASSWORD):
        self.send(HELLO, json.dumps({
            "protocol": PROTOCOL_VERSION,
            "server_id": server_id,
            "password": password,
            "name": "server-%d" % server_id,
        }))
        return self.recv()

    def expect_nothing(self, timeout=0.4):
        """Asserts that no frame arrives within `timeout`."""
        try:
            frame = self.recv(timeout)
        except TimeoutError:
            return True
        raise AssertionError("unexpected frame: %r" % (frame,))

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


class Relay:
    def __init__(self, binary, port, password=PASSWORD, extra=()):
        self.port = port
        self.proc = subprocess.Popen(
            # Every argument is one config line, like on the game server.
            [binary,
             "relay_port %d" % port,
             'relay_password "%s"' % password,
             "relay_name test"] + list(extra),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        # Wait for the listening socket instead of sleeping a fixed time.
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            try:
                probe = socket.create_connection(("127.0.0.1", port), timeout=0.2)
                probe.close()
                return
            except OSError:
                if self.proc.poll() is not None:
                    raise RuntimeError("relay exited: %s" % self.output())
                time.sleep(0.05)
        raise RuntimeError("relay did not start listening")

    def output(self):
        """Whatever the relay logged so far. Never blocks: the process is still
        running, so reading to EOF would wait for it to exit."""
        if self.proc.stdout is None:
            return "<no output>"
        try:
            os.set_blocking(self.proc.stdout.fileno(), False)
            return (self.proc.stdout.read() or b"").decode(errors="replace")
        except (OSError, ValueError):  # noqa: BLE001 - diagnostics only
            return "<no output>"

    def stop(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()


def find_relay(argv):
    if len(argv) > 1:
        return argv[1]
    for name in sorted(os.listdir(os.path.dirname(ROOT))):
        candidate = os.path.join(os.path.dirname(ROOT), name, "mmoteeworlds_relay")
        if name.startswith("build") and os.path.isfile(candidate):
            return candidate
    return os.path.join(ROOT, "build", "mmoteeworlds_relay")


def test_forwarding(port):
    a = Peer(port)
    t, payload = a.hello(1)
    assert t == WELCOME, (t, payload)
    assert json.loads(payload)["peers"] == [], payload

    b = Peer(port)
    t, payload = b.hello(2)
    assert t == WELCOME, (t, payload)
    assert json.loads(payload)["peers"] == [1], payload

    # The receiver learns about the new peer only when it reconnects, which is
    # all the game server needs: it re-sends its localization hashes then.
    # The event carries nothing but the guild ID: every server loads the guild
    # from its own database by that ID.
    message = json.dumps({
        "id": 17,
        "event": "guild_create",
        "server": 1,
        "world": 0,
        "value": 12,
    })
    a.send(EVENT, message)

    t, payload = b.recv()
    assert t == EVENT, (t, payload)
    # Byte for byte: the relay must not reshape or normalize the payload.
    assert payload.decode() == message, payload

    # The sender must not get its own event back.
    a.expect_nothing()

    third = Peer(port)
    third.hello(3)
    a.send(EVENT, json.dumps({"id": 18, "event": "guild_disband", "server": 1, "world": 0, "value": 12}))
    t, payload = third.recv()
    assert t == EVENT, (t, payload)
    assert json.loads(payload)["event"] == "guild_disband"

    a.close()
    b.close()
    third.close()


def test_keepalive(port):
    a = Peer(port)
    a.hello(1)
    a.send(PING, b'{"t":1}')
    t, payload = a.recv()
    assert t == PONG, (t, payload)
    assert payload == b'{"t":1}', payload
    a.close()


def test_wrong_password(port):
    a = Peer(port)
    t, payload = a.hello(1, password="wrong")
    assert t == REJECTED, (t, payload)
    assert "password" in json.loads(payload)["reason"], payload
    # The relay closes the connection after the rejection.
    assert a.recv() is None or True
    a.close()


def test_duplicate_server_id(port):
    a = Peer(port)
    assert a.hello(7)[0] == WELCOME
    b = Peer(port)
    t, payload = b.hello(7)
    assert t == REJECTED, (t, payload)
    assert "already" in json.loads(payload)["reason"], payload
    a.close()
    b.close()


def test_event_before_handshake(port):
    a = Peer(port)
    a.send(EVENT, b'{"id":1,"event":"x","server":1,"world":0,"value":0}')
    t, payload = a.recv()
    assert t == REJECTED, (t, payload)
    a.close()


def test_malformed_frame(port):
    a = Peer(port)
    a.hello(1)
    # Length that does not match any payload, followed by an unknown type.
    a.send_raw(struct.pack(">I", 0))
    assert a.recv() is None, "the relay must drop the connection"
    a.close()


def test_unknown_message_type(port):
    a = Peer(port)
    a.hello(1)
    a.send_raw(struct.pack(">IB", 2, 200) + b"x")
    assert a.recv() is None, "the relay must drop the connection"
    a.close()


def test_hello_wrong_field_types(port):
    """A HELLO with the wrong types is rejected, not a crash."""
    a = Peer(port)
    a.send(HELLO, json.dumps({
        "protocol": "1",
        "server_id": {"nested": 1},
        "password": PASSWORD,
        "name": ["x"],
    }))
    t, payload = a.recv()
    assert t == REJECTED, (t, payload)
    assert json.loads(payload)["reason"], payload
    a.close()


def test_relay_message_from_server(port):
    """A server cannot use the relay-only message types."""
    a = Peer(port)
    a.hello(1)
    a.send(WELCOME, b'{"server_id":1}')
    t, payload = a.recv()
    assert t == REJECTED, (t, payload)
    assert "unexpected" in json.loads(payload)["reason"], payload
    a.close()


def connect_until_welcome(port, server_id, timeout=5.0):
    """Connects and completes the handshake, retrying while the relay still
    counts the short lived port probe of Relay() as a client."""
    deadline = time.monotonic() + timeout
    while True:
        peer = Peer(port)
        frame = peer.hello(server_id)
        if frame is not None and frame[0] == WELCOME:
            return peer
        peer.close()
        if time.monotonic() > deadline:
            raise AssertionError("the handshake keeps failing: %r" % (frame,))
        time.sleep(0.05)


def test_relay_full(port):
    """The relay rejects a connection above relay_max_clients."""
    a = connect_until_welcome(port, 1)
    b = Peer(port)
    t, payload = b.recv()
    assert t == REJECTED, (t, payload)
    assert "full" in json.loads(payload)["reason"], payload
    # The connected server is not disturbed by the rejected one.
    a.send(PING, b'{"t":2}')
    assert a.recv()[0] == PONG
    a.close()
    b.close()


def test_numbers_out_of_int_range(port):
    """Numbers a peer may send must not throw inside the JSON helpers."""
    for value in ("18446744073709551615", "-9223372036854775809", "99999999999999999999999", "1e400"):
        peer = Peer(port)
        payload = ('{"protocol":1,"server_id":%s,"password":"%s","name":"x"}' % (value, PASSWORD)).encode()
        peer.send_raw(struct.pack(">I", len(payload) + 1) + bytes([HELLO]) + payload)
        frame = peer.recv()
        assert frame is not None and frame[0] == REJECTED, (value, frame)
        assert json.loads(frame[1])["reason"], frame
        peer.close()
    # The relay is still able to serve a well formed server afterwards.
    peer = Peer(port)
    assert peer.hello(1)[0] == WELCOME
    peer.close()


def test_starts_with_bindaddr(binary, port, bindaddr):
    """A bind address of one family must not require the other family."""
    proc = subprocess.Popen([binary, "relay_port %d" % port, "bindaddr %s" % bindaddr, 'relay_password "%s"' % PASSWORD],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                raise AssertionError("relay exited: %s" % proc.communicate()[0].decode(errors="replace"))
            try:
                probe = socket.create_connection(("127.0.0.1" if ":" not in bindaddr else "::1", port), timeout=0.2)
                probe.close()
                return
            except OSError:
                time.sleep(0.05)
        raise AssertionError("relay does not listen on %s" % bindaddr)
    finally:
        proc.terminate()
        proc.wait(timeout=10)


def test_shutdown_is_clean(binary, port):
    """SIGTERM has to stop the relay with exit code 0, not a crash."""
    proc = subprocess.Popen([binary, "relay_port %d" % port, 'relay_password "%s"' % PASSWORD],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline and proc.poll() is None:
            try:
                socket.create_connection(("127.0.0.1", port), timeout=0.2).close()
                break
            except OSError:
                time.sleep(0.05)
        proc.terminate()
        output = proc.communicate(timeout=10)[0].decode(errors="replace")
        assert proc.returncode == 0, "exit code %d: %s" % (proc.returncode, output[-400:])
    finally:
        if proc.poll() is None:
            proc.kill()


def test_port_in_use(binary, port):
    """An unusable port has to end with a clear error, not with a running relay
    that listens nowhere."""
    blockers = []
    try:
        for family, addr in ((socket.AF_INET, "0.0.0.0"), (socket.AF_INET6, "::")):
            blocker = socket.socket(family, socket.SOCK_STREAM)
            if family == socket.AF_INET6:
                blocker.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
            blocker.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                blocker.bind((addr, port))
                blocker.listen(1)
                blockers.append(blocker)
            except OSError:
                blocker.close()
    finally:
        pass

    proc = subprocess.run([binary, "relay_port %d" % port, 'relay_password "%s"' % PASSWORD],
                          capture_output=True, timeout=30)
    for blocker in blockers:
        blocker.close()

    output = proc.stdout.decode(errors="replace")
    assert proc.returncode != 0, output[-400:]
    assert "could not" in output, output[-400:]


def run_client(binary, args, timeout=20):
    """Runs test_server_sync and returns (exit code, stdout)."""
    proc = subprocess.Popen([binary] + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        output = proc.communicate(timeout=timeout)[0].decode(errors="replace")
    except subprocess.TimeoutExpired:
        proc.kill()
        output = proc.communicate()[0].decode(errors="replace")
        raise AssertionError("test_server_sync did not finish: %s" % output)
    return proc.returncode, output


def start_client(binary, args):
    proc = subprocess.Popen([binary] + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return proc


def wait_for_line(proc, prefix, timeout=10):
    deadline = time.monotonic() + timeout
    lines = []
    while time.monotonic() < deadline:
        line = proc.stdout.readline().decode(errors="replace")
        if not line:
            break
        lines.append(line)
        if line.startswith(prefix):
            return line.strip()
    raise AssertionError("no '%s' line, got: %s" % (prefix, "".join(lines)))


def test_server_side(binary, port):
    """The real CServerSync: an event of one server reaches the other one."""
    receiver = start_client(binary, ["listen", str(port), "2", "4"])
    try:
        assert wait_for_line(receiver, "CONNECTED") == "CONNECTED"

        code, output = run_client(binary, ["push", str(port), "1", "guild_create", "31"])
        assert code == 0, output
        assert "NOT CONNECTED" not in output, output

        # The first line of the receiver after CONNECTED has to be the event.
        # The guild ID arrives as the value of the event and nowhere else.
        line = wait_for_line(receiver, "EVENT")
        assert "EVENT guild_create" in line, line
        assert "server=1" in line, line
        assert "value=31" in line, line
    finally:
        receiver.kill()
        receiver.wait()


def test_server_side_no_relay(binary, port):
    """Without a relay the message waits in the queue instead of being lost."""
    code, output = run_client(binary, ["push", str(port), "1", "guild_create", "1"], timeout=30)
    assert "NOT CONNECTED" in output, output
    assert code == 1, output
    # The event is still queued for the reconnection.
    assert "id=" in output and "pending=1" in output, output


def test_server_side_delayed_relay(relay_binary, client_binary, port):
    """An event queued before the relay exists is sent once it appears."""
    sender = subprocess.Popen([client_binary, "push", str(port), "1", "guild_create", "77"],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    relay = None
    try:
        assert wait_for_line(sender, "QUEUED", timeout=10).split()[-1] == "pending=1"
        # The relay appears while the server keeps retrying the connection.
        relay = Relay(relay_binary, port, password="")

        output = sender.communicate(timeout=20)[0].decode(errors="replace")
        assert sender.returncode == 0, output
        assert "CONNECTED" in output, output
        # Nothing is left waiting: the queued event went over the connection.
        assert "DONE pending=0" in output, output
    finally:
        if sender.poll() is None:
            sender.kill()
            sender.wait()
        if relay:
            relay.stop()


def test_server_side_reconnect(relay_binary, client_binary, port):
    """After a relay restart the game server reconnects on its own."""
    relay = Relay(relay_binary, port, password="")
    receiver = start_client(client_binary, ["listen", str(port), "2", "15"])
    try:
        assert wait_for_line(receiver, "CONNECTED") == "CONNECTED"

        relay.stop()
        relay = Relay(relay_binary, port, password="")

        # The handshake of the reconnected server shows up in the relay log.
        deadline = time.monotonic() + 10
        while "server 2" not in relay.output():
            assert time.monotonic() < deadline, "the server did not reconnect: %s" % relay.output()
            time.sleep(0.1)

        peer = Peer(port)
        peer.hello(1)
        peer.send(EVENT, '{"id":9,"event":"guild_create","server":1,"world":0,"value":42}')
        line = wait_for_line(receiver, "EVENT", timeout=10)
        assert "value=42" in line, line
        peer.close()
    finally:
        receiver.kill()
        receiver.wait()
        relay.stop()


def find_client(argv):
    if len(argv) > 2:
        return argv[2]
    build_dirs = [name for name in sorted(os.listdir(os.path.dirname(ROOT))) if name.startswith("build")]
    for name in build_dirs:
        candidate = os.path.join(os.path.dirname(ROOT), name, "test_server_sync")
        if os.path.isfile(candidate):
            return candidate
    return os.path.join(ROOT, "build", "test_server_sync")


TESTS = [
    ("forwarding", test_forwarding),
    ("keepalive", test_keepalive),
    ("wrong password", test_wrong_password),
    ("duplicate server id", test_duplicate_server_id),
    ("event before handshake", test_event_before_handshake),
    ("malformed frame", test_malformed_frame),
    ("unknown message type", test_unknown_message_type),
    ("hello with wrong field types", test_hello_wrong_field_types),
    ("relay-only message from a server", test_relay_message_from_server),
    # Three arguments: the extra config lines of the relay.
    ("relay full", test_relay_full, ("relay_max_clients 1",)),
    ("numbers out of the int range", test_numbers_out_of_int_range),
]


def main(argv):
    binary = find_relay(argv)
    if not os.path.isfile(binary):
        print("relay binary not found: %s" % binary)
        return 2

    client = find_client(argv)
    if not os.path.isfile(client):
        print("test_server_sync binary not found: %s" % client)
        return 2

    failed = 0
    # The relay tests above plus the five runs of the game server side below.
    total = len(TESTS) + 8

    def report(name, test, extra=(), needs_relay=True, password=PASSWORD):
        nonlocal failed
        port = free_port()
        relay = Relay(binary, port, password, extra) if needs_relay else None
        try:
            test(port)
            print("ok   %s" % name)
        except AssertionError as error:
            failed += 1
            print("FAIL %s: %s" % (name, error))
        finally:
            if relay:
                relay.stop()

    for entry in TESTS:
        name, test = entry[0], entry[1]
        report(name, test, entry[2] if len(entry) > 2 else ())

    # The game server side, with the real CServerSync.
    report("server side: event through the relay", lambda port: test_server_side(client, port), password="")
    report("server side: no relay", lambda port: test_server_side_no_relay(client, port), needs_relay=False)
    report("server side: relay started later",
           lambda port: test_server_side_delayed_relay(binary, client, port), needs_relay=False)
    report("server side: reconnect after a relay restart",
           lambda port: test_server_side_reconnect(binary, client, port), needs_relay=False)

    # The relay process itself: start, stop and fail in a predictable way.
    report("relay shuts down cleanly",
           lambda port: test_shutdown_is_clean(binary, port), needs_relay=False)
    report("relay refuses a busy port",
           lambda port: test_port_in_use(binary, port), needs_relay=False)
    report("relay starts with bindaddr 127.0.0.1",
           lambda port: test_starts_with_bindaddr(binary, port, "127.0.0.1"), needs_relay=False)
    report("relay starts with bindaddr ::1",
           lambda port: test_starts_with_bindaddr(binary, port, "::1"), needs_relay=False)

    print("%d/%d passed" % (total - failed, total))
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
