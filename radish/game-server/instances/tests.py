import json
import signal
import subprocess
import tempfile
from pathlib import Path
from unittest import mock

from django.test import override_settings
from django.urls import reverse
from rest_framework import status
from rest_framework.test import APIClient, APITestCase


# Gekuerzt -- vollstaendig siehe radish/game/schema/spielstart.schema.json.
SPIELDATEN = {
    "spieldaten": [
        {"name": "test-user", "kennung": "aB3xK9pQ", "armee": {"name": "A"}},
        {"name": "test-user-2", "kennung": "Zz1Yy2Xx", "armee": {"name": "B"}},
    ]
}

# Was das Backend schickt: die Spieldaten und daneben die geheimen Codes.
CODES = ["1122334455667788", "99aabbccddeeff00"]
START = {**SPIELDATEN, "zugangscodes": CODES}


@mock.patch("instances.views._prepare_game")
class GameStartViewTests(APITestCase):
    def setUp(self):
        self.url = reverse("instances:game-start", args=["game-instance-1"])

    def test_post_prepares_game_and_reports_ready(self, prepare_game):
        response = self.client.post(self.url, START, format="json")
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.json(), {"instance": "game-instance-1", "status": "ready"})
        prepare_game.assert_called_once_with("game-instance-1", SPIELDATEN, CODES)

    def test_failed_preparation_is_server_error(self, prepare_game):
        prepare_game.side_effect = OSError("Platte voll")
        response = self.client.post(self.url, START, format="json")
        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)

    def test_post_without_spieldaten_is_rejected(self, prepare_game):
        response = self.client.post(self.url, {}, format="json")
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)
        prepare_game.assert_not_called()

    def test_post_without_valid_codes_is_rejected(self, prepare_game):
        for codes in (None, [], CODES[:1], [CODES[0], CODES[0]], ["0", CODES[1]],
                      ["kein-hex", CODES[1]], ["1" * 17, CODES[1]], [17, CODES[1]]):
            request = dict(SPIELDATEN)
            if codes is not None:
                request["zugangscodes"] = codes
            response = self.client.post(self.url, request, format="json")
            self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST, codes)
        prepare_game.assert_not_called()

    def test_post_with_one_player_is_rejected(self, prepare_game):
        one = {"spieldaten": SPIELDATEN["spieldaten"][:1], "zugangscodes": CODES}
        response = self.client.post(self.url, one, format="json")
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_get_not_allowed(self, prepare_game):
        response = self.client.get(self.url)
        self.assertEqual(response.status_code, status.HTTP_405_METHOD_NOT_ALLOWED)

    def test_post_without_csrf_token(self, prepare_game):
        # Das Backend ruft ohne CSRF-Token auf (Server zu Server).
        client = APIClient(enforce_csrf_checks=True)
        response = client.post(self.url, START, format="json")
        self.assertEqual(response.status_code, status.HTTP_200_OK)


@mock.patch("instances.views._cancel_game")
class GameCancelViewTests(APITestCase):
    def setUp(self):
        self.url = reverse("instances:game-cancel", args=["game-instance-1"])

    def test_post_no_content(self, cancel_game):
        response = self.client.post(self.url)
        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
        cancel_game.assert_called_once_with("game-instance-1")

    def test_get_not_allowed(self, cancel_game):
        response = self.client.get(self.url)
        self.assertEqual(response.status_code, status.HTTP_405_METHOD_NOT_ALLOWED)

    def test_post_without_csrf_token(self, cancel_game):
        client = APIClient(enforce_csrf_checks=True)
        response = client.post(self.url)
        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)


