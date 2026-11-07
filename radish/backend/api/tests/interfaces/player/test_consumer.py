import asyncio
import base64
import json
from typing import Any

from channels.db import database_sync_to_async
from channels.testing import WebsocketCommunicator
from django.test import TransactionTestCase

from api.domain.servers import access
from api.interfaces.player.views.consumers import EchoConsumer
from api.models import (
    Game,
    GameServer,
    GameStatus,
    Player,
)
from api.tests.helpers import (
    User,
)


class ConsumerTests(TransactionTestCase):
    """
    Der WebSocket-Consumer gegen einen echten UDP-Empfaenger an Stelle der
    Instanz: was beim Verbinden zum Client geht und was an der Instanz ankommt.
    TransactionTestCase, weil der Consumer die Datenbank aus einem anderen
    Thread liest (database_sync_to_async).
    """

    def setUp(self) -> None:
        self.user = User.objects.create(username="spieler")
        self.player = Player.objects.create(user=self.user)
        guest: Player = Player.objects.create(user=User.objects.create(username="gast"))
        self.server = GameServer.objects.create(
            name="server-ws", address="127.0.0.1", port=1, control_url="http://instanz:8000",
            instance_name="game-instance-1",
        )
        self.game = Game.objects.create(
            name="laeuft", password="geheim", server=self.server, host=self.player,
            second_player=guest, points_limit=100, status=GameStatus.RUNNING,
            host_zucchini_code="1122334455667788", second_player_zucchini_code="99aabbccddeeff00",
        )

    async def _listen(self) -> tuple[asyncio.DatagramTransport, asyncio.Queue[bytes]]:
        """Ein UDP-Empfaenger auf einem freien Port; der Server zeigt dorthin."""
        received: asyncio.Queue[bytes] = asyncio.Queue()

        class Receiver(asyncio.DatagramProtocol):
            # Any: die Signatur aus typeshed (asyncio.DatagramProtocol); ein
            # engerer Typ fuer addr waere eine Liskov-Verletzung [override].
            def datagram_received(self, data: bytes, addr: tuple[str | Any, int]) -> None:
                received.put_nowait(data)

        loop: asyncio.AbstractEventLoop = asyncio.get_running_loop()
        transport: asyncio.DatagramTransport
        transport, _ = await loop.create_datagram_endpoint(Receiver, local_addr=("127.0.0.1", 0))
        port: int = transport.get_extra_info("sockname")[1]
        await database_sync_to_async(GameServer.objects.filter(pk=self.server.pk).update)(port=port)
        return transport, received

    async def _connect(self) -> WebsocketCommunicator:
        communicator: WebsocketCommunicator = WebsocketCommunicator(EchoConsumer.as_asgi(), "/ws/echo/")
        communicator.scope["user"] = self.user
        connected: bool
        connected, _ = await communicator.connect()
        self.assertTrue(connected)
        return communicator

    async def test_client_learns_its_public_id_but_not_the_code(self) -> None:
        transport: asyncio.DatagramTransport
        transport, _ = await self._listen()
        communicator: WebsocketCommunicator = await self._connect()

        identity: dict[str, object] = json.loads(await communicator.receive_from())
        self.assertEqual(identity, {"type": "identity", "data": {"player_id": self.player.identifier}})
        self.assertNotIn("1122334455667788", json.dumps(identity))

        await communicator.disconnect()
        transport.close()

    async def test_command_goes_out_with_code_and_sender(self) -> None:
        transport: asyncio.DatagramTransport
        received: asyncio.Queue[bytes]
        transport, received = await self._listen()
        communicator: WebsocketCommunicator = await self._connect()
        await communicator.receive_from()

        await communicator.send_to(text_data=json.dumps(
            {"type": "command", "data": base64.b64encode(b"\x0a\x00").decode()}
        ))
        datagram: bytes = await asyncio.wait_for(received.get(), timeout=2)

        self.assertEqual(datagram[:8], bytes.fromhex("1122334455667788"))
        self.assertEqual(datagram[8:16], access.player_id(self.player.identifier).to_bytes(8, "big"))
        self.assertEqual(datagram[16:], b"\x0a\x00")

        await communicator.disconnect()
        transport.close()

    async def test_game_without_code_is_refused(self) -> None:
        await database_sync_to_async(Game.objects.filter(pk=self.game.pk).update)(host_zucchini_code="")


        communicator: WebsocketCommunicator = WebsocketCommunicator(EchoConsumer.as_asgi(), "/ws/echo/")
        communicator.scope["user"] = self.user
        # connect() liefert (True, Subprotokoll) oder (False, Close-Code).
        connected: bool
        code: int | str | None
        connected, code = await communicator.connect()

        self.assertFalse(connected)
        self.assertEqual(code, 4409)
