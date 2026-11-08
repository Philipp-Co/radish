# Die Mock-Parameter setzt der @mock.patch-Dekorator der Klasse in jeden
# Test ein, auch dort, wo der Test ihn nicht braucht.
# pylint: disable=unused-argument

from unittest import mock

from django.urls import reverse
from rest_framework import status
from rest_framework.test import APIClient, APISimpleTestCase

from instances.tests.helpers import TestResponse


@mock.patch("instances.interfaces.game_cancel_view.cancel_game")
class GameCancelViewTests(APISimpleTestCase):
    def setUp(self) -> None:
        self.url = reverse("instances:game-cancel", args=["game-instance-1"])

    def test_post_no_content(self, cancel_game: mock.MagicMock) -> None:
        response: TestResponse = self.client.post(self.url)
        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
        cancel_game.assert_called_once_with("game-instance-1")

    def test_get_not_allowed(self, cancel_game: mock.MagicMock) -> None:
        response: TestResponse = self.client.get(self.url)
        self.assertEqual(response.status_code, status.HTTP_405_METHOD_NOT_ALLOWED)

    def test_post_without_csrf_token(self, cancel_game: mock.MagicMock) -> None:
        client: APIClient = APIClient(enforce_csrf_checks=True)
        response: TestResponse = client.post(self.url)
        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
