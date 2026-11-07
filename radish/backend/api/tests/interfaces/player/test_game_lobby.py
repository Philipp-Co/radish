import json
from unittest import mock

from rest_framework import status

from api.models import (
    Game,
    GameStatus,
    Player,
)

from api.tests.helpers import (
    PLAYER_TOKEN,
    User,
)
from api.tests.game_test_case import GameTestCase


@mock.patch("api.domain.servers.instance_client.cancel_game")
@mock.patch("api.domain.servers.instance_client.start_game")
class GameLobbyTests(GameTestCase):
    """
    Lobby vor dem Spielstart (siehe models.GameStatus): der Host startet
    von Hand, sobald ein zweiter Spieler da ist; die Instanz setzt das Spiel
    noch im selben Aufruf auf, danach laeuft es. Die Aufrufe an die Instanz
    (instance_client) sind gemockt.
    """

    def setUp(self):
        super().setUp()
        self.create_game()
        self.game = Game.objects.get(name="spiel")
        self.guest = Player.objects.create(user=User.objects.create(username="gast"))
        self.game.second_player = self.guest
        self.game.second_player_army = self.army_for(self.guest)
        self.game.save()

    def start(self):
        return self.client.post("/api/client/game/start/")

    def as_guest(self):
        self.client.force_authenticate(user=self.guest.user, token=PLAYER_TOKEN)

    def current_status(self):
        return self.client.get("/api/client/game/current/").data["game"]["status"]

    def test_keycloak_username_is_taken_from_token(self, start_game, cancel_game):
        token = {**PLAYER_TOKEN, "preferred_username": "test-user"}
        self.client.force_authenticate(user=self.user, token=token)

        self.start()

        self.player.refresh_from_db()
        self.assertEqual(self.player.name, "test-user")

    def test_new_game_waits_in_lobby(self, start_game, cancel_game):
        self.assertEqual(self.current_status(), "lobby")
        start_game.assert_not_called()

    def test_is_host_tells_players_apart(self, start_game, cancel_game):
        self.assertTrue(self.client.get("/api/client/game/current/").data["game"]["is_host"])
        self.as_guest()
        self.assertFalse(self.client.get("/api/client/game/current/").data["game"]["is_host"])

    def test_host_starts_game(self, start_game, cancel_game):
        response = self.start()

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        self.assertEqual(response.data["status"], "running")
        start_game.assert_called_once()
        self.assertEqual(start_game.call_args.args[0].pk, self.game.pk)

    def test_guest_sees_running_game(self, start_game, cancel_game):
        self.start()
        self.as_guest()

        self.assertEqual(self.current_status(), "running")

    def test_only_host_can_start(self, start_game, cancel_game):
        self.as_guest()

        self.assertEqual(self.start().status_code, status.HTTP_403_FORBIDDEN)
        start_game.assert_not_called()

    def test_start_needs_second_player(self, start_game, cancel_game):
        self.game.second_player = None
        self.game.second_player_army = None
        self.game.save()

        self.assertEqual(self.start().status_code, status.HTTP_409_CONFLICT)
        start_game.assert_not_called()

    def test_start_only_once(self, start_game, cancel_game):
        self.start()

        self.assertEqual(self.start().status_code, status.HTTP_409_CONFLICT)
        start_game.assert_called_once()

    def test_failed_instance_call_stays_in_lobby(self, start_game, cancel_game):
        from api.domain.servers.instance_client import InstanceError

        start_game.side_effect = InstanceError("nicht erreichbar")

        response = self.start()

        self.assertEqual(response.status_code, status.HTTP_502_BAD_GATEWAY)
        self.game.refresh_from_db()
        self.assertEqual(self.game.status, GameStatus.LOBBY)
        # Ohne aufgesetztes Spiel keine Codes.
        self.assertEqual(self.game.host_zucchini_code, "")
        self.assertEqual(self.game.second_player_zucchini_code, "")

    def test_start_hands_out_secret_codes(self, start_game, cancel_game):
        response = self.start()

        self.game.refresh_from_db()
        host_code = self.game.host_zucchini_code
        guest_code = self.game.second_player_zucchini_code
        self.assertRegex(host_code, r"^[0-9a-f]{16}$")
        self.assertRegex(guest_code, r"^[0-9a-f]{16}$")
        self.assertNotEqual(host_code, guest_code)

        # Die Instanz bekam sie schon gesetzt.
        started = start_game.call_args.args[0]
        self.assertEqual(started.host_zucchini_code, host_code)

        # Und kein Client bekommt sie zu sehen.
        self.assertNotIn(host_code, json.dumps(response.data))
        self.assertNotIn(guest_code, json.dumps(response.data))
        current = self.client.get("/api/client/game/current/").data
        self.assertNotIn(host_code, json.dumps(current))
        self.assertNotIn(guest_code, json.dumps(current))

    def test_leaving_in_lobby_keeps_game(self, start_game, cancel_game):
        self.as_guest()

        self.client.post("/api/client/game/leave/")

        self.game.refresh_from_db()
        self.assertIsNone(self.game.second_player)
        cancel_game.assert_not_called()

    def test_leaving_started_game_ends_it_for_both(self, start_game, cancel_game):
        self.start()
        self.as_guest()

        with self.captureOnCommitCallbacks(execute=True):
            response = self.client.post("/api/client/game/leave/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertFalse(Game.objects.exists())
        self.server.refresh_from_db()
        self.assertFalse(self.server.is_occupied)
        cancel_game.assert_called_once()
        self.client.force_authenticate(user=self.user, token=PLAYER_TOKEN)
        self.assertIsNone(self.client.get("/api/client/game/current/").data["game"])

    def test_cannot_join_started_game(self, start_game, cancel_game):
        self.game.second_player = None
        self.game.second_player_army = None
        self.game.status = GameStatus.RUNNING
        self.game.save()
        newcomer = Player.objects.create(user=User.objects.create(username="neu"))
        self.client.force_authenticate(user=newcomer.user, token=PLAYER_TOKEN)

        response = self.client.post(
            "/api/client/game/join/",
            {"name": "spiel", "password": "geheim", "army_id": self.army_for(newcomer).id},
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)

    def test_game_list_only_shows_lobby_games(self, start_game, cancel_game):
        self.start()

        self.assertEqual(self.client.get("/api/client/games/").data["games"], [])
