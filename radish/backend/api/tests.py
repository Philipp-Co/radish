"""
Tests fuer api/. GameServer-Endpunkt (server_views.py) und die
Matchmaking-Regeln rund um Beitreten (client_views.GameJoinView) --
Authentifizierung laeuft ueber echte Keycloak-Tokens (siehe
authentication.KeycloakJWTAuthentication), fuer Tests deshalb per
force_authenticate umgangen statt ein echtes JWT zu bauen. HasPlayerRole
(permissions.py) liest die Realm-Rolle aus request.auth -- deshalb wird bei
force_authenticate zusaetzlich ein token-Objekt mitgegeben, das sich wie
ein validiertes JWT mit "realm_access.roles" verhaelt.
"""

from django.contrib.auth import get_user_model
from rest_framework import status
from rest_framework.test import APITestCase

from .models import Game, GameServer, Player

User = get_user_model()

# Reicht als Ersatz fuer ein echtes, validiertes Keycloak-Token in Tests --
# HasPlayerRole liest nur request.auth.get("realm_access", {})["roles"]
# (siehe permissions._realm_roles), ein einfaches dict genuegt dafuer.
PLAYER_TOKEN = {"realm_access": {"roles": ["Player"]}}

# Analog zu PLAYER_TOKEN, aber fuer HasAdminRole (permissions.py) --
# steht fuer ein Token eines Nutzers mit der Realm-Rolle "Radish-Admin"
# (siehe docker/keycloak/realm-radish.json, User "radish-admin").
ADMIN_TOKEN = {"realm_access": {"roles": ["Radish-Admin"]}}


class ServerListTests(APITestCase):
    def setUp(self):
        self.user = User.objects.create(username="technischer-server-user")
        self.client.force_authenticate(user=self.user)

    def test_list_returns_all_registered_servers(self):
        GameServer.objects.create(name="server-a", ip_address="10.0.0.1", port=7000)
        GameServer.objects.create(
            name="server-b", ip_address="10.0.0.2", port=7001, is_occupied=True
        )

        response = self.client.get("/api/servers/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        names = [entry["name"] for entry in response.data["servers"]]
        self.assertEqual(names, ["server-a", "server-b"])
        occupied_by_name = {
            entry["name"]: entry["is_occupied"] for entry in response.data["servers"]
        }
        self.assertEqual(occupied_by_name, {"server-a": False, "server-b": True})

    def test_list_is_empty_without_registered_servers(self):
        response = self.client.get("/api/servers/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data["servers"], [])

    def test_get_by_name_returns_single_server(self):
        GameServer.objects.create(name="server-a", ip_address="10.0.0.1", port=7000)

        response = self.client.get("/api/servers/server-a/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data["name"], "server-a")
        self.assertEqual(response.data["port"], 7000)

    def test_get_by_unknown_name_returns_404(self):
        response = self.client.get("/api/servers/does-not-exist/")

        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)

    def test_list_requires_authentication(self):
        self.client.force_authenticate(user=None)

        response = self.client.get("/api/servers/")

        self.assertEqual(response.status_code, status.HTTP_401_UNAUTHORIZED)


class AdminServerListTests(APITestCase):
    """
    Deckt admin_views.AdminServerListView ab: nur mit der Realm-Rolle
    "Radish-Admin" erreichbar, sonst 401 (kein Token) bzw. 403 (Token ohne
    diese Rolle, z.B. ein normaler Player).
    """

    def setUp(self):
        self.admin_user = User.objects.create(username="radish-admin-user")

    def test_requires_authentication(self):
        response = self.client.get("/api/admin/servers/")

        self.assertEqual(response.status_code, status.HTTP_401_UNAUTHORIZED)

    def test_player_role_is_not_sufficient(self):
        self.client.force_authenticate(user=self.admin_user, token=PLAYER_TOKEN)

        response = self.client.get("/api/admin/servers/")

        self.assertEqual(response.status_code, status.HTTP_403_FORBIDDEN)

    def test_admin_role_can_list_servers(self):
        GameServer.objects.create(name="server-a", ip_address="10.0.0.1", port=7000)
        GameServer.objects.create(
            name="server-b", ip_address="10.0.0.2", port=7001, is_occupied=True
        )
        self.client.force_authenticate(user=self.admin_user, token=ADMIN_TOKEN)

        response = self.client.get("/api/admin/servers/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        names = [entry["name"] for entry in response.data["servers"]]
        self.assertEqual(names, ["server-a", "server-b"])


class GameJoinTests(APITestCase):
    """
    Deckt die Regel ab, die GameJoinView.already_in_game durchsetzen soll:
    wer schon Host oder zweiter Spieler eines Spiels ist, darf keinem
    weiteren Spiel beitreten -- insbesondere auch nicht dem eigenen, das
    er selbst per GameCreateView eroeffnet hat.
    """

    def setUp(self):
        self.host_user = User.objects.create(username="host-user")
        self.host_player = Player.objects.create(user=self.host_user)
        self.server = GameServer.objects.create(
            name="server-a", ip_address="10.0.0.1", port=7000, is_occupied=True
        )
        self.game = Game.objects.create(
            name="own-game", password="secret", server=self.server, host=self.host_player
        )

    def test_host_cannot_join_own_game(self):
        self.client.force_authenticate(user=self.host_user, token=PLAYER_TOKEN)

        response = self.client.post(
            "/api/client/game/join/", {"name": self.game.name, "password": "secret"}
        )

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)
        self.game.refresh_from_db()
        self.assertIsNone(self.game.second_player_id)

    def test_second_player_cannot_join_a_second_game(self):
        other_server = GameServer.objects.create(
            name="server-b", ip_address="10.0.0.2", port=7001
        )
        other_game = Game.objects.create(
            name="other-game", password="secret2", server=other_server, host=self.host_player
        )

        second_user = User.objects.create(username="second-user")
        second_player = Player.objects.create(user=second_user)
        self.game.second_player = second_player
        self.game.save(update_fields=["second_player"])

        self.client.force_authenticate(user=second_user, token=PLAYER_TOKEN)
        response = self.client.post(
            "/api/client/game/join/", {"name": other_game.name, "password": "secret2"}
        )

        self.assertEqual(response.status_code, status.HTTP_409_CONFLICT)
        other_game.refresh_from_db()
        self.assertIsNone(other_game.second_player_id)

    def test_unrelated_player_can_join_open_game(self):
        joining_user = User.objects.create(username="joining-user")
        self.client.force_authenticate(user=joining_user, token=PLAYER_TOKEN)

        response = self.client.post(
            "/api/client/game/join/", {"name": self.game.name, "password": "secret"}
        )

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.game.refresh_from_db()
        self.assertEqual(self.game.second_player.user, joining_user)
