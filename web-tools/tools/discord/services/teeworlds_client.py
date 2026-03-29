# services/teeworlds_client.py
import asyncio
import re
import logging
from typing import Optional
from config import (
    TEESERVER_IP, TEESERVER_ECON_PORT, ECON_PASSWORD,
    MAX_MESSAGE_LENGTH, ECON_CONNECT_TIMEOUT,
    ECON_READ_TIMEOUT, ECON_AUTH_TIMEOUT, SERVER_TAG_TEEWORLDS
)
from models.econ import EconMessage, EconChatMessage, EconOtherMessage
from utils.helpers import clean_message

logger = logging.getLogger("TeeworldsBridge.EconClient")

class TeeworldsEconClient:
    """Manages the connection and communication with Teeworlds Econ."""

    def __init__(self):
        self.reader: Optional[asyncio.StreamReader] = None
        self.writer: Optional[asyncio.StreamWriter] = None
        self._connected: bool = False
        self._lock = asyncio.Lock()

        # regex pattern for reading econ messages
        escaped_discord_tag = re.escape(SERVER_TAG_TEEWORLDS)
        self.chat_pattern = re.compile(r"chat:\s*\d+:\d+:([^:]+):\s*(.*)$")
        self.system_chat_pattern = re.compile(rf"chat:\s+(\*\*\*)\s+(?!{escaped_discord_tag})(.*)")

    @property
    def is_connected(self) -> bool:
        """Checks if the client is currently connected and streams are valid."""
        return (
            self._connected
            and self.reader is not None and not self.reader.at_eof()
            and self.writer is not None and not self.writer.is_closing()
        )


    async def _close_connection(self, reason: str = "Requested"):
        """Closes the connection streams and updates status."""
        if self.writer and not self.writer.is_closing():
            try:
                self.writer.close()
                await self.writer.wait_closed()
            except Exception as e:
                logger.debug(f"Ignoring error closing writer: {e}")

        self.reader = None
        self.writer = None
        
        if self._connected:
            logger.info(f"🔌 Econ connection closed. Reason: {reason}")
            self._connected = False


    async def connect(self) -> bool:
        """Attempts to establish and authenticate the Econ connection, handling initial prompts."""
        async with self._lock:
            if self.is_connected:
                logger.debug("Connect called but already connected.")
                return True

            logger.info(f"🔗 Attempting to connect to Econ ({TEESERVER_IP}:{TEESERVER_ECON_PORT})...")
            try:
                await self._close_connection(reason="Pre-connect cleanup") # Закрываем предыдущее соединение

                logger.debug("Connect: Opening connection...")
                self.reader, self.writer = await asyncio.wait_for(
                    asyncio.open_connection(TEESERVER_IP, TEESERVER_ECON_PORT),
                    timeout=ECON_CONNECT_TIMEOUT
                )
                logger.debug("Connect: Connection established. Waiting for initial prompt (if any)...")

                # step 1: reading
                try:
                    initial_prompt_bytes = await asyncio.wait_for(
                        self.reader.readline(),
                        timeout=ECON_AUTH_TIMEOUT
                    )
                    initial_prompt = initial_prompt_bytes.decode('utf-8', 'ignore').strip()
                    logger.debug(f"Connect: Received initial prompt/message: '{initial_prompt}'")
                except asyncio.TimeoutError:
                    logger.debug("Connect: No initial prompt received within timeout, proceeding.")
                except (asyncio.IncompleteReadError, ConnectionResetError, BrokenPipeError, OSError) as e:
                     logger.error(f"❌ Econ connection error waiting for initial prompt: {e}")
                     await self._close_connection(reason=f"Error reading initial prompt: {e}")
                     return False

                # step 2: send password
                logger.debug("Connect: Sending password...")
                self.writer.write(f"{ECON_PASSWORD}\n".encode('utf-8'))
                await self.writer.drain()
                logger.debug("Connect: Password sent. Waiting for final authentication response...")

                # step 3: read result by password
                auth_response_bytes = await asyncio.wait_for(
                    self.reader.readline(),
                    timeout=ECON_AUTH_TIMEOUT
                )
                auth_response = auth_response_bytes.decode('utf-8', 'ignore').strip()
                logger.debug(f"Connect: Final auth response received: '{auth_response}'")

                # step 4: check result
                if "Authentication successful" in auth_response or "authenticated" in auth_response.lower() or not auth_response:
                     self._connected = True
                     logger.info("✅ Econ connected and authenticated successfully.")
                     return True
                else:
                    logger.error(f"❌ Econ authentication failed. Final Response: '{auth_response}'")
                    await self._close_connection(reason="Authentication failed (final response)")
                    return False

            except asyncio.TimeoutError:
                logger.error(f"❌ Econ connection/authentication process timed out.")
                await self._close_connection(reason="Timeout during connect/auth")
                return False
            except (ConnectionRefusedError, OSError) as e:
                logger.error(f"❌ Econ connection failed during connect attempt: {e}")
                await self._close_connection(reason=f"Connection error: {e}")
                return False
            except Exception as e:
                logger.exception(f"❌ Unexpected error connecting/authenticating to Econ: {e}")
                await self._close_connection(reason=f"Unexpected error: {e}")
                return False


    async def receive_econ_message(self) -> EconMessage:
        """Reads and parses one line from the Econ console."""
        if not self.is_connected:
            return None

        try:
            data = await asyncio.wait_for(
                self.reader.readline(),
                timeout=ECON_READ_TIMEOUT
            )
            # readuntil can return empty bytes is connection closed \n
            if not data:
                logger.warning("🔌 Econ connection closed by peer (received empty data from readuntil).")
                await self._close_connection(reason="Peer closed (empty read)")
                return None

            raw_line = data.decode('utf-8', errors='ignore')
            cleaned_line = clean_message(raw_line)

            if not cleaned_line:
                return None

            logger.debug(f"Econ Raw Cleaned: '{cleaned_line}'")

            # message parsin logic
            system_chat_match = self.system_chat_pattern.search(cleaned_line)
            if system_chat_match:
                nickname = system_chat_match.group(1).strip() # "***"
                message = system_chat_match.group(2).strip()
                if message:
                    logger.debug(f"Parsed as System Chat -> Nick: '{nickname}', Msg: '{message[:50]}...'")
                    return EconChatMessage(type="chat", nickname=nickname, message=message)
                else: return None

            chat_match = self.chat_pattern.search(cleaned_line)
            if chat_match:
                nickname = chat_match.group(1).strip()
                message = chat_match.group(2).strip()
                if message.startswith(SERVER_TAG_TEEWORLDS):
                    logger.debug(f"Ignoring own bridge message from '{nickname}'")
                    return None
                if message:
                    logger.debug(f"Parsed as Player Chat -> Nick: '{nickname}', Msg: '{message[:50]}...'")
                    return EconChatMessage(type="chat", nickname=nickname, message=message)
                else: return None

        except asyncio.TimeoutError:
            logger.debug(f"Econ read timed out after {ECON_READ_TIMEOUT}s.")
            return None
        except asyncio.IncompleteReadError as e:
            logger.warning(f"⚠️ Econ incomplete read (connection likely lost): {e}. Disconnecting.")
            await self._close_connection(reason=f"IncompleteReadError: {e}")
            return None
        except (ConnectionResetError, BrokenPipeError, OSError) as e:
            logger.warning(f"⚠️ Econ read error (connection lost): {e}. Disconnecting.")
            await self._close_connection(reason=f"Connection error: {e}")
            return None
        except Exception as e:
            logger.exception(f"💥 Unexpected error receiving/parsing Econ message: {e}")
            await self._close_connection(reason=f"Unexpected parsing error: {e}")
            return None


    async def send_message(self, text_to_say: str) -> bool:
        """Sends a 'say' command to the Econ console, splitting if needed."""
        async with self._lock:
            if not self.is_connected:
                logger.warning("Cannot send message: Econ not connected.")
                return False

            # clean unsupport symbols
            cleaned_message = clean_message(text_to_say)
            if not cleaned_message:
                 logger.warning("Attempted to send an empty or non-printable message.")
                 return False

            # send by chunks
            chunks = [cleaned_message[i:i+MAX_MESSAGE_LENGTH] for i in range(0, len(cleaned_message), MAX_MESSAGE_LENGTH)]

            try:
                for i, chunk in enumerate(chunks):
                    escaped_chunk = chunk.replace('\\', '\\\\').replace('"', "'")
                    command = f'say "{escaped_chunk}"\n'
                    logger.debug(f"Sending chunk {i+1}/{len(chunks)} to Teeworlds: {command.strip()}")
                    self.writer.write(command.encode('utf-8'))
                    await self.writer.drain()
                    if len(chunks) > 1 and i < len(chunks) - 1:
                        await asyncio.sleep(0.3)
                return True
            except (ConnectionResetError, BrokenPipeError, OSError) as e:
                logger.error(f"❌ Econ send error (connection lost): {e}. Disconnecting.")
                await self._close_connection(reason=f"Send error: {e}")
                return False
            except Exception as e:
                logger.exception(f"💥 Failed to send message via Econ: {e}")
                return False


    async def close(self):
        """Public method to gracefully close the Econ connection."""
        logger.info("Request received to close Econ connection.")
        async with self._lock:
            await self._close_connection(reason="Explicit close called")
