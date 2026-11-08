from django.contrib.auth.base_user import AbstractBaseUser
from rest_framework import status

from api.models import Army, Game, GameServer, Player
from api.tests.game_test_case import GameTestCase
from api.tests.helpers import PLAYER_TOKEN, TestResponse, User, authenticate


class GameArmyTests(GameTestCase):
    """
    Erstellen und Beitreten verlangen eine eigene Armee mit mindestens einer
    Einheit, die hoechstens das Punktelimit des Spiels kostet (siehe
    client_views._army_for_game). Dazu die Matchmaking-Regel aus
    GameJoinView.already_in_game: wer schon in einem Spiel ist, darf keinem
    weiteren beitreten -- auch nicht dem eigenen.

    Die Standard-Armee (army_payload: Anfuehrer + Soldat) kostet 36 Punkte.
    """

    def test_create_game_with_army(self) -> None:
        response: TestResponse = self.create_game(points_limit=500)

        self.assertEqual(response.status_code, status.HTTP_201_CREATED, response.data)
        self.assertEqual(response.data["points_limit"], 500)
        self.assertEqual(response.data["host_army_name"], "Erste Kompanie")
        game: Game = Game.objects.get(name="spiel")
        self.assertEqual(game.host_army, self.army)

    def test_create_game_requires_points_limit_and_army(self) -> None:
        response: TestResponse = self.client.post(
            "/api/client/game/", {"name": "spiel", "password": "geheim"}, format="json"
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("points_limit", response.data)
        self.assertIn("army_id", response.data)

    def test_create_game_rejects_army_over_limit(self) -> None:
        response: TestResponse = self.create_game(points_limit=35)

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertFalse(Game.objects.exists())
        self.server.refresh_from_db()
        self.assertFalse(self.server.is_occupied)

    def test_army_exactly_at_limit_is_allowed(self) -> None:
        self.assertEqual(self.create_game(points_limit=36).status_code, status.HTTP_201_CREATED)

    def test_create_game_rejects_foreign_army(self) -> None:
        other: Player = Player.objects.create(user=User.objects.create(username="other"))

        response: TestResponse = self.create_game(army_id=self.army_for(other).id)

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_create_game_rejects_empty_army(self) -> None:
        empty: Army = self.army_for(self.player, name="Leer", units=[])

        response: TestResponse = self.create_game(army_id=empty.id)

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_join_with_army(self) -> None:
        game: Game = self.open_game()

        response: TestResponse = self.join(game)

        self.assertEqual(response.status_code, status.HTTP_200_OK, response.data)
        game.refresh_from_db()
        self.assertEqual(game.second_player, self.player)
        self.assertEqual(game.second_player_army, self.army)

    def test_join_rejects_army_over_game_limit(self) -> None:
        game: Game = self.open_game(points_limit=30)

        response: TestResponse = self.join(game)

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        game.refresh_from_db()
        self.assertIsNone(game.second_player_id)

    def test_join_rejects_foreign_army(self) -> None:
        game: Game = self.open_game()

        response: TestResponse = self.join(game, army_id=game.host_army_id)

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_host_cannot_join_own_game(self) -> None:
        self.create_game()
        game: Game = Game.objects.get(name="spiel")

        response: TestResponse = self.join(game)

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)
        game.refresh_from_db()
        self.assertIsNone(game.second_player_id)

    def test_second_player_cannot_join_a_second_game(self) -> None:
        game: Game = self.open_game()
        self.join(game)
        other_server: GameServer = GameServer.objects.create(name="server-b", address="10.0.0.2", port=7001)
        other_host: Player = Player.objects.create(user=User.objects.create(username="other-host"))
        other_game: Game = Game.objects.create(
            name="anderes", password="geheim", server=other_server, host=other_host,
            points_limit=100, host_army=self.army_for(other_host),
        )

        response: TestResponse = self.join(other_game)

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)
        other_game.refresh_from_db()
        self.assertIsNone(other_game.second_player_id)

    def test_game_list_shows_points_limit(self) -> None:
        self.open_game(points_limit=750)

        response: TestResponse = self.client.get("/api/client/games/")

        self.assertEqual(response.data["games"][0]["points_limit"], 750)

    def open_games(self, count: int) -> None:
        """count Lobby-Spiele fremder Spieler, je mit eigenem Server."""
        index: int
        server: GameServer
        host: Player
        for index in range(count):
            server = GameServer.objects.create(
                name=f"server-{index}", address="10.0.1.1", port=8000 + index, is_occupied=True,
            )
            host = Player.objects.create(user=User.objects.create(username=f"host-{index}"))
            Game.objects.create(
                name=f"spiel-{index:02d}", password="geheim", server=server, host=host,
                points_limit=100,
            )

    def test_game_list_is_paginated(self) -> None:
        self.open_games(25)

        first: TestResponse = self.client.get("/api/client/games/")
        second: TestResponse = self.client.get("/api/client/games/?page=2")

        self.assertEqual(len(first.data["games"]), 20)
        self.assertEqual(first.data["count"], 25)
        self.assertEqual(first.data["page"], 1)
        self.assertEqual(first.data["page_size"], 20)
        self.assertEqual(first.data["games"][0]["name"], "spiel-00")
        self.assertEqual([game["name"] for game in second.data["games"]],
                         [f"spiel-{index:02d}" for index in range(20, 25)])
        self.assertEqual(second.data["page"], 2)

    def test_game_list_clamps_invalid_page(self) -> None:
        self.open_games(3)

        too_large: TestResponse = self.client.get("/api/client/games/?page=9")
        garbage: TestResponse = self.client.get("/api/client/games/?page=abc")

        self.assertEqual(too_large.data["page"], 1)
        self.assertEqual(len(too_large.data["games"]), 3)
        self.assertEqual(garbage.data["page"], 1)

    def test_game_list_filters_by_name_across_pages(self) -> None:
        self.open_games(25)

        response: TestResponse = self.client.get("/api/client/games/?name=spiel-23")

        self.assertEqual([game["name"] for game in response.data["games"]], ["spiel-23"])
        self.assertEqual(response.data["count"], 1)

    def test_second_player_takes_army_when_host_leaves(self) -> None:
        game: Game = self.open_game()
        self.join(game)
        host_user: AbstractBaseUser = game.host.user
        authenticate(self.client, host_user, PLAYER_TOKEN)

        response: TestResponse = self.client.post("/api/client/game/leave/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        game.refresh_from_db()
        self.assertEqual((game.host, game.host_army), (self.player, self.army))
        self.assertIsNone(game.second_player_army)

    def test_second_player_leaving_clears_army(self) -> None:
        game: Game = self.open_game()
        self.join(game)

        self.client.post("/api/client/game/leave/")

        game.refresh_from_db()
        self.assertIsNone(game.second_player_army)

    def test_army_in_game_cannot_be_changed_or_deleted(self) -> None:
        self.create_game()
        url: str = f"/api/client/armies/{self.army.id}/"

        response: TestResponse = self.client.put(url, self.army_payload(), format="json")
        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)

        response = self.client.delete(url)
        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)
        self.assertTrue(Army.objects.filter(pk=self.army.pk).exists())

    def test_army_is_free_again_after_game_ends(self) -> None:
        self.create_game()
        self.client.post("/api/client/game/leave/")

        response: TestResponse = self.client.delete(f"/api/client/armies/{self.army.id}/")

        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