class GameDataFileTests(APITestCase):
    """
    Start und Abbruch gegen ein echtes Verzeichnis (RADISH_GAME_DATA_DIR auf
    ein temporaeres umgebogen); nur das Signal selbst ist gemockt.
    """

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        override = override_settings(RADISH_GAME_DATA_DIR=self.tmp.name)
        override.enable()
        self.addCleanup(override.disable)

        self.directory = Path(self.tmp.name) / "game-instance-1"
        self.directory.mkdir()
        (self.directory / "radish_server.pid").write_text("4711\n")
        self.start_url = reverse("instances:game-start", args=["game-instance-1"])
        self.cancel_url = reverse("instances:game-cancel", args=["game-instance-1"])

        patcher = mock.patch("instances.views.os.kill")
        self.kill = patcher.start()
        self.addCleanup(patcher.stop)

        # Der Admin-Client von zucchini: nur seine Aufrufe zaehlen.
        patcher = mock.patch("instances.views.subprocess.run")
        self.run = patcher.start()
        self.addCleanup(patcher.stop)

    def whitelist_calls(self):
        return [call.args[0][1:] for call in self.run.call_args_list]

    def test_start_writes_file_and_signals_server(self):
        response = self.client.post(self.start_url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_200_OK)
        written = json.loads((self.directory / "spielstart.json").read_text())
        self.assertEqual(written, SPIELDATEN)
        self.assertFalse((self.directory / "spielstart.json.tmp").exists())
        self.kill.assert_called_once_with(4711, signal.SIGUSR1)

    def test_start_whitelists_codes_and_keeps_them_out_of_the_file(self):
        self.client.post(self.start_url, START, format="json")

        self.assertEqual(self.whitelist_calls(), [["whitelist-add", CODES[0]], ["whitelist-add", CODES[1]]])
        self.assertEqual(json.loads((self.directory / "zugang.json").read_text()), CODES)
        game_start = (self.directory / "spielstart.json").read_text()
        for code in CODES:
            self.assertNotIn(code, game_start)

    def test_restart_replaces_old_codes(self):
        (self.directory / "zugang.json").write_text(json.dumps(["aa", "bb"]))

        self.client.post(self.start_url, START, format="json")

        self.assertEqual(self.whitelist_calls(), [
            ["whitelist-remove", "aa"], ["whitelist-remove", "bb"],
            ["whitelist-add", CODES[0]], ["whitelist-add", CODES[1]],
        ])

    def test_failed_whitelist_is_server_error(self):
        self.run.side_effect = subprocess.CalledProcessError(1, "zucchini_admin_client")

        response = self.client.post(self.start_url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)
        self.assertFalse((self.directory / "spielstart.json").exists())
        self.kill.assert_not_called()
        # Gemerkt ist er trotzdem, damit er sich wieder austragen laesst.
        self.assertEqual(json.loads((self.directory / "zugang.json").read_text()), CODES)

    def test_cancel_revokes_codes(self):
        (self.directory / "zugang.json").write_text(json.dumps(CODES))

        self.client.post(self.cancel_url)

        self.assertEqual(self.whitelist_calls(), [["whitelist-remove", CODES[0]], ["whitelist-remove", CODES[1]]])
        self.assertFalse((self.directory / "zugang.json").exists())

    def test_start_replaces_previous_file(self):
        (self.directory / "spielstart.json").write_text("alt")

        self.client.post(self.start_url, START, format="json")

        self.assertEqual(json.loads((self.directory / "spielstart.json").read_text()), SPIELDATEN)

    def test_start_without_pid_file_fails(self):
        (self.directory / "radish_server.pid").unlink()

        response = self.client.post(self.start_url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)
        self.kill.assert_not_called()

    def test_start_with_stale_pid_fails(self):
        self.kill.side_effect = ProcessLookupError

        response = self.client.post(self.start_url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)

    def test_start_with_unreadable_pid_fails(self):
        (self.directory / "radish_server.pid").write_text("kaputt")

        response = self.client.post(self.start_url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)

    def test_start_without_instance_directory_fails(self):
        url = reverse("instances:game-start", args=["game-instance-2"])

        response = self.client.post(url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_500_INTERNAL_SERVER_ERROR)

    def test_instance_name_cannot_leave_data_dir(self):
        url = reverse("instances:game-start", args=[".."])

        response = self.client.post(url, START, format="json")

        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)
        self.assertFalse((Path(self.tmp.name).parent / "spielstart.json").exists())

    def test_cancel_removes_file_and_signals_server(self):
        (self.directory / "spielstart.json").write_text("{}")

        response = self.client.post(self.cancel_url)

        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
        self.assertFalse((self.directory / "spielstart.json").exists())
        self.kill.assert_called_once_with(4711, signal.SIGUSR1)

    def test_cancel_without_running_server_is_only_logged(self):
        (self.directory / "radish_server.pid").unlink()

        with self.assertLogs("instances.views", level="WARNING"):
            response = self.client.post(self.cancel_url)

        self.assertEqual(response.status_code, status.HTTP_204_NO_CONTENT)
