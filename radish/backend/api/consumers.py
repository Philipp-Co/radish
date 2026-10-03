"""
Platzhalter-Consumer: zeigt, dass Channels/ASGI/Auth-Middleware
zusammenspielen (siehe config/asgi.py, api/routing.py, api/middleware.py).

Nimmt Textnachrichten entgegen und schickt sie unveraendert zurueck.
Verbindungen ohne gueltiges Keycloak-Token (scope["user"] ist dann
AnonymousUser, siehe middleware.py) werden abgelehnt.

Oeffnet beim Verbindungsaufbau ein UDP-Socket zum Spielserver des
aktuellen Spiels (siehe _get_current_game/_UdpProtocol unten) und
reicht Daten in beide Richtungen weiter: eingehende UDP-Datagramme an
den WebSocket-Client (_UdpProtocol.datagram_received), eingehende
WebSocket-Nachrichten an den Spielserver (receive() unten). Schickt
ausserdem alle 10s einen Text-Ping an den Client (_ping_loop unten) --
bewusst als text_data statt bytes_data, damit der Client Ping-Nachrichten
von den (binaeren) UDP-Spieldaten unterscheiden kann.

Eingehende WebSocket-Nachrichten sind durchgehend JSON mit einem "type"-
Feld (siehe frontend/src/app/core/game-socket.service.ts): receive()
unten entpackt sie und leitet beim Typ "command" die darin Base64-
kodierten Rohdaten an den Spielserver weiter -- davor den geheimen
Zucchini-Code und die Spieler-Id des angemeldeten Spielers (access.py).
Jeder andere Typ (z.B. der "ping"-Heartbeat des Clients) wird nur
geloggt, nicht weitergereicht.

Gleich nach dem Verbindungsaufbau bekommt der Client {"type": "identity"}
mit seiner oeffentlichen Spieler-Id -- den Code nie.

Durch echte Consumer ersetzen/ergaenzen, sobald die eigentliche Logik
(Lobby-/Matchmaking-Status) feststeht -- die UDP-Bruecke selbst ist oben
bereits umgesetzt.
"""

import asyncio
from base64 import b64decode, b64encode
from datetime import datetime
from json import loads, dumps

from channels.db import database_sync_to_async
from channels.generic.websocket import AsyncWebsocketConsumer
from django.db.models import Q

from . import access
from .models import Game, GameStatus, Player

PING_INTERVAL_SECONDS = 10


class _UdpProtocol(asyncio.DatagramProtocol):
    """
    Gegenstueck zu loop.create_datagram_endpoint() unten -- reicht
    eingehende Datagramme an den WebSocket-Client weiter.
    """

    def __init__(self, consumer):
        self.consumer = consumer

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, addr):
        # datagram_received ist keine Coroutine (asyncio ruft sie ganz
        # normal synchron auf) -- self.consumer.send() dagegen schon,
        # deshalb ueber create_task statt eines direkten await. Laeuft
        # trotzdem ohne Thread-Uebergabe, weil derselbe Event-Loop sowohl
        # dieses Protocol als auch den Consumer bedient.
        print(f'Received data from {addr}: {data}')
        asyncio.create_task(
            self.consumer.send(
                text_data=dumps(
                    {
                        'type': 'event',
                        'data': b64encode(data).decode(),
                    }
                ),
            )
        )

    def error_received(self, exc):
        print(f"UDP-Fehler: {exc}")


