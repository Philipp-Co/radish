from rest_framework import status
from rest_framework.test import APITestCase

from api.models import GameServer
from api.tests.helpers import ADMIN_TOKEN, PLAYER_TOKEN, SERVER_TOKEN, TestResponse, User, authenticate


class ServerViewPermissionTests(APITestCase):
    """
    ServerView ist nur fuer den Server-Account (HasServerRole): wer einen
    Server registriert, bestimmt, wohin das Backend die Zucchini-Codes und
    die UDP-Pakete der Spieler schickt. Ein Spieler (oder Admin) bekommt 403
    -- fuer Auflisten, Registrieren und Loeschen.
    """

    def setUp(self) -> None:
        GameServer.objects.create(name="server-a", address="10.0.0.1", port=7000)
        self.user = User.objects.create(username="spieler")

    def test_player_cannot_list_servers(self) -> None:
        authenticate(self.client, self.user, PLAYER_TOKEN)

        self.assertEqual(self.client.get("/api/servers/").status_code, status.HTTP_403_FORBIDDEN)
        self.assertEqual(
            self.client.get("/api/servers/server-a/").status_code, status.HTTP_403_FORBIDDEN
        )

    def test_player_cannot_register_server(self) -> None:
        authenticate(self.client, self.user, PLAYER_TOKEN)

        response: TestResponse = self.client.post(
            "/api/servers/",
            {
                "name": "boese",
                "address": "10.0.0.66",
                "port": 9999,
                "control_url": "http://attacker.example",
                "instance_name": "game-instance-1",
            },
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_403_FORBIDDEN)
        self.assertFalse(GameServer.objects.filter(name="boese").exists())

    def test_player_cannot_delete_server(self) -> None:
        authenticate(self.client, self.user, PLAYER_TOKEN)

        response: TestResponse = self.client.delete("/api/servers/server-a/")

        self.assertEqual(response.status_code, status.HTTP_403_FORBIDDEN)
        self.assertTrue(GameServer.objects.filter(name="server-a").exists())

    def test_admin_cannot_register_server(self) -> None:
        authenticate(self.client, self.user, ADMIN_TOKEN)

        response: TestResponse = self.client.post(
            "/api/servers/", {"name": "x", "address": "a", "port": 1}, format="json"
        )

        self.assertEqual(response.status_code, status.HTTP_403_FORBIDDEN)

    def test_server_account_can_delete_server(self) -> None:
        authenticate(self.client, self.user, SERVER_TOKEN)

        response: TestResponse = self.client.delete("/api/servers/server-a/")

        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
        self.assertFalse(GameServer.objects.filter(name="server-a").exists())
