from typing import Unpack

from rest_framework import status
from rest_framework.test import APITestCase

from api.models import (
    GameServer,
)

from api.tests.helpers import (
    authenticate,
    SERVER_TOKEN,
    ServerRegistrationPayload,
    TestResponse,
    User,
)


class ServerRegisterTests(APITestCase):
    """
    server_views.ServerView.post: eine Instanz meldet sich mit globalem Namen,
    UDP-Adresse/-Port und ihrer Instanz-API (control_url + instance_name) an.
    """

    def setUp(self) -> None:
        authenticate(
            self.client, User.objects.create(username="technischer-server-user"), SERVER_TOKEN
        )

    def payload(self, **overrides: Unpack[ServerRegistrationPayload]) -> ServerRegistrationPayload:
        return {
            "name": "gs1-game-instance-1",
            "address": "game-server",
            "port": 9999,
            "control_url": "http://game-server:8000",
            "instance_name": "game-instance-1",
            **overrides,
        }

    def test_register_stores_instance_name(self) -> None:
        response: TestResponse = self.client.post("/api/servers/", self.payload(), format="json")

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        server: GameServer = GameServer.objects.get(name="gs1-game-instance-1")
        self.assertEqual(server.instance_name, "game-instance-1")
        self.assertEqual(server.port, 9999)

    def test_instance_name_may_repeat_across_containers(self) -> None:
        self.client.post("/api/servers/", self.payload(), format="json")

        response: TestResponse = self.client.post(
            "/api/servers/",
            self.payload(name="gs2-game-instance-1", control_url="http://game-server-2:8000"),
            format="json",
        )

        self.assertEqual(response.status_code, status.HTTP_201_CREATED)

    def test_instance_name_must_be_usable_in_a_url(self) -> None:
        response: TestResponse = self.client.post(
            "/api/servers/", self.payload(instance_name="../game-instance-1"), format="json"
        )

        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        self.assertIn("instance_name", response.data)
