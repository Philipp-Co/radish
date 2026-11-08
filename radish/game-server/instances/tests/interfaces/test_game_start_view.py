# Die Mock-Parameter setzt der @mock.patch-Dekorator der Klasse in jeden
# Test ein, auch dort, wo der Test ihn nicht braucht.
# pylint: disable=unused-argument

from unittest import mock

from django.urls import reverse
from rest_framework import status
from rest_framework.test import APIClient, APISimpleTestCase

from instances.tests.helpers import CODES, SPIELDATEN, START, TestResponse


@mock.patch("instances.interfaces.game_start_view.prepare_game")
class GameStartViewTests(APISimpleTestCase):
    def setUp(self) -> None:
        self.url = reverse("instances:game-start", args=["game-instance-1"])

    def test_post_prepares_game_and_reports_ready(self, prepare_game: mock.MagicMock) -> None:
        response: TestResponse = self.client.post(self.url, START, format="json")
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.json(), {"instance": "game-instance-1", "status": "ready"})
        prepare_game.assert_called_once_with("game-instance-1", SPIELDATEN, CODES)

    def test_failed_preparation_is_server_error(self, prepare_game: mock.MagicMock) -> None:
        prepare_game.side_effect = OSError("Platte voll")
        response: TestResponse = self.client.post(self.url, START, format="json")
        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)

    def test_post_without_spieldaten_is_rejected(self, prepare_game: mock.MagicMock) -> None:
        response: TestResponse = self.client.post(self.url, {}, format="json")
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        prepare_game.assert_not_called()

    def test_post_without_valid_codes_is_rejected(self, prepare_game: mock.MagicMock) -> None:
        codes: object
        for codes in (None, [], CODES[:1], [CODES[0], CODES[0]], ["0", CODES[1]],
                      ["kein-hex", CODES[1]], ["1" * 17, CODES[1]], [17, CODES[1]]):
            request: dict[str, object] = dict(SPIELDATEN)
            if codes is not None:
                request["zugangscodes"] = codes
            response: TestResponse = self.client.post(self.url, request, format="json")
            self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST, codes)
        prepare_game.assert_not_called()

    def test_post_with_one_player_is_rejected(self, prepare_game: mock.MagicMock) -> None:
        one: dict[str, object] = {**START, "spieldaten": SPIELDATEN["spieldaten"][:1]}
        response: TestResponse = self.client.post(self.url, one, format="json")
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_post_without_gelaende_is_rejected(self, prepare_game: mock.MagicMock) -> None:
        gelaende: object
        for gelaende in (None, [], {"forest": 1}):
            request: dict[str, object] = {"spieldaten": SPIELDATEN["spieldaten"], "zugangscodes": CODES}
            if gelaende is not None:
                request["gelaende"] = gelaende
            response: TestResponse = self.client.post(self.url, request, format="json")
            self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST, gelaende)
        prepare_game.assert_not_called()

    def test_get_not_allowed(self, prepare_game: mock.MagicMock) -> None:
        response: TestResponse = self.client.get(self.url)
        self.assertEqual(response.status_code, status.HTTP_405_METHOD_NOT_ALLOWED)

    def test_post_without_csrf_token(self, prepare_game: mock.MagicMock) -> None:
        # Das Backend ruft ohne CSRF-Token auf (Server zu Server).
        client: APIClient = APIClient(enforce_csrf_checks=True)
        response: TestResponse = client.post(self.url, START, format="json")
        self.assertEqual(response.status_code, status.HTTP_200_OK)
