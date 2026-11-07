# Lobby- und Spielende-Tests bauen dieselbe Ausgangslage auf und pruefen
# dasselbe Ende eines Spiels.
# pylint: disable=duplicate-code

# Die Mock-Parameter setzen die @mock.patch-Dekoratoren der Klasse in jeden
# Test ein, auch dort, wo der Test sie nicht braucht.
# pylint: disable=unused-argument

import json
from unittest import mock

from rest_framework import status

from api.domain.servers.instance_client import InstanceError
from api.models import (
    Game,
    GameStatus,
    Player,
)
from api.tests.game_test_case import GameTestCase
from api.tests.helpers import (
    PLAYER_TOKEN,
    TestResponse,
    TestToken,
    User,
    authenticate,
)


@mock.patch("api.domain.servers.instance_client.cancel_game")
@mock.patch("api.domain.servers.instance_client.start_game")
class GameLobbyTests(GameTestCase):
    """
    Lobby vor dem Spielstart (siehe models.GameStatus): der Host startet
    von Hand, sobald ein zweiter Spieler da ist; die Instanz setzt das Spiel
    noch im selben Aufruf auf, danach laeuft es. Die Aufrufe an die Instanz
    (instance_client) sind gemockt.
    """

    def setUp(self) -> None:
        super().setUp()
        self.create_game()
        self.game = Game.objects.get(name="spiel")
        self.guest = Player.objects.create(user=User.objects.create(username="gast"))
        self.game.second_player = self.guest
        self.game.second_player_army = self.army_for(self.guest)
        self.game.save()

    def start(self) -> TestResponse:
        return self.client.post("/api/client/game/start/")

    def as_guest(self) -> None:
        authenticate(self.client, self.guest.user, PLAYER_TOKEN)

    def current_status(self) -> str:
        return self.client.get("/api/client/game/current/").data["game"]["status"]

    def test_keycloak_username_is_taken_from_token(
        self, start_game: mock.MagicMock, cancel_game: mock.MagicMock
    ) -> None:
        token: TestToken = {**PLAYER_TOKEN, "preferred_username": "test-user"}
        authenticate(self.client, self.user, token)

        self.start()

        self.player.refresh_from_db()
        self.assertEqual(self.player.name, "test-user")

    def test_new_game_waits_in_lobby(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.assertEqual(self.current_status(), "lobby")
        start_game.assert_not_called()

    def test_is_host_tells_players_apart(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.assertTrue(self.client.get("/api/client/game/current/").data["game"]["is_host"])
        self.as_guest()
        self.assertFalse(self.client.get("/api/client/game/current/").data["game"]["is_host"])

    def test_host_starts_game(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        response: TestResponse = self.start()

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertEqual(response.data["status"], "running")
        start_game.assert_called_once()
        self.assertEqual(start_game.call_args.args[0].pk, self.game.pk)

    def test_guest_sees_running_game(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.start()
        self.as_guest()

        self.assertEqual(self.current_status(), "running")

    def test_only_host_can_start(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.as_guest()

        self.assertEqual(self.start().status_code, status.HTTP_403_FORBIDDEN)
        start_game.assert_not_called()

    def test_start_needs_second_player(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.game.second_player = None
        self.game.second_player_army = None
        self.game.save()

        self.assertEqual(self.start().status_code, status.HTTP_409_CONFLICT)
        start_game.assert_not_called()

    def test_start_only_once(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.start()

        self.assertEqual(self.start().status_code, status.HTTP_409_CONFLICT)
        start_game.assert_called_once()

    def test_failed_instance_call_stays_in_lobby(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        start_game.side_effect = InstanceError("nicht erreichbar")

        response: TestResponse = self.start()

        self.assertEqual(response.status_code, status.HTTP_502_BAD_GATEWAY)
        self.game.refresh_from_db()
        self.assertEqual(self.game.status, GameStatus.LOBBY)
        # Ohne aufgesetztes Spiel keine Codes.
        self.assertEqual(self.game.host_zucchini_code, "")
        self.assertEqual(self.game.second_player_zucchini_code, "")

    def test_start_hands_out_secret_codes(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        response: TestResponse = self.start()

        self.game.refresh_from_db()
        host_code: str = self.game.host_zucchini_code
        guest_code: str = self.game.second_player_zucchini_code
        self.assertRegex(host_code, r"^[0-9a-f]{16}$")
        self.assertRegex(guest_code, r"^[0-9a-f]{16}$")
        self.assertNotEqual(host_code, guest_code)

        # Die Instanz bekam sie schon gesetzt.
        started: Game = start_game.call_args.args[0]
        self.assertEqual(started.host_zucchini_code, host_code)

        # Und kein Client bekommt sie zu sehen.
        self.assertNotIn(host_code, json.dumps(response.data))
        self.assertNotIn(guest_code, json.dumps(response.data))
        current: dict[str, object] = self.client.get("/api/client/game/current/").data
        self.assertNotIn(host_code, json.dumps(current))
        self.assertNotIn(guest_code, json.dumps(current))

    def test_leaving_in_lobby_keeps_game(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.as_guest()

        self.client.post("/api/client/game/leave/")

        self.game.refresh_from_db()
        self.assertIsNone(self.game.second_player)
        cancel_game.assert_not_called()

    def test_leaving_started_game_ends_it_for_both(
        self, start_game: mock.MagicMock, cancel_game: mock.MagicMock
    ) -> None:
        self.start()
        self.as_guest()

        with self.captureOnCommitCallbacks(execute=True):
            response: TestResponse = self.client.post("/api/client/game/leave/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertFalse(Game.objects.exists())
        self.server.refresh_from_db()
        self.assertFalse(self.server.is_occupied)
        cancel_game.assert_called_once()
        authenticate(self.client, self.user, PLAYER_TOKEN)
        self.assertIsNone(self.client.get("/api/client/game/current/").data["game"])

    def test_cannot_join_started_game(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.game.second_player = None
        self.game.second_player_army = None
        self.game.status = GameStatus.RUNNING
        self.game.save()
        newcomer: Player = Player.objects.create(user=User.objects.create(username="neu"))
        authenticate(self.client, newcomer.user, PLAYER_TOKEN)

        response: TestResponse = self.client.post(
            "/api/client/game/join/",
            {"name": "spiel", "password": "geheim", "army_id": self.army_for(newcomer).id},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)

    def test_game_list_only_shows_lobby_games(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.start()

        self.assertEqual(self.client.get("/api/client/games/").data["games"], [])
