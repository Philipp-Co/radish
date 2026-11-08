# Lobby- und Spielende-Tests bauen dieselbe Ausgangslage auf und pruefen
# dasselbe Ende eines Spiels.
# pylint: disable=duplicate-code

# Die Mock-Parameter setzen die @mock.patch-Dekoratoren der Klasse in jeden
# Test ein, auch dort, wo der Test sie nicht braucht.
# pylint: disable=unused-argument

from unittest import mock

from rest_framework import status

from api.models import Game, Player
from api.tests.game_test_case import GameTestCase
from api.tests.helpers import PLAYER_TOKEN, SERVER_TOKEN, GameFinishedBody, TestResponse, TestToken, User, authenticate


@mock.patch("api.domain.servers.instance_client.cancel_game")
@mock.patch("api.domain.servers.instance_client.start_game")
class GameFinishedTests(GameTestCase):
    """
    Die Spielinstanz meldet das Ende eines Spiels (server_views
    .GameFinishedView). Danach ist es fuer beide vorbei wie beim Verlassen.
    Die Aufrufe an die Instanz (instance_client) sind gemockt.
    """

    def setUp(self) -> None:
        super().setUp()
        self.create_game()
        self.game = Game.objects.get(name="spiel")
        self.guest = Player.objects.create(user=User.objects.create(username="gast"))
        self.game.second_player = self.guest
        self.game.second_player_army = self.army_for(self.guest)
        self.game.save()
        self.server_user = User.objects.create(username="radish-game-server")

    def start(self) -> None:
        with mock.patch("api.domain.servers.instance_client.start_game"):
            authenticate(self.client, self.user, PLAYER_TOKEN)
            self.assertEqual(
                self.client.post("/api/client/game/start/").status_code, status.HTTP_200_OK
            )

    def finish(
        self, body: GameFinishedBody | None = None, name: str = "server-a", token: TestToken | None = None
    ) -> TestResponse:
        if body is None:
            body = {
                "winner": self.player.identifier,
                "loser": self.guest.identifier,
                "game-instance": name,
            }
        authenticate(self.client, self.server_user, SERVER_TOKEN if token is None else token)
        with self.captureOnCommitCallbacks(execute=True):
            return self.client.post(f"/api/servers/{name}/game-finished/", body, format="json")

    def test_finished_game_ends_for_both(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.start()

        response: TestResponse = self.finish()

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertFalse(Game.objects.exists())
        self.server.refresh_from_db()
        self.assertFalse(self.server.is_occupied)
        cancel_game.assert_called_once()
        self.assertEqual(cancel_game.call_args.args[0].name, "server-a")
        authenticate(self.client, self.user, PLAYER_TOKEN)
        self.assertIsNone(self.client.get("/api/client/game/current/").data["game"])

    def test_loser_may_be_the_host(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.start()

        response: TestResponse = self.finish({
            "winner": self.guest.identifier,
            "loser": self.player.identifier,
            "game-instance": "server-a",
        })

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertFalse(Game.objects.exists())

    def test_draw_has_neither_winner_nor_loser(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.start()

        response: TestResponse = self.finish({"winner": None, "loser": None, "game-instance": "server-a"})

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertFalse(Game.objects.exists())
        self.server.refresh_from_db()
        self.assertFalse(self.server.is_occupied)
        cancel_game.assert_called_once()

    def test_players_may_not_finish_a_game(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.start()

        response: TestResponse = self.finish(token=PLAYER_TOKEN)

        self.assertEqual(response.status_code, status.HTTP_403_FORBIDDEN)
        self.assertTrue(Game.objects.exists())
        cancel_game.assert_not_called()

    def test_unknown_server(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        response: TestResponse = self.finish(name="server-x")

        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)

    def test_game_in_lobby_is_not_finished(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        response: TestResponse = self.finish()

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)
        self.assertTrue(Game.objects.exists())
        cancel_game.assert_not_called()

    def test_second_report_finds_no_game(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.start()
        self.finish()

        response: TestResponse = self.finish()

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)
        cancel_game.assert_called_once()

    def test_instance_in_body_must_match_url(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.start()

        response: TestResponse = self.finish({
            "winner": self.player.identifier,
            "loser": self.guest.identifier,
            "game-instance": "server-b",
        })

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("game-instance", response.data)
        self.assertTrue(Game.objects.exists())

    def test_ids_must_be_the_players_of_the_game(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.start()
        stranger: Player = Player.objects.create(user=User.objects.create(username="fremd"))

        bodies: tuple[GameFinishedBody, ...] = (
            {"winner": stranger.identifier, "loser": self.guest.identifier},
            {"winner": self.player.identifier, "loser": self.player.identifier},
            {"winner": self.player.identifier},
            {"loser": self.guest.identifier},
            {"winner": None, "loser": self.guest.identifier},
            {"winner": self.player.identifier, "loser": None},
            {"winner": None},
        )
        body: GameFinishedBody
        for body in bodies:
            body["game-instance"] = "server-a"
            with self.subTest(body=body):
                response: TestResponse = self.finish(body)
                self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

        self.assertTrue(Game.objects.exists())
        cancel_game.assert_not_called()

    def test_body_needs_the_instance(self, start_game: mock.MagicMock, cancel_game: mock.MagicMock) -> None:
        self.start()

        response: TestResponse = self.finish({"winner": self.player.identifier, "loser": self.guest.identifier})

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("game-instance", response.data)
