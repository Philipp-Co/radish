from rest_framework import status
from rest_framework.test import APITestCase

from api.models import GameServer
from api.tests.helpers import SERVER_TOKEN, TestResponse, User, authenticate


class ServerListTests(APITestCase):
    def setUp(self) -> None:
        self.user = User.objects.create(username="technischer-server-user")
        authenticate(self.client, self.user, SERVER_TOKEN)

    def test_list_returns_all_registered_servers(self) -> None:
        GameServer.objects.create(name="server-a", address="10.0.0.1", port=7000)
        GameServer.objects.create(
            name="server-b", address="10.0.0.2", port=7001, is_occupied=True
        )

        response: TestResponse = self.client.get("/api/servers/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        names: list[str] = [entry["name"] for entry in response.data["servers"]]
        self.assertEqual(names, ["server-a", "server-b"])
        occupied_by_name: dict[str, bool] = {
            entry["name"]: entry["is_occupied"] for entry in response.data["servers"]
        }
        self.assertEqual(occupied_by_name, {"server-a": False, "server-b": True})

    def test_list_is_empty_without_registered_servers(self) -> None:
        response: TestResponse = self.client.get("/api/servers/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data["servers"], [])

    def test_get_by_name_returns_single_server(self) -> None:
        GameServer.objects.create(name="server-a", address="10.0.0.1", port=7000)

        response: TestResponse = self.client.get("/api/servers/server-a/")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data["name"], "server-a")
        self.assertEqual(response.data["port"], 7000)

    def test_get_by_unknown_name_returns_404(self) -> None:
        response: TestResponse = self.client.get("/api/servers/does-not-exist/")

        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)

    def test_list_requires_authentication(self) -> None:
        self.client.force_authenticate(user=None)

        response: TestResponse = self.client.get("/api/servers/")

        self.assertEqual(response.status_code, status.HTTP_401_UNAUTHORIZED)