class EchoConsumer(AsyncWebsocketConsumer):
    async def connect(self):
        # self.scope["client"] ist der ASGI-Scope-Eintrag fuer den
        # TCP-Peer dieser Verbindung (siehe ASGI-Spezifikation:
        # "client": [host, port]) -- bei einem Reverse-Proxy davor waere
        # das dessen Adresse, nicht die des eigentlichen Browsers (dafuer
        # muesste stattdessen X-Forwarded-For aus self.scope["headers"]
        # ausgewertet werden, was hier noch nicht passiert). Geloggt, noch
        # bevor Auth/Player/Spiel geprueft sind, damit auch abgelehnte
        # Verbindungsversuche (4401/4404/4409 unten) mit Adresse auftauchen.
        client_address = self.scope.get("client")
        print(f"WebSocket-Verbindungsversuch von {client_address}: {self.scope['user']}")
        if not self.scope["user"].is_authenticated:
            # 4401 statt eines Standard-Codes (4000-4999 sind laut RFC 6455
            # frei fuer Anwendungen) -- angelehnt an HTTP 401, damit der
            # Client den Grund am Code unterscheiden kann.
            await self.close(code=4401)
            return

        self.player = await self._get_player()
        if self.player is None:
            # 4404, angelehnt an HTTP 404 (gleiche Idee wie 4401 oben) --
            # kein Player-Profil zu diesem User (siehe client_views.py:
            # LeaveGameView behandelt das ebenfalls als
            # harten Fehlerfall statt stillschweigend einen Player
            # anzulegen -- ein frisch angelegter Player waere ohnehin
            # sofort in keinem Spiel).
            await self.close(code=4404)
            return

        game = await self._get_current_game()
        if game is None:
            # 4409, angelehnt an HTTP 409 (siehe client_views.py:
            # LeaveGameView gibt fuer "Spieler ist in
            # keinem Spiel" ebenfalls 409 statt 404 zurueck -- kein
            # Spiel zu haben ist kein fehlendes Objekt, sondern ein
            # Zustandskonflikt: ohne laufendes Spiel gibt es keine
            # Serveradresse, zu der dieser Consumer ueberhaupt ein
            # UDP-Socket oeffnen koennte.
            await self.close(code=4409)
            return

        # Der Code dieses Spielers, geheim (access.py). Ohne ihn liesse zucchini
        # keine Nachricht durch -- ein Spiel, das vor den Codes gestartet wurde,
        # hat keinen, und mit ihm ist dann nicht zu reden.
        zucchini_code = game.zucchini_code_for(self.player)
        if not zucchini_code:
            print(f"Spiel '{game.name}' ohne Zugangscode fuer {self.player.identifier}")
            await self.close(code=4409)
            return
        self.zucchini_code = zucchini_code

        server = game.server
        loop = asyncio.get_running_loop()
        self.transport, _ = await loop.create_datagram_endpoint(
            lambda: _UdpProtocol(self),
            remote_addr=(server.address, server.port),
        )

        await self.accept()

        # Die eigene, oeffentliche Spieler-Id (access.py) -- damit der Client
        # in Antworten und Ereignissen erkennt, was seins ist. Der Zucchini-Code
        # geht nicht mit: den kennt nur das Backend.
        await self.send(
            text_data=dumps({"type": "identity", "data": {"player_id": self.player.identifier}})
        )

        # Erst nach accept() starten: send() davor wuerde fehlschlagen,
        # weil die Verbindung noch nicht offen ist.
        self._ping_task = asyncio.create_task(self._ping_loop())

    async def _ping_loop(self):
        # Laeuft als eigener Task neben receive()/disconnect(), siehe
        # disconnect() unten fuer das zugehoerige cancel(). asyncio.CancelledError
        # nicht abfangen: das ist genau das Signal, mit dem disconnect()
        # diese Schleife sauber beendet.
        while True:
            await asyncio.sleep(PING_INTERVAL_SECONDS)
            await self.send(text_data='{"type":"ping","data":{"time":"%s"}}' % (datetime.now().isoformat()))

    @database_sync_to_async
    def _get_player(self):
        try:
            return Player.objects.get(user=self.scope["user"])
        except Player.DoesNotExist:
            return None

    @database_sync_to_async
    def _get_current_game(self):
        # select_related("server"): server.address/port werden gleich
        # in connect() gebraucht -- ohne select_related waere das ein
        # zweiter, synchroner ORM-Zugriff ausserhalb dieser Methode (und
        # damit ausserhalb von database_sync_to_async).
        #
        # Nur RUNNING: in der Lobby hat die Instanz das Spiel noch nicht
        # aufgesetzt, die UDP-Bruecke haette niemanden zum Reden --
        # connect() schliesst dann wie ohne Spiel mit 4409.
        return (
            Game.objects.filter(Q(host=self.player) | Q(second_player=self.player))
            .filter(status=GameStatus.RUNNING)
            .select_related("server")
            .first()
        )

    async def receive(self, text_data=None, bytes_data=None):
        # Der Client schickt nach dem Umbau auf JSON-Nachrichten nur noch
        # text_data (siehe game-socket.service.ts, connect()/sendCommand());
        # bytes_data bliebe nur fuer eine aeltere Client-Version relevant
        # und wird deshalb nicht mehr interpretiert, nur geloggt.
        if text_data is None:
            print(f"Nachricht ohne text_data verworfen (bytes_data={bytes_data!r}): {self.scope['user']}")
            return

        try:
            message = loads(text_data)
        except (TypeError, ValueError):
            print(f"Nachricht ist kein gueltiges JSON: {text_data!r}")
            return

        if not isinstance(message, dict):
            print(f"Nachricht ist kein JSON-Objekt: {message!r}")
            return

        message_type = message.get("type")
        if message_type != "command":
            # Alles ausser "command" ist kein Spielprotokoll fuers UDP-
            # Backend -- z.B. der eigene "ping"-Heartbeat des Clients
            # (siehe game-socket.service.ts, startHeartbeat()). Nur zur
            # Kenntnisnahme geloggt, nicht an den Spielserver
            # weitergereicht, der koennte mit dem JSON nichts anfangen.
            print(f"Nachricht vom Client ({message_type}): {message}")
            return

        try:
            payload = b64decode(message["data"])
        except (KeyError, TypeError, ValueError):
            print(f"'command'-Nachricht ohne gueltiges Base64-'data': {message!r}")
            return

        # sendto() statt send(): das ist die Methode des UDP-Transports
        # aus connect() (nicht zu verwechseln mit self.send(), das in die
        # andere Richtung, zum WebSocket-Client, schickt). Kein Ziel noetig,
        # weil der Transport mit remote_addr=(server.address, server.port)
        # erzeugt wurde -- er kennt sein Ziel also schon.
        #
        # Der Client schickt nur die NetUserRequest (protobuf/message.proto);
        # Code und Absender setzt das Backend davor (access.frame). So kommt
        # keine Nachricht durch, die nicht von hier stammt, und keine in einem
        # fremden Namen.
        self.transport.sendto(access.frame(self.zucchini_code, self.player.identifier, payload))

    async def disconnect(self, close_code):
        # Laeuft in beiden Faellen: wenn der Client die Verbindung beendet
        # UND wenn wir selbst vorher close() aufgerufen haben (z.B. der
        # 4401-Fall in connect() oben) -- close_code ist dann 4401 bzw. der
        # vom Client gesendete Code.
        #
        # Noch ohne Aufraeumarbeiten von Gruppen-State, weil dieser
        # Platzhalter keinen haelt (kein group_add in connect()). Sobald
        # echte Consumer mit Channels-Gruppen dazukommen (Lobby-/
        # Matchmaking-Status), gehoert hierhin group_discard(...) -- sonst
        # bleibt der Kanalname in der Gruppe haengen, obwohl niemand mehr
        # zuhoert.
        print(f"disconnect: {self.scope['user']} (code={close_code})")
        if getattr(self, "_ping_task", None) is not None:
            self._ping_task.cancel()
        if getattr(self, "transport", None) is not None:
            self.transport.close()
